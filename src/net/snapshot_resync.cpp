#include "net/snapshot_resync.h"

#include <algorithm>
#include <cstring>
#include <zlib.h>

namespace net {

namespace {

template <typename T>
void WritePod(std::vector<uint8_t>& buf, const T& val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

template <typename T>
bool ReadPod(const uint8_t*& cur, const uint8_t* end, T& out) {
    if (static_cast<size_t>(end - cur) < sizeof(T)) return false;
    std::memcpy(&out, cur, sizeof(T));
    cur += sizeof(T);
    return true;
}

bool ZlibCompressBuffer(const std::vector<uint8_t>& src, std::vector<uint8_t>& outCompressed) {
    if (src.empty()) {
        outCompressed.clear();
        return true;
    }
    uLongf bound = compressBound(static_cast<uLong>(src.size()));
    outCompressed.resize(static_cast<size_t>(bound));
    const int res = compress2(outCompressed.data(), &bound,
                              src.data(), static_cast<uLong>(src.size()),
                              Z_BEST_SPEED);
    if (res != Z_OK) {
        outCompressed.clear();
        return false;
    }
    outCompressed.resize(static_cast<size_t>(bound));
    return true;
}

bool ZlibDecompressBuffer(const std::vector<uint8_t>& compressed,
                          uint32_t expectedUncompressedSize,
                          std::vector<uint8_t>& outRaw) {
    if (expectedUncompressedSize == 0) {
        outRaw.clear();
        return true;
    }
    outRaw.resize(expectedUncompressedSize);
    uLongf destLen = static_cast<uLongf>(expectedUncompressedSize);
    const int res = uncompress(outRaw.data(), &destLen,
                               compressed.data(), static_cast<uLong>(compressed.size()));
    if (res != Z_OK || destLen != static_cast<uLongf>(expectedUncompressedSize)) {
        outRaw.clear();
        return false;
    }
    return true;
}

} // namespace

bool FastResyncKeyframePackage::Build(const SimStateSnapshot& keyframe,
                                      const std::vector<TurnBatch>& fullHistory,
                                      int32_t targetLiveTurn,
                                      uint64_t expectedFinalHash,
                                      FastResyncKeyframePackage& outPkg) {
    outPkg = FastResyncKeyframePackage{};
    outPkg.keyframeTurn = keyframe.gameTurnTick;
    outPkg.targetLiveTurn = std::max(keyframe.gameTurnTick, targetLiveTurn);
    outPkg.keyframeHash = keyframe.stateHash;
    outPkg.expectedFinalHash = expectedFinalHash;

    std::vector<uint8_t> rawSnap = keyframe.SerializeBinary();
    outPkg.uncompressedSnapshotBytes = static_cast<uint32_t>(rawSnap.size());
    if (!ZlibCompressBuffer(rawSnap, outPkg.compressedSnapshot)) {
        return false;
    }

    for (const TurnBatch& b : fullHistory) {
        if (b.turn >= outPkg.keyframeTurn && b.turn < outPkg.targetLiveTurn) {
            outPkg.trailingBatches.push_back(b);
        }
    }
    return true;
}

bool FastResyncKeyframePackage::ApplyToSimulation(CSimulation& sim) const {
    std::vector<uint8_t> rawSnap;
    if (!ZlibDecompressBuffer(compressedSnapshot, uncompressedSnapshotBytes, rawSnap)) {
        return false;
    }

    SimStateSnapshot snap;
    if (!SimStateSnapshot::DeserializeBinary(rawSnap.data(), rawSnap.size(), snap)) {
        return false;
    }
    if (!snap.Restore(sim)) {
        return false;
    }
    if (keyframeHash != 0 && sim.HashState() != keyframeHash) {
        return false;
    }

    size_t batchIdx = 0;
    while (sim.GetGameTurnTick() < targetLiveTurn) {
        const int32_t curTurn = sim.GetGameTurnTick();
        while (batchIdx < trailingBatches.size() && trailingBatches[batchIdx].turn < curTurn) {
            batchIdx++;
        }
        if (batchIdx < trailingBatches.size() && trailingBatches[batchIdx].turn == curTurn) {
            ApplyBatch(sim, trailingBatches[batchIdx]);
            batchIdx++;
        }
        sim.Update();
    }

    if (expectedFinalHash != 0 && sim.HashState() != expectedFinalHash) {
        return false;
    }
    return true;
}

std::vector<uint8_t> FastResyncKeyframePackage::SerializeWire() const {
    std::vector<uint8_t> buf;
    buf.reserve(64 + compressedSnapshot.size() + trailingBatches.size() * 32);

    WritePod(buf, kWireMagic);
    WritePod(buf, keyframeTurn);
    WritePod(buf, targetLiveTurn);
    WritePod(buf, keyframeHash);
    WritePod(buf, expectedFinalHash);
    WritePod(buf, uncompressedSnapshotBytes);

    const uint32_t compSize = static_cast<uint32_t>(compressedSnapshot.size());
    WritePod(buf, compSize);
    if (compSize > 0) {
        buf.insert(buf.end(), compressedSnapshot.begin(), compressedSnapshot.end());
    }

    const uint32_t batchCount = static_cast<uint32_t>(trailingBatches.size());
    WritePod(buf, batchCount);
    for (const TurnBatch& b : trailingBatches) {
        std::vector<uint8_t> bWire = b.Serialize();
        const uint32_t bSize = static_cast<uint32_t>(bWire.size());
        WritePod(buf, bSize);
        buf.insert(buf.end(), bWire.begin(), bWire.end());
    }
    return buf;
}

bool FastResyncKeyframePackage::DeserializeWire(const uint8_t* data,
                                                size_t size,
                                                FastResyncKeyframePackage& outPkg) {
    if (!data || size < 36) return false;
    const uint8_t* cur = data;
    const uint8_t* end = data + size;

    uint32_t magic = 0;
    if (!ReadPod(cur, end, magic) || magic != kWireMagic) return false;

    outPkg = FastResyncKeyframePackage{};
    if (!ReadPod(cur, end, outPkg.keyframeTurn) ||
        !ReadPod(cur, end, outPkg.targetLiveTurn) ||
        !ReadPod(cur, end, outPkg.keyframeHash) ||
        !ReadPod(cur, end, outPkg.expectedFinalHash) ||
        !ReadPod(cur, end, outPkg.uncompressedSnapshotBytes)) {
        return false;
    }

    uint32_t compSize = 0;
    if (!ReadPod(cur, end, compSize)) return false;
    if (static_cast<size_t>(end - cur) < compSize) return false;
    outPkg.compressedSnapshot.assign(cur, cur + compSize);
    cur += compSize;

    uint32_t batchCount = 0;
    if (!ReadPod(cur, end, batchCount) || batchCount > 100000u) return false;
    outPkg.trailingBatches.reserve(batchCount);
    for (uint32_t i = 0; i < batchCount; ++i) {
        uint32_t bSize = 0;
        if (!ReadPod(cur, end, bSize)) return false;
        if (static_cast<size_t>(end - cur) < bSize) return false;
        TurnBatch batch;
        if (!TurnBatch::Deserialize(cur, bSize, batch)) return false;
        outPkg.trailingBatches.push_back(std::move(batch));
        cur += bSize;
    }
    return true;
}

bool SnapshotDeltaPacket::EncodeDelta(const SimStateSnapshot& baseSnap,
                                      const SimStateSnapshot& targetSnap,
                                      SnapshotDeltaPacket& outDelta) {
    outDelta = SnapshotDeltaPacket{};
    outDelta.baseTurn = baseSnap.gameTurnTick;
    outDelta.targetTurn = targetSnap.gameTurnTick;
    outDelta.baseHash = baseSnap.stateHash;
    outDelta.targetHash = targetSnap.stateHash;

    std::vector<uint8_t> baseBytes = baseSnap.SerializeBinary();
    std::vector<uint8_t> targetBytes = targetSnap.SerializeBinary();
    outDelta.targetUncompressedSize = static_cast<uint32_t>(targetBytes.size());

    std::vector<uint8_t> xorBuf(targetBytes.size(), 0u);
    const size_t commonLen = std::min(baseBytes.size(), targetBytes.size());
    for (size_t i = 0; i < commonLen; ++i) {
        xorBuf[i] = targetBytes[i] ^ baseBytes[i];
    }
    for (size_t i = commonLen; i < targetBytes.size(); ++i) {
        xorBuf[i] = targetBytes[i];
    }

    return ZlibCompressBuffer(xorBuf, outDelta.compressedXorDelta);
}

bool SnapshotDeltaPacket::DecodeDelta(const SimStateSnapshot& baseSnap,
                                      const SnapshotDeltaPacket& delta,
                                      SimStateSnapshot& outTargetSnap) {
    if (baseSnap.stateHash != delta.baseHash) {
        return false;
    }
    std::vector<uint8_t> xorBuf;
    if (!ZlibDecompressBuffer(delta.compressedXorDelta, delta.targetUncompressedSize, xorBuf)) {
        return false;
    }

    std::vector<uint8_t> baseBytes = baseSnap.SerializeBinary();
    std::vector<uint8_t> targetBytes(xorBuf.size(), 0u);
    const size_t commonLen = std::min(baseBytes.size(), xorBuf.size());
    for (size_t i = 0; i < commonLen; ++i) {
        targetBytes[i] = xorBuf[i] ^ baseBytes[i];
    }
    for (size_t i = commonLen; i < xorBuf.size(); ++i) {
        targetBytes[i] = xorBuf[i];
    }

    if (!SimStateSnapshot::DeserializeBinary(targetBytes.data(), targetBytes.size(), outTargetSnap)) {
        return false;
    }
    return outTargetSnap.stateHash == delta.targetHash;
}

bool RollingSnapshotRingBuffer::RecordIfDue(const CSimulation& sim, bool forceCapture) {
    const int32_t turn = sim.GetGameTurnTick();
    if (!forceCapture && (turn % intervalTurns_ != 0)) {
        return false;
    }
    if (!ring_.empty() && ring_.back().gameTurnTick == turn) {
        ring_.back() = SimStateSnapshot::Capture(sim);
        return true;
    }
    while (ring_.size() >= maxKeyframes_) {
        ring_.pop_front();
    }
    ring_.push_back(SimStateSnapshot::Capture(sim));
    return true;
}

const SimStateSnapshot* RollingSnapshotRingBuffer::GetLatestKeyframe() const {
    return ring_.empty() ? nullptr : &ring_.back();
}

const SimStateSnapshot* RollingSnapshotRingBuffer::FindKeyframeAtOrBefore(int32_t turn) const {
    for (auto it = ring_.rbegin(); it != ring_.rend(); ++it) {
        if (it->gameTurnTick <= turn) {
            return &(*it);
        }
    }
    return nullptr;
}

} // namespace net

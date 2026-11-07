#pragma once
// ============================================================================
// Stage S: Compressed Keyframe + Tail Lockstep Fast-Resync & Delta Snapshot Codec
//          (Inspired by Unity Netcode / Rollback Keyframe + Command Tail Catch-Up)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "ecs/sim_snapshot.h"
#include "net/turn_batch.h"

namespace net {

// Zlib-compressed keyframe snapshot + trailing TurnBatch tail so late-joining
// spectators or desynced peers can jump to `keyframeTurn` in O(1) and replay
// only the trailing turns (instead of replaying from Turn 0).
struct FastResyncKeyframePackage {
    static constexpr uint32_t kWireMagic = 0x52534B57u; // "WKSR"

    int32_t keyframeTurn = 0;
    int32_t targetLiveTurn = 0;
    uint64_t keyframeHash = 0;
    uint64_t expectedFinalHash = 0;
    uint32_t uncompressedSnapshotBytes = 0;
    std::vector<uint8_t> compressedSnapshot;
    std::vector<TurnBatch> trailingBatches;

    static bool Build(const SimStateSnapshot& keyframe,
                      const std::vector<TurnBatch>& fullHistory,
                      int32_t targetLiveTurn,
                      uint64_t expectedFinalHash,
                      FastResyncKeyframePackage& outPkg);

    // Restores the compressed keyframe onto `sim` and replays only the
    // trailing turns [keyframeTurn .. targetLiveTurn), verifying final hash.
    bool ApplyToSimulation(CSimulation& sim) const;

    std::vector<uint8_t> SerializeWire() const;
    static bool DeserializeWire(const uint8_t* data, size_t size, FastResyncKeyframePackage& outPkg);
};

// XOR + Zlib differential encoder between two SimStateSnapshots
struct SnapshotDeltaPacket {
    int32_t baseTurn = 0;
    int32_t targetTurn = 0;
    uint64_t baseHash = 0;
    uint64_t targetHash = 0;
    uint32_t targetUncompressedSize = 0;
    std::vector<uint8_t> compressedXorDelta;

    static bool EncodeDelta(const SimStateSnapshot& baseSnap,
                            const SimStateSnapshot& targetSnap,
                            SnapshotDeltaPacket& outDelta);

    static bool DecodeDelta(const SimStateSnapshot& baseSnap,
                            const SnapshotDeltaPacket& delta,
                            SimStateSnapshot& outTargetSnap);
};

class RollingSnapshotRingBuffer {
public:
    explicit RollingSnapshotRingBuffer(size_t maxKeyframes = 16, int32_t intervalTurns = 20)
        : maxKeyframes_(maxKeyframes > 0 ? maxKeyframes : 1),
          intervalTurns_(intervalTurns > 0 ? intervalTurns : 1) {}

    bool RecordIfDue(const CSimulation& sim, bool forceCapture = false);

    const SimStateSnapshot* GetLatestKeyframe() const;
    const SimStateSnapshot* FindKeyframeAtOrBefore(int32_t turn) const;

    size_t Size() const { return ring_.size(); }
    size_t GetMaxKeyframes() const { return maxKeyframes_; }
    int32_t GetIntervalTurns() const { return intervalTurns_; }
    void Clear() { ring_.clear(); }

private:
    size_t maxKeyframes_ = 16;
    int32_t intervalTurns_ = 20;
    std::deque<SimStateSnapshot> ring_;
};

} // namespace net

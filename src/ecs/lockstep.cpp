#include "ecs/lockstep.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr char kMagic[8] = {'W', 'S', 'M', 'R', 'E', 'P', '0', '1'};
constexpr uint32_t kMaxReplayCommands = 1024u * 1024u;

} // namespace

void SimCommand::Serialize(uint8_t* out) const {
    SimWireWriteLE32(out + 0, static_cast<uint32_t>(turn));
    SimWireWriteLE32(out + 4, static_cast<uint32_t>(player));
    SimWireWriteLE32(out + 8, static_cast<uint32_t>(unit));
    SimWireWriteLE32(out + 12, static_cast<uint32_t>(orderId));
    SimWireWriteLE32(out + 16, static_cast<uint32_t>(targetKind));
    SimWireWriteLE32(out + 20, static_cast<uint32_t>(targetHandle));
    SimWireWriteFloat(out + 24, x);
    SimWireWriteFloat(out + 28, y);
    SimWireWriteFloat(out + 32, z);
}

bool SimCommand::Deserialize(const uint8_t* data, size_t size, SimCommand& out) {
    if (!data || size < kWireSize) return false;
    out.turn = static_cast<int32_t>(SimWireReadLE32(data + 0));
    out.player = static_cast<int32_t>(SimWireReadLE32(data + 4));
    out.unit = static_cast<int32_t>(SimWireReadLE32(data + 8));
    out.orderId = static_cast<int32_t>(SimWireReadLE32(data + 12));
    out.targetKind = static_cast<int32_t>(SimWireReadLE32(data + 16));
    out.targetHandle = static_cast<int32_t>(SimWireReadLE32(data + 20));
    out.x = SimWireReadFloat(data + 24);
    out.y = SimWireReadFloat(data + 28);
    out.z = SimWireReadFloat(data + 32);
    return true;
}

std::vector<uint8_t> ReplayLog::SerializeMemory() const {
    std::vector<uint8_t> out(sizeof(kMagic) + 8 + commands_.size() * SimCommand::kWireSize);
    std::memcpy(out.data(), kMagic, sizeof(kMagic));
    SimWireWriteLE32(out.data() + 8, seed_);
    SimWireWriteLE32(out.data() + 12, static_cast<uint32_t>(commands_.size()));
    for (size_t i = 0; i < commands_.size(); ++i) {
        commands_[i].Serialize(out.data() + 16 + i * SimCommand::kWireSize);
    }
    return out;
}

bool ReplayLog::DeserializeMemory(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(kMagic) + 8) return false;
    if (std::memcmp(data, kMagic, sizeof(kMagic)) != 0) return false;
    const uint32_t seed = SimWireReadLE32(data + 8);
    const uint32_t count = SimWireReadLE32(data + 12);
    if (count > kMaxReplayCommands) return false;
    const size_t expectedSize = sizeof(kMagic) + 8 + static_cast<size_t>(count) * SimCommand::kWireSize;
    // Trailing garbage or truncated buffer fails the load.
    if (size != expectedSize) return false;

    std::vector<SimCommand> loaded(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!SimCommand::Deserialize(data + 16 + static_cast<size_t>(i) * SimCommand::kWireSize,
                                     SimCommand::kWireSize, loaded[i])) {
            return false;
        }
    }
    seed_ = seed;
    commands_ = std::move(loaded);
    return true;
}

bool ReplayLog::Save(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const std::vector<uint8_t> wire = SerializeMemory();
    const bool ok = std::fwrite(wire.data(), 1, wire.size(), f) == wire.size();
    std::fclose(f);
    return ok;
}

bool ReplayLog::Load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return false;
    }
    const long len = std::ftell(f);
    const size_t minReplayBytes = sizeof(kMagic) + 8;
    const size_t maxReplayBytes = minReplayBytes + static_cast<size_t>(kMaxReplayCommands) * SimCommand::kWireSize;
    if (len < static_cast<long>(minReplayBytes) ||
        static_cast<size_t>(len) > maxReplayBytes ||
        std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    const bool readOk = (len == 0) || (std::fread(buf.data(), 1, buf.size(), f) == buf.size());
    std::fclose(f);
    return readOk && DeserializeMemory(buf.data(), buf.size());
}

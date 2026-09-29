#include "ecs/lockstep.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

void WriteLE32(uint8_t* out, uint32_t v) {
    out[0] = (uint8_t)(v & 0xFF);
    out[1] = (uint8_t)((v >> 8) & 0xFF);
    out[2] = (uint8_t)((v >> 16) & 0xFF);
    out[3] = (uint8_t)((v >> 24) & 0xFF);
}

uint32_t ReadLE32(const uint8_t* in) {
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

void WriteFloat(uint8_t* out, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    WriteLE32(out, bits);
}

float ReadFloat(const uint8_t* in) {
    uint32_t bits = ReadLE32(in);
    float v = 0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

constexpr char kMagic[8] = {'W', 'S', 'M', 'R', 'E', 'P', '0', '1'};

} // namespace

void SimCommand::Serialize(uint8_t* out) const {
    WriteLE32(out + 0, (uint32_t)turn);
    WriteLE32(out + 4, (uint32_t)player);
    WriteLE32(out + 8, (uint32_t)unit);
    WriteLE32(out + 12, (uint32_t)orderId);
    WriteLE32(out + 16, (uint32_t)targetKind);
    WriteLE32(out + 20, (uint32_t)targetHandle);
    WriteFloat(out + 24, x);
    WriteFloat(out + 28, y);
    WriteFloat(out + 32, z);
}

bool SimCommand::Deserialize(const uint8_t* data, size_t size, SimCommand& out) {
    if (size < kWireSize) return false;
    out.turn = (int32_t)ReadLE32(data + 0);
    out.player = (int32_t)ReadLE32(data + 4);
    out.unit = (int32_t)ReadLE32(data + 8);
    out.orderId = (int32_t)ReadLE32(data + 12);
    out.targetKind = (int32_t)ReadLE32(data + 16);
    out.targetHandle = (int32_t)ReadLE32(data + 20);
    out.x = ReadFloat(data + 24);
    out.y = ReadFloat(data + 28);
    out.z = ReadFloat(data + 32);
    return true;
}

bool ReplayLog::Save(const std::string& path) const {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(kMagic, 1, sizeof(kMagic), f) == sizeof(kMagic);
    uint8_t header[8];
    WriteLE32(header + 0, seed_);
    WriteLE32(header + 4, (uint32_t)commands_.size());
    ok = ok && std::fwrite(header, 1, sizeof(header), f) == sizeof(header);
    std::vector<uint8_t> wire(SimCommand::kWireSize);
    for (const auto& cmd : commands_) {
        cmd.Serialize(wire.data());
        if (std::fwrite(wire.data(), 1, wire.size(), f) != wire.size()) {
            ok = false;
            break;
        }
    }
    std::fclose(f);
    return ok;
}

bool ReplayLog::Load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char magic[8] = {};
    uint32_t count = 0;
    bool ok = std::fread(magic, 1, sizeof(magic), f) == sizeof(magic) &&
              std::memcmp(magic, kMagic, sizeof(kMagic)) == 0;
    uint8_t header[8] = {};
    ok = ok && std::fread(header, 1, sizeof(header), f) == sizeof(header);
    if (ok) {
        seed_ = ReadLE32(header + 0);
        count = ReadLE32(header + 4);
    }
    std::vector<SimCommand> loaded;
    std::vector<uint8_t> wire(SimCommand::kWireSize);
    for (uint32_t i = 0; ok && i < count; ++i) {
        if (std::fread(wire.data(), 1, wire.size(), f) != wire.size()) {
            ok = false;
            break;
        }
        SimCommand cmd;
        if (!SimCommand::Deserialize(wire.data(), wire.size(), cmd)) {
            ok = false;
            break;
        }
        loaded.push_back(cmd);
    }
    // Trailing garbage fails the load: a torn write must not replay half.
    if (ok) {
        int tail = std::fgetc(f);
        if (tail != EOF) ok = false;
    }
    std::fclose(f);
    if (!ok) return false;
    commands_ = std::move(loaded);
    return true;
}

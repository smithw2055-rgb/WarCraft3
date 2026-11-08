#pragma once
// Deterministic lockstep core (P4 foundation, ref GameTurnManager): every
// player-originated command funnels through CSimulation::IssueUnitOrder or
// CSimulation::IssuePlayerCommand, which logs a serializable SimCommand and
// forwards it. Sim state hashes per turn for desync detection; ReplayLog
// persists seed plus commands so a fresh sim re-executing them reproduces
// the run bit-for-bit (same binary; floats hash by bit pattern).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "ecs/orders/order.h"

// Shared Little-Endian wire primitives used across lockstep, turn_batch, and lockstep_host.
inline void SimWireWriteLE32(uint8_t* out, uint32_t v) {
    out[0] = static_cast<uint8_t>(v & 0xFFu);
    out[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    out[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    out[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

inline uint32_t SimWireReadLE32(const uint8_t* in) {
    return static_cast<uint32_t>(in[0]) |
           (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
}

inline void SimWireWriteLE64(uint8_t* out, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<uint8_t>((v >> (i * 8)) & 0xFFu);
    }
}

inline uint64_t SimWireReadLE64(const uint8_t* in) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(in[i]) << (i * 8);
    }
    return v;
}

inline void SimWireWriteFloat(uint8_t* out, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    SimWireWriteLE32(out, bits);
}

inline float SimWireReadFloat(const uint8_t* in) {
    const uint32_t bits = SimWireReadLE32(in);
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

// One player command, fully serializable (widget targets travel as
// handle ids; -1 means none; unit <= 0 denotes a player-level global command).
struct SimCommand {
    int32_t turn = 0;
    int32_t player = -1;
    int32_t unit = -1;
    int32_t orderId = 0;
    int32_t targetKind = 0; // COrderTargetKind
    int32_t targetHandle = -1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    static constexpr size_t kWireSize = 36;
    void Serialize(uint8_t* out) const;
    static bool Deserialize(const uint8_t* data, size_t size, SimCommand& out);

    bool IsGlobalPlayerCommand() const { return unit <= 0; }

    bool operator==(const SimCommand& o) const {
        return turn == o.turn && player == o.player && unit == o.unit &&
               orderId == o.orderId && targetKind == o.targetKind &&
               targetHandle == o.targetHandle && x == o.x && y == o.y && z == o.z;
    }
    bool operator!=(const SimCommand& o) const { return !(*this == o); }
};

class CSimulation;

// Recorded command stream plus the initial RNG seed.
class ReplayLog {
public:
    void Begin(uint32_t seed) {
        seed_ = seed;
        commands_.clear();
    }
    void Record(const SimCommand& cmd) { commands_.push_back(cmd); }

    uint32_t Seed() const { return seed_; }
    const std::vector<SimCommand>& Commands() const { return commands_; }

    // Phase 26: In-memory binary serialization for zero-disk-IO replay streaming
    std::vector<uint8_t> SerializeMemory() const;
    bool DeserializeMemory(const uint8_t* data, size_t size);

    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

private:
    uint32_t seed_ = 0;
    std::vector<SimCommand> commands_;
};

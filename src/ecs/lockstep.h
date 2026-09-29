#pragma once
// Deterministic lockstep core (P4 foundation, ref GameTurnManager): every
// player-originated command funnels through CSimulation::IssueUnitOrder,
// which logs a serializable SimCommand and forwards it. Sim state hashes
// per turn for desync detection; ReplayLog persists seed plus commands so
// a fresh sim re-executing them reproduces the run bit-for-bit (same
// binary; floats hash by bit pattern).
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ecs/orders/order.h"

// One player command, fully serializable (widget targets travel as
// handle ids; -1 means none).
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

    bool Save(const std::string& path) const;
    bool Load(const std::string& path);

private:
    uint32_t seed_ = 0;
    std::vector<SimCommand> commands_;
};

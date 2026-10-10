#include "turn_batch.h"

#include <cstring>

#include "ecs/csimulation.h"
#include "ecs/orders/order.h"

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

void WriteLE64(uint8_t* out, uint64_t v) {
    for (int i = 0; i < 8; i++) out[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
}

uint64_t ReadLE64(const uint8_t* in) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)in[i] << (i * 8);
    return v;
}

} // namespace

std::vector<uint8_t> TurnBatch::Serialize() const {
    std::vector<uint8_t> out(8 + commands.size() * SimCommand::kWireSize + 8);
    WriteLE32(out.data() + 0, (uint32_t)turn);
    WriteLE32(out.data() + 4, (uint32_t)commands.size());
    for (size_t i = 0; i < commands.size(); i++) {
        commands[i].Serialize(out.data() + 8 + i * SimCommand::kWireSize);
    }
    WriteLE64(out.data() + 8 + commands.size() * SimCommand::kWireSize, stateHash);
    return out;
}

bool TurnBatch::Deserialize(const uint8_t* data, size_t size, TurnBatch& out) {
    if (size < 16) return false;
    const uint32_t count = ReadLE32(data + 4);
    const size_t want = 8 + (size_t)count * SimCommand::kWireSize + 8;
    if (size < want) return false;
    out.turn = (int32_t)ReadLE32(data + 0);
    out.commands.clear();
    for (uint32_t i = 0; i < count; i++) {
        SimCommand cmd;
        if (!SimCommand::Deserialize(data + 8 + i * SimCommand::kWireSize,
                                     SimCommand::kWireSize, cmd)) {
            return false;
        }
        out.commands.push_back(cmd);
    }
    out.stateHash = ReadLE64(data + 8 + (size_t)count * SimCommand::kWireSize);
    return true;
}

std::unique_ptr<COrder> OrderFromCommand(CSimulation& sim, const SimCommand& cmd) {
    if (cmd.targetKind == 1) {
        return std::make_unique<COrderTargetPoint>(cmd.orderId, cmd.x, cmd.y, cmd.z);
    }
    if (cmd.targetKind == 2) {
        CWidget* target = sim.GetWidget(cmd.targetHandle);
        if (!target) return nullptr;
        return std::make_unique<COrderTargetWidget>(cmd.orderId, target);
    }
    return std::make_unique<COrderNoTarget>(cmd.orderId);
}

int ApplyBatch(CSimulation& sim, const TurnBatch& batch) {
    int applied = 0;
    for (const auto& cmd : batch.commands) {
        auto order = OrderFromCommand(sim, cmd);
        if (!order) continue;
        if (sim.IssueUnitOrder(cmd.unit, std::move(order))) applied++;
    }
    return applied;
}

std::vector<uint8_t> ResyncPackage::Serialize() const {
    std::vector<uint8_t> out(8 + commands.size() * SimCommand::kWireSize);
    WriteLE32(out.data() + 0, seed);
    WriteLE32(out.data() + 4, (uint32_t)commands.size());
    for (size_t i = 0; i < commands.size(); i++) {
        commands[i].Serialize(out.data() + 8 + i * SimCommand::kWireSize);
    }
    return out;
}

bool ResyncPackage::Deserialize(const uint8_t* data, size_t size, ResyncPackage& out) {
    if (size < 8) return false;
    const uint32_t count = ReadLE32(data + 4);
    if (count > kMaxCommands) return false;
    const size_t want = 8 + (size_t)count * SimCommand::kWireSize;
    if (size < want) return false;
    out.seed = ReadLE32(data + 0);
    out.commands.clear();
    for (uint32_t i = 0; i < count; i++) {
        SimCommand cmd;
        if (!SimCommand::Deserialize(data + 8 + i * SimCommand::kWireSize,
                                     SimCommand::kWireSize, cmd)) {
            return false;
        }
        out.commands.push_back(cmd);
    }
    return true;
}

ResyncPackage ResyncPackage::FromHistory(uint32_t seed,
                                         const std::vector<TurnBatch>& history) {
    ResyncPackage pkg;
    pkg.seed = seed;
    for (const auto& batch : history) {
        for (const auto& cmd : batch.commands) pkg.commands.push_back(cmd);
    }
    return pkg;
}

std::vector<uint8_t> StartGamePackage::Serialize() const {
    std::vector<uint8_t> out(16);
    WriteLE32(out.data() + 0, seed);
    WriteLE64(out.data() + 4, mapHash);
    WriteLE32(out.data() + 12, (uint32_t)numPlayers);
    return out;
}

bool StartGamePackage::Deserialize(const uint8_t* data, size_t size,
                                   StartGamePackage& out) {
    if (size < 16 || !data) return false;
    out.seed = ReadLE32(data + 0);
    out.mapHash = ReadLE64(data + 4);
    out.numPlayers = (int32_t)ReadLE32(data + 12);
    if (out.numPlayers < 0 || out.numPlayers > 64) return false;
    return true;
}

bool DesyncTracker::Check(int32_t turn, uint64_t expected, uint64_t actual) {
    if (expected == 0 && actual == 0) return true; // unstamped: nothing to compare
    if (expected == actual) return true;
    mismatchCount++;
    if (!desynced) {
        desynced = true;
        firstTurn = turn;
        expectedHash = expected;
        actualHash = actual;
    }
    return false;
}

void DesyncTracker::Reset() {
    desynced = false;
    firstTurn = -1;
    expectedHash = 0;
    actualHash = 0;
    mismatchCount = 0;
}

#include "turn_batch.h"

#include <cstring>

#include "ecs/csimulation.h"
#include "ecs/orders/order.h"

namespace {

void SerializeCommandArray(uint8_t* dst, const std::vector<SimCommand>& commands) {
    for (size_t i = 0; i < commands.size(); ++i) {
        commands[i].Serialize(dst + i * SimCommand::kWireSize);
    }
}

bool DeserializeCommandArray(const uint8_t* src, uint32_t count, std::vector<SimCommand>& outCommands) {
    outCommands.clear();
    outCommands.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        SimCommand cmd;
        if (!SimCommand::Deserialize(src + static_cast<size_t>(i) * SimCommand::kWireSize,
                                     SimCommand::kWireSize, cmd)) {
            return false;
        }
        outCommands.push_back(cmd);
    }
    return true;
}

} // namespace

std::vector<uint8_t> TurnBatch::Serialize() const {
    const size_t cmdBytes = commands.size() * SimCommand::kWireSize;
    std::vector<uint8_t> out(8 + cmdBytes + 8);
    SimWireWriteLE32(out.data() + 0, static_cast<uint32_t>(turn));
    SimWireWriteLE32(out.data() + 4, static_cast<uint32_t>(commands.size()));
    SerializeCommandArray(out.data() + 8, commands);
    SimWireWriteLE64(out.data() + 8 + cmdBytes, stateHash);
    return out;
}

bool TurnBatch::Deserialize(const uint8_t* data, size_t size, TurnBatch& out) {
    if (!data || size < 16) return false;
    const uint32_t count = SimWireReadLE32(data + 4);
    if (count > ResyncPackage::kMaxCommands) return false;
    const size_t cmdBytes = static_cast<size_t>(count) * SimCommand::kWireSize;
    if (size < 16 + cmdBytes) return false;
    out.turn = static_cast<int32_t>(SimWireReadLE32(data + 0));
    if (!DeserializeCommandArray(data + 8, count, out.commands)) return false;
    out.stateHash = SimWireReadLE64(data + 8 + cmdBytes);
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
        if (sim.ExecuteSimCommand(cmd)) {
            applied++;
        }
    }
    return applied;
}

std::vector<uint8_t> ResyncPackage::Serialize() const {
    const size_t cmdBytes = commands.size() * SimCommand::kWireSize;
    std::vector<uint8_t> out(8 + cmdBytes);
    SimWireWriteLE32(out.data() + 0, seed);
    SimWireWriteLE32(out.data() + 4, static_cast<uint32_t>(commands.size()));
    SerializeCommandArray(out.data() + 8, commands);
    return out;
}

bool ResyncPackage::Deserialize(const uint8_t* data, size_t size, ResyncPackage& out) {
    if (!data || size < 8) return false;
    const uint32_t count = SimWireReadLE32(data + 4);
    if (count > kMaxCommands) return false;
    const size_t want = 8 + static_cast<size_t>(count) * SimCommand::kWireSize;
    if (size < want) return false;
    out.seed = SimWireReadLE32(data + 0);
    return DeserializeCommandArray(data + 8, count, out.commands);
}

ResyncPackage ResyncPackage::FromHistory(uint32_t seed,
                                         const std::vector<TurnBatch>& history) {
    ResyncPackage pkg;
    pkg.seed = seed;
    for (const auto& batch : history) {
        pkg.commands.insert(pkg.commands.end(), batch.commands.begin(), batch.commands.end());
    }
    return pkg;
}

std::vector<uint8_t> StartGamePackage::Serialize() const {
    std::vector<uint8_t> out(16);
    SimWireWriteLE32(out.data() + 0, seed);
    SimWireWriteLE64(out.data() + 4, mapHash);
    SimWireWriteLE32(out.data() + 12, static_cast<uint32_t>(numPlayers));
    return out;
}

bool StartGamePackage::Deserialize(const uint8_t* data, size_t size,
                                   StartGamePackage& out) {
    if (size < 16 || !data) return false;
    out.seed = SimWireReadLE32(data + 0);
    out.mapHash = SimWireReadLE64(data + 4);
    out.numPlayers = static_cast<int32_t>(SimWireReadLE32(data + 12));
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

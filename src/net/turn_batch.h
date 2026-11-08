#pragma once
// Lockstep turn batches (ref GameTurnManager, wire transport-agnostic).
//
// Each turn every player submits its command list (possibly empty); the
// host merges them into one TurnBatch, broadcasts it, and every peer
// applies the same batch before stepping. stateHash carries the sender's
// post-turn hash for desync detection. Wire: turn LE32 + count LE32 +
// N x SimCommand::kWireSize + hash LE64.
#include <cstdint>
#include <memory>
#include <vector>

#include "ecs/lockstep.h"

class COrder;
class CSimulation;

struct TurnBatch {
    int32_t turn = 0;
    std::vector<SimCommand> commands;
    uint64_t stateHash = 0;

    std::vector<uint8_t> Serialize() const;
    static bool Deserialize(const uint8_t* data, size_t size, TurnBatch& out);
};

// Rebuild the engine order from a wire command. Null when a widget target
// no longer resolves (stale handle); callers skip nulls deterministically.
std::unique_ptr<COrder> OrderFromCommand(CSimulation& sim, const SimCommand& cmd);

// Apply every reconstructible command of a batch through
// CSimulation::ExecuteSimCommand (supporting both unit orders and module-level
// global player commands). Returns the number applied.
int ApplyBatch(CSimulation& sim, const TurnBatch& batch);

// Game-start handshake (the missing seed distribution that made every MP
// game desync-by-construction): the host broadcasts seed + map checksum +
// seat count; peers seed their sim RNG and verify the map before turn 0.
// Wire: seed LE32 + mapHash LE64 + numPlayers LE32 (fixed 16 bytes).
struct StartGamePackage {
    uint32_t seed = 0;
    uint64_t mapHash = 0;
    int32_t numPlayers = 0;

    std::vector<uint8_t> Serialize() const;
    static bool Deserialize(const uint8_t* data, size_t size, StartGamePackage& out);
};

// Peer-side desync detection for stamped TurnBatch.stateHash values.
// Feed every received batch AFTER applying it: Check(turn, batch.stateHash,
// sim.HashState()). A 0/0 pair (unstamped practice games) passes; any other
// mismatch latches the first occurrence for the UI pause dialog.
struct DesyncTracker {
    bool desynced = false;
    int32_t firstTurn = -1;
    uint64_t expectedHash = 0; // host's stamp
    uint64_t actualHash = 0;   // local post-turn hash
    int mismatchCount = 0;

    // False on mismatch (first one latches). Never throws.
    bool Check(int32_t turn, uint64_t expected, uint64_t actual);
    void Reset();
};

// Full-game resync package for rejoining peers: the initial RNG seed plus
// every command ever merged (turn-stamped, flat). A rejoiner seeds a fresh
// sim, replays per turn, and lands on the live hash. Wire: seed LE32 +
// count LE32 + N x SimCommand::kWireSize (capped at 1M commands).
struct ResyncPackage {
    static constexpr size_t kMaxCommands = 1024u * 1024u;

    uint32_t seed = 0;
    std::vector<SimCommand> commands;

    std::vector<uint8_t> Serialize() const;
    static bool Deserialize(const uint8_t* data, size_t size, ResyncPackage& out);

    // Flatten a turn history (e.g. TurnManager::History()) into one package.
    static ResyncPackage FromHistory(uint32_t seed,
                                     const std::vector<TurnBatch>& history);
};

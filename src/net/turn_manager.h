#pragma once
// Host-side turn collection (ref GameTurnManager gather phase): every
// player submits exactly one command list per turn (empty when idle); the
// batch is complete once all seats reported. Merged output is sorted by
// (player, unit, orderId) so all peers execute in one deterministic order
// regardless of arrival sequence.
#include <vector>

#include "ecs/lockstep.h"

struct TurnBatch;

class TurnManager {
public:
    explicit TurnManager(int numPlayers);

    int NumPlayers() const { return numPlayers_; }
    int Turn() const { return turn_; }

    // Submit one player's list for the current turn. False on bad player
    // index or double submit (no state change).
    bool SubmitPlayerCommands(int player, std::vector<SimCommand> commands);
    bool HasSubmitted(int player) const;
    bool IsComplete() const;

    // Drop a seat (disconnect): it stops gating completion and contributes
    // nothing further. Seat numbering never shifts. False on bad index.
    bool DropPlayer(int player);
    bool IsDropped(int player) const;
    // Re-admit a dropped seat (rejoin handshake): clears the dropped flag
    // so the seat gates completion again. Call at a turn boundary; the
    // rejoiner must first replay history to the live turn.
    bool RejoinPlayer(int player);

    // Merged, deterministically sorted batch; advances to the next turn.
    // Only valid when IsComplete(); empty commands when called early.
    // Taken batches append to History() for resync packages.
    TurnBatch TakeBatch();

    // Every merged batch so far, in turn order (unbounded; short games in
    // scope, a ring cap is a follow-up).
    const std::vector<TurnBatch>& History() const { return history_; }

    // Seed a fresh manager from another host's history (host migration):
    // turn counter continues, past batches are kept for resync packages.
    // Pending submits reset; dropped seats stay dropped until re-claimed.
    void SeedFromHistory(std::vector<TurnBatch> history);

private:
    int numPlayers_;
    int turn_ = 0;
    std::vector<std::vector<SimCommand>> pending_;
    std::vector<bool> submitted_;
    std::vector<bool> dropped_;
    std::vector<TurnBatch> history_;
};

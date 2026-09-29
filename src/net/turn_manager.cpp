#include "turn_manager.h"

#include <algorithm>

#include "net/turn_batch.h"

TurnManager::TurnManager(int numPlayers)
    : numPlayers_(numPlayers < 1 ? 1 : numPlayers),
      pending_((size_t)numPlayers_),
      submitted_((size_t)numPlayers_, false),
      dropped_((size_t)numPlayers_, false) {}

bool TurnManager::SubmitPlayerCommands(int player, std::vector<SimCommand> commands) {
    if (player < 0 || player >= numPlayers_) return false;
    if (dropped_[(size_t)player]) return false;
    if (submitted_[(size_t)player]) return false;
    pending_[(size_t)player] = std::move(commands);
    submitted_[(size_t)player] = true;
    return true;
}

bool TurnManager::HasSubmitted(int player) const {
    if (player < 0 || player >= numPlayers_) return false;
    return submitted_[(size_t)player] || dropped_[(size_t)player];
}

bool TurnManager::IsComplete() const {
    for (int i = 0; i < numPlayers_; i++) {
        if (!submitted_[(size_t)i] && !dropped_[(size_t)i]) return false;
    }
    return true;
}

bool TurnManager::DropPlayer(int player) {
    if (player < 0 || player >= numPlayers_) return false;
    dropped_[(size_t)player] = true;
    pending_[(size_t)player].clear();
    return true;
}

bool TurnManager::IsDropped(int player) const {
    if (player < 0 || player >= numPlayers_) return false;
    return dropped_[(size_t)player];
}

bool TurnManager::RejoinPlayer(int player) {
    if (player < 0 || player >= numPlayers_) return false;
    if (!dropped_[(size_t)player]) return false;
    dropped_[(size_t)player] = false;
    return true;
}

TurnBatch TurnManager::TakeBatch() {
    TurnBatch batch;
    batch.turn = turn_;
    if (!IsComplete()) return batch;
    for (auto& list : pending_) {
        for (auto& cmd : list) batch.commands.push_back(cmd);
    }
    std::sort(batch.commands.begin(), batch.commands.end(),
              [](const SimCommand& a, const SimCommand& b) {
                  if (a.player != b.player) return a.player < b.player;
                  if (a.unit != b.unit) return a.unit < b.unit;
                  return a.orderId < b.orderId;
              });
    pending_.assign((size_t)numPlayers_, {});
    submitted_.assign((size_t)numPlayers_, false);
    turn_++;
    history_.push_back(batch);
    return batch;
}

void TurnManager::SeedFromHistory(std::vector<TurnBatch> history) {
    history_ = std::move(history);
    turn_ = (int)history_.size();
    pending_.assign((size_t)numPlayers_, {});
    submitted_.assign((size_t)numPlayers_, false);
}

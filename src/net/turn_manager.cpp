#include "turn_manager.h"

#include <algorithm>

#include "net/turn_batch.h"

TurnManager::TurnManager(int numPlayers)
    : numPlayers_(numPlayers < 1 ? 1 : numPlayers),
      pending_(static_cast<size_t>(numPlayers_)),
      seatFlags_(static_cast<size_t>(numPlayers_), 0u) {}

bool TurnManager::SubmitPlayerCommands(int player, std::vector<SimCommand> commands) {
    if (player < 0 || player >= numPlayers_) return false;
    const size_t idx = static_cast<size_t>(player);
    if ((seatFlags_[idx] & (kSeatDropped | kSeatSubmitted)) != 0u) return false;
    pending_[idx] = std::move(commands);
    seatFlags_[idx] |= kSeatSubmitted;
    return true;
}

bool TurnManager::HasSubmitted(int player) const {
    if (player < 0 || player >= numPlayers_) return false;
    return (seatFlags_[static_cast<size_t>(player)] & (kSeatSubmitted | kSeatDropped)) != 0u;
}

bool TurnManager::IsComplete() const {
    for (int i = 0; i < numPlayers_; ++i) {
        if ((seatFlags_[static_cast<size_t>(i)] & (kSeatSubmitted | kSeatDropped)) == 0u) {
            return false;
        }
    }
    return true;
}

bool TurnManager::DropPlayer(int player) {
    if (player < 0 || player >= numPlayers_) return false;
    const size_t idx = static_cast<size_t>(player);
    seatFlags_[idx] |= kSeatDropped;
    pending_[idx].clear();
    return true;
}

bool TurnManager::IsDropped(int player) const {
    if (player < 0 || player >= numPlayers_) return false;
    return (seatFlags_[static_cast<size_t>(player)] & kSeatDropped) != 0u;
}

bool TurnManager::RejoinPlayer(int player) {
    if (player < 0 || player >= numPlayers_) return false;
    const size_t idx = static_cast<size_t>(player);
    if ((seatFlags_[idx] & kSeatDropped) == 0u) return false;
    seatFlags_[idx] &= static_cast<uint8_t>(~kSeatDropped);
    return true;
}

void TurnManager::SetMaxHistoryTurns(size_t maxTurns) {
    maxHistoryTurns_ = maxTurns;
    EnforceHistoryCap();
}

void TurnManager::EnforceHistoryCap() {
    if (maxHistoryTurns_ > 0 && history_.size() > maxHistoryTurns_) {
        const size_t excess = history_.size() - maxHistoryTurns_;
        history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(excess));
        prunedTurnCount_ += excess;
    }
}

TurnBatch TurnManager::TakeBatch() {
    TurnBatch batch;
    batch.turn = turn_;
    if (!IsComplete()) return batch;
    for (auto& list : pending_) {
        for (auto& cmd : list) batch.commands.push_back(cmd);
        list.clear();
    }
    std::sort(batch.commands.begin(), batch.commands.end(),
              [](const SimCommand& a, const SimCommand& b) {
                  if (a.player != b.player) return a.player < b.player;
                  if (a.unit != b.unit) return a.unit < b.unit;
                  return a.orderId < b.orderId;
              });
    for (uint8_t& flags : seatFlags_) {
        flags &= static_cast<uint8_t>(~kSeatSubmitted);
    }
    turn_++;
    history_.push_back(batch);
    EnforceHistoryCap();
    return batch;
}

void TurnManager::SeedFromHistory(std::vector<TurnBatch> history) {
    history_ = std::move(history);
    turn_ = history_.empty() ? 0 : (history_.back().turn + 1);
    prunedTurnCount_ = 0;
    EnforceHistoryCap();
    for (auto& list : pending_) list.clear();
    for (uint8_t& flags : seatFlags_) {
        flags &= static_cast<uint8_t>(~kSeatSubmitted);
    }
}

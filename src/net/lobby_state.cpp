#include "lobby_state.h"

LobbyState::LobbyState() {
    for (int i = 0; i < kMaxSlots; i++) {
        slots_[(size_t)i].color = -1;
    }
}

bool LobbyState::SetSlot(int index, const LobbySlot& slot) {
    if (index < 0 || index >= kMaxSlots) return false;
    if (slot.team < 0) return false;
    if (slot.color < -1 || slot.color >= kMaxColors) return false;
    if (slot.race < 0 || slot.race > maxRaceId_) return false;
    LobbySlot copy = slot;
    if (copy.controller == LobbySlotController::OPEN ||
        copy.controller == LobbySlotController::CLOSED) {
        copy.ready = false;
    }
    slots_[(size_t)index] = std::move(copy);
    return true;
}

int LobbyState::CountUsers() const {
    int n = 0;
    for (const auto& s : slots_) {
        if (s.controller == LobbySlotController::USER) n++;
    }
    return n;
}

int LobbyState::CountComputers() const {
    int n = 0;
    for (const auto& s : slots_) {
        if (s.controller == LobbySlotController::COMPUTER) n++;
    }
    return n;
}

bool LobbyState::AllUsersReady() const {
    bool any = false;
    for (const auto& s : slots_) {
        if (s.controller != LobbySlotController::USER) continue;
        any = true;
        if (!s.ready) return false;
    }
    return any;
}

bool LobbyState::IsReadyToStart() const {
    return CountUsers() >= 2 && AllUsersReady();
}

bool LobbyState::CanStartSkirmish() const {
    const int users = CountUsers();
    return users >= 1 && (users + CountComputers()) >= 2 && AllUsersReady();
}

void LobbyState::ResetReady() {
    for (auto& s : slots_) s.ready = false;
}

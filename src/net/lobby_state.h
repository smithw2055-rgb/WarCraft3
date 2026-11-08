#pragma once
// Pre-game lobby model (ref shared lobby state, transport-agnostic): 12
// slots with controller/team/race/color/name/ready. The host collects
// slots (locally or over the wire) and starts when every seated human is
// ready. Mapping to sim players (CPlayer/CMapControl) happens at game
// start; this class stays free of sim headers for UI reuse.
#include <array>
#include <cstdint>
#include <string>

enum class LobbySlotController : uint8_t {
    OPEN,     // free seat
    USER,     // human (local or remote)
    COMPUTER, // AI
    NEUTRAL,  // neutral/passive
    CLOSED,   // disabled seat
};

struct LobbySlot {
    std::string name;
    int team = 0;
    int race = 0;  // 0 random, 1..maxRaceId faction index
    int color = -1; // 0..11 player color, -1 unset
    LobbySlotController controller = LobbySlotController::OPEN;
    bool ready = false;
};

class LobbyState {
public:
    static constexpr int kMaxSlots = 12;
    static constexpr int kMaxColors = 12;
    static constexpr int kDefaultMaxRaceId = 4;

    LobbyState();

    int SlotCount() const { return kMaxSlots; }
    const LobbySlot& GetSlot(int index) const { return slots_.at((size_t)index); }
    // Validates index range, team >= 0, color in [-1, 11], race in [0, maxRaceId_].
    // Ready flags on CLOSED/OPEN seats are cleared. Returns false (no change) when invalid.
    bool SetSlot(int index, const LobbySlot& slot);

    // Phase 26: Configurable max faction/race index (defaults to 4 for War3 4 races + random)
    void SetMaxRaceId(int maxRaceId) { maxRaceId_ = maxRaceId >= 0 ? maxRaceId : 0; }
    int GetMaxRaceId() const { return maxRaceId_; }

    int CountUsers() const;
    int CountComputers() const;
    // Every seated human ready (vacuous with no humans -> false).
    bool AllUsersReady() const;
    // Retail multiplayer melee gate: at least 2 humans, all ready.
    bool IsReadyToStart() const;
    // Phase 26: Skirmish / coop gate: at least 1 human ready and at least 2 active players (USER + COMPUTER >= 2).
    bool CanStartSkirmish() const;
    void ResetReady();

private:
    int maxRaceId_ = kDefaultMaxRaceId;
    std::array<LobbySlot, kMaxSlots> slots_;
};

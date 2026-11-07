#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "resource/slk.h"

class CSimulation;
class CUnit;
class CDestructable;

enum class CAllianceType : uint8_t {
    PASSIVE, HELP, HELP_REQUEST, ALLIED, VISION, SHARED_CONTROL,
    SHARED_ADVANCED_CONTROL, SHARED_XP, SHARED_SPELLS, SHARED_VISION
};

enum class CMapControl : uint8_t {
    USER, COMPUTER, NEUTRAL, CREEP, NONE, RESCUABLE
};

enum class CPlayerState : uint8_t {
    GOLD, LUMBER, HERO_TOKENS, FOOD_CAP, FOOD_USED,
    GOLD_GATHERED, LUMBER_GATHERED, GIVES_BOUNTY,
    ALLIED_VICTORY, GAME_RESULT, OBSERVER
};

// Phase 14 Match / Victory-Defeat outcome per player
enum class CPlayerGameResult : uint8_t {
    PLAYING = 0,
    VICTORY = 1,
    DEFEAT  = 2,
    TIE     = 3,
    NEUTRAL = 4
};

class CPlayer {
public:
    CPlayer(int id, const std::string& name, CMapControl controller);

    int GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }
    CMapControl GetController() const { return controller_; }
    void SetController(CMapControl controller) { controller_ = controller; }

    // JASS player identity (ref CPlayer/CBasePlayer): race ordinal
    // (Human=1, Orc=2, Undead=3, NightElf=4), color index, observer flag.
    int GetRace() const { return race_; }
    void SetRace(int race) { race_ = race; }
    int GetColor() const { return color_; }
    void SetColor(int color) { color_ = color; }
    bool IsObserver() const { return observer_; }
    void SetObserver(bool observer) { observer_ = observer; }
    int GetTeam() const { return team_; }
    void SetTeam(int team) { team_ = team; }

    // Phase 14 Victory / Defeat / Crippled / Slot state
    CPlayerGameResult GetGameResult() const { return gameResult_; }
    void SetGameResult(CPlayerGameResult result) {
        gameResult_ = result;
        gameResultCode_ = static_cast<int>(result);
    }
    int GetGameResultCode() const { return gameResultCode_; }
    void SetGameResultCode(int code) {
        gameResultCode_ = code;
        if (code == 1) gameResult_ = CPlayerGameResult::VICTORY;
        else if (code == 2) gameResult_ = CPlayerGameResult::DEFEAT;
        else if (code == 3) gameResult_ = CPlayerGameResult::TIE;
        else if (code == 4) gameResult_ = CPlayerGameResult::NEUTRAL;
        else gameResult_ = CPlayerGameResult::PLAYING;
    }
    bool IsVictorious() const { return gameResult_ == CPlayerGameResult::VICTORY; }
    bool IsDefeated() const { return gameResult_ == CPlayerGameResult::DEFEAT; }
    bool IsPlaying() const { return slotActive_ && !observer_ && gameResult_ == CPlayerGameResult::PLAYING; }

    bool HasAlliedVictory() const { return alliedVictory_; }
    void SetAlliedVictory(bool v) { alliedVictory_ = v; }
    bool IsCrippled() const { return crippled_; }
    void SetCrippled(bool v) { crippled_ = v; }
    bool IsSlotActive() const { return slotActive_; }
    void SetSlotActive(bool v) { slotActive_ = v; }
    bool IsNoVictoryCheat() const { return noVictoryCheat_; }
    void SetNoVictoryCheat(bool v) { noVictoryCheat_ = v; }
    bool IsNoDefeatCheat() const { return noDefeatCheat_; }
    void SetNoDefeatCheat(bool v) { noDefeatCheat_ = v; }

    // Resources
    int GetGold() const { return gold_; }
    int GetLumber() const { return lumber_; }
    int GetFoodCap() const { return foodCap_; }
    int GetFoodUsed() const { return foodUsed_; }
    void SetGold(int v) { gold_ = v; }
    void SetLumber(int v) { lumber_ = v; }
    // Plain setter (ref CPlayer.setFoodCap): JASS SetPlayerState and map
    // setup bypass the ceiling. Gameplay increments use AddFoodCap below.
    void SetFoodCap(int v) { foodCap_ = v; }
    // Capped increment (ref Jass2 harvest-carry min with getFoodCapCeiling):
    // farms and carried food never push the cap past the ceiling.
    void AddFoodCap(int v) {
        foodCap_ = std::min(foodCap_ + v, (int)foodCapCeiling_);
    }
    void SetFoodUsed(int v) { foodUsed_ = v; }
    void AddGold(int v) { gold_ += v; }
    void AddLumber(int v) { lumber_ += v; }

    // Upkeep (Phase 14: delegates to active ISimulationModule when CSimulation is passed;
    // falls back to standard WarCraft 3 thresholds when called standalone without CSimulation).
    int GetGoldUpkeepRate() const { return goldUpkeepRate_; }
    int GetLumberUpkeepRate() const { return lumberUpkeepRate_; }
    void SetGoldUpkeepRate(int v) { goldUpkeepRate_ = v; }
    void SetLumberUpkeepRate(int v) { lumberUpkeepRate_ = v; }
    int GetUpkeepLevel(const CSimulation* sim = nullptr) const;
    int ApplyUpkeepGoldTax(int amount, const CSimulation* sim = nullptr) const;

    // Tech tree & Upgrade state (Phase 14: notifies ISimulationModule on level changes)
    bool IsTechUnlocked(War3ID id) const;
    void AddTechUnlocked(CSimulation& simulation, War3ID id);
    // Ref removeTechtreeUnlocked: possession counter, decremented on death
    // (non-heroes) and removal. Erased at zero so IsTechUnlocked flips.
    void RemoveTechUnlocked(CSimulation& simulation, War3ID id);
    int GetTechLevel(War3ID id) const;
    int GetEffectiveTechLevel(const CSimulation& simulation, War3ID id) const;
    void SetTechLevel(War3ID id, int level);
    void SetTechLevel(CSimulation& simulation, War3ID id, int level);
    int GetTechMaxAllowed(War3ID id) const;
    void SetTechMaxAllowed(War3ID id, int level);
    // Availability cap (ref CPlayer.isTechtreeAllowedByMax): banned or
    // capped tech (w3i lists, SetPlayerTechMaxAllowed) gates training and
    // research alongside the flat requirements. Counts live possession
    // (queued-but-unbuilt items are not counted yet).
    bool IsTechtreeAllowedByMax(War3ID id) const;
    int GetTechCount() const { return (int)techtreeUnlocked_.size(); }
    const std::unordered_map<uint32_t, int>& GetTechtreeUnlockedMap() const { return techtreeUnlocked_; }

    // Per-unit applied upgrade level tracking (used by War3GameModule upgrade engine)
    int GetAppliedUnitUpgradeLevel(int unitHandleId, War3ID upgradeId) const;
    void SetAppliedUnitUpgradeLevel(int unitHandleId, War3ID upgradeId, int level);
    void ClearAppliedUnitUpgrades(int unitHandleId);

    // Alliances
    bool HasAlliance(const CPlayer& other, CAllianceType type) const;
    void SetAlliance(int otherPlayerId, CAllianceType type, bool flag);

    // Units
    const std::vector<CUnit*>& GetUnits() const { return units_; }
    void AddUnit(CUnit* unit);
    void RemoveUnit(CUnit* unit);

    // Heroes
    const std::vector<CUnit*>& GetHeroes() const { return heroes_; }
    void AddHero(CUnit* hero);
    void RemoveHero(CUnit* hero);
    int GetHeroCount() const { return (int)heroes_.size(); }

    // Ability disable (player-level)
    bool IsAbilityDisabled(War3ID abilityCode) const {
        return disabledAbilities_.count(abilityCode) > 0;
    }
    void SetAbilityDisabled(War3ID abilityCode, bool disabled) {
        if (disabled) disabledAbilities_.insert(abilityCode);
        else disabledAbilities_.erase(abilityCode);
    }

    // Handicap (multiplier for XP, etc.)
    float GetHandicap() const { return handicap_; }
    void SetHandicap(float v) { handicap_ = v; }
    float GetHandicapXP() const { return handicapXP_; }
    void SetHandicapXP(float v) { handicapXP_ = v; }
    // Bounty flag (ref CPlayerState.GIVES_BOUNTY): only creep/neutral
    // aggressive players carry it; their kills pay gold/lumber bounties.
    bool GetGivesBounty() const { return givesBounty_; }
    void SetGivesBounty(bool v) { givesBounty_ = v; }

    // Hero tokens (retail PLAYER_STATE_RESOURCE_HERO_TOKENS): map-granted
    // currency spent INSTEAD of gold/lumber when training heroes (ref
    // queueTrainingUnit). Default 0; maps set it via SetPlayerState.
    int GetHeroTokens() const { return heroTokens_; }
    void SetHeroTokens(int v) { heroTokens_ = v < 0 ? 0 : v; }

    // Food
    float GetFoodCapMod() const { return foodCapCeiling_; }
    void SetFoodCapCeiling(float v) { foodCapCeiling_ = v; }

    float GetStartX() const { return startX_; }
    float GetStartY() const { return startY_; }
    void SetStartLocation(float x, float y) { startX_ = x; startY_ = y; }

private:
    int id_;
    std::string name_;
    CMapControl controller_;

    int gold_ = 0;
    int lumber_ = 0;
    int foodCap_ = 0;
    int foodUsed_ = 0;
    int heroTokens_ = 0;
    // Ref CPlayer default (101; retail increments clamp here). No MiscGame
    // key defines it; SetFoodCapCeiling overrides per map config.
    float foodCapCeiling_ = 101.0f;
    int goldUpkeepRate_ = 0;
    int lumberUpkeepRate_ = 0;

    float startX_ = 0;
    float startY_ = 0;

    std::vector<CUnit*> units_;
    std::vector<CUnit*> heroes_;

    std::unordered_map<uint32_t, int> techtreeUnlocked_;
    std::unordered_map<uint32_t, int> techtreeMaxAllowed_;
    std::unordered_map<uint64_t, int> appliedUnitUpgradeLevels_;
    std::unordered_set<War3ID> disabledAbilities_;
    float handicap_ = 1.0f;
    float handicapXP_ = 1.0f;
    bool givesBounty_ = false;
    int race_ = 0;
    int color_ = 0;
    bool observer_ = false;
    int team_ = -1;
    CPlayerGameResult gameResult_ = CPlayerGameResult::PLAYING;
    int gameResultCode_ = 0;
    bool alliedVictory_ = true;
    bool crippled_ = false;
    bool slotActive_ = true;
    bool noVictoryCheat_ = false;
    bool noDefeatCheat_ = false;
    int allianceData_[24] = {}; // bitfield per other player
};

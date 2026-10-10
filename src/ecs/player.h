#pragma once
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

    // Resources
    int GetGold() const { return gold_; }
    int GetLumber() const { return lumber_; }
    int GetFoodCap() const { return foodCap_; }
    int GetFoodUsed() const { return foodUsed_; }
    void SetGold(int v) { gold_ = v; }
    void SetLumber(int v) { lumber_ = v; }
    void SetFoodCap(int v) { foodCap_ = v; }
    void SetFoodUsed(int v) { foodUsed_ = v; }
    void AddGold(int v) { gold_ += v; }
    void AddLumber(int v) { lumber_ += v; }

    // Upkeep (ref CPlayer gold/lumberUpkeepRate + retail food thresholds):
    // rates are percent points set via PLAYER_STATE_GOLD/LUMBER_UPKEEP_RATE;
    // the food-derived level taxes gold income only (lumber untouched).
    // 0 = none (food <= 50), 1 = low (51-80, 30% tax), 2 = high (81+, 60%).
    int GetGoldUpkeepRate() const { return goldUpkeepRate_; }
    int GetLumberUpkeepRate() const { return lumberUpkeepRate_; }
    void SetGoldUpkeepRate(int v) { goldUpkeepRate_ = v; }
    void SetLumberUpkeepRate(int v) { lumberUpkeepRate_ = v; }
    int GetUpkeepLevel() const {
        if (foodUsed_ > 80) return 2;
        if (foodUsed_ > 50) return 1;
        return 0;
    }
    int ApplyUpkeepGoldTax(int amount) const {
        if (amount <= 0) return amount;
        int taxPct = goldUpkeepRate_ > 0 ? goldUpkeepRate_
                     : (GetUpkeepLevel() == 2 ? 60 : GetUpkeepLevel() == 1 ? 30 : 0);
        if (taxPct <= 0) return amount;
        if (taxPct >= 100) return 0;
        return amount - amount * taxPct / 100;
    }

    // Tech tree
    bool IsTechUnlocked(War3ID id) const;
    void AddTechUnlocked(CSimulation& simulation, War3ID id);
    int GetTechLevel(War3ID id) const;
    void SetTechLevel(War3ID id, int level);
    int GetTechMaxAllowed(War3ID id) const;
    void SetTechMaxAllowed(War3ID id, int level);
    int GetTechCount() const { return (int)techtreeUnlocked_.size(); }

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
    float foodCapCeiling_ = 100.0f;
    int goldUpkeepRate_ = 0;
    int lumberUpkeepRate_ = 0;

    float startX_ = 0;
    float startY_ = 0;

    std::vector<CUnit*> units_;
    std::vector<CUnit*> heroes_;

    std::unordered_map<uint32_t, int> techtreeUnlocked_;
    std::unordered_map<uint32_t, int> techtreeMaxAllowed_;
    std::unordered_set<War3ID> disabledAbilities_;
    float handicap_ = 1.0f;
    float handicapXP_ = 1.0f;
    int race_ = 0;
    int color_ = 0;
    bool observer_ = false;
    int team_ = -1;
    int allianceData_[24] = {}; // bitfield per other player
};

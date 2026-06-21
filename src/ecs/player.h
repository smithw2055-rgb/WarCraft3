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
    CMapControl GetController() const { return controller_; }

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

    // Tech tree
    bool IsTechUnlocked(War3ID id) const;
    void AddTechUnlocked(CSimulation& simulation, War3ID id);
    int GetTechLevel(War3ID id) const;
    void SetTechLevel(War3ID id, int level);

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

    float startX_ = 0;
    float startY_ = 0;

    std::vector<CUnit*> units_;
    std::vector<CUnit*> heroes_;

    std::unordered_map<uint32_t, int> techtreeUnlocked_;
    std::unordered_set<War3ID> disabledAbilities_;
    float handicap_ = 1.0f;
    int allianceData_[24] = {}; // bitfield per other player
};

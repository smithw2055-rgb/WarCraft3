#pragma once
#include "ecs/ability.h"
#include <unordered_set>
#include <vector>

// Hero stat value (STR/AGI/INT)
struct HeroStatValue {
    int base = 0;
    float perLevelFactor = 0;
    int bonus = 0;

    int GetCurrentBase(int level) const {
        return base + (int)(perLevelFactor * (float)(level - 1));
    }
    int GetCurrent(int level) const {
        return GetCurrentBase(level) + bonus;
    }
};

// CAbilityHero - the "AHer" ability
// Manages hero level, XP, skill points, and STR/AGI/INT stats
class CAbilityHero : public CAbility {
public:
    CAbilityHero(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Skill learning interception
    bool CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // XP and leveling
    int GetXP() const { return xp_; }
    void AddXP(CSimulation& game, CUnit* unit, int amount);
    int GetHeroLevel() const { return heroLevel_; }
    void SetHeroLevel(CSimulation& game, CUnit* unit, int level);

    // Skill points
    int GetSkillPoints() const { return skillPoints_; }
    void SetSkillPoints(int v) { skillPoints_ = v; }

    // Stats
    const HeroStatValue& GetStrength() const { return strength_; }
    const HeroStatValue& GetAgility() const { return agility_; }
    const HeroStatValue& GetIntelligence() const { return intelligence_; }

    void SetStrengthBase(int v) { strength_.base = v; }
    void SetAgilityBase(int v) { agility_.base = v; }
    void SetIntelligenceBase(int v) { intelligence_.base = v; }
    void SetStrengthPerLevel(float v) { strength_.perLevelFactor = v; }
    void SetAgilityPerLevel(float v) { agility_.perLevelFactor = v; }
    void SetIntelligencePerLevel(float v) { intelligence_.perLevelFactor = v; }

    // Proper name
    const std::string& GetProperName() const { return properName_; }
    void SetProperName(const std::string& name) { properName_ = name; }

    // Learnable skills
    void AddLearnableSkill(War3ID skillCode) { skillsAvailable_.insert(skillCode); }
    const std::unordered_set<War3ID>& GetLearnableSkills() const { return skillsAvailable_; }

private:
    void LearnSkill(CSimulation& game, CUnit* unit, War3ID skillCode);
    void CalculateDerivedStats(CSimulation& game, CUnit* unit);
    int GetXPForLevel(int level) const;

    int heroLevel_ = 1;
    int xp_ = 0;
    int skillPoints_ = 1; // heroes start with 1 skill point

    HeroStatValue strength_;
    HeroStatValue agility_;
    HeroStatValue intelligence_;

    std::string properName_;
    std::unordered_set<War3ID> skillsAvailable_;
};

#pragma once
#include "ecs/ability.h"
#include <unordered_set>
#include <vector>

// CAbilityHero - the "AHer" ability
// Manages hero level, XP, skill points, and STR/AGI/INT stats (HeroStatValue is defined in ecs/ability.h)
class CAbilityHero : public CAbility {
public:
    CAbilityHero(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Skill learning interception
    bool CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // XP and leveling (implements CAbility virtual hero capability contract)
    int GetXP() const override { return xp_; }
    void AddXP(CSimulation& game, CUnit* unit, int amount) override;
    void SetXP(CSimulation& game, CUnit* unit, int value) override;
    int GetHeroLevel() const override { return heroLevel_; }
    void SetHeroLevel(CSimulation& game, CUnit* unit, int level) override;
    bool StripLevels(CSimulation& game, CUnit* unit, int levels) override;
    bool SelectHeroSkill(CSimulation& game, CUnit* unit, War3ID skillCode) override;

    // Skill points
    int GetSkillPoints() const override { return skillPoints_; }
    void SetSkillPoints(int v) override { skillPoints_ = v; }

    // Stats
    const HeroStatValue& GetStrength() const override { return strength_; }
    const HeroStatValue& GetAgility() const override { return agility_; }
    const HeroStatValue& GetIntelligence() const override { return intelligence_; }

    void SetStrengthBase(int v) override { strength_.base = v; }
    void SetAgilityBase(int v) override { agility_.base = v; }
    void SetIntelligenceBase(int v) override { intelligence_.base = v; }
    void SetStrengthPerLevel(float v) { strength_.perLevelFactor = v; }
    void SetAgilityPerLevel(float v) { agility_.perLevelFactor = v; }
    void SetIntelligencePerLevel(float v) { intelligence_.perLevelFactor = v; }

    // Item/upgrade bonus stats (stack on top of base + per-level growth)
    void AddStrengthBonus(CSimulation& game, CUnit* unit, int v) override;
    void AddAgilityBonus(CSimulation& game, CUnit* unit, int v) override;
    void AddIntelligenceBonus(CSimulation& game, CUnit* unit, int v) override;

    // Proper name
    const std::string& GetProperName() const override { return properName_; }
    void SetProperName(const std::string& name) override { properName_ = name; }

    // Learnable skills
    void AddLearnableSkill(War3ID skillCode) { skillsAvailable_.insert(skillCode); }
    const std::unordered_set<War3ID>& GetLearnableSkills() const override { return skillsAvailable_; }

private:
    void LearnSkill(CSimulation& game, CUnit* unit, War3ID skillCode);
    void CalculateDerivedStats(CSimulation& game, CUnit* unit);
    int GetXPForLevel(int level) const;

    int heroLevel_ = 1;
    int xp_ = 0;
    int skillPoints_ = 1;

    HeroStatValue strength_;
    HeroStatValue agility_;
    HeroStatValue intelligence_;

    std::string properName_;
    std::unordered_set<War3ID> skillsAvailable_;
};

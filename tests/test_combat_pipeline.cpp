// Step-2 combat tests: attack/defense table, unified damage formula.
#include <cmath>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/combat/combat_system.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeCombatType(CDefenseType defType, int defense) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, defense, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, defType,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

} // namespace

bool TestAttackDefenseTableMatchesOfficialChart() {
    // Official TFT chart (classic.battle.net), columns L/M/H/F/Hero/U.
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::NORMAL, CDefenseType::MEDIUM), 1.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::PIERCE, CDefenseType::LIGHT), 2.0f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::PIERCE, CDefenseType::MEDIUM), 0.75f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::PIERCE, CDefenseType::UNARMORED), 1.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SIEGE, CDefenseType::FORTIFIED), 1.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SIEGE, CDefenseType::HERO), 0.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::MAGIC, CDefenseType::HEAVY), 2.0f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::MAGIC, CDefenseType::LIGHT), 1.25f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SPELLS, CDefenseType::HERO), 0.7f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SPELLS, CDefenseType::MEDIUM), 1.0f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::HERO, CDefenseType::FORTIFIED), 0.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::CHAOS, CDefenseType::FORTIFIED), 1.0f, 1e-5f);
    return true;
}

bool TestUnitDamageAppliesArmorTypeMultiplier() {
    CSimulation sim;
    // Normal attack vs Medium armor, 0 armor value: 100 * 1.5 = 150.
    CUnit target(1, MakeCombatType(CDefenseType::MEDIUM, 0), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 150.0f, 0.01f);
    return true;
}

bool TestNonNormalDamageTypeIgnoresArmorValue() {
    CSimulation sim;
    // 100 armor would block ~86% of physical; spell damage ignores it.
    CUnit target(2, MakeCombatType(CDefenseType::NONE, 100), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, false, false, CAttackType::SPELLS, CDamageType::COLD, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 100.0f, 0.01f);
    // ...while physical is heavily reduced.
    CUnit target2(3, MakeCombatType(CDefenseType::NONE, 100), 0, War3ID("hfoo"), 0, 0, 0);
    const float life2Before = target2.GetLife();
    target2.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(life2Before - target2.GetLife() < 20.0f);
    return true;
}

bool TestNegativeArmorAmplifiesDamage() {
    CSimulation sim;
    // Ref formula: 2 - 0.94^10 = ~1.4609x at -10 armor.
    CUnit target(4, MakeCombatType(CDefenseType::NONE, -10), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 100.0f * (2.0f - std::pow(0.94f, 10.0f)), 0.05f);
    return true;
}

bool TestBonusDamageIsMultipliedNotBypassing() {
    CSimulation sim;
    // (100 base + 10 bonus) * 1.5 (Normal vs Medium), 0 armor.
    CUnit target(5, MakeCombatType(CDefenseType::MEDIUM, 0), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 10.0f);
    TEST_NEAR(lifeBefore - target.GetLife(), 165.0f, 0.01f);
    return true;
}

bool TestCombatSystemBlockedMathConsistent() {
    CSimulation sim;
    // 100 dmg vs 6 armor (NONE type): reduction = 0.36/1.36.
    CUnit target(6, MakeCombatType(CDefenseType::NONE, 6), 0, War3ID("hfoo"), 0, 0, 0);
    DamageResult r = sim.GetCombatSystem()->CalculateDamage(
        nullptr, &target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    const float red = 0.36f / 1.36f;
    TEST_NEAR(r.damageBlocked, 100.0f * red, 0.01f);
    TEST_NEAR(r.damageDealt, 100.0f * (1.0f - red), 0.01f);
    TEST_NEAR(r.damageBlocked + r.damageDealt, 100.0f, 0.01f);
    return true;
}

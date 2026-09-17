// Splash / bounce / evasion / critical-strike combat tests.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/combat/combat_system.h"
#include "ecs/combat/projectile.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_attack.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " << (a) << " vs " << (b) << "\n"; return false; } } while (0)

namespace {

CUnitType* MakeAoeTestType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 0;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::NONE,
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

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(
        std::make_unique<CUnit>(handle, MakeAoeTestType(), player, War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestSplashDamagesEnemiesNotAllies() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3001, 0, -400, 0);
    auto* primary = AdoptAt(sim, 3002, 1, 0, 0);
    auto* halfRing = AdoptAt(sim, 3003, 1, 150, 0);   // within half(200), outside full(100)
    auto* quarterRing = AdoptAt(sim, 3004, 1, 250, 0); // within quarter(300)
    auto* ally = AdoptAt(sim, 3005, 0, 150, 0);        // same player as attacker
    auto* far = AdoptAt(sim, 3006, 1, 900, 0);
    TEST_ASSERT(attacker && primary && halfRing && quarterRing && ally && far);
    sim.Update();

    CUnitAttack splash;
    splash.attackType = CAttackType::NORMAL;
    splash.areaFull = 100.0f;
    splash.areaHalf = 200.0f;
    splash.areaQuarter = 300.0f;
    splash.damageFactorHalf = 0.5f;
    splash.damageFactorQuarter = 0.25f;

    auto* combat = sim.GetCombatSystem();
    TEST_ASSERT(combat != nullptr);
    const float dealt = combat->ApplySplashDamage(sim, attacker, primary, true, false,
                                                  CAttackType::NORMAL, CDamageType::NORMAL,
                                                  "", 100.0f, 0, 0, 0, splash);
    TEST_ASSERT(dealt > 0);
    TEST_NEAR(10000.0f - primary->GetLife(), 100.0f, 0.01f);
    TEST_NEAR(10000.0f - halfRing->GetLife(), 50.0f, 0.01f);
    TEST_NEAR(10000.0f - quarterRing->GetLife(), 25.0f, 0.01f);
    TEST_NEAR(ally->GetLife(), 10000.0f, 0.01f);
    TEST_NEAR(far->GetLife(), 10000.0f, 0.01f);
    return true;
}

bool TestProjectileBounceChainsWithFalloff() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3101, 0, -400, 0);
    auto* first = AdoptAt(sim, 3102, 1, 0, 0);
    auto* second = AdoptAt(sim, 3103, 1, 200, 0);
    TEST_ASSERT(attacker && first && second);
    sim.Update();

    CProjectile shot;
    shot.attackerHandleId = 3101;
    shot.targetHandleId = 3102;
    shot.x = -400.0f;
    shot.y = 0.0f;
    shot.speed = 2000.0f;
    shot.damage = 100.0f;
    shot.attackType = CAttackType::NORMAL;
    shot.maxTravel = 2000.0f;
    shot.bouncesLeft = 1;
    shot.bounceRange = 600.0f;
    shot.bounceLossFactor = 0.5f;
    sim.SpawnProjectile(std::move(shot));

    for (int i = 0; i < 500; i++) {
        sim.UpdateProjectiles();
        if (sim.GetProjectiles().empty()) break;
    }
    TEST_ASSERT(sim.GetProjectiles().empty());
    TEST_NEAR(10000.0f - first->GetLife(), 100.0f, 0.5f);
    TEST_NEAR(10000.0f - second->GetLife(), 50.0f, 0.5f);
    return true;
}

bool TestEvasionNegatesAttacks() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3201, 0, -100, 0);
    auto* dodgy = AdoptAt(sim, 3202, 1, 0, 0);
    auto* plain = AdoptAt(sim, 3203, 1, 0, 100);
    TEST_ASSERT(attacker && dodgy && plain);
    sim.Update();

    dodgy->SetEvasionChance(1.0f);
    auto* combat = sim.GetCombatSystem();
    DamageResult r = combat->CalculateDamage(attacker, dodgy, true, false,
                                             CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(r.isEvaded);
    TEST_ASSERT(r.damageDealt == 0.0f);
    DamageResult r2 = combat->CalculateDamage(attacker, plain, true, false,
                                              CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(!r2.isEvaded);
    TEST_ASSERT(r2.damageDealt > 0.0f);
    return true;
}

bool TestCriticalStrikeMultipliesDamage() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3301, 0, -100, 0);
    auto* target = AdoptAt(sim, 3302, 1, 0, 0);
    TEST_ASSERT(attacker && target);
    sim.Update();

    attacker->SetCriticalStrike(1.0f, 2.0f);
    auto* combat = sim.GetCombatSystem();
    DamageResult r = combat->CalculateDamage(attacker, target, true, false,
                                             CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(r.isCritical);
    TEST_NEAR(r.damageDealt, 200.0f, 0.01f);
    return true;
}

// Projectile tests: homing flight, arrival damage, fizzle rules.
#include <cmath>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/combat/projectile.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeProjType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 0.0f, 0.5f, CRegenType::ALWAYS,
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

void Fire(CSimulation& sim, int srcHandle, int tgtHandle,
          float sx, float sy, float speed, float damage, float maxTravel) {
    CProjectile shot;
    shot.attackerHandleId = srcHandle;
    shot.targetHandleId = tgtHandle;
    shot.x = sx;
    shot.y = sy;
    shot.speed = speed;
    shot.damage = damage;
    shot.attackType = CAttackType::NORMAL;
    shot.maxTravel = maxTravel;
    sim.SpawnProjectile(std::move(shot));
}

} // namespace

bool TestWeaponRangedClassification() {
    TEST_ASSERT(!CWeaponTypeInfo::IsRanged(CWeaponType::NONE));
    TEST_ASSERT(!CWeaponTypeInfo::IsRanged(CWeaponType::NORMAL));
    TEST_ASSERT(!CWeaponTypeInfo::IsRanged(CWeaponType::UNKNOWN));
    TEST_ASSERT(CWeaponTypeInfo::IsRanged(CWeaponType::INSTANT));
    TEST_ASSERT(CWeaponTypeInfo::IsRanged(CWeaponType::MISSILE));
    TEST_ASSERT(CWeaponTypeInfo::IsRanged(CWeaponType::MISSILE_BOUNCE));
    TEST_ASSERT(CWeaponTypeInfo::IsRanged(CWeaponType::MISSILE_LINE));
    TEST_ASSERT(CWeaponTypeInfo::IsRanged(CWeaponType::MISSILE_SPLASH));
    TEST_ASSERT(CWeaponTypeInfo::IsRanged(CWeaponType::ARTILLERY));
    TEST_ASSERT(CWeaponTypeInfo::IsRanged(CWeaponType::ARTILLERY_LINE));
    return true;
}

bool TestProjectileHitsAfterTravelTime() {
    CSimulation sim;
    auto* src = sim.AdoptUnit(std::make_unique<CUnit>(1, MakeProjType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* tgt = sim.AdoptUnit(std::make_unique<CUnit>(2, MakeProjType(), 1, War3ID("hfoo"), 1000, 0, 0));
    (void)src;
    // 1000 units at speed 1000 = 1s = 20 ticks; not there after 10 ticks.
    Fire(sim, 1, 2, 0, 0, 1000.0f, 100.0f, 2000.0f);
    const float lifeBefore = tgt->GetLife();
    for (int i = 0; i < 10; i++) sim.Update();
    TEST_ASSERT(tgt->GetLife() == lifeBefore);
    TEST_ASSERT(sim.GetProjectiles().size() == 1);
    for (int i = 0; i < 15; i++) sim.Update();
    TEST_NEAR(lifeBefore - tgt->GetLife(), 100.0f, 0.01f);
    TEST_ASSERT(sim.GetProjectiles().empty());
    return true;
}

bool TestProjectileFizzlesOnDeadTarget() {
    CSimulation sim;
    sim.AdoptUnit(std::make_unique<CUnit>(3, MakeProjType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* tgt = sim.AdoptUnit(std::make_unique<CUnit>(4, MakeProjType(), 1, War3ID("hfoo"), 500, 0, 0));
    tgt->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    TEST_ASSERT(tgt->IsDead());
    Fire(sim, 3, 4, 0, 0, 1000.0f, 100.0f, 2000.0f);
    for (int i = 0; i < 30; i++) sim.Update();
    TEST_ASSERT(sim.GetProjectiles().empty());
    return true;
}

bool TestProjectileHitsWithoutAttacker() {
    CSimulation sim;
    auto* tgt = sim.AdoptUnit(std::make_unique<CUnit>(6, MakeProjType(), 1, War3ID("hfoo"), 100, 0, 0));
    // Attacker handle 999 was never registered (killed mid-flight).
    Fire(sim, 999, 6, 0, 0, 1000.0f, 100.0f, 2000.0f);
    const float lifeBefore = tgt->GetLife();
    for (int i = 0; i < 10; i++) sim.Update();
    TEST_NEAR(lifeBefore - tgt->GetLife(), 100.0f, 0.01f);
    return true;
}

bool TestProjectileMaxTravelGuard() {
    CSimulation sim;
    sim.AdoptUnit(std::make_unique<CUnit>(7, MakeProjType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* tgt = sim.AdoptUnit(std::make_unique<CUnit>(8, MakeProjType(), 1, War3ID("hfoo"), 1000, 0, 0));
    // Slow missile that can never arrive: guard removes it without damage.
    Fire(sim, 7, 8, 0, 0, 10.0f, 100.0f, 50.0f);
    const float lifeBefore = tgt->GetLife();
    for (int i = 0; i < 300; i++) sim.Update();
    TEST_ASSERT(sim.GetProjectiles().empty());
    TEST_ASSERT(tgt->GetLife() == lifeBefore);
    return true;
}

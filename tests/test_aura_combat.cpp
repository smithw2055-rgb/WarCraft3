// Thorns/Vampiric aura buff wiring tests (ref lifesteal/thorns listeners).
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/abilities/auras.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " << (a) << " vs " << (b) << "\n"; return false; } } while (0)

namespace {

CUnitType* MakeLeechTestType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
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

} // namespace

bool TestThornsAuraGrantsReflectBuff() {
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit source(4001, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit target(4002, type, 0, War3ID("hfoo"), 100, 0, 0);
    CUnit attacker(4003, type, 1, War3ID("hfoo"), 200, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&target);
    sim.GetUnits().push_back(&attacker);

    CAbilityThornsAura aura(500);
    aura.SetRadius(1000.0f);
    aura.SetEnemyOnly(true); // test sim has no players; affect everyone in range
    aura.SetAffectsAllies(false);
    aura.SetAffectsSelf(false);
    aura.OnTick(sim, &source);
    TEST_ASSERT(target.HasBuff(War3ID("BEah")));

    // Melee attack on the thorny target reflects 10% (level-1 DataA1).
    const float atkBefore = attacker.GetLife();
    target.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    const float reflected = atkBefore - attacker.GetLife();
    TEST_ASSERT(reflected > 0.0f);

    // Ranged attacks are not reflected (ref listener is melee-only).
    CUnit archer(4004, type, 1, War3ID("hfoo"), 200, 0, 0);
    const float archerBefore = archer.GetLife();
    target.Damage(sim, &archer, true, true, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(archer.GetLife(), archerBefore, 0.001f);

    // Leaving the aura removes the buff.
    aura.OnRemove(sim, &source);
    TEST_ASSERT(!target.HasBuff(War3ID("BEah")));
    sim.GetUnits().clear();
    return true;
}

bool TestVampiricAuraHealsAttacker() {
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit attacker(4101, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit victim(4102, type, 1, War3ID("hfoo"), 100, 0, 0);

    attacker.AddBuff(std::make_unique<CBuffLifesteal>(10.0f, 0.15f), sim);
    attacker.SetLife(sim, 100.0f); // wound the attacker
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    // 15% of ~100 dealt, clamped to max 500.
    TEST_ASSERT(attacker.GetLife() > 100.0f);
    TEST_ASSERT(attacker.GetLife() <= attacker.GetMaxLife());
    return true;
}

bool TestVampiricAuraViaAuraAbility() {
    CSimulation sim;
    CUnitType* type = MakeLeechTestType();
    CUnit source(4201, type, 0, War3ID("hfoo"), 0, 0, 0);
    CUnit ally(4202, type, 0, War3ID("hfoo"), 100, 0, 0);
    sim.GetUnits().push_back(&source);
    sim.GetUnits().push_back(&ally);

    CAbilityVampiricAura aura(501);
    aura.SetRadius(1000.0f);
    aura.SetEnemyOnly(true);
    aura.SetAffectsAllies(false);
    aura.SetAffectsSelf(false);
    aura.OnTick(sim, &source);
    TEST_ASSERT(ally.HasBuff(War3ID("BUav")));
    aura.OnRemove(sim, &source);
    TEST_ASSERT(!ally.HasBuff(War3ID("BUav")));
    sim.GetUnits().clear();
    return true;
}

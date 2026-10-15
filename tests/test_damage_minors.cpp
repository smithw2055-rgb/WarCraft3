// P0-10 minors: splash miss halves the whole AoE, negative damage heals
// (capped), invulnerable targets still run taken-listeners.
#include <cmath>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_aura.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeMinorType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 0.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 300, 0, War3ID(),
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
        0.5f, 10.0f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

CUnit* AdoptMinor(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeMinorType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void SettleMinor(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

CUnitAttack MakeSplashAttack(CWeaponType weapon) {
    CUnitAttack attack;
    attack.weaponType = weapon;
    attack.areaFull = 200.0f;
    attack.areaHalf = 400.0f;
    attack.areaQuarter = 0.0f;
    attack.damageFactorHalf = 0.5f;
    attack.damageFactorQuarter = 0.5f;
    return attack;
}

} // namespace

// A missed MSPLASH swing deals base x MissDamageReduction (0.5) to the
// primary AND every ring (single roll; secondaries never re-roll, so even
// a 100% evasion secondary still takes the reduced hit). One miss text.
bool TestSplashMissHalvesWholeAoE() {
    CSimulation sim;
    auto* attacker = AdoptMinor(sim, 401, 0, 1000, 0);
    auto* primary = AdoptMinor(sim, 402, 1, 0, 0);
    auto* secondary = AdoptMinor(sim, 403, 1, 100, 0);
    SettleMinor(sim);
    primary->SetEvasionChance(1.0f);
    secondary->SetEvasionChance(1.0f);
    int missTexts = 0;
    sim.SetCombatTextCallback([&](int, int, CSimulation::CombatTextKind kind, float) {
        if (kind == CSimulation::CombatTextKind::MISS) missTexts++;
    });
    const float primaryBefore = primary->GetLife();
    const float secondaryBefore = secondary->GetLife();
    const float total = sim.GetCombatSystem()->ApplySplashDamage(
        sim, attacker, primary, true, false, CAttackType::NORMAL, CDamageType::NORMAL,
        "", 100.0f, 0.0f, 0, 0, MakeSplashAttack(CWeaponType::MISSILE_SPLASH));
    TEST_NEAR(primaryBefore - primary->GetLife(), 50.0f, 0.01f);
    TEST_NEAR(secondaryBefore - secondary->GetLife(), 50.0f, 0.01f);
    TEST_NEAR(total, 100.0f, 0.01f);
    TEST_ASSERT(missTexts == 1);
    return true;
}

// ARTILLERY misses the same way but stays silent (ref: no miss text).
bool TestArtilleryMissIsSilentButReduced() {
    CSimulation sim;
    auto* attacker = AdoptMinor(sim, 404, 0, 1000, 0);
    auto* primary = AdoptMinor(sim, 405, 1, 0, 0);
    SettleMinor(sim);
    primary->SetEvasionChance(1.0f);
    int missTexts = 0;
    sim.SetCombatTextCallback([&](int, int, CSimulation::CombatTextKind kind, float) {
        if (kind == CSimulation::CombatTextKind::MISS) missTexts++;
    });
    const float before = primary->GetLife();
    sim.GetCombatSystem()->ApplySplashDamage(
        sim, attacker, primary, true, false, CAttackType::NORMAL, CDamageType::NORMAL,
        "", 100.0f, 0.0f, 0, 0, MakeSplashAttack(CWeaponType::ARTILLERY));
    TEST_NEAR(before - primary->GetLife(), 50.0f, 0.01f);
    TEST_ASSERT(missTexts == 0);
    return true;
}

// A non-splash weapon with an AoE fizzles entirely on a miss (ref else
// branch zeroes the whole attack).
bool TestNonSplashMissZeroes() {
    CSimulation sim;
    auto* attacker = AdoptMinor(sim, 406, 0, 1000, 0);
    auto* primary = AdoptMinor(sim, 407, 1, 0, 0);
    auto* secondary = AdoptMinor(sim, 408, 1, 100, 0);
    SettleMinor(sim);
    primary->SetEvasionChance(1.0f);
    int missTexts = 0;
    sim.SetCombatTextCallback([&](int, int, CSimulation::CombatTextKind kind, float) {
        if (kind == CSimulation::CombatTextKind::MISS) missTexts++;
    });
    const float total = sim.GetCombatSystem()->ApplySplashDamage(
        sim, attacker, primary, true, false, CAttackType::NORMAL, CDamageType::NORMAL,
        "", 100.0f, 0.0f, 0, 0, MakeSplashAttack(CWeaponType::NORMAL));
    TEST_NEAR(total, 0.0f, 0.01f);
    TEST_NEAR(primary->GetLife(), 10000.0f, 0.01f);
    TEST_NEAR(secondary->GetLife(), 10000.0f, 0.01f);
    TEST_ASSERT(missTexts == 1);
    return true;
}

// Negative damage heals, capped at max life (ref CUnit.damage); the call
// reports the negative amount.
bool TestNegativeDamageHealsCapped() {
    CSimulation sim;
    auto* victim = AdoptMinor(sim, 409, 0, 0, 0);
    SettleMinor(sim);
    victim->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL,
                   "", 2000.0f, 0);
    TEST_NEAR(victim->GetLife(), 8000.0f, 0.01f);
    const float healed = victim->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                                        CDamageType::NORMAL, "", -5000.0f, 0);
    TEST_NEAR(victim->GetLife(), 10000.0f, 0.01f);
    TEST_NEAR(healed, -2000.0f, 0.01f);
    const float capped = victim->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                                        CDamageType::NORMAL, "", -100.0f, 0);
    TEST_NEAR(victim->GetLife(), 10000.0f, 0.01f);
    TEST_NEAR(capped, 0.0f, 0.01f);
    return true;
}

// The JASS/ApplyDamage path heals too and still reports the event.
bool TestNegativeDamageViaApplyDamage() {
    CSimulation sim;
    auto* victim = AdoptMinor(sim, 410, 0, 0, 0);
    SettleMinor(sim);
    victim->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL,
                   "", 2000.0f, 0);
    float reported = 0.0f;
    int events = 0;
    sim.SetUnitDamageCallback([&](int, int, float damage) {
        reported = damage;
        events++;
    });
    const float dealt = sim.GetCombatSystem()->ApplyDamage(
        sim, nullptr, victim, false, false, CAttackType::CHAOS, CDamageType::NORMAL,
        "", -5000.0f, 0.0f);
    TEST_NEAR(dealt, -5000.0f, 0.01f);
    TEST_NEAR(victim->GetLife(), 10000.0f, 0.01f);
    TEST_ASSERT(events == 1);
    TEST_NEAR(reported, -5000.0f, 0.01f);
    return true;
}

// Invulnerable targets take no life damage but still run taken-listeners:
// thorns reflects, sleep breaks, the damage event fires with 0, and crit
// text still shows (ref CUnit.damage guard scope).
bool TestInvulnerableRunsTakenListeners() {
    CSimulation sim;
    auto* attacker = AdoptMinor(sim, 411, 0, 100, 0);
    auto* victim = AdoptMinor(sim, 412, 1, 0, 0);
    SettleMinor(sim);
    victim->SetInvulnerable(true);
    victim->AddBuff(std::make_unique<CBuffThorns>(60.0f, 0.5f), sim);
    victim->AddBuff(std::make_unique<CBuffSleep>(60.0f), sim);
    const float attackerBefore = attacker->GetLife();
    int events = 0;
    float reported = 1.0f;
    sim.SetUnitDamageCallback([&](int, int, float damage) {
        reported = damage;
        events++;
    });
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL,
                   "", 100.0f, 0);
    TEST_NEAR(victim->GetLife(), 10000.0f, 0.01f); // no life change
    TEST_NEAR(attackerBefore - attacker->GetLife(), 50.0f, 0.01f); // thorns
    TEST_ASSERT(!victim->IsAsleep()); // sleep broke
    // Two damage events: the reflected hit on the attacker, then the
    // invulnerable victim's own zero report (retail event always fires).
    TEST_ASSERT(events == 2);
    TEST_NEAR(reported, 0.0f, 0.01f);
    return true;
}

bool TestInvulnerableStillShowsCritText() {
    CSimulation sim;
    auto* attacker = AdoptMinor(sim, 413, 0, 100, 0);
    auto* victim = AdoptMinor(sim, 414, 1, 0, 0);
    SettleMinor(sim);
    victim->SetInvulnerable(true);
    attacker->AddCritSource(War3ID("AHbh"), 1.0f, 2.0f);
    int critTexts = 0;
    sim.SetCombatTextCallback([&](int, int, CSimulation::CombatTextKind kind, float) {
        if (kind == CSimulation::CombatTextKind::CRIT) critTexts++;
    });
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL,
                   "", 100.0f, 0);
    TEST_NEAR(victim->GetLife(), 10000.0f, 0.01f);
    TEST_ASSERT(critTexts == 1);
    return true;
}

// P0-9 non-stacking attack bonuses: same key takes max, different keys
// sum (ref NonStackingStatBuff stackingKey = buff id).
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

CUnitType* MakeBonusType() {
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

CUnit* AdoptBonus(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeBonusType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void SettleBonus(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

// Roar (Broa) and Inner Fire (Binf) are different keys: both instances
// coexist and their damage sums (ref different stackingKeys sum).
bool TestRoarAndInnerFireSum() {
    CSimulation sim;
    auto* unit = AdoptBonus(sim, 301, 0, 0, 0);
    SettleBonus(sim);
    const int before = unit->GetBonusDamage();
    unit->AddBuff(std::make_unique<CBuffInnerFire>(60.0f, 8, 0), sim); // Binf
    unit->AddBuff(std::make_unique<CBuffInnerFire>(60.0f, 5, 0, War3ID("Broa")), sim);
    TEST_ASSERT(unit->GetBuff(War3ID("Binf")) != nullptr);
    TEST_ASSERT(unit->GetBuff(War3ID("Broa")) != nullptr);
    TEST_ASSERT(unit->GetBonusDamage() == before + 13);
    unit->RemoveBuff(War3ID("Binf"), sim);
    TEST_ASSERT(unit->GetBonusDamage() == before + 5);
    unit->RemoveBuff(War3ID("Broa"), sim);
    TEST_ASSERT(unit->GetBonusDamage() == before);
    return true;
}

// Same aura buff from two casters: strongest wins (ref max within a key).
bool TestSameAuraTwoCastersTakesMax() {
    CSimulation sim;
    auto* unit = AdoptBonus(sim, 302, 0, 0, 0);
    SettleBonus(sim);
    const int before = unit->GetBonusDamage();
    unit->AddAuraStatBonus(CAuraEffectType::DAMAGE, false, War3ID("AEar"), 10.0f, 11);
    unit->AddAuraStatBonus(CAuraEffectType::DAMAGE, false, War3ID("AEar"), 6.0f, 12);
    TEST_ASSERT(unit->GetBonusDamage() == before + 10);
    unit->RemoveAuraStatBonus(CAuraEffectType::DAMAGE, false, War3ID("AEar"), 11);
    TEST_ASSERT(unit->GetBonusDamage() == before + 6);
    unit->RemoveAuraStatBonus(CAuraEffectType::DAMAGE, false, War3ID("AEar"), 12);
    TEST_ASSERT(unit->GetBonusDamage() == before);
    return true;
}

// Two different damage auras sum (ref different stackingKeys sum; the old
// slot-wide max under-stacked this case).
bool TestDifferentAurasSum() {
    CSimulation sim;
    auto* unit = AdoptBonus(sim, 303, 0, 0, 0);
    SettleBonus(sim);
    const int before = unit->GetBonusDamage();
    unit->AddAuraStatBonus(CAuraEffectType::DAMAGE, false, War3ID("AEar"), 10.0f, 11);
    unit->AddAuraStatBonus(CAuraEffectType::DAMAGE, false, War3ID("AOac"), 6.0f, 12);
    TEST_ASSERT(unit->GetBonusDamage() == before + 16);
    unit->RemoveAuraStatBonus(CAuraEffectType::DAMAGE, false, War3ID("AEar"), 11);
    TEST_ASSERT(unit->GetBonusDamage() == before + 6);
    return true;
}

// Bloodlust + Berserk add (1.8x), not multiply (1.96x); removal restores.
bool TestHasteAddsAcrossKeys() {
    CSimulation sim;
    auto* unit = AdoptBonus(sim, 304, 0, 0, 0);
    SettleBonus(sim);
    TEST_NEAR(unit->GetAttackSpeedMultiplier(), 1.0f, 1e-5f);
    unit->AddBuff(std::make_unique<CBuffBloodlust>(60.0f, 0.0f, 0.4f), sim);
    TEST_NEAR(unit->GetAttackSpeedMultiplier(), 1.4f, 1e-5f);
    unit->AddBuff(std::make_unique<CBuffBerserk>(60.0f, 1.4f, 1.0f), sim);
    TEST_NEAR(unit->GetAttackSpeedMultiplier(), 1.8f, 1e-5f);
    unit->RemoveBuff(War3ID("Bblo"), sim);
    TEST_NEAR(unit->GetAttackSpeedMultiplier(), 1.4f, 1e-5f);
    unit->RemoveBuff(War3ID("Babr"), sim);
    TEST_NEAR(unit->GetAttackSpeedMultiplier(), 1.0f, 1e-5f);
    return true;
}

// Stacking item haste adds with buff haste (retail 1 + sum).
bool TestItemHasteStacksWithBuffHaste() {
    CSimulation sim;
    auto* unit = AdoptBonus(sim, 305, 0, 0, 0);
    SettleBonus(sim);
    unit->AddStackingAttackSpeed(0.15f);
    unit->AddBuff(std::make_unique<CBuffBloodlust>(60.0f, 0.0f, 0.4f), sim);
    TEST_NEAR(unit->GetAttackSpeedMultiplier(), 1.55f, 1e-5f);
    unit->AddStackingAttackSpeed(-0.15f);
    TEST_NEAR(unit->GetAttackSpeedMultiplier(), 1.4f, 1e-5f);
    return true;
}

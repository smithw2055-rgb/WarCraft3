// Phase 2 Engine Modernization Tests:
// 1. UnitAttributeAggregator (Unity GAS / Godot Resource style stat & aura pipeline)
// 2. Data-Driven ABSpellDescriptor & CAbilityDataDrivenSpell (zero-subclass spell authoring + SLK level scaling)
// 3. O(1) AbilityFactory Registry, runtime Creator/Alias registration
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/attribute_aggregator.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/abilities/ability_return_resources.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "data/abilitydata.h"
#include "data/unitdata.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

#define TEST_NEAR(a, b, eps)                                                \
    do {                                                                    \
        if (std::fabs((a) - (b)) > (eps)) {                                 \
            std::cerr << "NEAR @" << __LINE__ << ": " #a " (" << (a)        \
                      << ") vs " #b " (" << (b) << ")\n";                   \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

CUnitType* MakePhase2UnitType(War3ID code, int maxLife = 600, int speed = 270) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 20;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", code,
        maxLife, 1.0f, 0.5f, CRegenType::ALWAYS,
        200, 200, speed, 2, War3ID(),
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

bool TestUnitAttributeAggregatorPipeline() {
    UnitAttributeAggregator agg;
    agg.SetBaseSpeed(270);
    agg.SetBaseMaxLife(500);
    agg.SetBaseMaxMana(200);
    agg.SetCurrentDefense(2);

    // Layered life & mana
    agg.SetHeroBonusLife(150.0f);
    agg.AddBonusMaxLife(75.0f);
    TEST_NEAR(agg.ComputeMaxLife(), 725.0f, 1e-4f);

    agg.SetHeroBonusMana(60);
    agg.AddBonusMaxMana(40);
    TEST_ASSERT(agg.ComputeMaxMana() == 300);

    // Speed modifiers & clamping [150, 522]
    agg.AddBonusSpeed(30); // base+bonus = 300
    agg.AddSpeedModifier(War3ID("Bslo"), -0.25f); // 300 * 0.75 = 225
    TEST_NEAR(agg.ComputeEffectiveSpeed(), 225.0f, 1e-4f);
    agg.AddSpeedModifier(War3ID("Bbsk"), 1.50f); // 300 * 2.25 = 675 -> clamped to 522
    TEST_NEAR(agg.ComputeEffectiveSpeed(), 522.0f, 1e-4f);
    agg.RemoveSpeedModifier(War3ID("Bbsk"));
    TEST_NEAR(agg.ComputeEffectiveSpeed(), 225.0f, 1e-4f);

    // Non-stacking keyed damage + direct bonus + hero bonus
    agg.AddBonusDamage(10);
    agg.SetHeroBonusDamage(15);
    agg.SetNonStackingDamageBonus(War3ID("Broa"), 20);
    agg.SetNonStackingDamagePctBonus(War3ID("Binf"), 8);
    TEST_NEAR(agg.ComputeTotalBonusDamage(), 53.0f, 1e-4f);
    agg.ClearNonStackingDamageBonus(War3ID("Broa"));
    TEST_NEAR(agg.ComputeTotalBonusDamage(), 33.0f, 1e-4f);

    // Attack speed stacking + non-stacking + clamp [-0.9, 4.0]
    agg.AddStackingAttackSpeed(0.15f);
    agg.SetNonStackingAttackSpeed(War3ID("Bblo"), 0.40f);
    TEST_NEAR(agg.ComputeAttackSpeedMultiplier(), 1.55f, 1e-4f);

    // Independent Evasion sources: 1 - (1 - 0.20) * (1 - 0.50) = 0.60
    agg.AddEvasionSource(War3ID("AEev"), 0.20f);
    agg.AddEvasionSource(War3ID("ANdb"), 0.50f);
    TEST_NEAR(agg.ComputeEvasionChance(), 0.60f, 1e-4f);
    agg.RemoveEvasionSource(War3ID("AEev"));
    TEST_NEAR(agg.ComputeEvasionChance(), 0.50f, 1e-4f);

    // Crit sources: strongest chance picks display multiplier
    agg.AddCritSource(War3ID("AOcr"), 0.15f, 2.0f);
    agg.AddCritSource(War3ID("ANdb"), 0.25f, 3.0f);
    TEST_NEAR(agg.ComputeCritChance(), 0.25f, 1e-4f);
    TEST_NEAR(agg.ComputeCritMultiplier(), 3.0f, 1e-4f);

    // Aura stat deduplication: same buffId from two casters takes max
    agg.AddAuraStatBonus(CAuraEffectType::ARMOR, false, War3ID("BHad"), 3.0f, 1, nullptr, 300);
    agg.AddAuraStatBonus(CAuraEffectType::ARMOR, false, War3ID("BHad"), 5.0f, 2, nullptr, 300);
    TEST_ASSERT(agg.ComputeTotalDefense() == 2 + 5);
    agg.RemoveAuraStatBonus(CAuraEffectType::ARMOR, false, War3ID("BHad"), 2, nullptr, 300);
    TEST_ASSERT(agg.ComputeTotalDefense() == 2 + 3);

    return true;
}

bool TestDataDrivenSpellDescriptorAndRegistry() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    CUnitType* casterType = MakePhase2UnitType(War3ID("Hpal"), 1000, 270);
    CUnitType* enemyType = MakePhase2UnitType(War3ID("ogru"), 600, 270);

    // Register a brand-new data-driven AoE frost-pulse spell (A901) without any C++ subclass!
    const War3ID customCode("A901");
    ABSpellDescriptor desc = ABSpellDescriptor::MakeAreaDamageAndSlow(
        customCode, /*radius=*/300.0f, /*defaultDamage=*/50.0f,
        /*slowPct=*/0.40f, /*slowDur=*/4.0f, /*cooldown=*/5.0f, /*manaCost=*/40,
        CDamageType::COLD);
    AbilityFactory::RegisterSpellDescriptor(desc);
    TEST_ASSERT(AbilityFactory::HasRegisteredCreator(customCode));

    CUnit* caster = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), casterType, 0, War3ID("Hpal"), 0.0f, 0.0f, 0.0f));
    CUnit* enemy = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), enemyType, 1, War3ID("ogru"), 120.0f, 0.0f, 0.0f));
    sim.Update();
    TEST_ASSERT(caster != nullptr && enemy != nullptr);

    std::unique_ptr<CAbility> ability = AbilityFactory::CreateAbility(sim.AllocateHandleId(), customCode);
    TEST_ASSERT(ability != nullptr);
    auto* ddSpell = dynamic_cast<CAbilityDataDrivenSpell*>(ability.get());
    TEST_ASSERT(ddSpell != nullptr);

    // Apply multi-level SLK CAbilityDefFull scaling (Level 1 = 50 dmg, Level 2 = 90 dmg, Level 3 = 140 dmg)
    CAbilityDefFull slkDef;
    slkDef.id = customCode;
    slkDef.levels = 3;
    slkDef.dataLevels = {
        {"50", "0.40", "0", "0", "0", "0", "0", "0", "0"},
        {"90", "0.50", "0", "0", "0", "0", "0", "0", "0"},
        {"140", "0.60", "0", "0", "0", "0", "0", "0", "0"},
    };
    slkDef.areas = {300.0f, 350.0f, 400.0f};
    slkDef.durations = {4.0f, 5.0f, 6.0f};
    slkDef.manaCosts = {40, 50, 60};
    slkDef.cooldowns = {5.0f, 5.0f, 5.0f};

    ddSpell->ApplyData(slkDef);
    ddSpell->SetLevel(3);
    TEST_NEAR(ddSpell->GetEffectContext().data[0], 140.0f, 1e-4f);
    TEST_NEAR(ddSpell->GetEffectContext().area, 400.0f, 1e-4f);
    TEST_ASSERT(ddSpell->GetManaCost() == 60);

    caster->AddAbility(sim, std::move(ability));
    ddSpell->DoEffect(sim, caster);

    // Enemy in radius should have taken 140 spell damage and received slow buff (Bslo)
    TEST_NEAR(enemy->GetLife(), 600.0f - 140.0f, 1e-3f);
    TEST_ASSERT(enemy->HasBuff(War3ID("Bslo")));
    TEST_ASSERT(enemy->GetSpeed() < 270.0f);

    AbilityBuilderRegistry::Instance().Unregister(customCode);
    return true;
}

bool TestAbilityFactoryO1TableAndAliases() {
    TEST_ASSERT(AbilityFactory::GetRegisteredCount() >= 170);
    TEST_ASSERT(AbilityFactory::HasRegisteredCreator(War3ID("AHbz")));
    TEST_ASSERT(AbilityFactory::HasRegisteredCreator(War3ID("AOws")));

    // Register a dynamic alias A99H -> AHhb (Holy Light)
    const War3ID aliasCode("A99H");
    AbilityFactory::RegisterAlias(aliasCode, War3ID("AHhb"));
    TEST_ASSERT(AbilityFactory::HasRegisteredCreator(aliasCode));

    auto aliased = AbilityFactory::CreateAbility(777, aliasCode);
    TEST_ASSERT(aliased != nullptr);
    TEST_ASSERT(aliased->GetCode() == War3ID("AHhb"));
    TEST_ASSERT(aliased->GetAlias() == aliasCode);

    return true;
}

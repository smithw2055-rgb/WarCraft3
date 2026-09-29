// Batch-19 tests: Banish (AHbn) and Anti-Magic Shell (Aam2). Both attach
// their buffs through the AbilityBuilder single-target path; tests pin the
// buff ids, the combat consequences, factory coverage and retail order ids.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/spells19.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/combat/combat_system.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell19Type() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell19Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestBanishEtherealizes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 1901, 0, 0, 0);
    auto* target = AdoptAt(sim, 1902, 1, 100, 0);
    Settle(sim);
    CAbilityBanish banish(19901);
    banish.DoEffect(sim, caster, target);
    TEST_ASSERT(target->IsEthereal());
    TEST_ASSERT(target->GetBuff(War3ID("Bban")) != nullptr);
    // Banished victim shrugs physical attacks...
    DamageResult r = sim.GetCombatSystem()->CalculateDamage(
        caster, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(r.damageDealt == 0.0f);
    // ...but takes bonus magic.
    DamageResult m = sim.GetCombatSystem()->CalculateDamage(
        caster, target, false, false, CAttackType::MAGIC, CDamageType::MAGIC, 100, 0, 0, 0);
    TEST_ASSERT(m.damageDealt > 100.0f);
    return true;
}

bool TestAntiMagicShellImmunizes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 1911, 0, 0, 0);
    auto* target = AdoptAt(sim, 1912, 0, 100, 0);
    Settle(sim);
    CAbilityAntiMagicShell shell(19902);
    shell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->IsMagicImmune());
    TEST_ASSERT(target->GetBuff(War3ID("Bam2")) != nullptr);
    DamageResult m = sim.GetCombatSystem()->CalculateDamage(
        caster, target, false, false, CAttackType::MAGIC, CDamageType::FIRE, 100, 0, 0, 0);
    TEST_ASSERT(m.damageDealt == 0.0f);
    DamageResult p = sim.GetCombatSystem()->CalculateDamage(
        caster, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(p.damageDealt > 0.0f);
    return true;
}

bool TestSpells19FactoryCoverage() {
    auto banish = AbilityFactory::CreateAbility(19911, War3ID("AHbn"));
    TEST_ASSERT(banish != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityBanish*>(banish.get()) != nullptr);
    auto shell = AbilityFactory::CreateAbility(19912, War3ID("Aam2"));
    TEST_ASSERT(shell != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityAntiMagicShell*>(shell.get()) != nullptr);
    return true;
}

bool TestSpells19OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("AHbn")) == 852486);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aam2")) == 852186);
    TEST_ASSERT(OrderIdFromString("banish") == 852486);
    TEST_ASSERT(OrderIdFromString("antimagicshell") == 852186);
    TEST_ASSERT(SpellMatchesOrder(War3ID("AHbn"), 852486));
    TEST_ASSERT(SpellMatchesOrder(War3ID("Aam2"), 852186));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("AHbn"), 852186));
    return true;
}

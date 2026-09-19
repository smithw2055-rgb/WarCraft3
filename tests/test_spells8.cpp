// Batch-8 tests: charged-item abilities (healing scroll, town portal,
// invuln/mana potions, clarity, mana steal, protection scroll).
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells8.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell8Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell8Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

void Wound(CSimulation& sim, CUnit* unit, float amount) {
    unit->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", amount, 0);
}

} // namespace

bool TestItemHealGroupHeals() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 801, 0, 0, 0);
    auto* ally = AdoptAt(sim, 802, 0, 100, 100);
    auto* foe = AdoptAt(sim, 803, 1, 150, 100);
    Settle(sim);
    Wound(sim, ally, 5000.0f);
    Wound(sim, foe, 5000.0f);
    const float allyBefore = ally->GetLife();
    const float foeBefore = foe->GetLife();
    CAbilityItemHealGroup spell(8001);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(ally->GetLife() > allyBefore);
    TEST_ASSERT(foe->GetLife() == foeBefore);
    return true;
}

bool TestItemTownPortalMovesGroup() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 811, 0, 0, 0);
    auto* near = AdoptAt(sim, 812, 0, 100, 0);
    auto* far = AdoptAt(sim, 813, 0, 5000, 5000);
    auto* foe = AdoptAt(sim, 814, 1, 100, 100);
    Settle(sim);
    CAbilityItemTownPortal spell(8101);
    spell.DoEffect(sim, caster, 9000, 9000);
    TEST_ASSERT(caster->GetX() == 9000.0f && caster->GetY() == 9000.0f);
    TEST_ASSERT(near->GetX() == 9000.0f && near->GetY() == 9000.0f);
    TEST_ASSERT(far->GetX() == 5000.0f && far->GetY() == 5000.0f);
    TEST_ASSERT(foe->GetX() == 100.0f && foe->GetY() == 100.0f);
    return true;
}

bool TestItemInvulnerableShields() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 821, 0, 0, 0);
    Settle(sim);
    CAbilityItemInvulnerable spell(8201);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->IsInvulnerable());
    const float before = caster->GetLife();
    caster->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 5000.0f, 0);
    TEST_ASSERT(caster->GetLife() == before); // shield holds
    return true;
}

bool TestItemHealSelfCaps() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 831, 0, 0, 0);
    Settle(sim);
    Wound(sim, caster, 200.0f);
    CAbilityItemHealSelf spell(8301);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife()); // capped, not over
    return true;
}

bool TestItemManaSelfCaps() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 841, 0, 0, 0);
    Settle(sim);
    caster->SetMana(10.0f);
    CAbilityItemManaSelf spell(8401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetMana() > 10.0f);
    TEST_ASSERT(caster->GetMana() <= static_cast<float>(caster->GetMaxMana()));
    return true;
}

bool TestItemClarityRegens() {
    CSimulation sim;
    AddPlayers(sim);
    auto* hasted = AdoptAt(sim, 851, 0, 0, 0);
    auto* plain = AdoptAt(sim, 852, 0, 100, 0);
    Settle(sim);
    hasted->SetMana(50.0f);
    plain->SetMana(50.0f);
    CAbilityItemClarity spell(8501);
    spell.DoEffect(sim, hasted);
    TEST_ASSERT(hasted->GetBuff(War3ID("Bclf")) != nullptr);
    for (int i = 0; i < 40; i++) sim.Update();
    TEST_ASSERT(hasted->GetMana() > plain->GetMana()); // clarity out-regens base
    hasted->RemoveBuff(War3ID("Bclf"), sim);
    TEST_ASSERT(hasted->GetBonusManaRegen() == 0.0f);
    return true;
}

bool TestItemManaStealDrains() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 861, 0, 0, 0);
    auto* target = AdoptAt(sim, 862, 1, 100, 0);
    Settle(sim);
    caster->SetMana(0.0f);
    target->SetMana(200.0f);
    CAbilityItemManaSteal spell(8601);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetMana() == 100.0f);
    TEST_ASSERT(caster->GetMana() == 100.0f);
    // Cannot steal from an empty pool.
    spell.DoEffect(sim, caster, target);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetMana() == 0.0f);
    TEST_ASSERT(caster->GetMana() == 200.0f);
    return true;
}

bool TestItemProtectionArmors() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 871, 0, 0, 0);
    Settle(sim);
    const int base = caster->GetTotalDefense();
    CAbilityItemProtection spell(8701);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("AIda")) != nullptr);
    TEST_ASSERT(caster->GetTotalDefense() == base + 3);
    return true;
}

bool TestSpells8FactoryCoverage() {
    const char* codes[] = {"AIha", "AItp", "AIvl", "AIh1",
                           "AIm1", "AIpl", "Aste", "AIda"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(8800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

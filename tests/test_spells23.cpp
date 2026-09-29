// Batch-23 tests: Chain Dispel (faction-sensitive bouncing cleanse) and
// Taunt (logged forced-attack orders); factory + retail order ids.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/spells23.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell23Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell23Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestChainDispelCleansesBothSides() {
    CSimulation sim;
    // Ally checks read player alliances; same-player units are allied.
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2301, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2302, 1, 100, 0);
    auto* friend_ = AdoptAt(sim, 2303, 0, 150, 0);
    Settle(sim);
    // Enemy buffed, ally debuffed.
    foe->AddBuff(std::make_unique<CBuffBloodlust>(60.0f, 0.1f, 0.1f), sim);
    friend_->AddBuff(std::make_unique<CBuffSlow>(60.0f, 0.3f), sim);
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) != nullptr);
    TEST_ASSERT(friend_->GetBuff(War3ID("Bslo")) != nullptr);

    CAbilityChainDispel dispel(23901);
    dispel.DoEffect(sim, caster, foe);
    // Enemy sheds the buff, ally sheds the debuff.
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) == nullptr);
    TEST_ASSERT(friend_->GetBuff(War3ID("Bslo")) == nullptr);
    return true;
}

bool TestTauntForcesAttack() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2311, 0, 0, 0);
    auto* foe1 = AdoptAt(sim, 2312, 1, 100, 0);
    auto* foe2 = AdoptAt(sim, 2313, 1, 150, 50);
    auto* friend_ = AdoptAt(sim, 2314, 0, 120, 0);
    Settle(sim);
    CAbilityTaunt taunt(23902);
    taunt.DoEffect(sim, caster);
    TEST_ASSERT(foe1->GetLastOrderId() == OrderIds::ATTACK);
    TEST_ASSERT(foe2->GetLastOrderId() == OrderIds::ATTACK);
    TEST_ASSERT(friend_->GetLastOrderId() == 0);
    // And the orders went through the lockstep log (replayable).
    TEST_ASSERT(!sim.LoggedCommands().empty());
    return true;
}

bool TestSpells23FactoryCoverage() {
    auto dispel = AbilityFactory::CreateAbility(23911, War3ID("Ache"));
    TEST_ASSERT(dispel != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChainDispel*>(dispel.get()) != nullptr);
    auto taunt = AbilityFactory::CreateAbility(23912, War3ID("ANta"));
    TEST_ASSERT(taunt != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTaunt*>(taunt.get()) != nullptr);
    return true;
}

bool TestSpells23OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Ache")) == 852615);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANta")) == 852520);
    TEST_ASSERT(OrderIdFromString("taunt") == 852520);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Ache"), 852615));
    TEST_ASSERT(SpellMatchesOrder(War3ID("ANta"), 852520));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Ache"), 852520));
    return true;
}

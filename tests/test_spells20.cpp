// Batch-20 tests: Possession (Apos) converts + sacrifices, Stampede
// (ANst) tramples a point corridor; factory + retail order ids.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/spells20.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell20Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell20Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestPossessionConvertsAndSacrifices() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2001, 0, 0, 0);
    auto* victim = AdoptAt(sim, 2002, 1, 100, 0);
    Settle(sim);
    CAbilityPossession possess(20901);
    possess.DoEffect(sim, caster, victim);
    // Victim converted...
    TEST_ASSERT(victim->GetPlayerIndex() == 0);
    // ...and the banshee was removed (queued removal drains on Update).
    Settle(sim);
    TEST_ASSERT(sim.GetUnit(2001) == nullptr);
    TEST_ASSERT(sim.GetUnit(2002) == victim);
    return true;
}

bool TestPossessionNoSacrificeWithoutConversion() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2011, 0, 0, 0);
    auto* dead = AdoptAt(sim, 2012, 1, 100, 0);
    Settle(sim);
    // Kill the victim first: possession of a corpse is a no-op and must
    // NOT sacrifice the caster.
    dead->SetLife(sim, 0.0f);
    CAbilityPossession possess(20902);
    possess.DoEffect(sim, caster, dead);
    Settle(sim);
    TEST_ASSERT(sim.GetUnit(2011) == caster);
    return true;
}

bool TestStampedeTramplesCorridor() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2021, 0, 0, 0);
    auto* foeIn = AdoptAt(sim, 2022, 1, 400, 0);
    auto* foeOut = AdoptAt(sim, 2023, 1, 400, 500);
    Settle(sim);
    const float lifeBefore = foeIn->GetLife();
    CAbilityStampede stampede(20903);
    stampede.DoEffect(sim, caster, 800, 0);
    TEST_ASSERT(foeIn->GetLife() < lifeBefore);
    TEST_ASSERT(foeOut->GetLife() == 10000.0f);
    return true;
}

bool TestSpells20FactoryCoverage() {
    auto possess = AbilityFactory::CreateAbility(20911, War3ID("Apos"));
    TEST_ASSERT(possess != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityPossession*>(possess.get()) != nullptr);
    auto stampede = AbilityFactory::CreateAbility(20912, War3ID("ANst"));
    TEST_ASSERT(stampede != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityStampede*>(stampede.get()) != nullptr);
    return true;
}

bool TestSpells20OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Apos")) == 852196);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANst")) == 852593);
    TEST_ASSERT(OrderIdFromString("possession") == 852196);
    TEST_ASSERT(OrderIdFromString("stampede") == 852593);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Apos"), 852196));
    TEST_ASSERT(SpellMatchesOrder(War3ID("ANst"), 852593));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Apos"), 852593));
    // Fixed codes resolve in the SLK now (ABan->AHbn, AEtr->AEtq).
    TEST_ASSERT(SpellBaseOrderId(War3ID("AHbn")) == 852486);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AEtq")) == 852184);
    return true;
}

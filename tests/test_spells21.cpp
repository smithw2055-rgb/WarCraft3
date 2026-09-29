// Batch-21 tests: Stasis Trap mine (proximity stun + consume) and
// Tornado wanderers (drift + toss + expiry); factory + order ids.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/spells21.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell21Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell21Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

} // namespace

bool TestStasisTrapTripsAndStuns() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2101, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2102, 1, 500, 0);
    Settle(sim);
    CAbilityStasisTrap trap(21901);
    trap.DoEffect(sim, caster, 500, 0);
    TEST_ASSERT(sim.GetHazards().size() == 1);
    Settle(sim, 10); // proximity polls run; foe stands on the mine
    TEST_ASSERT(sim.GetHazards().empty()); // consumed
    TEST_ASSERT(foe->GetBuff(War3ID("Bstn")) != nullptr);
    return true;
}

bool TestStasisTrapIgnoresAllies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2111, 0, 0, 0);
    auto* friend_ = AdoptAt(sim, 2112, 0, 500, 0);
    Settle(sim);
    CAbilityStasisTrap trap(21902);
    trap.DoEffect(sim, caster, 500, 0);
    Settle(sim, 30);
    TEST_ASSERT(sim.GetHazards().size() == 1); // never tripped
    TEST_ASSERT(friend_->GetBuff(War3ID("Bstn")) == nullptr);
    return true;
}

bool TestTornadoWandersAndTosses() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2121, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2122, 1, 100, 0);
    Settle(sim);
    CAbilityTornado tornado(21903);
    tornado.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(sim.GetHazards().size() == 3);
    const float hx = sim.GetHazards()[0].x;
    const float hy = sim.GetHazards()[0].y;
    Settle(sim, 40); // wander + two toss ticks
    const float hx2 = sim.GetHazards()[0].x;
    const float hy2 = sim.GetHazards()[0].y;
    TEST_ASSERT(std::fabs(hx2 - hx) + std::fabs(hy2 - hy) > 1.0f); // drifted
    // The foe started inside a toss radius: cyclone buff or damage landed.
    const bool tossed = foe->GetBuff(War3ID("Bcyc")) != nullptr;
    const bool hurt = foe->GetLife() < 10000.0f;
    TEST_ASSERT(tossed || hurt);
    (void)hx; (void)hy;
    return true;
}

bool TestTornadoExpires() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2131, 0, 0, 0);
    Settle(sim);
    CAbilityTornado tornado(21904);
    tornado.DoEffect(sim, caster, 0, 0);
    TEST_ASSERT(sim.GetHazards().size() == 3);
    Settle(sim, 20 * 25); // past the 20s lifetime
    TEST_ASSERT(sim.GetHazards().empty());
    return true;
}

bool TestSpells21FactoryCoverage() {
    auto stasis = AbilityFactory::CreateAbility(21911, War3ID("Asta"));
    TEST_ASSERT(stasis != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityStasisTrap*>(stasis.get()) != nullptr);
    auto tornado = AbilityFactory::CreateAbility(21912, War3ID("ANto"));
    TEST_ASSERT(tornado != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTornado*>(tornado.get()) != nullptr);
    return true;
}

bool TestSpells21OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Asta")) == 852114);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANto")) == 852597);
    TEST_ASSERT(OrderIdFromString("stasistrap") == 852114);
    TEST_ASSERT(OrderIdFromString("tornado") == 852597);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Asta"), 852114));
    TEST_ASSERT(SpellMatchesOrder(War3ID("ANto"), 852597));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Asta"), 852597));
    return true;
}

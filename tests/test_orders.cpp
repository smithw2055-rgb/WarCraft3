// Order-table tests: retail order strings/ids (ref OrderIds +
// OrderIdUtils) and spell order dispatch (ref getBaseOrderId): the
// right spell fires for its order, wrong orders and SMART never cast,
// passives never match. Adopted units only, no MPQ.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/spells.h"
#include "ecs/abilities/spells3.h"
#include "ecs/abilities/spells4.h"
#include "ecs/abilities/spells5.h"
#include "ecs/abilities/spells18.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "ecs/destructable.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeOrderType() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeOrderType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

void Grant(CSimulation& sim, CUnit* unit, War3ID code, int handle) {
    auto ability = AbilityFactory::CreateAbility(handle, code);
    unit->AddAbility(sim, std::move(ability));
}

} // namespace

bool TestOrderStringTable() {
    // Retail ids (ref OrderIds.java), incl. the corrected harvest/return.
    TEST_ASSERT(OrderIdFromString("whirlwind") == 852128);
    TEST_ASSERT(OrderIdFromString("immolation") == 852177);
    TEST_ASSERT(OrderIdFromString("unimmolation") == 852178);
    TEST_ASSERT(OrderIdFromString("scout") == 852181);
    TEST_ASSERT(OrderIdFromString("starfall") == 852183);
    TEST_ASSERT(OrderIdFromString("flamestrike") == 852488);
    TEST_ASSERT(OrderIdFromString("blizzard") == 852089);
    TEST_ASSERT(OrderIdFromString("harvest") == 852018);
    TEST_ASSERT(OrderIdFromString("returnresources") == 852020);
    TEST_ASSERT(OrderIdFromString("repair") == 852024);
    TEST_ASSERT(OrderIdFromString("smart") == 851971);
    TEST_ASSERT(OrderIdFromString("move") == 851986);
    TEST_ASSERT(OrderIdFromString("stop") == 851972);
    TEST_ASSERT(OrderIdFromString("holdposition") == 851993);
    // Case-insensitive like ref OrderIdUtils.
    TEST_ASSERT(OrderIdFromString("WhirlWind") == 852128);
    TEST_ASSERT(OrderIdFromString("no_such_order_xyz") == 0);
    // Reverse mapping round-trips.
    TEST_ASSERT(OrderStringFromId(852128) == "whirlwind");
    TEST_ASSERT(OrderStringFromId(852183) == "starfall");
    TEST_ASSERT(OrderStringFromId(851971) == "smart");
    TEST_ASSERT(OrderStringFromId(999999999) == "");
    return true;
}

bool TestOrderIdConstantsMatchRetail() {
    // order.h constants must equal the retail ids in ref OrderIds.java
    // AND the generated string table, or JASS OrderId("...") comparisons
    // against engine-issued ids silently never match. Guards the fixed
    // misassignments (build menus were dropitem+; cancels were buildmenu).
    TEST_ASSERT(OrderIds::HUMAN_BUILD == 851995);
    TEST_ASSERT(OrderIds::ORC_BUILD == 851996);
    TEST_ASSERT(OrderIds::NIGHT_ELF_BUILD == 851997);
    TEST_ASSERT(OrderIds::UNDEAD_BUILD == 851998);
    TEST_ASSERT(OrderIds::CANCEL == 851976);
    TEST_ASSERT(OrderIds::CANCEL_BUILD == 851976);
    TEST_ASSERT(OrderIds::CANCEL_TRAIN == 851976);
    TEST_ASSERT(OrderIds::HERO_SKILL == 852000);
    TEST_ASSERT(OrderIds::SELECT_SKILL == 852000);
    TEST_ASSERT(OrderIdFromString("humanbuild") == OrderIds::HUMAN_BUILD);
    TEST_ASSERT(OrderIdFromString("orcbuild") == OrderIds::ORC_BUILD);
    TEST_ASSERT(OrderIdFromString("nightelfbuild") == OrderIds::NIGHT_ELF_BUILD);
    TEST_ASSERT(OrderIdFromString("undeadbuild") == OrderIds::UNDEAD_BUILD);
    TEST_ASSERT(OrderIdFromString("cancel") == OrderIds::CANCEL);
    TEST_ASSERT(OrderIdFromString("skillmenu") == OrderIds::HERO_SKILL);
    TEST_ASSERT(OrderIdFromString("dropitem") == OrderIds::ITEM_DROP);
    TEST_ASSERT(OrderIdFromString("returnresources") == OrderIds::RETURN_RESOURCES);
    // No collisions between the fixed groups.
    TEST_ASSERT(OrderIds::HUMAN_BUILD != OrderIds::ITEM_DROP);
    TEST_ASSERT(OrderIds::CANCEL_BUILD != OrderIds::HUMAN_BUILD);
    TEST_ASSERT(OrderIds::RETURN_RESOURCES != OrderIds::HUMAN_BUILD);
    return true;
}

bool TestSpellBaseOrderIds() {
    auto orderOf = [](const char* code) {
        auto ability = AbilityFactory::CreateAbility(19000, War3ID(code));
        return ability ? ability->GetBaseOrderId() : -1;
    };
    TEST_ASSERT(orderOf("AHbz") == 852089); // blizzard
    TEST_ASSERT(orderOf("AOww") == 852128); // whirlwind
    TEST_ASSERT(orderOf("AEst") == 852181); // scout
    TEST_ASSERT(orderOf("AEsf") == 852183); // starfall
    TEST_ASSERT(orderOf("AHfs") == 852488); // flamestrike
    TEST_ASSERT(orderOf("AEim") == 852177); // immolation
    TEST_ASSERT(orderOf("ANfl") == 852587); // forkedlightning
    // Passives and item powers carry no engine order.
    TEST_ASSERT(orderOf("AEev") == 0); // evasion
    TEST_ASSERT(orderOf("AOcr") == 0); // critical strike
    TEST_ASSERT(orderOf("Apxf") == 0); // phoenix fire
    TEST_ASSERT(orderOf("AIha") == 0); // item heal group
    return true;
}

bool TestSpellOrderDispatch() {
    for (const char* orderStr : {"flamestrike", "blizzard"}) {
        CSimulation sim;
        AddPlayers(sim);
        auto* caster = AdoptAt(sim, 1911, 0, 0, 0);
        auto* foe = AdoptAt(sim, 1912, 1, 200, 0);
        (void)foe;
        Settle(sim);
        caster->SetMana(500.0f);
        // Blizzard first: routing must follow the order, not position.
        Grant(sim, caster, War3ID("AHbz"), 19101);
        Grant(sim, caster, War3ID("AHfs"), 19102);
        auto* blizz = caster->GetAbility(War3ID("AHbz"));
        auto* flame = caster->GetAbility(War3ID("AHfs"));
        TEST_ASSERT(blizz && flame);
        caster->IssueOrder(std::make_unique<COrderTargetPoint>(
            OrderIdFromString(orderStr), 200, 0));
        for (int i = 0; i < 90; i++) sim.Update();
        const bool wantFlame = std::string(orderStr) == "flamestrike";
        auto* spell = dynamic_cast<CAbilitySpellBase*>(wantFlame ? flame : blizz);
        auto* other = dynamic_cast<CAbilitySpellBase*>(wantFlame ? blizz : flame);
        TEST_ASSERT(spell && other);
        TEST_ASSERT(spell->GetCooldownRemaining() > 0.0f); // fired
        TEST_ASSERT(other->GetCooldownRemaining() == 0.0f); // spared
    }
    return true;
}

bool TestWrongOrderDoesNotCast() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1921, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1922, 1, 200, 0);
    (void)foe;
    Settle(sim);
    caster->SetMana(500.0f);
    Grant(sim, caster, War3ID("AHbz"), 19201);
    auto* blizz = dynamic_cast<CAbilitySpellBase*>(caster->GetAbility(War3ID("AHbz")));
    TEST_ASSERT(blizz != nullptr);
    // A foreign spell order must not trigger Blizzard.
    TEST_ASSERT(!blizz->CheckCanTargetPoint(sim, caster, OrderIdFromString("flamestrike"), 200, 0));
    caster->IssueOrder(std::make_unique<COrderTargetPoint>(
        OrderIdFromString("flamestrike"), 200, 0));
    for (int i = 0; i < 90; i++) sim.Update();
    TEST_ASSERT(blizz->GetCooldownRemaining() == 0.0f);
    return true;
}

bool TestSmartDoesNotCastSpells() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1931, 0, 0, 0);
    Settle(sim);
    caster->SetMana(500.0f);
    Grant(sim, caster, War3ID("AHbz"), 19301);
    auto* blizz = dynamic_cast<CAbilitySpellBase*>(caster->GetAbility(War3ID("AHbz")));
    TEST_ASSERT(blizz != nullptr);
    // Right-click (SMART) must move, never cast (ref order checks).
    TEST_ASSERT(!blizz->CheckCanTargetPoint(sim, caster, OrderIds::SMART, 200, 0));
    caster->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, 200, 0));
    for (int i = 0; i < 90; i++) sim.Update();
    TEST_ASSERT(blizz->GetCooldownRemaining() == 0.0f);
    return true;
}

bool TestPassiveNeverMatchesOrders() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1941, 0, 0, 0);
    Settle(sim);
    Grant(sim, unit, War3ID("AEev"), 19401);
    auto* evasion = dynamic_cast<CAbilitySpellBase*>(unit->GetAbility(War3ID("AEev")));
    TEST_ASSERT(evasion != nullptr);
    TEST_ASSERT(!evasion->CheckCanTargetNoTarget(sim, unit, OrderIds::STOP));
    TEST_ASSERT(!evasion->CheckCanTargetNoTarget(sim, unit, OrderIds::SMART));
    TEST_ASSERT(evasion->CheckCanTargetNoTarget(sim, unit, 0));
    return true;
}

bool TestSmartPointOrderMovesUnit() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1951, 0, 0, 0);
    Settle(sim);
    Grant(sim, unit, War3ID("Amov"), 19501);
    unit->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, 300.0f, 0.0f));
    for (int i = 0; i < 30; i++) sim.Update();
    TEST_ASSERT(unit->GetX() > 20.0f);
    return true;
}

bool TestSmartWidgetFallbackMovesUnit() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1961, 0, 0, 0);
    Settle(sim);
    Grant(sim, unit, War3ID("Amov"), 19601);
    auto* dest = sim.CreateDestructable(War3ID("LTlt"), 400.0f, 0.0f, 100.0f);
    unit->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::SMART, dest));
    for (int i = 0; i < 30; i++) sim.Update();
    TEST_ASSERT(unit->GetX() > 20.0f);
    return true;
}

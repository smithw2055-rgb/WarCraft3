// Transport boarding tests: range behavior, cargo tracking, unload.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_transport.h"
#include "ecs/behaviors/behavior_board_transport.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeTransportType() {
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

} // namespace

bool TestLoadInRangeIsInstant() {
    CSimulation sim;
    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(1, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(2, MakeTransportType(), 0, War3ID("hfoo"), 100, 0, 0));
    CAbilityLoad load(50);
    CBehavior* b = load.Begin(sim, ship, 0, foot);
    TEST_ASSERT(b == nullptr); // in range: no behavior needed
    TEST_ASSERT(ship->GetCargoUnits().size() == 1);
    TEST_ASSERT(foot->IsLoadedAboard());
    TEST_ASSERT(foot->IsHidden());
    delete b;
    return true;
}

bool TestLoadOutOfRangeWalksThenLoads() {
    CSimulation sim;
    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(3, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(4, MakeTransportType(), 0, War3ID("hfoo"), 1000, 0, 0));
    CAbilityLoad load(51);
    CBehavior* b = load.Begin(sim, ship, 0, foot);
    TEST_ASSERT(b != nullptr); // out of range: board behavior
    auto* board = dynamic_cast<CBehaviorBoardTransport*>(b);
    TEST_ASSERT(board != nullptr);
    board->SetUnit(ship);
    board->Begin(sim);
    bool done = false;
    for (int i = 0; i < 2000 && !done; i++) done = board->Update(sim);
    TEST_ASSERT(done);
    TEST_ASSERT(foot->IsLoadedAboard());
    TEST_ASSERT(ship->GetCargoUnits().size() == 1);
    const float dx = foot->GetX() - ship->GetX();
    const float dy = foot->GetY() - ship->GetY();
    TEST_ASSERT(std::sqrt(dx * dx + dy * dy) <= 256.0f);
    delete b;
    return true;
}

bool TestLoadCapacityEnforced() {
    CSimulation sim;
    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(5, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(6, MakeTransportType(), 0, War3ID("hfoo"), 50, 0, 0));
    auto* c = sim.AdoptUnit(std::make_unique<CUnit>(7, MakeTransportType(), 0, War3ID("hfoo"), 60, 0, 0));
    CAbilityLoad load(52);
    load.SetCargoCapacity(1);
    TEST_ASSERT(load.CheckCanTarget(sim, ship, 0, a));
    CBehavior* b1 = load.Begin(sim, ship, 0, a);
    TEST_ASSERT(b1 == nullptr);
    TEST_ASSERT(!load.CheckCanTarget(sim, ship, 0, c)); // full
    CBehavior* b2 = load.Begin(sim, ship, 0, c);
    TEST_ASSERT(b2 == nullptr); // refused, no walk-out
    TEST_ASSERT(!c->IsLoadedAboard());
    TEST_ASSERT(ship->GetCargoUnits().size() == 1);
    delete b1;
    delete b2;
    return true;
}

bool TestUnloadRestoresUnits() {
    CSimulation sim;
    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(8, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(9, MakeTransportType(), 0, War3ID("hfoo"), 50, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(10, MakeTransportType(), 0, War3ID("hfoo"), 60, 0, 0));
    CAbilityLoad load(53);
    TEST_ASSERT(load.Begin(sim, ship, 0, a) == nullptr);
    TEST_ASSERT(load.Begin(sim, ship, 0, b) == nullptr);
    CAbilityUnload unload(54);
    TEST_ASSERT(unload.CheckCanTargetPoint(sim, ship, 0, 500, 500));
    // Timed unload (ref CBehaviorDrop): one passenger per duration tick.
    CBehavior* drop = unload.Begin(sim, ship, 0, 500, 500);
    TEST_ASSERT(drop != nullptr);
    drop->SetUnit(ship);
    drop->Begin(sim);
    bool unloaded = false;
    for (int i = 0; i < 500 && !unloaded; i++) unloaded = drop->Update(sim);
    TEST_ASSERT(unloaded);
    delete drop;
    TEST_ASSERT(ship->GetCargoUnits().empty());
    for (auto* u : {a, b}) {
        TEST_ASSERT(!u->IsLoadedAboard());
        TEST_ASSERT(!u->IsHidden());
        const float dx = u->GetX() - 500.0f;
        const float dy = u->GetY() - 500.0f;
        TEST_NEAR(std::sqrt(dx * dx + dy * dy), 48.0f, 1.0f);
        TEST_ASSERT(u->CanBeTargetedBy(sim, ship, {}));
    }
    TEST_ASSERT(!unload.CheckCanTargetPoint(sim, ship, 0, 500, 500)); // empty now
    return true;
}

bool TestCargoFollowsTransport() {
    CSimulation sim;
    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(11, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(12, MakeTransportType(), 0, War3ID("hfoo"), 50, 0, 0));
    CAbilityLoad load(55);
    TEST_ASSERT(load.Begin(sim, ship, 0, foot) == nullptr);
    TEST_ASSERT(!foot->CanBeTargetedBy(sim, ship, {})); // hidden cargo untargetable
    ship->SetPosition(900, 700);
    sim.Update(); // first tick joins adopted units
    sim.Update(); // cargo rides along
    TEST_ASSERT(foot->GetX() == 900.0f);
    TEST_ASSERT(foot->GetY() == 700.0f);
    return true;
}

bool TestTransportDeathKillsCargo() {
    CSimulation sim;
    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(13, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(14, MakeTransportType(), 0, War3ID("hfoo"), 50, 0, 0));
    CAbilityLoad load(56);
    TEST_ASSERT(load.Begin(sim, ship, 0, foot) == nullptr);
    ship->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 1e9f, 0);
    sim.Update(); // first tick joins adopted units
    sim.Update(); // death processing kills cargo
    TEST_ASSERT(ship->IsDead());
    TEST_ASSERT(foot->IsDead());
    return true;
}

bool TestLoadFiresUnitLoadedCallback() {
    // AdoptUnit hook: fresh boardings reach the sim callback exactly once;
    // re-boarding an aboard passenger is a silent no-op.
    CSimulation sim;
    std::vector<std::pair<int, int>> loads;
    sim.SetUnitLoadedCallback([&](int t, int p) { loads.emplace_back(t, p); });
    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(21, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(22, MakeTransportType(), 0, War3ID("hfoo"), 50, 0, 0));
    TEST_ASSERT(ship->LoadUnit(foot));
    TEST_ASSERT(loads.size() == 1);
    TEST_ASSERT(loads[0].first == 21 && loads[0].second == 22);
    TEST_ASSERT(ship->LoadUnit(foot)); // already aboard
    TEST_ASSERT(loads.size() == 1);
    return true;
}

namespace {

const char* kLoadedScript = R"JASS(
globals
    integer loadedFires = 0
    integer loadedSeen = 0
    integer transportSeen = 0
endglobals

function OnLoaded takes nothing returns nothing
    set loadedFires = loadedFires + 1
    set loadedSeen = GetLoadedUnit()
    set transportSeen = GetTransportUnit()
endfunction
)JASS";

JassValue CallTransportNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) {
        std::cerr << "missing native " << name << "\n";
        return JassValue::Null();
    }
    return it->second(args);
}

} // namespace

bool TestLoadedTriggerEvent() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    if (!vm.LoadScript(kLoadedScript)) return false;
    auto data = natives.GetData();
    // Same wiring as JassCommonEnv.
    sim.SetUnitLoadedCallback([&](int transportHandle, int passengerHandle) {
        natives.FireLoadedEvent(transportHandle, passengerHandle);
    });

    auto* ship = sim.AdoptUnit(std::make_unique<CUnit>(23, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(24, MakeTransportType(), 0, War3ID("hfoo"), 50, 0, 0));
    TEST_ASSERT(ship && foot);
    sim.Update();
    const int hFoot = data->AllocHandle(JassHandleInfo::UNIT, 24);
    const int trig = CallTransportNative(vm, "CreateTrigger", {}).intVal;
    CallTransportNative(vm, "TriggerRegisterUnitEvent", {JassValue::Int(trig), JassValue::Int(hFoot), JassValue::Int(88)});
    CallTransportNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnLoaded")});

    TEST_ASSERT(ship->LoadUnit(foot));
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("loadedFires").intVal == 1);
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("loadedSeen").intVal, JassHandleInfo::UNIT) == 24);
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("transportSeen").intVal, JassHandleInfo::UNIT) == 23);
    return true;
}

bool TestDecayFiresUnitDecayCallback() {
    // Corpse begins decaying: exactly one callback (deathTime 1s @ 20Hz).
    CSimulation sim;
    int fires = 0;
    int firedHandle = -1;
    sim.SetUnitDecayCallback([&](int h) { fires++; firedHandle = h; });
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(25, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    foot->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 1e9f, 0);
    for (int i = 0; i < 500 && fires == 0; i++) sim.Update();
    TEST_ASSERT(fires == 1 && firedHandle == 25);
    for (int i = 0; i < 500; i++) sim.Update();
    TEST_ASSERT(fires == 1); // one-shot: later decay stages never refire
    return true;
}

namespace {

const char* kDecayScript = R"JASS(
globals
    integer decayFires = 0
    integer decayingSeen = 0
endglobals

function OnDecay takes nothing returns nothing
    set decayFires = decayFires + 1
    set decayingSeen = GetDecayingUnit()
endfunction
)JASS";

} // namespace

bool TestDecayTriggerEvent() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    if (!vm.LoadScript(kDecayScript)) return false;
    auto data = natives.GetData();
    // Same wiring as JassCommonEnv.
    sim.SetUnitDecayCallback([&](int unitHandle) {
        natives.FireDecayEvent(unitHandle);
    });

    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(26, MakeTransportType(), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(foot);
    sim.Update();
    const int hFoot = data->AllocHandle(JassHandleInfo::UNIT, 26);
    const int trig = CallTransportNative(vm, "CreateTrigger", {}).intVal;
    CallTransportNative(vm, "TriggerRegisterUnitEvent", {JassValue::Int(trig), JassValue::Int(hFoot), JassValue::Int(54)});
    CallTransportNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnDecay")});

    foot->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 1e9f, 0);
    for (int i = 0; i < 500 && vm.GetGlobal("decayFires").intVal == 0; i++) sim.Update();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("decayFires").intVal == 1);
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("decayingSeen").intVal, JassHandleInfo::UNIT) == 26);
    return true;
}

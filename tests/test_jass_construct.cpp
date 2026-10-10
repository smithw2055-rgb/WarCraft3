// Construction lifecycle JASS events (ref CONSTRUCT_FINISH/CANCEL):
// sim fire sites through to trigger actions and structure getters.
#include <iostream>
#include <memory>
#include <vector>

#include "data/unitdata.h"
#include "ecs/abilities/ability_build.h"
#include "ecs/behaviors/behavior_build.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

CUnitType* MakeConstructType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
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

const char* kConstructScript = R"JASS(
globals
    integer finishFires = 0
    integer finishedSeen = 0
    integer finishingSeen = 0
    integer cancelFires = 0
    integer cancelledSeen = 0
    integer cancellingSeen = 0
endglobals

function OnConstructFinish takes nothing returns nothing
    set finishFires = finishFires + 1
    set finishedSeen = GetConstructedStructure()
    set finishingSeen = GetConstructingStructure()
endfunction

function OnConstructCancel takes nothing returns nothing
    set cancelFires = cancelFires + 1
    set cancelledSeen = GetCancelledStructure()
    set cancellingSeen = GetConstructingStructure()
endfunction
)JASS";

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) {
        std::cerr << "missing native " << name << "\n";
        return JassValue::Null();
    }
    return it->second(args);
}

void WireConstruct(JassNatives& natives, CSimulation& sim) {
    sim.SetConstructEventCallback([&](int buildingHandle, bool finished) {
        if (finished) {
            natives.FireConstructFinishEvent(buildingHandle);
        } else {
            natives.FireConstructCancelEvent(buildingHandle);
        }
    });
}

} // namespace

bool TestConstructFinishEvent() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("hfoo"),
                              std::make_unique<CUnitType>(*MakeConstructType()));
    sim.SetUnitData(&unitData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kConstructScript));
    auto data = natives.GetData();
    WireConstruct(natives, sim);

    auto* worker = sim.AdoptUnit(std::make_unique<CUnit>(
        9101, MakeConstructType(), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(worker);
    sim.Update();

    const int playerHandle = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trig), JassValue::Int(playerHandle), JassValue::Int(28)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnConstructFinish")});

    CBehaviorBuild build;
    build.SetUnit(worker);
    build.SetBuildTarget(0.0f, 0.0f, War3ID("hfoo"), CBuildRace::HUMAN);
    build.Begin(sim);
    // Unit data gives a 30s build time (0.05s ticks); drive to completion.
    for (int i = 0; i < 2000 && !build.Update(sim); i++) {
        sim.Update();
    }
    TEST_ASSERT(!build.IsActivelyBuilding());
    natives.PumpQueuedThreads(1024);

    TEST_ASSERT(vm.GetGlobal("finishFires").intVal == 1);
    const int finishedSim =
        data->GetSimHandle(vm.GetGlobal("finishedSeen").intVal, JassHandleInfo::UNIT);
    const int finishingSim =
        data->GetSimHandle(vm.GetGlobal("finishingSeen").intVal, JassHandleInfo::UNIT);
    TEST_ASSERT(finishedSim >= 0 && finishedSim == finishingSim);
    CUnit* finished = sim.GetUnit(finishedSim);
    TEST_ASSERT(finished && finished->GetTypeId() == War3ID("hfoo"));
    // Human worker survives completion.
    TEST_ASSERT(sim.GetUnit(9101) != nullptr);
    return true;
}

bool TestConstructCancelEvent() {
    JassVM vm;
    CSimulation sim;
    auto player = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
    player->SetGold(1000);
    player->SetLumber(1000);
    sim.AddPlayer(std::move(player));
    // Unit data so the 100g/50l cost deducts (and refunds on cancel).
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("hfoo"),
                              std::make_unique<CUnitType>(*MakeConstructType()));
    sim.SetUnitData(&unitData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kConstructScript));
    auto data = natives.GetData();
    WireConstruct(natives, sim);

    auto* worker = sim.AdoptUnit(std::make_unique<CUnit>(
        9111, MakeConstructType(), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(worker);
    sim.Update();

    const int playerHandle = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trig), JassValue::Int(playerHandle), JassValue::Int(27)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnConstructCancel")});

    // Behavior owned by the unit so the order path runs end to end.
    auto build = std::make_unique<CBehaviorBuild>();
    build->SetUnit(worker);
    build->SetBuildTarget(0.0f, 0.0f, War3ID("hfoo"), CBuildRace::HUMAN);
    build->Begin(sim);
    worker->SetCurrentBehavior(std::move(build));
    for (int i = 0; i < 10; i++) {
        sim.Update();
        auto* active = dynamic_cast<CBehaviorBuild*>(worker->GetCurrentBehavior());
        if (active && active->IsActivelyBuilding()) break;
    }
    {
        auto* active = dynamic_cast<CBehaviorBuild*>(worker->GetCurrentBehavior());
        TEST_ASSERT(active && active->IsActivelyBuilding());
    }
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 900);

    // ESC-cancel through the real order dispatch.
    worker->IssueOrder(std::make_unique<COrderNoTarget>(OrderIds::CANCEL_BUILD));
    for (int i = 0; i < 5; i++) sim.Update();
    natives.PumpQueuedThreads(1024);

    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 1000);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 1000);
    TEST_ASSERT(vm.GetGlobal("cancelFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("cancelledSeen").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("cancelledSeen").intVal == vm.GetGlobal("cancellingSeen").intVal);
    // Foundation removed, worker alive and idle.
    sim.Update();
    TEST_ASSERT(sim.GetUnit(9111) != nullptr);
    TEST_ASSERT(sim.GetUnits().size() == 1);
    return true;
}

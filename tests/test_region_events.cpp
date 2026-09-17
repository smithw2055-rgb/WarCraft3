// Region enter/leave + lifecycle (train/summon/research/revive) event tests.
// Drives JassNatives through its registered native table with real JASS
// action functions, mirroring ref CRegionTriggerEnter/Leave and
// TriggerRegisterPlayerUnitEvent semantics.
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeRegionTestType() {
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

const char* kRegionScript = R"JASS(
globals
    integer enterCount = 0
    integer leaveCount = 0
    integer lastEnter = 0
    integer lastLeave = 0
endglobals

function OnRegionEnter takes nothing returns nothing
    set enterCount = enterCount + 1
    set lastEnter = GetEnteringUnit()
endfunction

function OnRegionLeave takes nothing returns nothing
    set leaveCount = leaveCount + 1
    set lastLeave = GetLeavingUnit()
endfunction
)JASS";

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

} // namespace

bool TestRegionEnterLeaveFiresOnMigration() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kRegionScript));

    const int rect = CallNative(vm, "Rect", {JassValue::Real(-100.0f), JassValue::Real(-100.0f),
                                             JassValue::Real(100.0f), JassValue::Real(100.0f)}).intVal;
    TEST_ASSERT(rect != 0);
    const int region = CallNative(vm, "CreateRegion", {}).intVal;
    TEST_ASSERT(region != 0);
    CallNative(vm, "RegionAddRect", {JassValue::Int(region), JassValue::Int(rect)});
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterEnterRegion",
               {JassValue::Int(trig), JassValue::Int(region), JassValue::Null()});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnRegionEnter")});
    const int trigLeave = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterLeaveRegion",
               {JassValue::Int(trigLeave), JassValue::Int(region), JassValue::Null()});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trigLeave), JassValue::Str("OnRegionLeave")});

    CUnitType* type = MakeRegionTestType();
    auto* outsider = sim.AdoptUnit(std::make_unique<CUnit>(1001, type, 0, War3ID("hfoo"), 500, 500, 0));
    auto* insider = sim.AdoptUnit(std::make_unique<CUnit>(1002, type, 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(outsider && insider);
    sim.Update(); // publish adopted units to GetUnits()

    // Baseline: preplaced insider must not fire.
    natives.UpdateRegions();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("enterCount").intVal == 0);
    TEST_ASSERT(vm.GetGlobal("leaveCount").intVal == 0);

    // Migrate outsider in.
    outsider->SetPosition(10.0f, 10.0f);
    natives.UpdateRegions();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("enterCount").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("lastEnter").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("leaveCount").intVal == 0);

    // No repeat while stationary.
    natives.UpdateRegions();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("enterCount").intVal == 1);

    // Migrate insider out.
    insider->SetPosition(500.0f, 0.0f);
    natives.UpdateRegions();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("leaveCount").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("lastLeave").intVal != 0);
    return true;
}

const char* kLifecycleScript = R"JASS(
globals
    integer trainFires = 0
    integer trainedSeen = 0
    integer trainedTypeSeen = 0
    integer summonFires = 0
    integer summonedSeen = 0
    integer summonerSeen = 0
    integer researchFires = 0
    integer researchedSeen = 0
    integer researchingSeen = 0
    integer reviveFires = 0
    integer revivedSeen = 0
endglobals

function OnTrain takes nothing returns nothing
    set trainFires = trainFires + 1
    set trainedSeen = GetTrainedUnit()
    set trainedTypeSeen = GetTrainedUnitType()
endfunction

function OnSummon takes nothing returns nothing
    set summonFires = summonFires + 1
    set summonedSeen = GetSummonedUnit()
    set summonerSeen = GetSummoningUnit()
endfunction

function OnResearch takes nothing returns nothing
    set researchFires = researchFires + 1
    set researchedSeen = GetResearched()
    set researchingSeen = GetResearchingUnit()
endfunction

function OnRevive takes nothing returns nothing
    set reviveFires = reviveFires + 1
    set revivedSeen = GetRevivingUnit()
endfunction
)JASS";

bool TestLifecycleEventsAndPlayerUnitDualMatch() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kLifecycleScript));

    CUnitType* type = MakeRegionTestType();
    auto* building = sim.AdoptUnit(std::make_unique<CUnit>(2001, type, 0, War3ID("hfoo"), 0, 0, 0));
    auto* trained = sim.AdoptUnit(std::make_unique<CUnit>(2002, type, 0, War3ID("hfoo"), 64, 0, 0));
    TEST_ASSERT(building && trained);
    sim.Update();

    auto data = natives.GetData();
    const int playerHandle = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int buildingHandle = data->AllocHandle(JassHandleInfo::UNIT, 2001);

    // Register BOTH the unit-event (71) and player-unit-event (34) variants:
    // one FireTrainedEvent must wake both, proving the dual-id match.
    const int tUnit = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterUnitEvent",
               {JassValue::Int(tUnit), JassValue::Int(buildingHandle), JassValue::Int(71)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(tUnit), JassValue::Str("OnTrain")});
    const int tPlayer = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(tPlayer), JassValue::Int(playerHandle), JassValue::Int(34)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(tPlayer), JassValue::Str("OnTrain")});

    natives.FireTrainedEvent(2001, 2002);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("trainFires").intVal == 2);
    TEST_ASSERT(vm.GetGlobal("trainedSeen").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("trainedTypeSeen").intVal == (int)War3ID("hfoo").value);

    const int tSummon = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(tSummon), JassValue::Int(playerHandle), JassValue::Int(47)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(tSummon), JassValue::Str("OnSummon")});
    natives.FireSummonEvent(2001, 2002);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("summonFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("summonedSeen").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("summonerSeen").intVal != 0);

    const int tResearch = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(tResearch), JassValue::Int(playerHandle), JassValue::Int(37)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(tResearch), JassValue::Str("OnResearch")});
    natives.FireResearchEvent(2001, 1234);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("researchFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("researchedSeen").intVal == 1234);
    TEST_ASSERT(vm.GetGlobal("researchingSeen").intVal != 0);

    const int tRevive = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(tRevive), JassValue::Int(playerHandle), JassValue::Int(46)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(tRevive), JassValue::Str("OnRevive")});
    natives.FireReviveEvent(2001, 2002);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("reviveFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("revivedSeen").intVal != 0);
    return true;
}

// Rescue behavior (ref SetUnitRescuable): proximity transfer to the
// rescuing player with RESCUED triggers and GetRescuer context.
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
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

CUnitType* MakeRescueType() {
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

const char* kRescueScript = R"JASS(
globals
    integer rescueFires = 0
    integer rescuerSeen = 0
endglobals

function OnRescue takes nothing returns nothing
    set rescueFires = rescueFires + 1
    set rescuerSeen = GetRescuer()
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

} // namespace

bool TestRescueTransfersOwner() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kRescueScript));
    auto data = natives.GetData();

    auto* victim = sim.AdoptUnit(std::make_unique<CUnit>(
        9301, MakeRescueType(), 1, War3ID("hfoo"), 0, 0, 0));
    auto* rescuer = sim.AdoptUnit(std::make_unique<CUnit>(
        9302, MakeRescueType(), 0, War3ID("hfoo"), 500, 500, 0));
    TEST_ASSERT(victim && rescuer);
    sim.Update();
    const int victimHandle = data->AllocHandle(JassHandleInfo::UNIT, 9301);
    const int p0 = data->AllocHandle(JassHandleInfo::PLAYER, 0);

    CallNative(vm, "SetUnitRescuable",
               {JassValue::Int(victimHandle), JassValue::Int(p0), JassValue::Bool(true)});
    CallNative(vm, "SetUnitRescueRange", {JassValue::Int(victimHandle), JassValue::Real(200.0f)});

    const int trigPlayer = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trigPlayer), JassValue::Int(p0), JassValue::Int(19)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trigPlayer), JassValue::Str("OnRescue")});
    const int trigUnit = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterUnitEvent",
               {JassValue::Int(trigUnit), JassValue::Int(victimHandle), JassValue::Int(63)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trigUnit), JassValue::Str("OnRescue")});

    // Out of range: nothing happens.
    natives.UpdateRescue();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("rescueFires").intVal == 0);
    TEST_ASSERT(victim->GetPlayerIndex() == 1);

    // In range: ownership transfers, both variants fire once.
    rescuer->SetPosition(10.0f, 10.0f);
    natives.UpdateRescue();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(victim->GetPlayerIndex() == 0);
    TEST_ASSERT(vm.GetGlobal("rescueFires").intVal == 2);
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("rescuerSeen").intVal, JassHandleInfo::UNIT) == 9302);

    // Settled: no refire on later polls.
    natives.UpdateRescue();
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("rescueFires").intVal == 2);
    return true;
}

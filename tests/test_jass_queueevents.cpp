// Queue lifecycle JASS events (ref TRAIN/RESEARCH/UPGRADE start/cancel/
// finish): sim fire sites through to trigger actions and type getters.
#include <iostream>
#include <memory>
#include <vector>

#include "data/unitdata.h"
#include "data/upgradedata.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/abilities/ability_upgrade.h"
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

CUnitType* MakeQueueTestType(const char* name, War3ID id) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        name, name, "icon", id,
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

const char* kQueueEventScript = R"JASS(
globals
    integer trainStartFires = 0
    integer trainStartType = 0
    integer trainCancelFires = 0
    integer trainCancelType = 0
    integer trainFinishFires = 0
    integer trainedSeen = 0
    integer researchStartFires = 0
    integer researchStartSeen = 0
    integer researchCancelFires = 0
    integer researchCancelSeen = 0
    integer upgradeStartFires = 0
    integer upgradeStartSeen = 0
    integer upgradeFinishFires = 0
    integer upgradeFinishSeen = 0
endglobals

function OnTrainStart takes nothing returns nothing
    set trainStartFires = trainStartFires + 1
    set trainStartType = GetTrainedUnitType()
endfunction

function OnTrainCancel takes nothing returns nothing
    set trainCancelFires = trainCancelFires + 1
    set trainCancelType = GetTrainedUnitType()
endfunction

function OnTrainFinish takes nothing returns nothing
    set trainFinishFires = trainFinishFires + 1
    set trainedSeen = GetTrainedUnit()
endfunction

function OnResearchStart takes nothing returns nothing
    set researchStartFires = researchStartFires + 1
    set researchStartSeen = GetResearched()
endfunction

function OnResearchCancel takes nothing returns nothing
    set researchCancelFires = researchCancelFires + 1
    set researchCancelSeen = GetResearched()
endfunction

function OnUpgradeStart takes nothing returns nothing
    set upgradeStartFires = upgradeStartFires + 1
    set upgradeStartSeen = GetResearched()
endfunction

function OnUpgradeFinish takes nothing returns nothing
    set upgradeFinishFires = upgradeFinishFires + 1
    set upgradeFinishSeen = GetResearched()
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

void WireQueue(JassNatives& natives, CSimulation& sim) {
    sim.SetUnitTrainedCallback([&](int buildingHandle, int trainedHandle) {
        natives.FireTrainedEvent(buildingHandle, trainedHandle);
    });
    sim.SetQueueEventCallback([&](int buildingHandle, int typeId, int kind, bool started) {
        if (kind == 0) {
            if (started) {
                natives.FireTrainStartEvent(buildingHandle, typeId);
            } else {
                natives.FireTrainCancelEvent(buildingHandle, typeId);
            }
        } else if (kind == 1) {
            if (started) {
                natives.FireResearchStartEvent(buildingHandle, typeId);
            } else {
                natives.FireResearchCancelEvent(buildingHandle, typeId);
            }
        } else {
            if (started) {
                natives.FireUpgradeStartEvent(buildingHandle, typeId);
            } else {
                natives.FireUpgradeFinishEvent(buildingHandle, typeId);
            }
        }
    });
}

struct QueueFixture {
    JassVM vm;
    CSimulation sim;
    std::unique_ptr<JassNatives> natives;
    std::shared_ptr<JassNativesData> data;
    CUnit* factory = nullptr;
    CAbilityQueue* queue = nullptr;
    int playerHandle = 0;

    QueueFixture() {
        auto player = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
        player->SetGold(100000);
        player->SetLumber(100000);
        player->SetFoodCap(100);
        sim.AddPlayer(std::move(player));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
        WireQueue(*natives, sim);
    }

    bool Load() {
        if (!vm.LoadScript(kQueueEventScript)) return false;
        playerHandle = data->AllocHandle(JassHandleInfo::PLAYER, 0);
        return true;
    }

    void Watch(int playerEventCode, const std::string& action) {
        const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
        CallNative(vm, "TriggerRegisterPlayerUnitEvent",
                   {JassValue::Int(trig), JassValue::Int(playerHandle), JassValue::Int(playerEventCode)});
        CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str(action)});
    }

    void BuildFactory() {
        factory = sim.AdoptUnit(std::make_unique<CUnit>(
            9201, MakeQueueTestType("Barracks", War3ID("hbar")), 0, War3ID("hbar"), 0, 0, 0));
        factory->AddAbility(sim, std::make_unique<CAbilityQueue>(9202));
        queue = dynamic_cast<CAbilityQueue*>(factory->GetAbility(War3ID("Aque")));
        sim.Update();
    }
};

} // namespace

bool TestTrainStartCancelEvent() {
    QueueFixture f;
    TEST_ASSERT(f.Load());
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("hfoo"),
                              std::make_unique<CUnitType>(*MakeQueueTestType("Foot", War3ID("hfoo"))));
    f.sim.SetUnitData(&unitData);
    f.BuildFactory();
    TEST_ASSERT(f.queue != nullptr);
    f.Watch(32, "OnTrainStart");
    f.Watch(33, "OnTrainCancel");

    // Queue: start fires with the type (no unit exists yet).
    f.queue->QueueUnit(f.sim, f.factory, War3ID("hfoo"));
    f.natives->PumpQueuedThreads(1024);
    TEST_ASSERT(f.vm.GetGlobal("trainStartFires").intVal == 1);
    TEST_ASSERT(f.vm.GetGlobal("trainStartType").intVal == (int)War3ID("hfoo").value);

    // Cancel: refunded, event carries the removed type.
    const int goldBefore = f.sim.GetPlayer(0)->GetGold();
    f.queue->CancelQueueItem(f.sim, f.factory, 0);
    f.natives->PumpQueuedThreads(1024);
    TEST_ASSERT(f.vm.GetGlobal("trainCancelFires").intVal == 1);
    TEST_ASSERT(f.vm.GetGlobal("trainCancelType").intVal == (int)War3ID("hfoo").value);
    TEST_ASSERT(f.sim.GetPlayer(0)->GetGold() == goldBefore + 100);
    return true;
}

bool TestTrainFinishEvent() {
    QueueFixture f;
    TEST_ASSERT(f.Load());
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("hfoo"),
                              std::make_unique<CUnitType>(*MakeQueueTestType("Foot", War3ID("hfoo"))));
    f.sim.SetUnitData(&unitData);
    f.BuildFactory();
    f.Watch(34, "OnTrainFinish");

    f.queue->QueueUnit(f.sim, f.factory, War3ID("hfoo"));
    // 30s build time at 0.05s ticks; drive past completion.
    for (int i = 0; i < 800; i++) f.sim.Update();
    f.natives->PumpQueuedThreads(1024);
    TEST_ASSERT(f.vm.GetGlobal("trainFinishFires").intVal == 1);
    TEST_ASSERT(f.vm.GetGlobal("trainedSeen").intVal != 0);
    return true;
}

bool TestResearchStartCancelEvent() {
    QueueFixture f;
    TEST_ASSERT(f.Load());
    CUpgradeData upgradeData;
    auto upgrade = std::make_unique<CUpgradeType>();
    upgrade->id = War3ID("Rhme");
    upgrade->goldCost = 100;
    upgrade->lumberCost = 50;
    upgrade->maxLevel = 1;
    upgradeData.RegisterUpgrade(War3ID("Rhme"), std::move(upgrade));
    f.sim.SetUpgradeData(&upgradeData);
    f.BuildFactory();
    TEST_ASSERT(f.queue != nullptr);
    f.Watch(35, "OnResearchStart");
    f.Watch(36, "OnResearchCancel");

    f.queue->QueueResearch(f.sim, f.factory, War3ID("Rhme"));
    f.natives->PumpQueuedThreads(1024);
    TEST_ASSERT(f.vm.GetGlobal("researchStartFires").intVal == 1);
    TEST_ASSERT(f.vm.GetGlobal("researchStartSeen").intVal == (int)War3ID("Rhme").value);

    f.queue->CancelQueueItem(f.sim, f.factory, 0);
    f.natives->PumpQueuedThreads(1024);
    TEST_ASSERT(f.vm.GetGlobal("researchCancelFires").intVal == 1);
    TEST_ASSERT(f.vm.GetGlobal("researchCancelSeen").intVal == (int)War3ID("Rhme").value);
    return true;
}

bool TestUpgradeStartFinishEvent() {
    QueueFixture f;
    TEST_ASSERT(f.Load());
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("hkee"),
                              std::make_unique<CUnitType>(*MakeQueueTestType("Keep", War3ID("hkee"))));
    f.sim.SetUnitData(&unitData);
    f.BuildFactory();
    auto upgrade = std::make_unique<CAbilityUpgrade>(9203);
    upgrade->AddUpgradeTo(War3ID("hkee"));
    f.factory->AddAbility(f.sim, std::move(upgrade));
    f.Watch(29, "OnUpgradeStart");
    f.Watch(31, "OnUpgradeFinish");

    auto* up = dynamic_cast<CAbilityUpgrade*>(f.factory->GetAbility(War3ID("Aupg")));
    TEST_ASSERT(up != nullptr);
    up->BeginNoTarget(f.sim, f.factory, (int)War3ID("hkee").value);
    f.natives->PumpQueuedThreads(1024);
    TEST_ASSERT(f.vm.GetGlobal("upgradeStartFires").intVal == 1);
    TEST_ASSERT(f.vm.GetGlobal("upgradeStartSeen").intVal == (int)War3ID("hkee").value);

    // Build time comes from the form type (30s); drive past morphing.
    for (int i = 0; i < 800; i++) f.sim.Update();
    f.natives->PumpQueuedThreads(1024);
    TEST_ASSERT(f.vm.GetGlobal("upgradeFinishFires").intVal == 1);
    TEST_ASSERT(f.vm.GetGlobal("upgradeFinishSeen").intVal == (int)War3ID("hkee").value);
    return true;
}

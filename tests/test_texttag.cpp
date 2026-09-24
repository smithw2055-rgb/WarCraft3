// Floating-text lifecycle: age/velocity tick, lifespan expiry frees the
// tag, PosUnit snaps to the unit.
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/slk.h"
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

JassValue CallNative(JassVM& vm, const std::string& name,
                     const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

CUnitType* MakeTagType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Footman", "Footman", "icon", War3ID("hfoo"),
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

} // namespace

bool TestTextTagAgeAndExpiry() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    TEST_ASSERT(data != nullptr);

    const int tag = CallNative(vm, "CreateTextTag", {}).intVal;
    TEST_ASSERT(tag != 0);
    TEST_ASSERT(data->textTags.size() == 1);
    auto stored = data->textTags.begin()->second;
    CallNative(vm, "SetTextTagText",
        {JassValue::Int(tag), JassValue::Str("hi"), JassValue::Real(10.0f)});
    TEST_ASSERT(stored->text == "hi");
    CallNative(vm, "SetTextTagLifespan", {JassValue::Int(tag), JassValue::Real(1.0f)});
    CallNative(vm, "SetTextTagAge", {JassValue::Int(tag), JassValue::Real(0.5f)});
    TEST_ASSERT(stored->age == 0.5f);
    // Half a second of ticks must not retire it yet.
    natives.UpdateTextTags(0.4f);
    TEST_ASSERT(data->textTags.size() == 1);
    // Past lifespan without permanent flag: retired and handle freed.
    natives.UpdateTextTags(0.2f);
    TEST_ASSERT(data->textTags.empty());
    return true;
}

bool TestTextTagPermanentSurvives() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    const int tag = CallNative(vm, "CreateTextTag", {}).intVal;
    CallNative(vm, "SetTextTagLifespan", {JassValue::Int(tag), JassValue::Real(1.0f)});
    CallNative(vm, "SetTextTagPermanent", {JassValue::Int(tag), JassValue::Bool(true)});
    natives.UpdateTextTags(5.0f);
    TEST_ASSERT(data->textTags.size() == 1);
    // Suspended tags neither age nor drift.
    auto stored = data->textTags.begin()->second;
    CallNative(vm, "SetTextTagSuspended", {JassValue::Int(tag), JassValue::Bool(true)});
    const float ageBefore = stored->age;
    natives.UpdateTextTags(5.0f);
    TEST_ASSERT(stored->age == ageBefore);
    return true;
}

bool TestTextTagPosUnitSnap() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* unit = sim.AdoptUnit(
        std::make_unique<CUnit>(8601, MakeTagType(), 0, War3ID("hfoo"), 111.0f, 222.0f, 0));
    const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, 8601);
    const int tag = CallNative(vm, "CreateTextTag", {}).intVal;
    CallNative(vm, "SetTextTagPosUnit",
        {JassValue::Int(tag), JassValue::Int(unitHandle), JassValue::Real(30.0f)});
    auto stored = data->textTags.begin()->second;
    TEST_ASSERT(stored->x == 111.0f && stored->y == 222.0f);
    TEST_ASSERT(stored->height == 30.0f);
    (void)unit;
    return true;
}

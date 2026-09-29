// Effect attachment tests: AddSpecialEffectTarget keeps the widget +
// attach point (previously dropped), AddSpecialEffectLoc resolves the
// location (previously dropped), BlzGetSpecialEffectX/Y follow targets.
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

CUnitType* MakeEffectType() {
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

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

} // namespace

bool TestPointEffectReportsStoredCoords() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    const int fx = CallNative(vm, "AddSpecialEffect",
        {JassValue::Str("Abilities\\Spells\\Human\\FlameStrike\\FlameStrike.mdx"),
         JassValue::Real(300), JassValue::Real(400)}).intVal;
    TEST_ASSERT(fx != 0);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectX", {JassValue::Int(fx)}).realVal == 300.0f);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectY", {JassValue::Int(fx)}).realVal == 400.0f);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectZ", {JassValue::Int(fx)}).realVal == 0.0f);
    return true;
}

bool TestLocEffectResolvesLocation() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    const int loc = CallNative(vm, "Location", {JassValue::Real(111), JassValue::Real(222)}).intVal;
    const int fx = CallNative(vm, "AddSpecialEffectLoc",
        {JassValue::Str("Foo.mdx"), JassValue::Int(loc)}).intVal;
    TEST_ASSERT(fx != 0);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectX", {JassValue::Int(fx)}).realVal == 111.0f);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectY", {JassValue::Int(fx)}).realVal == 222.0f);
    return true;
}

bool TestTargetEffectFollowsUnit() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(8201, MakeEffectType(), 0, War3ID("hfoo"), 50, 60, 0));
    sim.Update();
    const int h = data->AllocHandle(JassHandleInfo::UNIT, 8201);
    const int fx = CallNative(vm, "AddSpecialEffectTarget",
        {JassValue::Str("Foo.mdx"), JassValue::Int(h), JassValue::Str("hand")}).intVal;
    TEST_ASSERT(fx != 0);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectX", {JassValue::Int(fx)}).realVal == 50.0f);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectY", {JassValue::Int(fx)}).realVal == 60.0f);
    // The effect rides the unit.
    u->SetPosition(700, 800);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectX", {JassValue::Int(fx)}).realVal == 700.0f);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectY", {JassValue::Int(fx)}).realVal == 800.0f);
    // Destroyed handles read 0.
    CallNative(vm, "DestroyEffect", {JassValue::Int(fx)});
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectX", {JassValue::Int(fx)}).realVal == 0.0f);
    return true;
}

bool TestTargetEffectFallsBackWhenTargetGone() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    sim.AdoptUnit(std::make_unique<CUnit>(8301, MakeEffectType(), 0, War3ID("hfoo"), 50, 60, 0));
    sim.Update();
    const int h = data->AllocHandle(JassHandleInfo::UNIT, 8301);
    const int fx = CallNative(vm, "AddSpecialEffectTarget",
        {JassValue::Str("Foo.mdx"), JassValue::Int(h), JassValue::Str("origin")}).intVal;
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectX", {JassValue::Int(fx)}).realVal == 50.0f);
    // Removing the target drops back to the stored point (0,0 here).
    sim.RemoveUnit(sim.GetUnit(8301));
    sim.Update();
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectX", {JassValue::Int(fx)}).realVal == 0.0f);
    TEST_ASSERT(CallNative(vm, "BlzGetSpecialEffectY", {JassValue::Int(fx)}).realVal == 0.0f);
    return true;
}

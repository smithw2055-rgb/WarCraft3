// P2-18 spell event context: FireSpellEvent populates GetSpellTarget*
// state per widget kind; illusion flag; effect Z rides fly height.
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/item.h"
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

CUnitType* MakeSpellType() {
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

struct Fixture {
    JassVM vm;
    CSimulation sim;
    std::unique_ptr<JassNatives> natives;
    std::shared_ptr<JassNativesData> data;
    Fixture() {
        sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
    }
};

} // namespace

bool TestSpellTargetWidgets() {
    // Each cast populates exactly one widget-kind target plus the aimed
    // point (ref spell event state); a targetless cast clears all three.
    Fixture f;
    auto* caster = f.sim.AdoptUnit(std::make_unique<CUnit>(
        9101, MakeSpellType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* foe = f.sim.AdoptUnit(std::make_unique<CUnit>(
        9102, MakeSpellType(), 0, War3ID("hfoo"), 100, 0, 0));
    TEST_ASSERT(caster && foe);
    f.sim.Update();
    CDestructable* tree = f.sim.CreateDestructable(War3ID("LTlt"), 200, 0, 100, 0);
    CItem* scroll = f.sim.CreateItem(War3ID("rde1"), 300, 0);
    TEST_ASSERT(tree && scroll);

    // Unit target: unit handle live, dest/item zero, point follows.
    f.natives->FireSpellEvent(9101, 2, 9102, 100.0f, 0.0f);
    const int tu = CallNative(f.vm, "GetSpellTargetUnit", {}).intVal;
    TEST_ASSERT(tu != 0);
    TEST_ASSERT(f.data->GetSimHandle(tu, JassHandleInfo::UNIT) == 9102);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetDestructable", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetItem", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetX", {}).realVal == 100.0f);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetY", {}).realVal == 0.0f);

    // Destructable target: only the dest handle is live.
    f.natives->FireSpellEvent(9101, 2, tree->GetHandleId(), 200.0f, 0.0f);
    const int td = CallNative(f.vm, "GetSpellTargetDestructable", {}).intVal;
    TEST_ASSERT(td != 0);
    TEST_ASSERT(f.data->GetSimHandle(td, JassHandleInfo::DESTRUCTABLE) == tree->GetHandleId());
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetUnit", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetItem", {}).intVal == 0);

    // Item target: only the item handle is live.
    f.natives->FireSpellEvent(9101, 2, scroll->GetHandleId(), 300.0f, 0.0f);
    const int ti = CallNative(f.vm, "GetSpellTargetItem", {}).intVal;
    TEST_ASSERT(ti != 0);
    TEST_ASSERT(f.data->GetSimHandle(ti, JassHandleInfo::ITEM) == scroll->GetHandleId());
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetUnit", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetDestructable", {}).intVal == 0);

    // Point-only cast clears all widget targets but keeps the point.
    f.natives->FireSpellEvent(9101, 2, -1, 400.0f, 50.0f);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetUnit", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetDestructable", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetItem", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetX", {}).realVal == 400.0f);
    TEST_ASSERT(CallNative(f.vm, "GetSpellTargetY", {}).realVal == 50.0f);
    return true;
}

bool TestUnitIllusionFlag() {
    // Copies raised by the illusion producers read true; everyone else
    // reads false (flag, not the timed-life buff, ref IsUnitIllusion).
    Fixture f;
    CUnitData unitData;
    {
        CUnitAttack atk;
        atk.range = 100.0f;
        atk.cooldown = 1.0f;
        atk.damageBase = 10;
        unitData.RegisterUnitType(War3ID("hfoo"), std::make_unique<CUnitType>(
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
    }
    f.sim.SetUnitData(&unitData);
    auto* caster = f.sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* foe = f.sim.CreateUnit(War3ID("hfoo"), 0, 100, 0, 0);
    TEST_ASSERT(caster && foe);
    f.sim.Update();
    const int hc = f.data->AllocHandle(JassHandleInfo::UNIT, caster->GetHandleId());
    TEST_ASSERT(!CallNative(f.vm, "IsUnitIllusion", {JassValue::Int(hc)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "IsUnitIllusion", {JassValue::Int(999999)}).boolVal);
    const size_t before = f.sim.GetUnits().size();
    ABActionSummonCopyOfTarget illusion(War3ID("AIil"), 60.0f);
    illusion.Run(f.sim, caster, foe, false, 0, 0);
    f.sim.Update();
    TEST_ASSERT(f.sim.GetUnits().size() == before + 1);
    CUnit* copy = f.sim.GetUnits().back();
    TEST_ASSERT(copy->IsIllusion());
    TEST_ASSERT(!caster->IsIllusion() && !foe->IsIllusion());
    const int hi = f.data->AllocHandle(JassHandleInfo::UNIT, copy->GetHandleId());
    TEST_ASSERT(CallNative(f.vm, "IsUnitIllusion", {JassValue::Int(hi)}).boolVal);
    return true;
}

bool TestEffectZRidesFlyHeight() {
    // Point effects report the stored height; attached effects ride the
    // target's fly height plus the stored offset.
    Fixture f;
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(
        9301, MakeSpellType(), 0, War3ID("hfoo"), 50, 60, 0));
    TEST_ASSERT(u);
    f.sim.Update();
    const int h = f.data->AllocHandle(JassHandleInfo::UNIT, 9301);
    const int fx = CallNative(f.vm, "AddSpecialEffectTarget",
        {JassValue::Str("Foo.mdx"), JassValue::Int(h), JassValue::Str("hand")}).intVal;
    TEST_ASSERT(fx != 0);
    TEST_ASSERT(CallNative(f.vm, "BlzGetSpecialEffectZ", {JassValue::Int(fx)}).realVal == 0.0f);
    u->SetFlyingHeight(200.0f);
    TEST_ASSERT(CallNative(f.vm, "BlzGetSpecialEffectZ", {JassValue::Int(fx)}).realVal == 200.0f);
    u->SetFlyingHeight(0.0f);
    TEST_ASSERT(CallNative(f.vm, "BlzGetSpecialEffectZ", {JassValue::Int(fx)}).realVal == 0.0f);
    TEST_ASSERT(CallNative(f.vm, "BlzGetSpecialEffectZ", {JassValue::Int(999999)}).realVal == 0.0f);
    return true;
}

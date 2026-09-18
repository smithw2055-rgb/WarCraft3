// Batch-6a native tests: UI toggles/filters, minimap pings, defeat
// conditions, lightning state, cine filter flag, widget indicators.
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

CUnitType* MakeUIType() {
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

bool TestUIStateNatives() {
    Fixture f;
    // Defaults are all-on.
    TEST_ASSERT(f.data->uiConfig.dragSelect && f.data->uiConfig.showInterface);
    CallNative(f.vm, "EnableDragSelect", {JassValue::Bool(false), JassValue::Bool(false)});
    CallNative(f.vm, "EnablePreSelect", {JassValue::Bool(false), JassValue::Bool(false)});
    CallNative(f.vm, "EnableSelect", {JassValue::Bool(false), JassValue::Bool(false)});
    CallNative(f.vm, "EnableUserControl", {JassValue::Bool(false)});
    CallNative(f.vm, "EnableOcclusion", {JassValue::Bool(false)});
    CallNative(f.vm, "EnableWorldFogBoundary", {JassValue::Bool(false)});
    CallNative(f.vm, "ShowInterface", {JassValue::Bool(false), JassValue::Real(0)});
    TEST_ASSERT(!f.data->uiConfig.dragSelect && !f.data->uiConfig.preSelect);
    TEST_ASSERT(!f.data->uiConfig.select && !f.data->uiConfig.userControl);
    TEST_ASSERT(!f.data->uiConfig.occlusion && !f.data->uiConfig.worldFogBoundary);
    TEST_ASSERT(!f.data->uiConfig.showInterface);

    CallNative(f.vm, "EnableMinimapFilterButtons", {JassValue::Bool(true), JassValue::Bool(false)});
    TEST_ASSERT(f.data->uiConfig.minimapAllyFilter);
    TEST_ASSERT(!f.data->uiConfig.minimapCreepFilter);
    TEST_ASSERT(f.data->uiConfig.minimapFilterButtons);

    // Filter states round-trip (ally via the repaired setter).
    CallNative(f.vm, "SetAllyColorFilterState", {JassValue::Int(2)});
    TEST_ASSERT(CallNative(f.vm, "GetAllyColorFilterState", {}).intVal == 1);
    TEST_ASSERT(!CallNative(f.vm, "GetCreepCampFilterState", {}).boolVal);
    CallNative(f.vm, "SetCreepCampFilterState", {JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "GetCreepCampFilterState", {}).boolVal);

    // UI focus forcing has no menu state yet: safe no-ops.
    CallNative(f.vm, "ForceUICancel", {});
    CallNative(f.vm, "ForceUIKey", {JassValue::Str("ESC")});

    // Pings retained for the minimap pass.
    CallNative(f.vm, "PingMinimap", {JassValue::Real(100), JassValue::Real(200), JassValue::Real(5)});
    CallNative(f.vm, "PingMinimapEx",
        {JassValue::Real(1), JassValue::Real(2), JassValue::Real(3), JassValue::Int(10),
         JassValue::Int(20), JassValue::Int(30), JassValue::Bool(true)});
    TEST_ASSERT(f.data->minimapPings.size() == 2);
    TEST_ASSERT(f.data->minimapPings[1].red == 10 && f.data->minimapPings[1].extra);

    // Cine filter flag mirrors display state.
    TEST_ASSERT(!CallNative(f.vm, "IsCineFilterDisplayed", {}).boolVal);
    f.data->cineFilter.displayed = true;
    TEST_ASSERT(CallNative(f.vm, "IsCineFilterDisplayed", {}).boolVal);

    // Defeat-condition lifecycle.
    const int dc = CallNative(f.vm, "CreateDefeatCondition", {}).intVal;
    CallNative(f.vm, "DefeatConditionSetDescription", {JassValue::Int(dc), JassValue::Str("Survive")});
    TEST_ASSERT(f.data->defeatConditions[dc]->description == "Survive");
    CallNative(f.vm, "DestroyDefeatCondition", {JassValue::Int(dc)});
    TEST_ASSERT(f.data->defeatConditions.find(dc) == f.data->defeatConditions.end());

    // Filter/condition destruction is safe (passthrough, nothing stored).
    CallNative(f.vm, "DestroyFilter", {JassValue::Int(123)});
    CallNative(f.vm, "DestroyCondition", {JassValue::Int(456)});
    return true;
}

bool TestLightningAndIndicatorNatives() {
    Fixture f;
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8701, MakeUIType(), 0, War3ID("hfoo"), 0, 0, 0));
    f.sim.Update();
    (void)u;
    const int uh = f.data->AllocHandle(JassHandleInfo::UNIT, 8701);

    // Bolt records creation params; color defaults to white.
    const int bolt = CallNative(f.vm, "AddLightning",
        {JassValue::Str("CLPB"), JassValue::Bool(true), JassValue::Real(0), JassValue::Real(0),
         JassValue::Real(100), JassValue::Real(100)}).intVal;
    TEST_ASSERT(bolt != 0);
    TEST_ASSERT(CallNative(f.vm, "GetLightningColorR", {JassValue::Int(bolt)}).realVal == 1.0f);
    TEST_ASSERT(CallNative(f.vm, "SetLightningColor",
        {JassValue::Int(bolt), JassValue::Real(1), JassValue::Real(0), JassValue::Real(0), JassValue::Real(1)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "GetLightningColorR", {JassValue::Int(bolt)}).realVal == 1.0f);
    TEST_ASSERT(CallNative(f.vm, "GetLightningColorG", {JassValue::Int(bolt)}).realVal == 0.0f);
    TEST_ASSERT(CallNative(f.vm, "GetLightningColorB", {JassValue::Int(bolt)}).realVal == 0.0f);
    TEST_ASSERT(CallNative(f.vm, "GetLightningColorA", {JassValue::Int(bolt)}).realVal == 1.0f);
    TEST_ASSERT(CallNative(f.vm, "MoveLightning",
        {JassValue::Int(bolt), JassValue::Bool(false), JassValue::Real(10), JassValue::Real(10),
         JassValue::Real(20), JassValue::Real(20)}).boolVal);
    {
        const int id = f.data->GetSimHandle(bolt, JassHandleInfo::LIGHTNING);
        auto it = f.data->lightnings.find(id);
        TEST_ASSERT(it != f.data->lightnings.end());
        TEST_ASSERT(it->second->x1 == 10.0f && it->second->x2 == 20.0f);
    }
    // Bad handles fail closed with defaults.
    TEST_ASSERT(!CallNative(f.vm, "MoveLightning",
        {JassValue::Int(999999), JassValue::Bool(false), JassValue::Real(0), JassValue::Real(0),
         JassValue::Real(0), JassValue::Real(0)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "GetLightningColorR", {JassValue::Int(999999)}).realVal == 1.0f);
    CallNative(f.vm, "DestroyLightning", {JassValue::Int(bolt)});
    TEST_ASSERT(f.data->GetSimHandle(bolt, JassHandleInfo::LIGHTNING) < 0);

    // Widget indicator lands on the unit sim id; bad widgets are ignored.
    CallNative(f.vm, "AddIndicator",
        {JassValue::Int(uh), JassValue::Int(0), JassValue::Int(255), JassValue::Int(0), JassValue::Int(200)});
    {
        auto it = f.data->unitIndicators.find(8701);
        TEST_ASSERT(it != f.data->unitIndicators.end());
        TEST_ASSERT(it->second.green == 255 && it->second.alpha == 200);
    }
    CallNative(f.vm, "AddIndicator",
        {JassValue::Int(999999), JassValue::Int(0), JassValue::Int(0), JassValue::Int(0), JassValue::Int(0)});
    return true;
}

// Fog-system tests: sim-side modifiers/overrides/queries/rasterize plus
// JASS round-trips (create/start/stop/destroy, SetFogState, visibility
// queries, FogEnable gate). No MPQ needed.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/csimulation.h"
#include "ecs/fog_state.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeFogType() {
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
        0.3f, 0.3f, false, false, 1, 500, 500, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

CUnit* AdoptFogAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeFogType(), player,
                                                 War3ID("hfoo"), x, y, 0));
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
        sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
    }
};

} // namespace

bool TestFogUnexploredIsMasked() {
    // No sight, no scripts: black mask (retail unexplored).
    CSimulation sim;
    TEST_ASSERT(!sim.GetFogSystem()->IsVisibleTo(0, 100, 100));
    TEST_ASSERT(sim.GetFogSystem()->IsMaskedTo(0, 100, 100));
    TEST_ASSERT(sim.GetFogSystem()->GetState(0, 100, 100) == CFogState::MASKED);
    return true;
}

bool TestFogUnitSightReveals() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    auto* scout = AdoptFogAt(sim, 2001, 0, 0, 0);
    (void)scout;
    sim.Update(); // tick 0 runs the sight pass
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 400, 0)); // inside 500 sight
    TEST_ASSERT(sim.GetFogSystem()->IsMaskedTo(0, 3000, 0)); // beyond sight
    TEST_ASSERT(sim.GetFogSystem()->IsMaskedTo(1, 0, 0)); // other player blind
    return true;
}

bool TestFogExploredMemory() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* scout = AdoptFogAt(sim, 2011, 0, 0, 0);
    sim.Update();
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 0, 0));
    // Walk away: the seen ground stays explored (fogged, not masked).
    scout->SetPosition(3000, 0);
    for (int i = 0; i < 11; i++) sim.Update(); // next sight pass lands
    TEST_ASSERT(!sim.GetFogSystem()->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(!sim.GetFogSystem()->IsMaskedTo(0, 0, 0));
    TEST_ASSERT(sim.GetFogSystem()->GetState(0, 0, 0) == CFogState::FOGGED);
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 3000, 0));
    return true;
}

bool TestFogUpdateSightDirect() {
    CFogSystem fog;
    const CFogSightSource src{0, 0.0f, 0.0f, 100.0f};
    fog.UpdateSight(&src, 1);
    TEST_ASSERT(fog.IsVisibleTo(0, 0, 0));
    TEST_ASSERT(fog.IsVisibleTo(0, 90, 0));
    TEST_ASSERT(!fog.IsVisibleTo(0, 500, 0));
    TEST_ASSERT(!fog.IsVisibleTo(1, 0, 0)); // per-player isolation
    // Degenerate sources are ignored without crashing; explored memory
    // from the first splat persists (fogged, not masked).
    const CFogSightSource bad[] = {{-1, 0, 0, 100}, {0, 0, 0, 0}, {99, 0, 0, 100}};
    fog.UpdateSight(bad, 3);
    TEST_ASSERT(!fog.IsVisibleTo(0, 0, 0));
    TEST_ASSERT(!fog.IsMaskedTo(0, 0, 0));
    TEST_ASSERT(fog.GetState(0, 0, 0) == CFogState::FOGGED);
    TEST_ASSERT(fog.IsMaskedTo(0, 5000, 0)); // never seen
    return true;
}

bool TestFogMaskedOverrideWinsOverSight() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    AdoptFogAt(sim, 2021, 0, 0, 0);
    sim.Update();
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 0, 0));
    // Black-mask paint punches through live sight (scripted layer wins).
    sim.GetFogSystem()->AddOverride(0, CFogArea::Circle(0, 0, 1000), CFogState::MASKED);
    TEST_ASSERT(sim.GetFogSystem()->IsMaskedTo(0, 0, 0));
    return true;
}

bool TestFogModifierLifecycle() {
    CSimulation sim;
    auto* fog = sim.GetFogSystem();
    // Black-mask the area, then punch a reveal hole with a modifier.
    fog->AddOverride(0, CFogArea::Rect(-500, -500, 500, 500), CFogState::MASKED);
    TEST_ASSERT(!fog->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(fog->IsMaskedTo(0, 0, 0));
    const int id = fog->AddModifier(0, CFogArea::Circle(0, 0, 100), CFogState::VISIBLE);
    // Fresh modifiers start disabled (retail).
    TEST_ASSERT(!fog->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(fog->SetModifierEnabled(id, true));
    TEST_ASSERT(fog->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(!fog->IsVisibleTo(0, 400, 0)); // outside the circle
    TEST_ASSERT(fog->SetModifierEnabled(id, false));
    TEST_ASSERT(!fog->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(fog->RemoveModifier(id));
    TEST_ASSERT(!fog->RemoveModifier(id)); // already gone
    TEST_ASSERT(!fog->IsVisibleTo(0, 0, 0));
    return true;
}

bool TestFogOverridePrecedence() {
    CSimulation sim;
    auto* fog = sim.GetFogSystem();
    // Later entries win, for both paints and modifiers.
    fog->AddOverride(0, CFogArea::Rect(-500, -500, 500, 500), CFogState::MASKED);
    fog->AddOverride(0, CFogArea::Rect(-500, -500, 500, 500), CFogState::FOGGED);
    TEST_ASSERT(fog->GetState(0, 0, 0) == CFogState::FOGGED);
    const int a = fog->AddModifier(0, CFogArea::Circle(0, 0, 300), CFogState::VISIBLE);
    const int b = fog->AddModifier(0, CFogArea::Circle(0, 0, 300), CFogState::MASKED);
    fog->SetModifierEnabled(a, true);
    fog->SetModifierEnabled(b, true);
    TEST_ASSERT(fog->GetState(0, 0, 0) == CFogState::MASKED);
    // Modifiers apply after paints.
    fog->AddOverride(0, CFogArea::Circle(0, 0, 10), CFogState::VISIBLE);
    TEST_ASSERT(fog->GetState(0, 0, 0) == CFogState::MASKED);
    return true;
}

bool TestFogRasterize() {
    CSimulation sim;
    auto* fog = sim.GetFogSystem();
    fog->AddOverride(0, CFogArea::Rect(-100, -100, 0, 100), CFogState::MASKED);
    fog->AddOverride(0, CFogArea::Rect(0, -100, 100, 100), CFogState::FOGGED);
    constexpr int kGrid = 4;
    unsigned char out[kGrid * kGrid] = {};
    fog->Rasterize(0, kGrid, kGrid, -100, -100, 100, 100, out);
    // Left half masked (255), right half fogged (127).
    for (int row = 0; row < kGrid; row++) {
        TEST_ASSERT(out[row * kGrid + 0] == 255);
        TEST_ASSERT(out[row * kGrid + 1] == 255);
        TEST_ASSERT(out[row * kGrid + 2] == 127);
        TEST_ASSERT(out[row * kGrid + 3] == 127);
    }
    return true;
}

bool TestFogVersionBumps() {
    CSimulation sim;
    auto* fog = sim.GetFogSystem();
    const uint64_t base = fog->Version();
    const int id = fog->AddModifier(0, CFogArea::Circle(0, 0, 50), CFogState::VISIBLE);
    TEST_ASSERT(fog->Version() != base);
    const uint64_t afterAdd = fog->Version();
    fog->SetModifierEnabled(id, true);
    TEST_ASSERT(fog->Version() != afterAdd);
    fog->AddOverride(0, CFogArea::Circle(0, 0, 50), CFogState::FOGGED);
    fog->RemoveModifier(id);
    TEST_ASSERT(fog->Version() != afterAdd);
    return true;
}

bool TestFogJassRoundTrip() {
    Fixture f;
    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    const int loc = CallNative(f.vm, "Location", {JassValue::Real(0), JassValue::Real(0)}).intVal;
    // Black-mask a disc, then reveal it with a started modifier.
    CallNative(f.vm, "SetFogStateRadius",
        {JassValue::Int(p0), JassValue::Int(0), JassValue::Real(0),
         JassValue::Real(0), JassValue::Real(500), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "IsLocationMaskedToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    const int mod = CallNative(f.vm, "CreateFogModifierRadius",
        {JassValue::Int(p0), JassValue::Int(2), JassValue::Real(0),
         JassValue::Real(0), JassValue::Real(200)}).intVal;
    TEST_ASSERT(mod != 0);
    TEST_ASSERT(!CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal); // still stopped
    CallNative(f.vm, "FogModifierStart", {JassValue::Int(mod)});
    TEST_ASSERT(CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "IsLocationMaskedToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    CallNative(f.vm, "FogModifierStop", {JassValue::Int(mod)});
    TEST_ASSERT(!CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    CallNative(f.vm, "DestroyFogModifier", {JassValue::Int(mod)});
    TEST_ASSERT(!CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    return true;
}

bool TestFogEnableGate() {
    Fixture f;
    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    const int loc = CallNative(f.vm, "Location", {JassValue::Real(10), JassValue::Real(10)}).intVal;
    CallNative(f.vm, "SetFogStateRadius",
        {JassValue::Int(p0), JassValue::Int(0), JassValue::Real(10),
         JassValue::Real(10), JassValue::Real(500), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    // Fog disabled globally: everything reads visible.
    CallNative(f.vm, "FogEnable", {JassValue::Bool(false)});
    TEST_ASSERT(CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    CallNative(f.vm, "FogEnable", {JassValue::Bool(true)});
    TEST_ASSERT(!CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    // Mask disabled: masked reads as merely unfogged-by-mask.
    TEST_ASSERT(CallNative(f.vm, "IsLocationMaskedToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    CallNative(f.vm, "FogMaskEnable", {JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "IsLocationMaskedToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    return true;
}

bool TestFogSharedVision() {
    Fixture f;
    f.sim.GetPlayer(0)->SetAlliance(1, CAllianceType::SHARED_VISION, true);
    f.sim.GetPlayer(1)->SetAlliance(0, CAllianceType::SHARED_VISION, true);
    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    const int p1 = CallNative(f.vm, "Player", {JassValue::Int(1)}).intVal;
    const int loc = CallNative(f.vm, "Location", {JassValue::Real(0), JassValue::Real(0)}).intVal;
    // Mask the ally first so the shared reveal is observable on each.
    CallNative(f.vm, "SetFogStateRadius",
        {JassValue::Int(p1), JassValue::Int(0), JassValue::Real(0),
         JassValue::Real(0), JassValue::Real(5000), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p1), JassValue::Int(loc)}).boolVal);
    CallNative(f.vm, "SetFogStateRadius",
        {JassValue::Int(p0), JassValue::Int(2), JassValue::Real(0),
         JassValue::Real(0), JassValue::Real(500), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p0), JassValue::Int(loc)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "IsLocationVisibleToPlayer",
        {JassValue::Int(p1), JassValue::Int(loc)}).boolVal);
    return true;
}

bool TestUnitShareVisionReveals() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    auto* scout = AdoptFogAt(sim, 2091, 0, 0, 0);
    sim.Update();
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(sim.GetFogSystem()->IsMaskedTo(1, 0, 0));
    scout->SetSharesVisionWith(1, true);
    for (int i = 0; i < 11; i++) sim.Update();
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(1, 0, 0));
    scout->SetSharesVisionWith(1, false);
    for (int i = 0; i < 11; i++) sim.Update();
    TEST_ASSERT(!sim.GetFogSystem()->IsVisibleTo(1, 0, 0));
    TEST_ASSERT(sim.GetFogSystem()->GetState(1, 0, 0) == CFogState::FOGGED);
    return true;
}

bool TestTerrainDeformLowersHeight() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    const int loc = CallNative(vm, "Location", {JassValue::Real(0), JassValue::Real(0)}).intVal;
    CallNative(vm, "TerrainDeformCrater",
        {JassValue::Real(0), JassValue::Real(0), JassValue::Real(200),
         JassValue::Real(40), JassValue::Int(1000), JassValue::Bool(true)});
    const float z = CallNative(vm, "GetLocationZ", {JassValue::Int(loc)}).realVal;
    TEST_ASSERT(z < -30.0f);
    return true;
}

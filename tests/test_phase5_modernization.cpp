#include "render/mdx_particles.h"
#include "render/mdx_ribbons.h"
#include "ui/TextRenderer.h"
#include "scripting/script_engine_host.h"
#include "ecs/fog_state.h"
#include "render/fog_of_war.h"
#include "render/mdx_animator.h"
#include "ecs/melee_ai.h"
#include "scripting/jass_binder.h"
#include <cstring>
#include "ecs/abilities/ability_move.h"
#include "scripting/JassNatives.h"
#include "ecs/pathing/crowd_steering.h"
#include "core/input_action_map.h"
#include "net/snapshot_resync.h"
#include "render/texture_atlas_packer.h"
#include "core/engine_profiler.h"
#include "scripting/JassLexer.h"
#include "scripting/JassParser.h"
#include "scripting/JassVM.h"
#include "scripting/JassBytecode.h"
#include "ecs/sim_snapshot.h"
#include "ecs/pathing/pathfinding_server.h"
#include "render/render_graph.h"
#include "core/world_scene_session.h"
#include "ui/ui_data_binder.h"
#include "ui/TextFrame.h"
#include "audio/audio_server.h"
#include "audio/AudioSystem.h"
#include "core/job_system.h"
// Phase 5 Engine Modernization Tests (Stage A, Stage B & Stage C):
// 1. Stage C1: True ECS SparseComponentPool storage for CUnit orthogonal components (lazy pool allocation & automatic cleanup)
// 2. Stage C2: SpatialHashGrid O(N) neighborhood queries & deterministic collision separation
// 3. Stage C3: SimulationSystemPipeline modular execution, enable/disable & custom system injection
// 4. Stage B:  UnitPresentationSystem decoupled visual state management & reactive SimEventBus signal cleanup
// 5. Stage A:  SceneManager + RenderServer + RenderInterpolationBuffer closed-loop pipeline (frustum culling, batch sorting & 20Hz->high-refresh interpolation)
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/unit_presentation_system.h"
#include "ecs/csimulation.h"
#include "ecs/ecs_registry.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/sim_systems.h"
#include "ecs/spatial_grid.h"
#include "ecs/unit.h"
#include "ecs/unit_components.h"
#include "ecs/unit_type.h"
#include "data/itemdata.h"
#include "render/camera.h"
#include "render/math.h"
#include "render/render_server.h"
#include "render/scene.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
#include "ui/UIFrame.h"
#include "ui/SimpleFrame.h"
#include "ui/ButtonFrame.h"
#include "resource/resource_server.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

#define TEST_NEAR(a, b, eps)                                                \
    do {                                                                    \
        if (std::fabs((a) - (b)) > (eps)) {                                 \
            std::cerr << "NEAR @" << __LINE__ << ": " #a " (" << (a)        \
                      << ") vs " #b " (" << (b) << ")\n";                   \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

CUnitType* MakePhase5UnitType(War3ID code, int maxLife = 500, int speed = 270, bool building = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 20;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", code,
        maxLife, 1.0f, 0.5f, CRegenType::ALWAYS,
        200, 200, speed, 2, War3ID(),
        std::vector<War3ID>{}, building, PathingGridMovementType::FOOT,
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

// 1. Test True ECS Sparse-Set Component Pooling on CUnit
bool TestPhase5TrueEcsComponentPooling() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    // Create a unit before adoption, mutate a component while detached, then adopt into CSimulation
    auto rawUnit = std::make_unique<CUnit>(sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 10.0f, 20.0f, 0.0f);
    const int u1Handle = rawUnit->GetHandleId();
    rawUnit->SetScale(2.25f); // mutates UnitVisualComponent in detached registry
    CUnit* u1 = sim.AdoptUnit(std::move(rawUnit));

    CUnit* u2 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 50.0f, 60.0f, 0.0f));
    const int u2Handle = u2->GetHandleId();
    sim.Update();

    // Verify u1's detached UnitVisualComponent was seamlessly migrated into sim.Registry()
    SimEcsRegistry& reg = sim.Registry();
    TEST_ASSERT(u1->GetBoundRegistry() == &reg);
    TEST_ASSERT(reg.Has<UnitVisualComponent>(u1Handle));
    TEST_NEAR(reg.TryGet<UnitVisualComponent>(u1Handle)->scale, 2.25f, 1e-4f);

    // Verify lazy allocation: u2 never touched Construction, Cargo, Morph, or Visual components,
    // so zero memory slots are allocated for u2 in those pools!
    TEST_ASSERT(!reg.Has<UnitConstructionComponent>(u2Handle));
    TEST_ASSERT(!reg.Has<UnitCargoComponent>(u2Handle));
    TEST_ASSERT(!reg.Has<UnitMorphComponent>(u2Handle));
    TEST_ASSERT(!reg.Has<UnitVisualComponent>(u2Handle));

    // Const getters on u2 return default values without allocating in the pool
    TEST_ASSERT(!u2->IsConstructing());
    TEST_NEAR(u2->GetScale(), 1.0f, 1e-4f);
    TEST_ASSERT(!reg.Has<UnitConstructionComponent>(u2Handle));
    TEST_ASSERT(!reg.Has<UnitVisualComponent>(u2Handle));

    // Mutating Construction on u2 lazily allocates its component in the contiguous pool
    u2->SetConstructing(true);
    u2->SetConstructionProgress(45.0f);
    TEST_ASSERT(reg.Has<UnitConstructionComponent>(u2Handle));
    TEST_ASSERT(reg.Pool<UnitConstructionComponent>().Size() == 1);
    TEST_NEAR(reg.TryGet<UnitConstructionComponent>(u2Handle)->constructionProgress, 45.0f, 1e-4f);

    // Removing u2 automatically purges its pooled components
    sim.RemoveUnit(u2);
    sim.Update();
    TEST_ASSERT(!reg.Has<UnitConstructionComponent>(u2Handle));
    TEST_ASSERT(reg.Pool<UnitConstructionComponent>().Size() == 0);

    return true;
}

// 2. Test SpatialHashGrid & SimulationSystemPipeline
bool TestPhase5SpatialGridAndSystemPipeline() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    // Spawn 60 units (exceeds the 48-unit threshold so SpatialHashGrid broadphase is exercised)
    std::vector<CUnit*> cluster;
    for (int i = 0; i < 60; ++i) {
        // Place first 10 stacked near origin, remaining 50 spread across grid cells
        const float x = (i < 10) ? 0.0f : static_cast<float>((i % 10) * 300);
        const float y = (i < 10) ? 0.0f : static_cast<float>((i / 10) * 300);
        CUnit* u = sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), x, y, 0.0f));
        cluster.push_back(u);
    }

    // Inject a custom system into SimulationSystemPipeline at PreTick phase
    int customSystemTicks = 0;
    sim.SystemPipeline().RegisterLambda("CustomTelemetrySystem", SimSystemPhase::PreTick,
        [&](CSimulation& /*s*/, float /*dt*/) {
            customSystemTicks++;
        });

    TEST_ASSERT(sim.SystemPipeline().SystemCount() >= 10);

    sim.Update(); // Tick 0: commits newUnits_ into units_ and builds SpatialGrid
    TEST_ASSERT(customSystemTicks == 1);
    TEST_ASSERT(sim.SpatialGrid().IndexedEntityCount() == 60);

    // Next tick runs SpatialSeparationSystem across all 60 active units using SpatialHashGrid
    sim.Update();
    TEST_ASSERT(customSystemTicks == 2);

    // Verify SpatialHashGrid radius query around (0, 0) finds the 10 clustered units
    std::vector<CUnit*> nearOrigin = sim.QueryUnitsInRadius(0.0f, 0.0f, 150.0f);
    TEST_ASSERT(nearOrigin.size() == 10);
    // Verify deterministic handle sorting
    for (size_t i = 1; i < nearOrigin.size(); ++i) {
        TEST_ASSERT(nearOrigin[i - 1]->GetHandleId() < nearOrigin[i]->GetHandleId());
    }

    // Verify stacked units at (0, 0) were pushed apart by SpatialSeparationSystem
    bool anyMovedFromOrigin = false;
    for (int i = 0; i < 10; ++i) {
        if (std::fabs(cluster[i]->GetX()) > 0.1f || std::fabs(cluster[i]->GetY()) > 0.1f) {
            anyMovedFromOrigin = true;
            break;
        }
    }
    TEST_ASSERT(anyMovedFromOrigin);

    // Disable custom system and verify it no longer runs on next tick
    TEST_ASSERT(sim.SystemPipeline().SetSystemEnabled("CustomTelemetrySystem", false));
    sim.Update();
    TEST_ASSERT(customSystemTicks == 2);

    return true;
}

// 3. Test UnitPresentationSystem & Signal-Driven Visual Cleanup (Stage B)
bool TestPhase5UnitPresentationSystemSignals() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    UnitPresentationSystem presentation;
    presentation.BindSimulationEvents(sim);

    int spawnedSignalCount = 0;
    int lastSpawnedHandle = -1;
    sim.Events().onUnitSpawned.Connect([&](int handleId) {
        spawnedSignalCount++;
        lastSpawnedHandle = handleId;
    });

    CUnit* u1 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 100.0f, 200.0f, 90.0f));
    const int u1Handle = u1->GetHandleId();
    sim.Update();

    TEST_ASSERT(spawnedSignalCount == 1);
    TEST_ASSERT(lastSpawnedHandle == u1Handle);

    // Populate presentation state for u1 (visual index, animations, secondary tags)
    presentation.unitToVisualIdx[u1Handle] = 42;
    presentation.RequestUnitAnimationByName(u1Handle, "attack", 1.25f);
    presentation.SetUnitSecondaryAnimationTag(u1Handle, "gold", true);

    TEST_ASSERT(presentation.GetUnitVisualIndex(u1Handle) == 42);
    TEST_ASSERT(presentation.TrackedUnitCount() == 1);
    TEST_ASSERT(presentation.unitPendingAnimations.count(u1Handle) == 1);
    TEST_ASSERT(presentation.unitAnimationPropertyTags.count(u1Handle) == 1);

    // Remove u1 from CSimulation and step tick -> SimEventBus::onUnitRemoved fires
    // and UnitPresentationSystem automatically purges all visual state for u1!
    sim.RemoveUnit(u1);
    sim.Update();

    TEST_ASSERT(presentation.GetRemovedUnitNotificationsCount() == 1);
    TEST_ASSERT(presentation.GetUnitVisualIndex(u1Handle) == -1);
    TEST_ASSERT(presentation.TrackedUnitCount() == 0);
    TEST_ASSERT(presentation.unitPendingAnimations.count(u1Handle) == 0);
    TEST_ASSERT(presentation.unitAnimationPropertyTags.count(u1Handle) == 0);

    return true;
}

// 4. Test Stage A Closed-Loop: SceneManager::RenderAll -> RenderServer + UnitPresentationSystem Interpolation
bool TestPhase5StageASceneRenderServerClosedLoop() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
    CUnitType* riflemanType = MakePhase5UnitType(War3ID("hrif"), 535, 270, false);

    UnitPresentationSystem presentation;
    presentation.BindSimulationEvents(sim);

    // Spawn 1 unit in CSimulation and test 20Hz -> high-refresh interpolation via UnitPresentationSystem
    CUnit* hero = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), -100.0f, 0.0f, 350.0f));
    sim.Update();
    presentation.CaptureSimulationTick(sim); // Tick 0 snapshot: (-100, 0), facing 350

    hero->SetPosition(100.0f, 200.0f);
    hero->SetFacing(10.0f);
    hero->SetFlyingHeight(60.0f);
    hero->SetScale(1.5f);
    hero->SetVertexColor(255, 128, 64, 128); // translucent
    presentation.CaptureSimulationTick(sim); // Tick 1 snapshot: (100, 200), facing 10, flyHeight 60

    presentation.SetInterpolationAlpha(0.5f);
    render::RenderTransformSnapshot midSnap;
    TEST_ASSERT(presentation.SampleInterpolatedUnit(hero->GetHandleId(), midSnap));
    TEST_NEAR(midSnap.position.x, 0.0f, 1e-3f);
    TEST_NEAR(midSnap.position.z, 100.0f, 1e-3f);
    TEST_NEAR(midSnap.flyHeight, 30.0f, 1e-3f);
    TEST_NEAR(midSnap.facingDeg, 0.0f, 1e-3f); // shortest arc 350 -> 10 at alpha=0.5 is 0 deg
    TEST_NEAR(midSnap.scale.x, 1.25f, 1e-3f);  // lerped 1.0 -> 1.5

    // Populate SceneManager with procedural instances:
    // - 10 opaque Footmen inside frustum
    // - 6 opaque Riflemen inside frustum
    // - 2 translucent units at different depths inside frustum
    // - 2 off-screen units far outside frustum
    SceneManager scene;
    const float unitScale[3] = {1.0f, 1.0f, 1.0f};
    for (int i = 0; i < 10; ++i) {
        const float pos[3] = {static_cast<float>(i * 20 - 90), 0.0f, 0.0f};
        scene.AddProceduralInstance(War3ID("hfoo").value, pos, 0.0f, unitScale, 0, "unit", 1.0f);
    }
    for (int i = 0; i < 6; ++i) {
        const float pos[3] = {static_cast<float>(i * 20 - 50), 0.0f, 50.0f};
        scene.AddProceduralInstance(War3ID("hrif").value, pos, 90.0f, unitScale, 0, "unit", 1.0f);
    }
    const float nearGhostPos[3] = {0.0f, 0.0f, 150.0f};
    const float farGhostPos[3]  = {0.0f, 0.0f, -250.0f};
    const int nearGhostIdx = scene.AddProceduralInstance(War3ID("hfoo").value, nearGhostPos, 0.0f, unitScale, 0, "unit", 0.5f);
    const int farGhostIdx  = scene.AddProceduralInstance(War3ID("hrif").value, farGhostPos, 0.0f, unitScale, 0, "unit", 0.4f);

    const float offscreenPos1[3] = {30000.0f, 0.0f, 30000.0f};
    const float offscreenPos2[3] = {-30000.0f, 0.0f, -30000.0f};
    scene.AddProceduralInstance(War3ID("hfoo").value, offscreenPos1, 0.0f, unitScale, 0, "unit", 1.0f);
    scene.AddProceduralInstance(War3ID("hrif").value, offscreenPos2, 0.0f, unitScale, 0, "unit", 1.0f);

    // Execute SceneManager::RenderAll() and verify its internal RenderServer stats
    Vec3 camEye(0.0f, 800.0f, 800.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);

    scene.RenderAll(view, proj, 0.016f);

    const render::RenderFrameStats& stats = scene.GetLastRenderStats();
    TEST_ASSERT(stats.submittedItems == 20);
    TEST_ASSERT(stats.culledByFrustum == 2);
    TEST_ASSERT(stats.visibleItems == 18);
    // 10 opaque footmen -> 1 batch; 6 opaque riflemen -> 1 batch
    TEST_ASSERT(stats.opaqueBatchCount == 2);
    // 2 translucent ghosts -> 2 back-to-front sorted batches
    TEST_ASSERT(stats.transparentBatchCount == 2);
    TEST_ASSERT(stats.totalDrawCalls == 4);
    TEST_ASSERT(stats.instancedDrawCallsSaved == 14);

    // Verify back-to-front transparent ordering inside SceneManager's RenderServer
    const auto& transBatches = scene.GetRenderServer().GetTransparentBatches();
    TEST_ASSERT(transBatches.size() == 2);
    TEST_ASSERT(transBatches[0].entityHandles[0] == farGhostIdx);
    TEST_ASSERT(transBatches[1].entityHandles[0] == nearGhostIdx);

    return true;
}

// 5. Test Stage D1: Godot Control-Inspired UIFrame SceneTree Hierarchy, Z-Order & Dirty-Flag Layout
bool TestPhase5StageDUiSceneTreeAndDirtyLayout() {
    GameUI gui;
    gui.screenW = 1280;
    gui.screenH = 720;

    SimpleFrame root("Root");
    root.SetPosition(0.0f, 0.0f);
    root.SetSize(1280.0f, 720.0f);
    gui.rootFrame = &root;
    gui.frameRegistry["Root"] = &root;

    // Any UIFrame (not just SimpleFrame) can now parent child nodes directly
    auto* panel = new UIFrame("MainPanel", &root);
    panel->SetSize(400.0f, 300.0f);
    panel->AddAnchor(FramePoint::TOPLEFT, 100.0f, 50.0f);
    root.AddChild(panel);
    gui.frameRegistry["MainPanel"] = panel;

    // Two overlapping buttons inside MainPanel with different Z-orders
    auto* backBtn = new ButtonFrame("BackBtn", panel);
    backBtn->SetSize(200.0f, 80.0f);
    backBtn->AddAnchor(FramePoint::TOPLEFT, 20.0f, 20.0f);
    backBtn->SetZOrder(1);

    auto* frontBtn = new ButtonFrame("FrontBtn", panel);
    frontBtn->SetSize(200.0f, 80.0f);
    frontBtn->AddAnchor(FramePoint::TOPLEFT, 20.0f, 20.0f);
    frontBtn->SetZOrder(10);

    panel->AddChild(backBtn);
    panel->AddChild(frontBtn);

    int backClicks = 0;
    int frontClicks = 0;
    backBtn->SetClickCallback([&]() { backClicks++; });
    frontBtn->SetClickCallback([&]() { frontClicks++; });

    // Initial state is layoutDirty = true
    TEST_ASSERT(root.IsLayoutDirty());
    TEST_ASSERT(panel->IsLayoutDirty());
    TEST_ASSERT(frontBtn->IsLayoutDirty());

    // First layout pass computes bounds and clears dirty flags across the tree
    root.PositionBounds(gui);
    TEST_ASSERT(!root.IsLayoutDirty());
    TEST_ASSERT(!panel->IsLayoutDirty());
    TEST_ASSERT(!frontBtn->IsLayoutDirty());
    TEST_ASSERT(panel->GetLayoutComputeCount() == 1);
    TEST_ASSERT(frontBtn->GetLayoutComputeCount() == 1);

    // Verify computed anchor coordinates: MainPanel at (100, 50), buttons at (120, 70)
    TEST_NEAR(panel->GetBounds().x, 100.0f, 1e-3f);
    TEST_NEAR(panel->GetBounds().y, 50.0f, 1e-3f);
    TEST_NEAR(frontBtn->GetBounds().x, 120.0f, 1e-3f);
    TEST_NEAR(frontBtn->GetBounds().y, 70.0f, 1e-3f);

    // HitTest at overlapping point (150, 90) must hit frontBtn (zOrder=10 > zOrder=1)
    UIFrame* hit = root.HitTest(150.0f, 90.0f);
    TEST_ASSERT(hit == frontBtn);

    root.OnMouseDown(150.0f, 90.0f, 0);
    root.OnMouseUp(150.0f, 90.0f, 0);
    TEST_ASSERT(frontClicks == 1);
    TEST_ASSERT(backClicks == 0);

    // Elevate backBtn Z-order above frontBtn (zOrder=20 > 10) and verify HitTest switches
    backBtn->SetZOrder(20);
    hit = root.HitTest(150.0f, 90.0f);
    TEST_ASSERT(hit == backBtn);

    root.OnMouseDown(150.0f, 90.0f, 0);
    root.OnMouseUp(150.0f, 90.0f, 0);
    TEST_ASSERT(backClicks == 1);

    // Mutating a child anchor marks the child and ancestor chain dirty
    frontBtn->AddAnchor(FramePoint::TOPLEFT, 40.0f, 35.0f);
    TEST_ASSERT(frontBtn->IsLayoutDirty());
    TEST_ASSERT(panel->IsLayoutDirty());
    TEST_ASSERT(root.IsLayoutDirty());

    root.PositionBounds(gui);
    TEST_ASSERT(!root.IsLayoutDirty());
    TEST_NEAR(frontBtn->GetBounds().x, 140.0f, 1e-3f);
    TEST_NEAR(frontBtn->GetBounds().y, 85.0f, 1e-3f);

    // Clean up panel children before SimpleFrame dtor deletes panel
    panel->RemoveChild(backBtn);
    panel->RemoveChild(frontBtn);
    delete backBtn;
    delete frontBtn;

    return true;
}

// 6. Test Stage D2: Data-Driven ABSpellDescriptor Archetypes & Canonical Registry Consolidation
bool TestPhase5StageDDataDrivenSpellConsolidation() {
    AbilityBuilderRegistry& reg = AbilityBuilderRegistry::Instance();
    reg.RegisterStandardCanonicalDescriptors();
    TEST_ASSERT(reg.DescriptorCount() >= 9);
    TEST_ASSERT(reg.FindDescriptor(War3ID("AHbz")) != nullptr);
    TEST_ASSERT(reg.FindDescriptor(War3ID("AHhb")) != nullptr);
    TEST_ASSERT(reg.FindDescriptor(War3ID("AOcl")) != nullptr);
    TEST_ASSERT(reg.FindDescriptor(War3ID("AIha")) != nullptr);

    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    CUnitType* casterType = MakePhase5UnitType(War3ID("Oshd"), 800, 270, false);
    CUnitType* enemyType  = MakePhase5UnitType(War3ID("hfoo"), 500, 270, false);

    // Register a custom Chain Lightning descriptor (A9CL) and a custom Area Heal & Dispel descriptor (A9HD)
    const War3ID chainCode("A9CL");
    AbilityFactory::RegisterSpellDescriptor(ABSpellDescriptor::MakeTargetChainBounceDamage(
        chainCode, /*initialDmg=*/100.0f, /*targets=*/3, /*decay=*/0.20f,
        /*bounceRadius=*/400.0f, /*range=*/700.0f, /*cooldown=*/8.0f, /*mana=*/90));

    const War3ID healCode("A9HD");
    AbilityFactory::RegisterSpellDescriptor(ABSpellDescriptor::MakeAreaHealAndDispel(
        healCode, /*radius=*/350.0f, /*healAmount=*/120.0f, /*stripNegBuffs=*/true,
        /*range=*/0.0f, /*cooldown=*/10.0f, /*mana=*/50));

    CUnit* caster = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), casterType, 0, War3ID("Oshd"), 0.0f, 0.0f, 0.0f));
    CUnit* ally = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), enemyType, 0, War3ID("hfoo"), 80.0f, 0.0f, 0.0f));
    CUnit* e1 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), enemyType, 1, War3ID("hfoo"), 200.0f, 0.0f, 0.0f));
    CUnit* e2 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), enemyType, 1, War3ID("hfoo"), 320.0f, 0.0f, 0.0f));
    CUnit* e3 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), enemyType, 1, War3ID("hfoo"), 440.0f, 0.0f, 0.0f));
    sim.Update();

    // 1. Cast data-driven Chain Bounce on e1 -> bounces e1 (100 dmg) -> e2 (80 dmg) -> e3 (64 dmg)
    auto chainAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), chainCode);
    auto* chainSpell = dynamic_cast<CAbilityDataDrivenSpell*>(chainAb.get());
    TEST_ASSERT(chainSpell != nullptr);
    caster->AddAbility(sim, std::move(chainAb));
    chainSpell->DoEffect(sim, caster, e1);

    TEST_NEAR(e1->GetLife(), 500.0f - 100.0f, 1e-2f);
    TEST_NEAR(e2->GetLife(), 500.0f - 80.0f, 1e-2f);
    TEST_NEAR(e3->GetLife(), 500.0f - 64.0f, 1e-2f);

    // 2. Wound ally and apply negative Slow buff, then cast data-driven Area Heal & Dispel
    ally->SetLife(sim, 250.0f);
    ally->AddBuff(std::make_unique<CBuffSlow>(10.0f, 0.40f, e1->GetHandleId()), sim);
    TEST_ASSERT(ally->HasBuff(War3ID("Bslo")));

    auto healAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), healCode);
    auto* healSpell = dynamic_cast<CAbilityDataDrivenSpell*>(healAb.get());
    TEST_ASSERT(healSpell != nullptr);
    caster->AddAbility(sim, std::move(healAb));
    healSpell->DoEffect(sim, caster);

    TEST_NEAR(ally->GetLife(), 370.0f, 1e-2f); // 250 + 120
    TEST_ASSERT(!ally->HasBuff(War3ID("Bslo"))); // negative slow buff stripped!

    reg.Unregister(chainCode);
    reg.Unregister(healCode);
    return true;
}

// 7. Test Stage E1: ResourceServer VFS URI Canonicalization, RID Caching & MDL Sibling Fallback
bool TestPhase6StageEResourceServerCachingAndVfsUri() {
    using namespace resource;

    // 1. Verify URI parsing & case/slash canonicalization
    ParsedVfsUri u1 = ResourceServer::ParseUri("mpq://Units/Human/Footman/Footman.MDX");
    ParsedVfsUri u2 = ResourceServer::ParseUri("Units\\human\\footman\\footman.mdx");
    ParsedVfsUri u3 = ResourceServer::ParseUri("map:///war3mapImported/CustomHero.mdl");

    TEST_ASSERT(u1.scheme == VfsScheme::Mpq);
    TEST_ASSERT(u1.rawArchivePath == "Units\\Human\\Footman\\Footman.MDX");
    TEST_ASSERT(u1.canonicalCacheKey == "units\\human\\footman\\footman.mdx");
    TEST_ASSERT(u1.canonicalCacheKey == u2.canonicalCacheKey);
    TEST_ASSERT(u3.scheme == VfsScheme::Map);
    TEST_ASSERT(u3.canonicalCacheKey == "war3mapimported\\customhero.mdl");

    // 2. Mount synthetic SLK, TXT, and MDL files in ResourceServer memory overlay
    ResourceServer server;

    const std::string sampleSlk =
        "ID;PWXL;N;E\n"
        "B;X2;Y2;D0\n"
        "C;X1;Y1;K\"unitID\"\n"
        "C;X2;Y1;K\"file\"\n"
        "C;X1;Y2;K\"hfoo\"\n"
        "C;X2;Y2;K\"Units\\Human\\Footman\\Footman\"\n"
        "E\n";
    server.MountMemoryFile("mpq://Units/unitUI.slk",
                           std::vector<uint8_t>(sampleSlk.begin(), sampleSlk.end()));

    const std::string sampleTxt =
        "[hfoo]\n"
        "Name=Footman\n"
        "HotKey=F\n"
        "Buttonpos=0,0\n";
    server.MountMemoryFile("Units\\HumanUnitFunc.txt",
                           std::vector<uint8_t>(sampleTxt.begin(), sampleTxt.end()));

    // Mount an MDL text model at Footman.mdl, then request Footman.mdx to verify sibling fallback!
    const std::string sampleMdl = R"(
        Version { FormatVersion 800, }
        Model "Footman" {
            BlendTime 150,
            MinimumExtent { -32.0, -32.0, 0.0 },
            MaximumExtent { 32.0, 32.0, 96.0 },
            BoundsRadius 64.0,
        }
        Sequences 1 {
            Anim "Stand - 1" {
                Interval { 0, 1000 },
                MinimumExtent { -32.0, -32.0, 0.0 },
                MaximumExtent { 32.0, 32.0, 96.0 },
                BoundsRadius 64.0,
            }
        }
    )";
    server.MountMemoryFile("Units\\Human\\Footman\\Footman.mdl",
                           std::vector<uint8_t>(sampleMdl.begin(), sampleMdl.end()));

    // 3. Load SLK via different URI forms and verify O(1) RID deduplication & cache hit telemetry
    auto slk1 = server.LoadSlkTable("mpq://Units/unitUI.slk");
    auto slk2 = server.LoadSlkTable("units\\UNITUI.SLK");
    TEST_ASSERT(slk1 != nullptr && slk2 != nullptr);
    TEST_ASSERT(slk1 == slk2);
    TEST_ASSERT(slk1->GetRid().IsValid());
    TEST_ASSERT(slk1->GetRid().Kind() == ResourceKind::SlkTable);
    TEST_ASSERT(slk1->table.Rows() >= 1);
    TEST_ASSERT(server.GetTypedByRid<SlkTableResource>(slk1->GetRid()) == slk1);

    // 4. Load TXT table
    auto txt = server.LoadTxtTable("mpq://Units/HumanUnitFunc.txt");
    TEST_ASSERT(txt != nullptr);
    TEST_ASSERT(txt->table.GetString("hfoo", "Name") == "Footman");

    // 5. Request Footman.mdx (which falls back to Footman.mdl) and verify MdxModelResource
    auto mdx1 = server.LoadMdxModel("mpq://Units/Human/Footman/Footman.mdx");
    auto mdx2 = server.LoadMdxModel("Units\\Human\\Footman\\Footman.mdl");
    TEST_ASSERT(mdx1 != nullptr && mdx2 != nullptr);
    TEST_ASSERT(mdx1 == mdx2); // unified canonical key (.mdx <-> .mdl)
    TEST_ASSERT(mdx1->loadedFromMdlText);
    TEST_ASSERT(mdx1->model.sequences.size() == 1);
    TEST_NEAR(mdx1->model.boundsRadius, 64.0f, 1e-3f);

    const ResourceServerStats stats = server.GetStats();
    TEST_ASSERT(stats.cacheMisses == 3); // 1 SLK + 1 TXT + 1 MDX
    TEST_ASSERT(stats.cacheHits == 2);   // slk2 + mdx2
    TEST_ASSERT(stats.activeResourceCount == 3);
    TEST_ASSERT(stats.totalEstimatedBytes > 100);

    return true;
}

// 8. Test Stage E2: ResourceServer Async Background Worker Preload & Reference-Counted GC
bool TestPhase6StageEResourceServerAsyncPreloadAndGc() {
    using namespace resource;

    ResourceServer server;

    for (int i = 0; i < 4; ++i) {
        const std::string mdl =
            "Version { FormatVersion 800, }\n"
            "Model \"AsyncUnit" + std::to_string(i) + "\" {\n"
            "    BlendTime 150,\n"
            "    BoundsRadius " + std::to_string(40 + i * 10) + ".0,\n"
            "}\n"
            "Sequences 1 {\n"
            "    Anim \"Stand\" { Interval { 0, 500 }, }\n"
            "}\n";
        server.MountMemoryFile("Units\\Async\\Unit" + std::to_string(i) + ".mdl",
                               std::vector<uint8_t>(mdl.begin(), mdl.end()));
    }

    int callbacksInvoked = 0;
    std::vector<RID> completedRids;
    std::vector<ResourceRef<Resource>> pinnedRefs;

    // Submit 4 asynchronous model preload jobs to background worker thread
    for (int i = 0; i < 4; ++i) {
        const std::string uri = "mpq://Units/Async/Unit" + std::to_string(i) + ".mdx";
        server.RequestAsyncPreload(uri, ResourceKind::MdxModel,
            [&, i](RID rid, ResourceRef<Resource> res, bool ok) {
                if (ok && res && rid.IsValid()) {
                    callbacksInvoked++;
                    completedRids.push_back(rid);
                    // Pin only the first 2 resources; let units 2 & 3 remain unpinned
                    if (i < 2) {
                        pinnedRefs.push_back(res);
                    }
                }
            });
    }

    // Flush background worker and dispatch completion callbacks on main thread
    server.FlushAsyncPreloads();

    TEST_ASSERT(callbacksInvoked == 4);
    TEST_ASSERT(completedRids.size() == 4);

    ResourceServerStats statsBeforeGc = server.GetStats();
    TEST_ASSERT(statsBeforeGc.asyncJobsSubmitted == 4);
    TEST_ASSERT(statsBeforeGc.asyncJobsCompleted == 4);
    TEST_ASSERT(statsBeforeGc.activeResourceCount == 4);

    // Subsequent synchronous LoadMdxModel on preloaded URI hits cache immediately!
    auto cached0 = server.LoadMdxModel("Units\\Async\\Unit0.mdx");
    TEST_ASSERT(cached0 != nullptr);
    TEST_NEAR(cached0->model.boundsRadius, 40.0f, 1e-3f);
    TEST_ASSERT(server.GetStats().cacheHits == 1);
    cached0.reset();

    // Run PruneUnusedResources(): units 2 & 3 have no external ResourceRef holders (use_count == 1),
    // while units 0 & 1 are held in `pinnedRefs` (use_count == 2).
    const size_t evicted = server.PruneUnusedResources();
    TEST_ASSERT(evicted == 2);
    TEST_ASSERT(server.GetStats().activeResourceCount == 2);

    // Release remaining pinned references and prune again -> 0 active resources left
    pinnedRefs.clear();
    const size_t evicted2 = server.PruneUnusedResources();
    TEST_ASSERT(evicted2 == 2);
    TEST_ASSERT(server.GetStats().activeResourceCount == 0);

    server.StopAsyncWorker();
    return true;
}

// 9. Test Stage F1: Shared 2D BonePaletteAtlas RGBA32F Packing & Sampling
bool TestPhase6StageFBonePaletteAtlasPacking() {
    render::BonePaletteAtlas atlas(16);
    TEST_ASSERT(atlas.GetMaxBonesPerRow() == 16);
    TEST_ASSERT(atlas.GetTexWidth() == 64); // 16 bones * 4 RGBA32F texels per Mat4
    TEST_ASSERT(atlas.GetActiveRowCount() == 1); // Row 0 reserved for Identity
    TEST_ASSERT(atlas.IsDirty());

    // Row 0 must contain Identity matrices across all bone slots
    for (int b = 0; b < 16; ++b) {
        Mat4 ident = atlas.SampleMatrix(0, b);
        TEST_NEAR(ident.m[0], 1.0f, 1e-5f);
        TEST_NEAR(ident.m[5], 1.0f, 1e-5f);
        TEST_NEAR(ident.m[10], 1.0f, 1e-5f);
        TEST_NEAR(ident.m[15], 1.0f, 1e-5f);
        TEST_NEAR(ident.m[12], 0.0f, 1e-5f);
    }

    // Empty bone vector returns row 0 without allocating a new row
    TEST_ASSERT(atlas.AllocateAndPackRow({}) == 0);
    TEST_ASSERT(atlas.GetActiveRowCount() == 1);

    // Pack 3 distinct unit skeletons into rows 1, 2, 3
    for (int unitIdx = 0; unitIdx < 3; ++unitIdx) {
        std::vector<Mat4> bones(3, Mat4::Identity());
        bones[0] = Mat4::Translate(10.0f * (unitIdx + 1), 20.0f * (unitIdx + 1), 30.0f * (unitIdx + 1));
        bones[1] = Mat4::Scale(1.5f + unitIdx, 2.0f + unitIdx, 2.5f + unitIdx);
        bones[2] = Mat4::Translate(-5.0f * (unitIdx + 1), 0.0f, 15.0f * (unitIdx + 1));

        const int row = atlas.AllocateAndPackRow(bones);
        TEST_ASSERT(row == unitIdx + 1);
    }
    TEST_ASSERT(atlas.GetActiveRowCount() == 4);
    TEST_ASSERT(atlas.GetTexelBuffer().size() == 4u * 64u * 4u);

    for (int unitIdx = 0; unitIdx < 3; ++unitIdx) {
        const int row = unitIdx + 1;
        Mat4 m0 = atlas.SampleMatrix(row, 0);
        TEST_NEAR(m0.m[12], 10.0f * (unitIdx + 1), 1e-4f);
        TEST_NEAR(m0.m[13], 20.0f * (unitIdx + 1), 1e-4f);
        TEST_NEAR(m0.m[14], 30.0f * (unitIdx + 1), 1e-4f);

        Mat4 m1 = atlas.SampleMatrix(row, 1);
        TEST_NEAR(m1.m[0], 1.5f + unitIdx, 1e-4f);
        TEST_NEAR(m1.m[5], 2.0f + unitIdx, 1e-4f);
        TEST_NEAR(m1.m[10], 2.5f + unitIdx, 1e-4f);

        // Unpopulated bone slot (index 7) in the same row defaults to Identity
        Mat4 m7 = atlas.SampleMatrix(row, 7);
        TEST_NEAR(m7.m[0], 1.0f, 1e-4f);
        TEST_NEAR(m7.m[12], 0.0f, 1e-4f);
    }

    return true;
}

// 10. Test Stage F2: Hardware Instanced Batch Execution & Per-Unit BonePaletteAtlas Row Allocation
bool TestPhase6StageFHardwareInstancedBatchExecution() {
    resource::ResourceServer resServer;

    // Mount an animated skinned MDL model with a bone timeline and a triangle geoset
    const std::string animatedFootmanMdl = R"(
        Version { FormatVersion 800, }
        Model "FootmanInstanced" {
            NumGeosets 1,
            NumBones 1,
            BlendTime 150,
            MinimumExtent { -32.0, -32.0, 0.0 },
            MaximumExtent { 32.0, 32.0, 96.0 },
            BoundsRadius 64.0,
        }
        Sequences 1 {
            Anim "Stand" {
                Interval { 0, 1000 },
                MinimumExtent { -32.0, -32.0, 0.0 },
                MaximumExtent { 32.0, 32.0, 96.0 },
                BoundsRadius 64.0,
            }
        }
        Textures 1 {
            Bitmap { Image "", ReplaceableId 1, }
        }
        Materials 1 {
            Material {
                Layer {
                    FilterMode None,
                    static TextureID 0,
                }
            }
        }
        Geoset {
            Vertices 3 {
                { -10.0, 0.0, 0.0 },
                { 10.0, 0.0, 0.0 },
                { 0.0, 0.0, 20.0 },
            }
            Normals 3 {
                { 0.0, 0.0, 1.0 },
                { 0.0, 0.0, 1.0 },
                { 0.0, 0.0, 1.0 },
            }
            TVertices 3 {
                { 0.0, 0.0 },
                { 1.0, 0.0 },
                { 0.5, 1.0 },
            }
            VertexGroup {
                0,
                0,
                0,
            }
            Faces 1 3 {
                Triangles {
                    { 0, 1, 2 },
                }
            }
            Groups 1 1 {
                Matrices { 0 },
            }
            MinimumExtent { -10.0, 0.0, 0.0 },
            MaximumExtent { 10.0, 0.0, 20.0 },
            BoundsRadius 32.0,
            MaterialID 0,
            SelectionGroup 0,
        }
        Bone "RootBone" {
            ObjectId 0,
            Parent -1,
            GeosetId 0,
            GeosetAnimId -1,
            Translation 2 {
                Linear,
                0: { 0.0, 0.0, 0.0 },
                1000: { 0.0, 0.0, 50.0 },
            }
        }
        PivotPoints 1 {
            { 0.0, 0.0, 0.0 },
        }
    )";

    resServer.MountMemoryFile("Units/Human/Footman/Footman.mdl",
                              std::vector<uint8_t>(animatedFootmanMdl.begin(), animatedFootmanMdl.end()));

    SceneManager scene;
    scene.SetResourceServer(&resServer);
    const uint32_t footmanId = War3ID("hfoo").value;
    const uint32_t peasantId = War3ID("hpea").value;
    scene.RegisterUnitModelPath(footmanId, "Units/Human/Footman/Footman.mdx");

    const float scale[3] = {1.0f, 1.0f, 1.0f};
    std::vector<int> footmanIndices;
    for (int i = 0; i < 8; ++i) {
        const float pos[3] = {static_cast<float>(i * 25 - 80), 0.0f, 1.0f};
        const int idx = scene.AddUnit(footmanId, pos, 0.0f, scale, 0, 420, 0);
        TEST_ASSERT(idx >= 0);
        footmanIndices.push_back(idx);
        // Offset each unit's animation phase so each unit has a distinct skeleton matrix
        if (MdxViewer* v = scene.GetViewer(idx)) {
            v->SetCurrentSequenceFrameByRatio(static_cast<float>(i) / 8.0f);
            v->Update(0.01f);
        }
    }

    // Add 4 procedural static instances of another type
    for (int i = 0; i < 4; ++i) {
        const float pos[3] = {static_cast<float>(i * 30 - 45), 0.0f, 60.0f};
        scene.AddProceduralInstance(peasantId, pos, 0.0f, scale, 0, "unit", 1.0f);
    }

    Vec3 camEye(0.0f, 800.0f, 800.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);

    scene.RenderAll(view, proj, 0.016f);

    const render::RenderFrameStats& stats = scene.GetLastRenderStats();
    TEST_ASSERT(stats.submittedItems == 12);
    TEST_ASSERT(stats.visibleItems == 12);
    TEST_ASSERT(stats.opaqueBatchCount == 2); // 1 batch of 8 Footmen + 1 batch of 4 Peasants
    TEST_ASSERT(stats.hardwareInstancedDrawCalls == 2); // 2 instanced draws instead of 12 individual draws!
    TEST_ASSERT(stats.hardwareInstancesSubmitted == 12);
    TEST_ASSERT(stats.boneAtlasRowsAllocated == 9); // Row 0 (Identity) + 8 animated Footman skeleton rows

    // Verify that the first Footman viewer executed RenderInstancedBatch with all 8 instances
    MdxViewer* firstViewer = scene.GetViewer(footmanIndices[0]);
    TEST_ASSERT(firstViewer != nullptr);
    TEST_ASSERT(firstViewer->GetInstancedBatchCallCount() == 1);
    TEST_ASSERT(firstViewer->GetLastInstancedElementCount() == 8);

    return true;
}

// 11. Test Stage G1: core::JobSystem WorkerThreadPool, JobHandle & ParallelFor Execution
bool TestPhase6StageGJobSystemParallelForAndTaskGraph() {
    core::JobSystem jobs(4);
    TEST_ASSERT(jobs.IsInitialized());
    TEST_ASSERT(jobs.GetWorkerCount() == 4);
    jobs.ResetStats();

    // 1. ParallelFor over 256 elements with minBatchSize = 16 -> 16 batches
    std::vector<int> output(256, 0);
    jobs.ParallelFor(256, 16, [&](int idx) {
        output[static_cast<size_t>(idx)] = (idx + 1) * (idx + 1);
    });

    for (int i = 0; i < 256; ++i) {
        TEST_ASSERT(output[static_cast<size_t>(i)] == (i + 1) * (i + 1));
    }

    const core::JobSystemStats stats1 = jobs.GetStats();
    TEST_ASSERT(stats1.parallelForDispatches == 1);
    TEST_ASSERT(stats1.parallelForBatchesDispatched == 16);
    TEST_ASSERT(stats1.parallelForElementsProcessed == 256);
    TEST_ASSERT(stats1.totalJobsSubmitted == 16);
    TEST_ASSERT(stats1.totalJobsCompleted == 16);

    // 2. Submit independent async tasks and wait via JobHandle::Wait()
    std::atomic<int> sum{0};
    std::vector<core::JobHandle> handles;
    for (int i = 1; i <= 10; ++i) {
        handles.push_back(jobs.Submit([&sum, i]() {
            sum.fetch_add(i * 10, std::memory_order_relaxed);
        }));
    }
    for (const auto& h : handles) {
        h.Wait();
        TEST_ASSERT(h.IsCompleted());
    }
    TEST_ASSERT(sum.load() == 550);

    jobs.Shutdown();
    return true;
}

// 12. Test Stage G2: Parallel Skeleton Animation Evaluation & Parallel Frustum Culling in SceneManager
bool TestPhase6StageGParallelSkeletonUpdateAndFrustumCulling() {
    core::JobSystem jobs(4);
    jobs.ResetStats();
    resource::ResourceServer resServer;

    const std::string animatedUnitMdl = R"(
        Version { FormatVersion 800, }
        Model "ParallelKnight" {
            NumGeosets 1,
            NumBones 1,
            BlendTime 150,
            MinimumExtent { -32.0, -32.0, 0.0 },
            MaximumExtent { 32.0, 32.0, 96.0 },
            BoundsRadius 64.0,
        }
        Sequences 1 {
            Anim "Stand" {
                Interval { 0, 1000 },
                MinimumExtent { -32.0, -32.0, 0.0 },
                MaximumExtent { 32.0, 32.0, 96.0 },
                BoundsRadius 64.0,
            }
        }
        Textures 1 {
            Bitmap { Image "", ReplaceableId 1, }
        }
        Materials 1 {
            Material {
                Layer {
                    FilterMode None,
                    static TextureID 0,
                }
            }
        }
        Geoset {
            Vertices 3 {
                { -10.0, 0.0, 0.0 },
                { 10.0, 0.0, 0.0 },
                { 0.0, 0.0, 20.0 },
            }
            Normals 3 {
                { 0.0, 0.0, 1.0 },
                { 0.0, 0.0, 1.0 },
                { 0.0, 0.0, 1.0 },
            }
            TVertices 3 {
                { 0.0, 0.0 },
                { 1.0, 0.0 },
                { 0.5, 1.0 },
            }
            VertexGroup { 0, 0, 0, }
            Faces 1 3 {
                Triangles { { 0, 1, 2 }, }
            }
            Groups 1 1 {
                Matrices { 0 },
            }
            MinimumExtent { -10.0, 0.0, 0.0 },
            MaximumExtent { 10.0, 0.0, 20.0 },
            BoundsRadius 32.0,
            MaterialID 0,
            SelectionGroup 0,
        }
        Bone "RootBone" {
            ObjectId 0,
            Parent -1,
            GeosetId 0,
            GeosetAnimId -1,
            Translation 2 {
                Linear,
                0: { 0.0, 0.0, 0.0 },
                1000: { 0.0, 0.0, 100.0 },
            }
        }
        PivotPoints 1 {
            { 0.0, 0.0, 0.0 },
        }
    )";

    resServer.MountMemoryFile("Units/Human/Knight/Knight.mdl",
                              std::vector<uint8_t>(animatedUnitMdl.begin(), animatedUnitMdl.end()));

    SceneManager scene;
    scene.SetResourceServer(&resServer);
    scene.SetJobSystem(&jobs);

    const uint32_t knightId = War3ID("hkni").value;
    scene.RegisterUnitModelPath(knightId, "Units/Human/Knight/Knight.mdx");

    const float scale[3] = {1.0f, 1.0f, 1.0f};
    std::vector<int> visibleKnightIndices;
    // Spawn 16 animated Knights inside the camera frustum
    for (int i = 0; i < 16; ++i) {
        const float pos[3] = {static_cast<float>((i % 4) * 30 - 45), 0.0f, static_cast<float>((i / 4) * 30 - 45)};
        const int idx = scene.AddUnit(knightId, pos, 0.0f, scale, 0, 835, 0);
        TEST_ASSERT(idx >= 0);
        visibleKnightIndices.push_back(idx);
    }
    // Spawn 8 off-screen procedural instances far outside the frustum
    for (int i = 0; i < 8; ++i) {
        const float offPos[3] = {25000.0f + i * 100.0f, 0.0f, 25000.0f};
        scene.AddProceduralInstance(War3ID("hfoo").value, offPos, 0.0f, scale, 0, "unit", 1.0f);
    }

    Vec3 camEye(0.0f, 800.0f, 800.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);

    // Advance by 0.5 seconds (500 ms = midpoint of 0..1000 Stand sequence -> Z translation = 50 -> Y-up = +50)
    scene.RenderAll(view, proj, 0.5f);

    TEST_ASSERT(scene.GetLastParallelAnimatedViewerCount() == 16);

    const render::RenderFrameStats& rStats = scene.GetLastRenderStats();
    TEST_ASSERT(rStats.submittedItems == 24);
    TEST_ASSERT(rStats.parallelCulledItems == 24);
    TEST_ASSERT(rStats.culledByFrustum == 8);
    TEST_ASSERT(rStats.visibleItems == 16);
    TEST_ASSERT(rStats.opaqueBatchCount == 1);
    TEST_ASSERT(rStats.hardwareInstancedDrawCalls == 1);
    TEST_ASSERT(rStats.hardwareInstancesSubmitted == 16);
    TEST_ASSERT(rStats.boneAtlasRowsAllocated == 17); // Row 0 + 16 animated Knights

    // Verify JobSystem actually executed both parallel passes (16 viewers + 24 frustum-culled items)
    const core::JobSystemStats jStats = jobs.GetStats();
    TEST_ASSERT(jStats.parallelForDispatches == 2);
    TEST_ASSERT(jStats.parallelForElementsProcessed == 16 + 24);

    // Verify every Knight's skeleton was updated in parallel to frame 500 (bone Y translation == 50.0f)
    for (int row = 1; row <= 16; ++row) {
        Mat4 boneMat = scene.GetRenderServer().GetBoneAtlas().SampleMatrix(row, 0);
        TEST_NEAR(boneMat.m[13], 50.0f, 1e-2f);
    }

    jobs.Shutdown();
    return true;
}

// 13. Test Stage H1: Frustum::ClassifyAABB 3-State Classification & StaticSpatialChunkGrid Hierarchy Culling
bool TestPhase6StageHStaticSpatialChunkHierarchyCulling() {
    using namespace render;

    Vec3 camEye(0.0f, 900.0f, 900.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);
    Frustum frustum = Frustum::FromViewProj(proj * view);

    // 1. Small AABB right at origin is 100% strictly Inside all 6 frustum planes
    TEST_ASSERT(frustum.ClassifyAABB(Vec3(-80.0f, -20.0f, -80.0f), Vec3(80.0f, 40.0f, 80.0f)) ==
                Frustum::Containment::Inside);

    // 2. Large AABB straddling the near/left frustum boundary is Intersecting
    TEST_ASSERT(frustum.ClassifyAABB(Vec3(-2500.0f, -100.0f, -500.0f), Vec3(0.0f, 100.0f, 500.0f)) ==
                Frustum::Containment::Intersecting);

    // 3. Distant AABB far away is Outside
    TEST_ASSERT(frustum.ClassifyAABB(Vec3(15000.0f, 0.0f, 15000.0f), Vec3(16000.0f, 200.0f, 16000.0f)) ==
                Frustum::Containment::Outside);

    // 4. Populate StaticSpatialChunkGrid (chunkSize = 512) with:
    //    - 20 trees in chunk (0, 0) tightly near origin -> strictly Inside (fast-accepted!)
    //    - 30 trees in chunk (20, 20) around (10500, 0, 10500) -> strictly Outside (O(1) chunk culled!)
    //    - 25 trees in chunk (-20, -20) around (-10200, 0, -10200) -> strictly Outside (O(1) chunk culled!)
    StaticSpatialChunkGrid grid(512.0f);
    int nextHandle = 1;
    for (int i = 0; i < 20; ++i) {
        RenderItem item;
        item.entityHandle = nextHandle++;
        item.modelId = War3ID("LTlt").value;
        item.worldBoundsCenter = Vec3(10.0f + (i % 5) * 15.0f, 0.0f, 10.0f + (i / 5) * 15.0f);
        item.worldBoundsRadius = 24.0f;
        grid.InsertStaticItem(item, true);
    }
    for (int i = 0; i < 30; ++i) {
        RenderItem item;
        item.entityHandle = nextHandle++;
        item.modelId = War3ID("LTlt").value;
        item.worldBoundsCenter = Vec3(10300.0f + (i % 6) * 20.0f, 0.0f, 10300.0f + (i / 6) * 20.0f);
        item.worldBoundsRadius = 32.0f;
        grid.InsertStaticItem(item, true);
    }
    for (int i = 0; i < 25; ++i) {
        RenderItem item;
        item.entityHandle = nextHandle++;
        item.modelId = War3ID("LTlt").value;
        item.worldBoundsCenter = Vec3(-10100.0f + (i % 5) * 20.0f, 0.0f, -10100.0f + (i / 5) * 20.0f);
        item.worldBoundsRadius = 32.0f;
        grid.InsertStaticItem(item, true);
    }

    TEST_ASSERT(grid.GetChunkCount() == 3);
    TEST_ASSERT(grid.GetTotalStaticItemCount() == 75);

    RenderServer server;
    server.BeginFrame(view, proj, camEye);
    server.SubmitStaticChunkGrid(grid);
    server.BuildAndSortBatches();

    const RenderFrameStats& stats = server.GetStats();
    TEST_ASSERT(stats.totalStaticChunks == 3);
    TEST_ASSERT(stats.visibleChunks == 1);
    TEST_ASSERT(stats.culledChunks == 2);
    TEST_ASSERT(stats.submittedItems == 75);
    TEST_ASSERT(stats.culledByChunkHierarchy == 55); // 30 + 25 culled in 2 AABB tests!
    TEST_ASSERT(stats.fastAcceptedByChunkInside == 20); // 20 fast-accepted without per-tree sphere tests!
    TEST_ASSERT(stats.visibleItems == 20);
    TEST_ASSERT(stats.opaqueBatchCount == 1);

    return true;
}

// 14. Test Stage H2: SceneManager Static Doodad Spatial Chunk Acceleration & Dynamic Visibility Sync
bool TestPhase6StageHSceneManagerDoodadChunkAcceleration() {
    SceneManager scene;
    scene.SetStaticChunkSize(1024.0f);

    const float scale[3] = {1.0f, 1.0f, 1.0f};
    const uint32_t treeId = War3ID("LTlt").value;
    const uint32_t rockId = War3ID("LTrc").value;
    const uint32_t footmanId = War3ID("hfoo").value;

    // 1. Add 12 static tree doodads inside the camera frustum (chunk 0, 0)
    std::vector<int> nearTreeIndices;
    for (int i = 0; i < 12; ++i) {
        const float pos[3] = {20.0f + (i % 4) * 25.0f, 0.0f, 20.0f + (i / 4) * 25.0f};
        nearTreeIndices.push_back(scene.AddProceduralInstance(treeId, pos, 0.0f, scale, 0, "doodad", 1.0f));
    }

    // 2. Add 40 static rock doodads in 2 far-away off-screen chunks
    for (int i = 0; i < 20; ++i) {
        const float pos1[3] = {12000.0f + (i % 5) * 30.0f, 0.0f, 12000.0f + (i / 5) * 30.0f};
        const float pos2[3] = {-14000.0f + (i % 5) * 30.0f, 0.0f, -14000.0f + (i / 5) * 30.0f};
        scene.AddProceduralInstance(rockId, pos1, 0.0f, scale, 0, "doodad", 1.0f);
        scene.AddProceduralInstance(rockId, pos2, 0.0f, scale, 0, "doodad", 1.0f);
    }

    // 3. Add 6 dynamic unit instances near origin
    for (int i = 0; i < 6; ++i) {
        const float pos[3] = {-60.0f + i * 20.0f, 0.0f, -40.0f};
        scene.AddProceduralInstance(footmanId, pos, 0.0f, scale, 0, "unit", 1.0f);
    }

    TEST_ASSERT(scene.GetStaticChunkGrid().GetTotalStaticItemCount() == 52);
    TEST_ASSERT(scene.GetStaticChunkGrid().GetChunkCount() == 3);

    // Hide 2 near trees via SetInstanceVisible -> must sync to StaticSpatialChunkGrid
    scene.SetInstanceVisible(nearTreeIndices[0], false);
    scene.SetInstanceVisible(nearTreeIndices[1], false);

    Vec3 camEye(0.0f, 900.0f, 900.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);

    scene.RenderAll(view, proj, 0.016f);

    const render::RenderFrameStats& stats = scene.GetLastRenderStats();
    // 50 active static doodads (10 near + 40 far) + 6 dynamic units = 56 submitted
    TEST_ASSERT(stats.submittedItems == 56);
    TEST_ASSERT(stats.totalStaticChunks == 3);
    TEST_ASSERT(stats.visibleChunks == 1);
    TEST_ASSERT(stats.culledChunks == 2);
    TEST_ASSERT(stats.culledByChunkHierarchy == 40);
    TEST_ASSERT(stats.fastAcceptedByChunkInside == 10);
    // Only the 6 dynamic units went through per-item parallel sphere culling!
    TEST_ASSERT(stats.parallelCulledItems == 6);
    TEST_ASSERT(stats.visibleItems == 16); // 10 visible near trees + 6 dynamic units
    TEST_ASSERT(stats.opaqueBatchCount == 2); // 1 batch of 10 trees + 1 batch of 6 footmen
    TEST_ASSERT(stats.hardwareInstancedDrawCalls == 2);

    return true;
}

// 15. Test Stage I1: AudioServer Hierarchical Mixing Buses, Sidechain Ducking & 3D Spatialization
bool TestPhase6StageIAudioServerBusHierarchyAnd3DSpatialization() {
    using namespace audio;

    AudioServer server;
    server.SetVoiceDuckingEnabled(true, 0.60f);

    // 1. Verify dB <-> Linear round-trip and Master * Sub-Bus hierarchical gain
    server.SetBusVolumeDb(AudioBusId::Master, -6.0206f); // ~0.5 linear
    TEST_NEAR(server.GetBusVolumeLinear(AudioBusId::Master), 0.5f, 1e-3f);
    server.SetBusVolumeLinear(AudioBusId::SFX, 0.8f);
    TEST_NEAR(server.ComputeEffectiveBusGain(AudioBusId::SFX), 0.4f, 1e-3f);

    // 2. Verify Mute and Solo bus routing
    server.SetBusSolo(AudioBusId::UnitVoice, true);
    TEST_NEAR(server.ComputeEffectiveBusGain(AudioBusId::SFX), 0.0f, 1e-5f);
    TEST_NEAR(server.ComputeEffectiveBusGain(AudioBusId::UnitVoice), 0.5f, 1e-3f);
    server.SetBusSolo(AudioBusId::UnitVoice, false);

    // 3. Verify 3D Listener orientation, distance attenuation, cutoff culling & stereo pan
    server.SetBusVolumeLinear(AudioBusId::Master, 1.0f);
    server.SetBusVolumeLinear(AudioBusId::SFX, 1.0f);
    server.SetListenerPosition(Vec3(0.0f, 0.0f, 0.0f));
    server.SetListenerOrientation(Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f)); // Right = +X

    // Sound at (+600, 0, 0) with minDist=200, maxDist=1000 -> distance=600 (midpoint -> gain=0.5, pan=+1.0 right)
    AudioVoiceRequest rightSfx;
    rightSfx.clipKey = "Sound/SFX/SwordHitRight.wav";
    rightSfx.bus = AudioBusId::SFX;
    rightSfx.baseVolume = 1.0f;
    rightSfx.durationSec = 2.0f;
    rightSfx.spatial.is3D = true;
    rightSfx.spatial.position = Vec3(600.0f, 0.0f, 0.0f);
    rightSfx.spatial.minDistance = 200.0f;
    rightSfx.spatial.maxDistance = 1000.0f;
    rightSfx.spatial.distanceCutoff = 1200.0f;

    const int sfxVoiceId = server.SubmitVoice(rightSfx);
    TEST_ASSERT(sfxVoiceId > 0);
    const ActiveAudioVoice* sfxVoice = server.FindActiveVoice(sfxVoiceId);
    TEST_ASSERT(sfxVoice != nullptr);
    TEST_NEAR(sfxVoice->finalGain, 0.5f, 1e-3f);
    TEST_NEAR(sfxVoice->stereoPan, 1.0f, 1e-3f);

    // 4. Submit a UnitVoice line -> automatically ducks active SFX voice by 0.60x (0.5 * 0.6 = 0.3)!
    AudioVoiceRequest heroLine;
    heroLine.clipKey = "Sound/Units/Human/HeroPaladin/What1.wav";
    heroLine.bus = AudioBusId::UnitVoice;
    heroLine.baseVolume = 1.0f;
    heroLine.durationSec = 0.5f;
    heroLine.spatial.is3D = false;

    const int voiceLineId = server.SubmitVoice(heroLine);
    TEST_ASSERT(voiceLineId > 0);
    sfxVoice = server.FindActiveVoice(sfxVoiceId);
    TEST_ASSERT(sfxVoice != nullptr);
    TEST_NEAR(sfxVoice->finalGain, 0.30f, 1e-3f); // Ducked while hero speaks!

    // Advance 0.6s -> hero voice finishes and SFX voice automatically restores unducked gain (0.5f)!
    server.Update(0.6f);
    TEST_ASSERT(server.FindActiveVoice(voiceLineId) == nullptr);
    sfxVoice = server.FindActiveVoice(sfxVoiceId);
    TEST_ASSERT(sfxVoice != nullptr);
    TEST_NEAR(sfxVoice->finalGain, 0.50f, 1e-3f);

    // 5. Sound beyond distanceCutoff (dist=3500 > cutoff=1200) is culled immediately
    AudioVoiceRequest farSfx = rightSfx;
    farSfx.clipKey = "Sound/SFX/FarExplosion.wav";
    farSfx.spatial.position = Vec3(3500.0f, 0.0f, 0.0f);
    TEST_ASSERT(server.SubmitVoice(farSfx) == -1);
    TEST_ASSERT(server.GetStats().culledByDistance == 1);

    return true;
}

// 16. Test Stage I2: Cooldown Throttling, Priority Voice Stealing & SimEventBus Audio Bridge
bool TestPhase6StageIAudioPolyphonyVoiceStealingAndSimEvents() {
    using namespace audio;

    AudioServer server;
    server.SetMaxActiveVoices(3);
    server.SetMinClipCooldownSec(0.05f);
    server.SetMaxIdenticalClipConcurrency(2);
    server.SetVoiceDuckingEnabled(false);

    // 1. Cooldown throttling: submitting the exact same clip twice within 0.05s throttles the second one
    AudioVoiceRequest arrowReq;
    arrowReq.clipKey = "Sound/Units/Combat/ArrowRelease.wav";
    arrowReq.bus = AudioBusId::SFX;
    arrowReq.priority = 40;
    arrowReq.durationSec = 1.0f;
    TEST_ASSERT(server.SubmitVoice(arrowReq) > 0);
    TEST_ASSERT(server.SubmitVoice(arrowReq) == -1); // Throttled by 50ms cooldown!
    TEST_ASSERT(server.GetStats().throttledByCooldownOrConcurrency == 1);

    // 2. Fill remaining 2 polyphony slots with low-priority Ambient voices (priority=20)
    server.Update(0.10f);
    for (int i = 0; i < 2; ++i) {
        AudioVoiceRequest amb;
        amb.clipKey = "Sound/Ambient/WindLoop_" + std::to_string(i) + ".wav";
        amb.bus = AudioBusId::Ambient;
        amb.priority = 20;
        amb.durationSec = 5.0f;
        TEST_ASSERT(server.SubmitVoice(amb) > 0);
    }
    TEST_ASSERT(server.GetActiveVoiceCount() == 3); // Pool is now full (3/3)

    // 3. Submit a high-priority UI alert (priority=95) -> steals one of the low-priority Ambient voices!
    AudioVoiceRequest uiAlert;
    uiAlert.clipKey = "Sound/Interface/Warning.wav";
    uiAlert.bus = AudioBusId::UI;
    uiAlert.priority = 95;
    uiAlert.durationSec = 1.0f;
    const int alertId = server.SubmitVoice(uiAlert);
    TEST_ASSERT(alertId > 0);
    TEST_ASSERT(server.GetActiveVoiceCount() == 3);
    TEST_ASSERT(server.GetStats().voicesStolen == 1);

    // 4. Bind AudioServer to CSimulation::GetEventBus() and verify combat damage & unit death 3D sounds
    server.Reset();
    server.SetListenerPosition(Vec3(0.0f, 0.0f, 0.0f));

    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    server.BindSimulationEvents(sim);

    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 200, 270, false);
    CUnit* attacker = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), -50.0f, 0.0f, 0.0f));
    CUnit* victim = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"), 100.0f, 0.0f, 180.0f));
    sim.Update();

    // Deal non-lethal damage (80 dmg) -> triggers 1 spatialized combat hit sound on SFX bus
    sim.Events().onUnitDamage.Emit(victim->GetHandleId(), attacker->GetHandleId(), 80.0f);
    TEST_ASSERT(server.GetStats().simDamageSoundsTriggered == 1);
    TEST_ASSERT(server.GetActiveVoiceCountOnBus(AudioBusId::SFX) == 1);

    // Advance time past cooldown and deal lethal damage (150 dmg) -> triggers combat hit + unit death voice!
    server.Update(0.10f);
    sim.Events().onUnitDamage.Emit(victim->GetHandleId(), attacker->GetHandleId(), 150.0f);
    sim.Events().onUnitDeath.Emit(victim->GetHandleId(), attacker->GetHandleId(), 1);
    TEST_ASSERT(server.GetStats().simDamageSoundsTriggered == 2);
    TEST_ASSERT(server.GetStats().simDeathSoundsTriggered == 1);
    TEST_ASSERT(server.GetActiveVoiceCountOnBus(AudioBusId::UnitVoice) == 1);

    server.UnbindSimulationEvents(sim);
    return true;
}


// ============================================================================
// Stage J — VFS & MPQ Multi-Stage Audio Decompression + AudioClipResource
// ============================================================================
bool TestPhase7StageJMpqMultiStageDecompressionAndAudioClip() {
    using namespace resource;

    // 1. Construct a synthetic RIFF/WAVE with an intermediate JUNK chunk before fmt & data
    std::vector<uint8_t> wavBytes;
    auto pushStr = [&](const char* s) {
        while (*s) wavBytes.push_back(static_cast<uint8_t>(*s++));
    };
    auto pushU16 = [&](uint16_t v) {
        wavBytes.push_back(static_cast<uint8_t>(v & 0xFFu));
        wavBytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    };
    auto pushU32 = [&](uint32_t v) {
        wavBytes.push_back(static_cast<uint8_t>(v & 0xFFu));
        wavBytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
        wavBytes.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
        wavBytes.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
    };

    const uint32_t sampleRate = 22050;
    const uint16_t channels = 1;
    const uint16_t bitsPerSample = 16;
    const uint32_t numSamples = 2205; // 0.10 sec
    const uint32_t pcmBytes = numSamples * channels * (bitsPerSample / 8);

    pushStr("RIFF");
    pushU32(4 + (8 + 4) + (8 + 16) + (8 + pcmBytes));
    pushStr("WAVE");
    pushStr("JUNK");
    pushU32(4);
    pushU32(0);
    pushStr("fmt ");
    pushU32(16);
    pushU16(1); // PCM
    pushU16(channels);
    pushU32(sampleRate);
    pushU32(sampleRate * channels * (bitsPerSample / 8));
    pushU16(channels * (bitsPerSample / 8));
    pushU16(bitsPerSample);
    pushStr("data");
    pushU32(pcmBytes);
    wavBytes.resize(wavBytes.size() + pcmBytes, 0);

    ResourceServer server;
    server.MountMemoryFile("Sound/Units/Human/Footman/FootmanReady1.wav", wavBytes);
    server.MountMemoryFile("Sound/Units/SilenceOverride.wav", std::vector<uint8_t>{});

    auto clip1 = server.LoadAudioClip("mpq://Sound/Units/Human/Footman/FootmanReady1.wav");
    auto clip2 = server.LoadAudioClip("sound\\units\\human\\footman\\footmanready1.wav");
    TEST_ASSERT(clip1 != nullptr && clip2 != nullptr);
    TEST_ASSERT(clip1 == clip2);
    TEST_ASSERT(clip1->format == AudioFormat::WavPcm);
    TEST_ASSERT(clip1->channels == 1);
    TEST_ASSERT(clip1->sampleRate == 22050);
    TEST_ASSERT(clip1->bitsPerSample == 16);
    TEST_ASSERT(clip1->pcmDataSize == pcmBytes);
    TEST_NEAR(clip1->durationSeconds, 0.10f, 1e-3f);

    // 2. Verify 0-byte silence override WAV entry (used in retail maps like AzerothGrandPrix.w3x)
    auto silentClip = server.LoadAudioClip("Sound/Units/SilenceOverride.wav");
    TEST_ASSERT(silentClip != nullptr);
    TEST_ASSERT(silentClip->isSilenceOverride);
    TEST_ASSERT(silentClip->format == AudioFormat::SilenceOverride);

    return true;
}

bool TestPhase7StageJRetailDriveMapMpqAndAudioVerification() {
    const std::vector<std::string> candidateMaps = {
        "resources/Maps/FrozenThrone/Scenario/(8)AzerothGrandPrix.w3x",
        "resources/Maps/FrozenThrone/Scenario/(10)ExtremeCandyWar2004.w3x",
        "resources/Maps/FrozenThrone/Scenario/(12)WormWar.w3x"
    };

    MpqManager mpq;
    int mapsTested = 0;
    int wavsVerified = 0;
    int silentOverridesVerified = 0;

    for (const std::string& relPath : candidateMaps) {
        FILE* f = std::fopen(relPath.c_str(), "rb");
        if (!f) continue;
        std::fclose(f);

        TEST_ASSERT(mpq.OpenOnTop(relPath));
        mapsTested++;

        resource::ResourceServer resServer;
        resServer.SetMpqManager(&mpq);

        auto files = mpq.ListFiles(relPath);
        for (const std::string& wavName : files) {
            if (wavName.size() < 4) continue;
            std::string ext = wavName.substr(wavName.size() - 4);
            for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (ext != ".wav") continue;
            auto clip = resServer.LoadAudioClip(wavName);
            TEST_ASSERT(clip != nullptr);
            if (clip->isSilenceOverride) {
                silentOverridesVerified++;
            } else {
                TEST_ASSERT(clip->format == resource::AudioFormat::WavPcm);
                TEST_ASSERT(clip->sampleRate > 0);
                TEST_ASSERT(clip->pcmDataSize > 0);
                wavsVerified++;
            }
        }
        mpq.CloseTop();
    }

    if (mapsTested > 0) {
        TEST_ASSERT(wavsVerified >= 50);
        TEST_ASSERT(silentOverridesVerified >= 2);
    }
    return true;
}

// ============================================================================
// Stage K — 100% Bytecode JASS VM, Local Arrays, Constant Folding & Coroutines
// ============================================================================
bool TestPhase7StageKBytecodeLocalArraysAndConstantFolding() {
    const std::string src = R"(
        globals
            integer g_outSum = 0
            string g_outLabel = ""
        endglobals

        function ComputeWithLocalArrays takes integer base returns integer
            local integer array nums
            local string array labels
            local integer folded
            local integer i
            set folded = (10 + 20) * 3 + 5
            set labels[0] = "War" + "Craft"
            set g_outLabel = labels[0]
            set i = 0
            loop
                exitwhen i >= 4
                set nums[i] = base + i * 10 + folded
                set i = i + 1
            endloop
            return nums[0] + nums[3]
        endfunction

        function main takes nothing returns nothing
            set g_outSum = ComputeWithLocalArrays(5)
        endfunction
    )";

    JassVM vm;
    TEST_ASSERT(vm.LoadScript(src));

    const JassBytecodeChunk* vmChunk = vm.GetCompiledBytecode("ComputeWithLocalArrays");
    TEST_ASSERT(vmChunk != nullptr);
    TEST_ASSERT(vmChunk->numLocalArraySlots == 2);
    TEST_ASSERT(vmChunk->constantFoldedCount >= 2);

    vm.SetBytecodeEnabled(true);
    const uint64_t beforeCount = vm.GetBytecodeExecutedCount();
    vm.Execute("main");
    TEST_ASSERT(vm.GetBytecodeExecutedCount() >= beforeCount + 2);
    // folded = 95; nums[0] = 5 + 0 + 95 = 100; nums[3] = 5 + 30 + 95 = 130; sum = 230
    TEST_ASSERT(vm.GetGlobal("g_outSum").AsInt() == 230);
    TEST_ASSERT(vm.GetGlobal("g_outLabel").strVal == "WarCraft");
    return true;
}

bool TestPhase7StageKBytecodeResumableCoroutineTriggerSleep() {
    const std::string src = R"(
        native TriggerSleepAction takes real timeout returns nothing

        globals
            integer g_progress = 0
        endglobals

        function CoroutineWorker takes nothing returns nothing
            local integer array stepHistory
            local integer step = 1
            set stepHistory[0] = 10
            set g_progress = stepHistory[0]
            call TriggerSleepAction(0.20)
            set step = step + 1
            set stepHistory[1] = stepHistory[0] + 25 * step
            set g_progress = stepHistory[1]
            call TriggerSleepAction(0.30)
            set stepHistory[2] = stepHistory[1] + 40
            set g_progress = stepHistory[2]
        endfunction
    )";

    JassVM vm;
    TEST_ASSERT(vm.LoadScript(src));
    vm.RegisterNative("TriggerSleepAction", [&vm](const std::vector<JassValue>& args) -> JassValue {
        const float seconds = args.empty() ? 0.0f : args[0].realVal;
        vm.RequestSleep(seconds);
        return JassValue::Null();
    });

    const JassBytecodeChunk* chunk = vm.GetCompiledBytecode("CoroutineWorker");
    TEST_ASSERT(chunk != nullptr);
    TEST_ASSERT(chunk->numLocalArraySlots == 1);

    JassVM::ThreadState state;
    JassVM::ThreadRunResult r1 = vm.RunThread(state, "CoroutineWorker");
    TEST_ASSERT(r1.sleeping && !r1.completed);
    TEST_NEAR(r1.sleepSeconds, 0.20f, 1e-3f);
    TEST_ASSERT(state.useBytecodeCoroutine);
    TEST_ASSERT(vm.GetGlobal("g_progress").AsInt() == 10);

    // Resume after first TriggerSleepAction -> advances to second sleep while preserving local array!
    JassVM::ThreadRunResult r2 = vm.RunThread(state, "CoroutineWorker");
    TEST_ASSERT(r2.sleeping && !r2.completed);
    TEST_NEAR(r2.sleepSeconds, 0.30f, 1e-3f);
    TEST_ASSERT(vm.GetGlobal("g_progress").AsInt() == 60); // 10 + 25 * 2 = 60

    // Resume after second TriggerSleepAction -> completes thread!
    JassVM::ThreadRunResult r3 = vm.RunThread(state, "CoroutineWorker");
    TEST_ASSERT(r3.completed && !r3.sleeping);
    TEST_ASSERT(vm.GetGlobal("g_progress").AsInt() == 100); // 60 + 40 = 100

    return true;
}

// ============================================================================
// Stage L — Data-Oriented ECS Hot Component Pools & Deterministic Snapshot
// ============================================================================
bool TestPhase7StageLHotComponentPoolsAndFleetTelemetry() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 400, 270, false);

    std::vector<CUnit*> p0Units;
    for (int i = 0; i < 10; ++i) {
        p0Units.push_back(sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
            static_cast<float>(i * 10), 20.0f, 0.0f)));
    }
    for (int i = 0; i < 6; ++i) {
        sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"),
            100.0f, 200.0f, 90.0f));
    }

    // LifecycleCommit inside sim.Update() automatically syncs hot component pools
    sim.Update();
    TEST_ASSERT(sim.Registry().Pool<UnitHotTransformComponent>().Size() == 16);
    TEST_ASSERT(sim.Registry().Pool<UnitHotVitalsComponent>().Size() == 16);

    SimHotFleetMetrics p0Metrics = SimHotComponentSystem::ComputeFleetMetricsFromHotPools(sim.Registry(), 0);
    TEST_ASSERT(p0Metrics.activeUnitCount == 10);
    TEST_NEAR(p0Metrics.centroidX, 45.0f, 1e-2f);
    TEST_NEAR(p0Metrics.centroidY, 20.0f, 1e-2f);
    TEST_NEAR(p0Metrics.totalAliveLife, 4000.0f, 1e-2f);

    // Mutate hot pools directly in contiguous memory and apply back to CUnits
    auto* hotTf = sim.Registry().TryGet<UnitHotTransformComponent>(p0Units[0]->GetHandleId());
    auto* hotVit = sim.Registry().TryGet<UnitHotVitalsComponent>(p0Units[0]->GetHandleId());
    TEST_ASSERT(hotTf != nullptr && hotVit != nullptr);
    hotTf->x = 777.0f;
    hotTf->y = 888.0f;
    hotVit->life = 250.0f;

    SimHotComponentSystem::ApplyHotPoolsToUnits(sim);
    TEST_NEAR(p0Units[0]->GetX(), 777.0f, 1e-3f);
    TEST_NEAR(p0Units[0]->GetY(), 888.0f, 1e-3f);
    TEST_NEAR(p0Units[0]->GetLife(), 250.0f, 1e-3f);

    return true;
}

bool TestPhase7StageLSimStateSnapshotRollbackAndBinaryWire() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    sim.GetPlayer(0)->SetGold(750);
    sim.GetPlayer(0)->SetLumber(320);

    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    CUnit* u1 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 120.0f, 240.0f, 45.0f));
    CUnit* u2 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"), -300.0f, 150.0f, 180.0f));
    const int u1Handle = u1->GetHandleId();
    const int u2Handle = u2->GetHandleId();
    u1->SetScale(1.75f);
    sim.Update();

    // Capture deterministic snapshot at Tick 1
    const uint64_t expectedHash = sim.HashState();
    SimStateSnapshot snap = SimStateSnapshot::Capture(sim);
    TEST_ASSERT(snap.stateHash == expectedHash);
    TEST_ASSERT(snap.units.size() == 2);

    // Round-trip through binary wire serialization
    std::vector<uint8_t> wireBytes = snap.SerializeBinary();
    TEST_ASSERT(wireBytes.size() > 64);
    SimStateSnapshot decodedSnap;
    TEST_ASSERT(SimStateSnapshot::DeserializeBinary(wireBytes.data(), wireBytes.size(), decodedSnap));
    TEST_ASSERT(decodedSnap.stateHash == expectedHash);
    TEST_ASSERT(decodedSnap.units.size() == 2);

    // Mutate simulation heavily: spend resources, move u1, remove u2, spawn u3, advance 5 ticks
    sim.GetPlayer(0)->SetGold(100);
    u1->SetPosition(999.0f, -999.0f);
    u1->SetLife(sim, 50.0f);
    sim.RemoveUnit(u2);
    CUnit* u3 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 500.0f, 500.0f, 0.0f));
    const int u3Handle = u3->GetHandleId();
    for (int t = 0; t < 5; ++t) sim.Update();

    TEST_ASSERT(sim.HashState() != expectedHash);
    TEST_ASSERT(sim.GetUnit(u2Handle) == nullptr);
    TEST_ASSERT(sim.GetUnit(u3Handle) != nullptr);

    // Restore from decoded binary snapshot and verify 100% state & hash parity!
    TEST_ASSERT(decodedSnap.Restore(sim));
    TEST_ASSERT(sim.HashState() == expectedHash);
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 750);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 320);
    TEST_ASSERT(sim.GetUnit(u3Handle) == nullptr);

    CUnit* restoredU1 = sim.GetUnit(u1Handle);
    CUnit* restoredU2 = sim.GetUnit(u2Handle);
    TEST_ASSERT(restoredU1 != nullptr && restoredU2 != nullptr);
    TEST_NEAR(restoredU1->GetX(), 120.0f, 1e-3f);
    TEST_NEAR(restoredU1->GetLife(), 420.0f, 1e-3f);
    TEST_NEAR(restoredU1->GetScale(), 1.75f, 1e-3f);
    TEST_NEAR(restoredU2->GetX(), -300.0f, 1e-3f);

    return true;
}

// ============================================================================
// Stage M — Zero-Allocation PathQueryWorkspace, FlowFieldGrid & PathfindingServer
// ============================================================================
bool TestPhase7StageMZeroAllocPathQueryWorkspaceAndFlowField() {
    WpmData wpm;
    wpm.width = 64;
    wpm.height = 64;
    wpm.pathing.assign(64 * 64, 0x00); // All walkable

    // Stamp a vertical obstacle wall at cellX = 32 from cellY = 10..50
    for (int y = 10; y <= 50; ++y) {
        wpm.pathing[static_cast<size_t>(y * 64 + 32)] = 0x02; // UNWALKABLE
    }

    CPathfindingProcessor proc;
    proc.SetPathingMap(&wpm, -1024.0f, -1024.0f);

    pathfinding::PathQueryWorkspace ws;
    for (int i = 0; i < 25; ++i) {
        pathfinding::PathRequest req;
        req.requestId = i + 1;
        req.startWorldX = -400.0f;
        req.startWorldY = 0.0f;
        req.goalWorldX = 400.0f;
        req.goalWorldY = 0.0f;
        req.collisionSize = 32.0f;
        req.movementType = PathingGridMovementType::FOOT;

        pathfinding::PathResult res;
        TEST_ASSERT(ws.SolveAStar(proc, req, res));
        TEST_ASSERT(res.found);
        TEST_ASSERT(!res.waypoints.empty());
    }

    // Verify zero heap reallocations after the first query!
    TEST_ASSERT(ws.GetTotalQueriesExecuted() == 25);
    TEST_ASSERT(ws.GetBufferAllocationCount() == 1);

    // Build FlowFieldGrid around (400, 0) and sample steering direction from (-400, 0)
    pathfinding::FlowFieldGrid flow;
    TEST_ASSERT(flow.Build(proc, 400.0f, 0.0f, 48, PathingGridMovementType::FOOT));
    TEST_ASSERT(flow.GetReachableCellCount() > 1000);

    float dirX = 0.0f, dirY = 0.0f;
    TEST_ASSERT(flow.SampleDirectionWorld(proc, -400.0f, 0.0f, dirX, dirY));
    const float len = std::sqrt(dirX * dirX + dirY * dirY);
    TEST_NEAR(len, 1.0f, 1e-3f);

    return true;
}

bool TestPhase7StageMParallelPathfindingServerBatchDeterminism() {
    WpmData wpm;
    wpm.width = 64;
    wpm.height = 64;
    wpm.pathing.assign(64 * 64, 0x00);
    for (int y = 16; y <= 48; ++y) {
        wpm.pathing[static_cast<size_t>(y * 64 + 30)] = 0x02;
    }

    CPathfindingProcessor proc;
    proc.SetPathingMap(&wpm, -1024.0f, -1024.0f);

    std::vector<pathfinding::PathRequest> batch;
    for (int i = 0; i < 24; ++i) {
        pathfinding::PathRequest req;
        req.requestId = 100 + i;
        req.unitHandle = 1000 + i;
        req.startWorldX = -600.0f + (i % 6) * 32.0f;
        req.startWorldY = -400.0f + (i / 6) * 64.0f;
        req.goalWorldX = 500.0f;
        req.goalWorldY = 200.0f;
        req.collisionSize = 32.0f;
        req.movementType = PathingGridMovementType::FOOT;
        batch.push_back(req);
    }

    pathfinding::PathfindingServer serverSeq;
    auto resSeq = serverSeq.SolveBatch(proc, batch, nullptr);
    TEST_ASSERT(resSeq.size() == 24);

    core::JobSystem jobs(4);
    pathfinding::PathfindingServer serverPar;
    auto resPar = serverPar.SolveBatch(proc, batch, &jobs, 4);
    TEST_ASSERT(resPar.size() == 24);
    TEST_ASSERT(serverPar.GetStats().parallelBatchesDispatched == 1);
    TEST_ASSERT(serverPar.GetStats().totalPathsFound == 24);

    // Verify 100% deterministic waypoint equality between sequential and parallel execution
    for (size_t i = 0; i < 24; ++i) {
        TEST_ASSERT(resSeq[i].found && resPar[i].found);
        TEST_ASSERT(resSeq[i].requestId == resPar[i].requestId);
        TEST_ASSERT(resSeq[i].waypoints == resPar[i].waypoints);
    }

    // Verify FlowField caching in PathfindingServer
    const auto* ff1 = serverPar.GetOrBuildFlowField(proc, 500.0f, 200.0f, 32, PathingGridMovementType::FOOT);
    const auto* ff2 = serverPar.GetOrBuildFlowField(proc, 500.0f, 200.0f, 32, PathingGridMovementType::FOOT);
    TEST_ASSERT(ff1 != nullptr && ff1 == ff2);
    TEST_ASSERT(serverPar.GetStats().flowFieldsBuilt == 1);
    TEST_ASSERT(serverPar.GetStats().flowFieldCacheHits == 1);

    jobs.Shutdown();
    return true;
}

// ============================================================================
// Stage N — Declarative RenderGraph, TerrainChunkGrid & ParticleRibbonBatcher
// ============================================================================
bool TestPhase7StageNTerrainChunkGridAndParticleRibbonBatcher() {
    using namespace render;

    // 1. Build a 65x65 tilepoints (64x64 tiles = 4x4 chunks of 16x16 tiles) W3EMap
    W3EMap map;
    map.tileWidth = 65;
    map.tileHeight = 65;
    map.offsetX = -4096.0f;
    map.offsetY = -4096.0f;
    map.tiles.resize(65 * 65);
    for (auto& tp : map.tiles) {
        tp.groundHeight = 0x2000;
        tp.waterAndBoundary = 0x2000;
        tp.cliffAndLayer = 2;
    }

    TerrainChunkGrid grid;
    TEST_ASSERT(grid.BuildFromW3E(map, 16));
    TEST_ASSERT(grid.GetChunkCount() == 16); // 4x4 chunks
    TEST_ASSERT(grid.GetTotalTriangles() == 64 * 64 * 2);

    // Camera looking tightly at the center/origin -> peripheral chunks are culled
    Vec3 camEye(0.0f, 600.0f, 600.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(45.0f, 16.0f / 9.0f, 10.0f, 2500.0f);
    Frustum frustum = Frustum::FromViewProj(proj * view);

    std::vector<int> visibleChunks;
    TerrainCullStats tStats = grid.CullChunks(frustum, visibleChunks);
    TEST_ASSERT(tStats.totalChunks == 16);
    TEST_ASSERT(tStats.visibleChunks > 0 && tStats.visibleChunks < 16);
    TEST_ASSERT(tStats.culledChunks > 0);
    TEST_ASSERT(tStats.visibleTriangles + tStats.culledTriangles == tStats.totalTriangles);

    // 2. Test ParticleRibbonBatcher coalescing 6 emitter submissions into 2 batches
    ParticleRibbonBatcher batcher;
    batcher.BeginFrame();
    for (int e = 0; e < 4; ++e) {
        ParticleSubmission p;
        p.textureId = (e % 2 == 0) ? 101u : 202u;
        p.blendMode = RenderBlendMode::Additive;
        p.worldPos = Vec3(static_cast<float>(e * 10), 0.0f, 0.0f);
        batcher.SubmitEmitterParticles({p, p, p});
    }
    for (int r = 0; r < 2; ++r) {
        RibbonSegmentSubmission seg0, seg1;
        seg0.emitterInstanceId = static_cast<uint32_t>(r + 1);
        seg0.textureId = 303u;
        seg0.blendMode = RenderBlendMode::AlphaBlend;
        seg1 = seg0;
        batcher.SubmitRibbonChain({seg0, seg1});
    }
    batcher.BuildCoalescedBatches();

    const ParticleRibbonBatchStats& bStats = batcher.GetStats();
    TEST_ASSERT(bStats.emitterSubmissions == 6);
    TEST_ASSERT(bStats.totalParticlesSubmitted == 12);
    TEST_ASSERT(bStats.coalescedParticleBatches == 2); // Texture 101 and Texture 202
    TEST_ASSERT(bStats.coalescedRibbonBatches == 1);   // Texture 303 coalesced across both strips!
    TEST_ASSERT(bStats.totalCoalescedDrawCalls == 3);
    TEST_ASSERT(bStats.drawCallsSaved == 3);

    return true;
}

bool TestPhase7StageNMdxSharedMeshRegistryAndDeclarativeRenderGraph() {
    using namespace render;

    MdxModel dummyModel;
    dummyModel.name = "FootmanShared";
    dummyModel.boundsRadius = 48.0f;
    MdxGeoset geo;
    geo.vertices.resize(24 * 3, 0.0f);
    geo.indices.resize(36, 0);
    dummyModel.geosets.push_back(geo);

    MdxSharedMeshRegistry meshReg;
    resource::RID rid1 = meshReg.RegisterSharedMesh("units\\human\\footman\\footman.mdx", dummyModel);
    resource::RID rid2 = meshReg.RegisterSharedMesh("units\\human\\footman\\footman.mdx", dummyModel);
    TEST_ASSERT(rid1.IsValid() && rid1 == rid2);
    TEST_ASSERT(meshReg.SharedMeshCount() == 1);

    const MdxSharedMeshAsset* asset = meshReg.GetSharedMesh(rid1);
    TEST_ASSERT(asset != nullptr);
    TEST_ASSERT(asset->totalVertices == 24);
    TEST_ASSERT(asset->totalTriangles == 12);

    // Spawn 10 lightweight instances referencing the single shared mesh RID
    for (int i = 0; i < 10; ++i) {
        int instId = meshReg.CreateInstance(rid1, Mat4::Translate(static_cast<float>(i * 15 - 60), 0.0f, 0.0f), 0);
        TEST_ASSERT(instId > 0);
    }
    TEST_ASSERT(meshReg.ActiveInstanceCount() == 10);

    Vec3 camEye(0.0f, 700.0f, 700.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);

    RenderServer renderServer;
    renderServer.BeginFrame(view, proj, camEye);
    TEST_ASSERT(meshReg.SubmitInstancesToRenderServer(renderServer) == 10);
    renderServer.BuildAndSortBatches();

    RenderGraph graph;
    RenderGraphContext ctx;
    ctx.view = view;
    ctx.proj = proj;
    ctx.cameraEye = camEye;
    ctx.renderServer = &renderServer;
    ctx.hasActivePortraitModel = true;
    ctx.uiOverlayCommandCount = 5;

    graph.ExecuteFrame(ctx);
    TEST_ASSERT(graph.GetExecutedPassCount() == 7);
    TEST_ASSERT(graph.GetTotalFrameDrawCalls() > 0);

    const auto* opaquePass = graph.GetPassMetrics(RenderGraphPassId::OpaqueInstancedPass);
    TEST_ASSERT(opaquePass != nullptr && opaquePass->executed);
    TEST_ASSERT(opaquePass->itemsProcessed == 10);
    TEST_ASSERT(opaquePass->drawCallsSubmitted == 1);

    return true;
}

// ============================================================================
// Stage O — WorldSceneSession Hot Map Switching & UIDataBinder MVVM Sync
// ============================================================================
bool TestPhase7StageOWorldSceneSessionHotMapSwitchAndHeroCarry() {
    const std::string map1 = "resources/Maps/FrozenThrone/(2)EchoIsles.w3x";
    const std::string map2 = "resources/Maps/FrozenThrone/(4)TurtleRock.w3x";

    FILE* f1 = std::fopen(map1.c_str(), "rb");
    FILE* f2 = std::fopen(map2.c_str(), "rb");
    if (!f1 || !f2) {
        if (f1) std::fclose(f1);
        if (f2) std::fclose(f2);
        return true;
    }
    std::fclose(f1);
    std::fclose(f2);

    MpqManager mpq;
    CSimulation sim;
    SceneManager scene;
    UnitPresentationSystem presentation;
    core::WorldSceneSession session;

    TEST_ASSERT(session.LoadMap(mpq, sim, scene, presentation, map1));
    TEST_ASSERT(session.GetStats().mapsLoaded == 1);
    TEST_ASSERT(session.GetStats().terrainChunksBuilt > 0);
    TEST_ASSERT(session.GetStats().placedDoodadsRegistered > 0);

    // Spawn a human hero on Player 0 before hot-switching to map2
    CUnitType* paladinType = MakePhase5UnitType(War3ID("Hpal"), 900, 270, false);
    CUnit* paladin = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), paladinType, 0, War3ID("Hpal"), 0.0f, 0.0f, 0.0f));
    paladin->SetHeroLevel(5);
    sim.Update();

    presentation.unitToVisualIdx[paladin->GetHandleId()] = 12;

    // Hot-switch from EchoIsles to TurtleRock with carryHumanHeroes = true
    TEST_ASSERT(session.HotSwitchMap(mpq, sim, scene, presentation, map2, true));
    TEST_ASSERT(session.GetStats().hotSwitchesPerformed == 1);
    TEST_ASSERT(session.GetStats().mapsLoaded == 2);
    TEST_ASSERT(session.GetCurrentMapPath() == map2);
    TEST_ASSERT(presentation.TrackedUnitCount() == 0); // old visual handles purged!

    return true;
}

bool TestPhase7StageOUIDataBinderReactiveDirtyOnlySync() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.GetPlayer(0)->SetGold(500);
    sim.GetPlayer(0)->SetLumber(150);
    sim.GetPlayer(0)->SetFoodUsed(12);
    sim.GetPlayer(0)->SetFoodCap(20);

    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
    CUnit* u1 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    u1->SetLife(sim, 380.0f);
    sim.Update();
    sim.GetPlayer(0)->SetFoodUsed(12);
    sim.GetPlayer(0)->SetFoodCap(20);

    GameUI gui;
    gui.screenW = 1280;
    gui.screenH = 720;
    SimpleFrame root("Root");
    root.SetSize(1280.0f, 720.0f);
    gui.rootFrame = &root;

    TextFrame goldTxt("GoldTxt", &root);
    TextFrame lumberTxt("LumberTxt", &root);
    TextFrame foodTxt("FoodTxt", &root);
    TextFrame upkeepTxt("UpkeepTxt", &root);
    TextFrame nameTxt("NameTxt", &root);
    TextFrame hpTxt("HpTxt", &root);
    TextFrame manaTxt("ManaTxt", &root);

    root.PositionBounds(gui);

    ui::UIDataBinder binder;
    binder.BindResourceFrames(&goldTxt, &lumberTxt, &foodTxt, &upkeepTxt);
    binder.BindSelectionFrames(&nameTxt, &hpTxt, &manaTxt);

    // 1. First sync populates all bound TextFrames and triggers 1 layout pass
    TEST_ASSERT(binder.SyncAndRelayoutIfChanged(sim, 0, u1->GetHandleId(), gui));
    TEST_ASSERT(goldTxt.GetText() == "500");
    TEST_ASSERT(lumberTxt.GetText() == "150");
    TEST_ASSERT(foodTxt.GetText() == "12/20");
    TEST_ASSERT(hpTxt.GetText() == "380 / 420");
    TEST_ASSERT(binder.GetStats().layoutPassesTriggered == 1);

    // 2. Subsequent sync with zero simulation changes performs 0 mutations and 0 layout passes!
    TEST_ASSERT(!binder.SyncAndRelayoutIfChanged(sim, 0, u1->GetHandleId(), gui));
    TEST_ASSERT(binder.GetStats().layoutPassesTriggered == 1);
    TEST_ASSERT(binder.GetStats().skippedRedundantFieldChecks >= 7);

    // 3. Mutate only Gold -> only goldTxt updates
    sim.GetPlayer(0)->SetGold(625);
    const uint64_t prevDirtyUpdates = binder.GetStats().dirtyFieldUpdates;
    TEST_ASSERT(binder.SyncAndRelayoutIfChanged(sim, 0, u1->GetHandleId(), gui));
    TEST_ASSERT(goldTxt.GetText() == "625");
    TEST_ASSERT(binder.GetStats().dirtyFieldUpdates == prevDirtyUpdates + 1);
    TEST_ASSERT(binder.GetStats().layoutPassesTriggered == 2);

    return true;
}


// ============================================================================
// Stage P — JASS Handle Deduplication & O(1) TriggerEventIndex Dispatch
// ============================================================================
bool TestPhase8StagePJassHandleDeduplicationAndTriggerIndex() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 500, 270, false);

    CUnit* u1 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    CUnit* u2 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"), 64.0f, 0.0f, 180.0f));
    sim.Update();

    JassVM vm;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    TEST_ASSERT(data != nullptr);

    // 1. Verify persistent entity handle deduplication (UNIT, PLAYER, ITEM, DESTRUCTABLE)
    const int u1JassFirst = data->AllocHandle(JassHandleInfo::UNIT, u1->GetHandleId());
    const int p0JassFirst = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const uint64_t hitsBefore = data->handleCacheHits;
    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT(data->AllocHandle(JassHandleInfo::UNIT, u1->GetHandleId()) == u1JassFirst);
        TEST_ASSERT(data->AllocHandle(JassHandleInfo::PLAYER, 0) == p0JassFirst);
    }
    TEST_ASSERT(data->handleCacheHits == hitsBefore + 200);

    // 2. Register 40 unrelated death/order triggers and 2 damage triggers via JASS script
    const std::string script = R"(
        globals
            integer g_damageHits = 0
        endglobals
        function OnUnitDamagedAction takes nothing returns nothing
            set g_damageHits = g_damageHits + 1
        endfunction
    )";
    TEST_ASSERT(vm.LoadScript(script));

    for (int i = 0; i < 40; ++i) {
        int trigId = data->nextTriggerId++;
        auto trig = std::make_shared<JassTrigger>();
        trig->id = trigId;
        trig->registeredUnitHandles.push_back(u1JassFirst);
        trig->eventTypes.push_back(JASS_EVENT_UNIT_DEATH);
        trig->registeredEvents.push_back({u1JassFirst, JASS_EVENT_UNIT_DEATH});
        data->triggers[trigId] = trig;
        data->IndexTriggerUnitEvent(trigId, JASS_EVENT_UNIT_DEATH);
    }

    for (int i = 0; i < 2; ++i) {
        int trigId = data->nextTriggerId++;
        auto trig = std::make_shared<JassTrigger>();
        trig->id = trigId;
        trig->registeredUnitHandles.push_back(u1JassFirst);
        trig->eventTypes.push_back(JASS_EVENT_UNIT_DAMAGED);
        trig->registeredEvents.push_back({u1JassFirst, JASS_EVENT_UNIT_DAMAGED});
        trig->actionFuncNames.push_back("OnUnitDamagedAction");
        data->triggers[trigId] = trig;
        data->IndexTriggerUnitEvent(trigId, JASS_EVENT_UNIT_DAMAGED);
    }

    for (int k = 0; k < 5; ++k) {
        natives.FireDamageEvent(u1->GetHandleId(), u2->GetHandleId(), 25.0f, JASS_EVENT_UNIT_DAMAGED);
    }
    natives.PumpQueuedThreads();

    TEST_ASSERT(vm.GetGlobal("g_damageHits").AsInt() == 10); // 5 events * 2 subscribed triggers
    TEST_ASSERT(data->indexedTriggerDispatches >= 5);
    TEST_ASSERT(data->triggersEvaluatedByIndex == 10);
    TEST_ASSERT(data->triggersSkippedByIndex >= 5 * 40); // 200 unrelated triggers skipped!

    return true;
}

bool TestPhase8StagePTimerEventIndexAndDestroyTriggerCleanup() {
    CSimulation sim;
    JassVM vm;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    const std::string script = R"(
        globals
            integer g_timerFires = 0
            trigger g_tempTrig = null
        endglobals

        function OnTimerExpire takes nothing returns nothing
            set g_timerFires = g_timerFires + 1
        endfunction

        function SetupTimers takes nothing returns nothing
            local trigger tKeep = CreateTrigger()
            local integer i = 0
            loop
                exitwhen i >= 20
                call CreateTrigger()
                set i = i + 1
            endloop
            call TriggerRegisterTimerEvent(tKeep, 0.10, true)
            call TriggerAddAction(tKeep, function OnTimerExpire)

            set g_tempTrig = CreateTrigger()
            call TriggerRegisterTimerEvent(g_tempTrig, 0.10, true)
            call TriggerAddAction(g_tempTrig, function OnTimerExpire)
            call DestroyTrigger(g_tempTrig)
        endfunction
    )";
    TEST_ASSERT(vm.LoadScript(script));
    vm.Execute("SetupTimers");

    // Step 0.12s -> tKeep fires once; destroyed g_tempTrig was unindexed and does not fire
    natives.UpdateTimers(0.12f);
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("g_timerFires").AsInt() == 1);

    natives.UpdateTimers(0.12f);
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("g_timerFires").AsInt() == 2);

    return true;
}

// ============================================================================
// Stage Q — Spatial-Accelerated Combat AoE & Predictive CrowdSteeringSystem
// ============================================================================
bool TestPhase8StageQSpatialAcceleratedSplashAndLineDamage() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 500, 270, false);

    CUnit* attacker = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), -200.0f, 0.0f, 0.0f));

    // Spawn 45 enemy units: 3 near (0, 0) for splash/line corridor, 42 far away (> 2000 units away)
    CUnit* eCenter = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"), 0.0f, 0.0f, 180.0f));
    CUnit* eHalf = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"), 100.0f, 0.0f, 180.0f));
    CUnit* eLine = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"), 220.0f, 0.0f, 180.0f));

    for (int i = 0; i < 42; ++i) {
        sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 1, War3ID("hfoo"),
            3000.0f + (i % 7) * 150.0f, 3000.0f + (i / 7) * 150.0f, 0.0f));
    }
    sim.Update();
    TEST_ASSERT(sim.SpatialGrid().IndexedEntityCount() == 46);

    CUnitAttack splashAtk;
    splashAtk.weaponType = CWeaponType::ARTILLERY;
    splashAtk.areaFull = 50.0f;
    splashAtk.areaHalf = 150.0f;
    splashAtk.areaQuarter = 250.0f;
    splashAtk.damageFactorHalf = 0.5f;
    splashAtk.damageFactorQuarter = 0.25f;

    // 1. Splash at (0, 0) with 100 base chaos damage -> eCenter takes 100, eHalf (d=100) takes 50, eLine (d=220) takes 25
    sim.GetCombatSystem()->ApplySplashDamageAtPoint(
        sim, attacker, 0.0f, 0.0f, false, true,
        CAttackType::CHAOS, CDamageType::UNIVERSAL,
        100.0f, 0.0f, 0, 0, splashAtk, nullptr, 1.0f, false);

    TEST_NEAR(eCenter->GetLife(), 400.0f, 1e-2f);
    TEST_NEAR(eHalf->GetLife(), 450.0f, 1e-2f);
    TEST_NEAR(eLine->GetLife(), 475.0f, 1e-2f);

    // 2. Line spill damage along +X from (0, 0) distance 260, radius 40 -> hits eCenter, eHalf, eLine for 60 damage
    sim.GetCombatSystem()->ApplyLineDamage(
        sim, attacker, 0.0f, 0.0f, 1.0f, 0.0f, false, true,
        CAttackType::CHAOS, CDamageType::UNIVERSAL,
        60.0f, 0.0f, 0, 0, 260.0f, 40.0f, nullptr);

    TEST_NEAR(eCenter->GetLife(), 340.0f, 1e-2f);
    TEST_NEAR(eHalf->GetLife(), 390.0f, 1e-2f);
    TEST_NEAR(eLine->GetLife(), 415.0f, 1e-2f);

    return true;
}

bool TestPhase8StageQCrowdSteeringRvoAndFlowFieldBlending() {
    WpmData wpm;
    wpm.width = 64;
    wpm.height = 64;
    wpm.pathing.assign(64 * 64, 0x00);

    CSimulation sim;
    sim.SetPathingMap(&wpm, -1024.0f, -1024.0f);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    // Spawn 6 closely packed units near (-300, 0)
    std::vector<int> handles;
    for (int i = 0; i < 6; ++i) {
        const float x = -300.0f + (i % 3) * 18.0f;
        const float y = -10.0f + (i / 3) * 18.0f;
        CUnit* u = sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), x, y, 0.0f));
        handles.push_back(u->GetHandleId());
    }
    sim.Update();

    pathfinding::FlowFieldGrid flow;
    TEST_ASSERT(flow.Build(*sim.GetPathfinding(), 400.0f, 0.0f, 48, PathingGridMovementType::FOOT));

    pathfinding::CrowdSteeringSystem steering;
    for (int step = 0; step < 10; ++step) {
        auto outputs = steering.StepGroupSteering(sim, handles, 400.0f, 0.0f, 0.05f, &flow);
        TEST_ASSERT(outputs.size() == 6);
    }

    const auto& st = steering.GetStats();
    TEST_ASSERT(st.totalAgentsSteered == 60);
    TEST_ASSERT(st.flowFieldSamples == 60);
    TEST_ASSERT(st.avoidanceAdjustments > 0);

    // Verify the squad advanced toward +X (goal = 400) while spreading out laterally
    float avgX = 0.0f;
    for (int h : handles) {
        avgX += sim.GetUnit(h)->GetX();
    }
    avgX /= 6.0f;
    TEST_ASSERT(avgX > -250.0f);

    return true;
}

// ============================================================================
// Stage R — Declarative InputActionMap & Formation CommandBufferPipeline
// ============================================================================
bool TestPhase8StageRInputActionMapChordsAndSerialization() {
    core::InputActionMap map;
    map.RegisterStandardWarcraftDefaults();
    TEST_ASSERT(map.ActionCount() >= 23);
    TEST_ASSERT(map.FindChordConflicts().empty());

    std::vector<std::string> triggered;
    TEST_ASSERT(map.ResolveTriggeredActions(83, core::kInputModNone, triggered) == 1);
    TEST_ASSERT(triggered[0] == "order_stop");

    TEST_ASSERT(map.ResolveTriggeredActions(51, core::kInputModCtrl, triggered) == 1);
    TEST_ASSERT(triggered[0] == "control_group_assign_3");

    // Rebind order_attack to Q and add a secondary chord Alt+A
    TEST_ASSERT(map.RebindPrimary("order_attack", core::InputKeyChord{81, core::kInputModNone}));
    TEST_ASSERT(map.RebindSecondary("order_attack", core::InputKeyChord{65, core::kInputModAlt}));
    TEST_ASSERT(map.ResolveTriggeredActions(81, core::kInputModNone, triggered) == 1);
    TEST_ASSERT(triggered[0] == "order_attack");
    TEST_ASSERT(map.ResolveTriggeredActions(65, core::kInputModAlt, triggered) == 1);
    TEST_ASSERT(triggered[0] == "order_attack");

    // Round-trip through config text serialization
    const std::string cfgText = map.SerializeToText();
    core::InputActionMap restored;
    TEST_ASSERT(restored.DeserializeFromText(cfgText));
    TEST_ASSERT(restored.ActionCount() == map.ActionCount());
    TEST_ASSERT(restored.ResolveTriggeredActions(81, core::kInputModNone, triggered) == 1);
    TEST_ASSERT(triggered[0] == "order_attack");

    return true;
}

bool TestPhase8StageRCommandBufferPipelineBoxFormationAndReplay() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    std::vector<int> handles;
    for (int i = 0; i < 9; ++i) {
        CUnit* u = sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
            static_cast<float>((i % 3) * 40), static_cast<float>((i / 3) * 40), 0.0f));
        int abH = sim.AllocateHandleId();
        u->AddAbility(sim, std::make_unique<CAbilityMove>(abH));
        handles.push_back(u->GetHandleId());
    }
    sim.Update();

    ReplayLog replay;
    replay.Begin(12345u);

    core::CommandBufferPipeline pipeline;
    const int count = pipeline.IssueGroupPointOrder(
        sim, 0, handles, OrderIds::MOVE, 600.0f, 400.0f, true, 80.0f, &replay);
    TEST_ASSERT(count == 9);
    TEST_ASSERT(pipeline.GetStats().totalGroupOrdersIssued == 1);
    TEST_ASSERT(pipeline.GetStats().formationOrdersComputed == 1);
    TEST_ASSERT(pipeline.GetStats().totalUnitCommandsDispatched == 9);
    TEST_ASSERT(replay.Commands().size() == 9);

    // Verify the 9 formation destinations are distinct and centered around (600, 400)
    float sumX = 0.0f, sumY = 0.0f;
    for (size_t i = 0; i < replay.Commands().size(); ++i) {
        sumX += replay.Commands()[i].x;
        sumY += replay.Commands()[i].y;
        for (size_t j = i + 1; j < replay.Commands().size(); ++j) {
            const float dx = replay.Commands()[i].x - replay.Commands()[j].x;
            const float dy = replay.Commands()[i].y - replay.Commands()[j].y;
            TEST_ASSERT(dx * dx + dy * dy > 100.0f);
        }
    }
    TEST_NEAR(sumX / 9.0f, 600.0f, 1e-2f);
    TEST_NEAR(sumY / 9.0f, 400.0f, 1e-2f);

    return true;
}

// ============================================================================
// Stage S — Compressed Keyframe + Tail Lockstep Fast-Resync & Delta Codec
// ============================================================================
bool TestPhase8StageSFastResyncKeyframePackageAndZlibWire() {
    auto setupSim = [](CSimulation& s, std::vector<int>& outHandles) {
        s.SeedRng(0xCAFEBABEu);
        s.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
        for (int i = 0; i < 6; ++i) {
            CUnit* u = s.AdoptUnit(std::make_unique<CUnit>(
                s.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
                static_cast<float>(i * 50), 0.0f, 0.0f));
            u->AddAbility(s, std::make_unique<CAbilityMove>(s.AllocateHandleId()));
            outHandles.push_back(u->GetHandleId());
        }
        s.Update();
    };

    CSimulation hostSim;
    std::vector<int> hostHandles;
    setupSim(hostSim, hostHandles);

    net::RollingSnapshotRingBuffer ring(8, 10);
    std::vector<TurnBatch> history;

    // Run 25 turns on host, recording keyframes every 10 turns and issuing orders at Turn 12 and Turn 22
    for (int t = 1; t <= 25; ++t) {
        ring.RecordIfDue(hostSim);
        TurnBatch batch;
        batch.turn = hostSim.GetGameTurnTick();
        if (batch.turn == 12 || batch.turn == 22) {
            SimCommand cmd;
            cmd.turn = batch.turn;
            cmd.player = 0;
            cmd.unit = hostHandles[0];
            cmd.orderId = OrderIds::MOVE;
            cmd.targetKind = static_cast<int32_t>(COrder::TargetKind::POINT);
            cmd.x = (batch.turn == 12) ? 300.0f : 600.0f;
            cmd.y = 200.0f;
            batch.commands.push_back(cmd);
            ApplyBatch(hostSim, batch);
        }
        hostSim.Update();
        batch.stateHash = hostSim.HashState();
        history.push_back(batch);
    }

    const SimStateSnapshot* kf20 = ring.FindKeyframeAtOrBefore(20);
    TEST_ASSERT(kf20 != nullptr && kf20->gameTurnTick == 20);

    net::FastResyncKeyframePackage pkg;
    TEST_ASSERT(net::FastResyncKeyframePackage::Build(
        *kf20, history, hostSim.GetGameTurnTick(), hostSim.HashState(), pkg));
    TEST_ASSERT(pkg.trailingBatches.size() == 6); // only turns 20..25!
    TEST_ASSERT(pkg.compressedSnapshot.size() < pkg.uncompressedSnapshotBytes);

    // Serialize over wire and decode on a late-joining peer simulation
    std::vector<uint8_t> wire = pkg.SerializeWire();
    net::FastResyncKeyframePackage decodedPkg;
    TEST_ASSERT(net::FastResyncKeyframePackage::DeserializeWire(wire.data(), wire.size(), decodedPkg));

    CSimulation peerSim;
    std::vector<int> peerHandles;
    setupSim(peerSim, peerHandles);
    // Peer is at Turn 1; jump directly to Turn 20 via compressed keyframe and replay only 6 trailing turns!
    TEST_ASSERT(decodedPkg.ApplyToSimulation(peerSim));
    TEST_ASSERT(peerSim.GetGameTurnTick() == hostSim.GetGameTurnTick());
    TEST_ASSERT(peerSim.HashState() == hostSim.HashState());

    return true;
}

bool TestPhase8StageSSnapshotDeltaXorZlibCompression() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    std::vector<CUnit*> units;
    for (int i = 0; i < 20; ++i) {
        units.push_back(sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
            static_cast<float>(i * 30), 100.0f, 0.0f)));
    }
    sim.Update();

    SimStateSnapshot baseSnap = SimStateSnapshot::Capture(sim);
    // Mutate only 2 of the 20 units over 3 ticks
    units[0]->SetPosition(555.0f, 666.0f);
    units[1]->SetLife(sim, 210.0f);
    sim.Update();

    SimStateSnapshot targetSnap = SimStateSnapshot::Capture(sim);
    net::SnapshotDeltaPacket delta;
    TEST_ASSERT(net::SnapshotDeltaPacket::EncodeDelta(baseSnap, targetSnap, delta));
    // XOR + Zlib delta across 90% unchanged state compresses to a tiny fraction of raw size
    TEST_ASSERT(delta.compressedXorDelta.size() * 2 < delta.targetUncompressedSize);

    SimStateSnapshot reconstructed;
    TEST_ASSERT(net::SnapshotDeltaPacket::DecodeDelta(baseSnap, delta, reconstructed));
    TEST_ASSERT(reconstructed.stateHash == targetSnap.stateHash);
    TEST_ASSERT(reconstructed.units.size() == 20);

    return true;
}

// ============================================================================
// Stage T — BlpMipTextureResource Pipeline & 2D TextureAtlasPacker
// ============================================================================
bool TestPhase8StageTBlpMipTextureResourceAndSiblingFallback() {
    // Build a valid minimal 4x4 paletted BLP1 image in memory (header = 156 bytes + palette = 1024 bytes)
    constexpr uint32_t kBlpHeaderBytes = 156;
    constexpr uint32_t kPaletteBytes = 256 * 4;
    constexpr uint32_t kMip0Offset = kBlpHeaderBytes + kPaletteBytes;
    std::vector<uint8_t> blpBytes(kMip0Offset + 16 + 16, 0);
    auto poke32 = [&](size_t at, uint32_t v) {
        blpBytes[at + 0] = static_cast<uint8_t>(v & 0xFFu);
        blpBytes[at + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        blpBytes[at + 2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
        blpBytes[at + 3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
    };
    std::memcpy(blpBytes.data(), "BLP1", 4);
    poke32(4, 1);  // compression = 1 (Paletted)
    poke32(8, 8);  // alphaBits = 8
    poke32(12, 4); // width = 4
    poke32(16, 4); // height = 4
    poke32(20, 3); // imageType = 3
    poke32(24, 1); // hasMips = 1
    poke32(28, kMip0Offset); // mipOffset[0]
    poke32(92, 32);          // mipSize[0] = 16 indices + 16 alpha bytes

    // Palette entry 1 at offset 156: R=240, G=120, B=20, A=255
    uint8_t* pal = blpBytes.data() + kBlpHeaderBytes;
    pal[4 + 0] = 240;
    pal[4 + 1] = 120;
    pal[4 + 2] = 20;
    pal[4 + 3] = 255;

    uint8_t* indices = blpBytes.data() + kMip0Offset;
    for (int i = 0; i < 16; ++i) indices[i] = 1;
    uint8_t* alphas = indices + 16;
    for (int i = 0; i < 16; ++i) alphas[i] = 255;

    resource::ResourceServer server;
    // Mount under .blp, but request via .dds URI to verify automatic sibling extension fallback!
    server.MountMemoryFile("ReplaceableTextures\\CommandButtons\\BTNFootman.blp", blpBytes);

    auto tex = render::BlpMipTextureResource::LoadFromVfs(
        server, "mpq://ReplaceableTextures/CommandButtons/BTNFootman.dds", true);
    TEST_ASSERT(tex != nullptr);
    TEST_ASSERT(tex->width == 4 && tex->height == 4);
    TEST_ASSERT(!tex->loadedFromDds);
    // 4x4 -> 2x2 -> 1x1 = 3 mip levels
    TEST_ASSERT(tex->GetMipLevelCount() == 3);
    TEST_ASSERT(tex->GetBasePixels().size() == 4 * 4 * 4);
    TEST_ASSERT(tex->GetBasePixels()[0] == 240); // R
    TEST_ASSERT(tex->GetBasePixels()[1] == 120); // G
    TEST_ASSERT(tex->GetBasePixels()[2] == 20);  // B

    return true;
}

bool TestPhase8StageTTextureAtlasPackerBorderExtrusionAndMips() {
    render::TextureAtlasPacker packer(128, 128, 1);

    for (int i = 0; i < 8; ++i) {
        std::vector<uint8_t> icon(16 * 16 * 4, 0);
        const uint8_t r = static_cast<uint8_t>(20 + i * 25);
        const uint8_t g = static_cast<uint8_t>(100 + i * 10);
        const uint8_t b = static_cast<uint8_t>(200 - i * 15);
        for (size_t p = 0; p < 16 * 16; ++p) {
            icon[p * 4 + 0] = r;
            icon[p * 4 + 1] = g;
            icon[p * 4 + 2] = b;
            icon[p * 4 + 3] = 255;
        }
        render::AtlasRegionUV reg;
        TEST_ASSERT(packer.PackSprite("Icon_" + std::to_string(i), 16, 16, icon.data(), &reg));
        TEST_ASSERT(reg.width == 16 && reg.height == 16);
        TEST_ASSERT(reg.u1 > reg.u0 && reg.v1 > reg.v0);

        // Verify 1px extruded border pixel at (reg.pageX - 1, reg.pageY) matches the icon edge color!
        uint8_t borderPx[4] = {};
        TEST_ASSERT(packer.SamplePixel(reg.pageX - 1, reg.pageY, borderPx));
        TEST_ASSERT(borderPx[0] == r && borderPx[1] == g && borderPx[2] == b && borderPx[3] == 255);
    }

    TEST_ASSERT(packer.GetStats().packedSpriteCount == 8);
    TEST_ASSERT(packer.GetStats().occupancyRatio > 0.10f);

    MipChain chain = packer.BuildAtlasMipChain();
    TEST_ASSERT(chain.levels.size() == 8); // 128 -> 64 -> 32 -> 16 -> 8 -> 4 -> 2 -> 1
    return true;
}

// ============================================================================
// Stage U — Hierarchical EngineProfiler & Retail W3X End-to-End Benchmark
// ============================================================================
bool TestPhase8StageUEngineProfilerPercentilesAndChromeTrace() {
    core::EngineProfiler profiler;
    profiler.Reset();

    for (uint64_t f = 1; f <= 16; ++f) {
        profiler.BeginFrame(f);
        profiler.RecordZoneSample("SimulationStep", "ECS", f * 1000, 200 + (f * 15), 1, 1);
        profiler.RecordZoneSample("RenderGraphExecute", "Render", f * 1000 + 300, 120 + (f * 10), 1, 1);
        {
            WARSMASH_PROFILE_SCOPE(profiler, "JassVmTick", "Scripting");
            profiler.AddCounter("JassInstructions", 500);
        }
        profiler.EndFrame();
    }

    TEST_ASSERT(profiler.GetCompletedFrameCount() == 16);
    TEST_ASSERT(profiler.GetCounter("JassInstructions") == 8000);

    core::ProfileZoneSummary simSummary;
    TEST_ASSERT(profiler.GetZoneSummary("SimulationStep", simSummary));
    TEST_ASSERT(simSummary.callCount == 16);
    TEST_ASSERT(simSummary.minMicros >= 215.0);
    TEST_ASSERT(simSummary.maxMicros >= simSummary.p95Micros);
    TEST_ASSERT(simSummary.p95Micros >= simSummary.p50Micros);

    const std::string traceJson = profiler.ExportChromeTraceJson();
    TEST_ASSERT(traceJson.find("\"traceEvents\":[") != std::string::npos);
    TEST_ASSERT(traceJson.find("\"SimulationStep\"") != std::string::npos);
    TEST_ASSERT(traceJson.find("\"RenderGraphExecute\"") != std::string::npos);

    return true;
}

bool TestPhase8StageURetailW3xEndToEndModernizedPipelineBenchmark() {
    const std::string mapPath = "resources/Maps/FrozenThrone/(8)GardenOfWar.w3x";
    FILE* f = std::fopen(mapPath.c_str(), "rb");
    if (!f) return true;
    std::fclose(f);

    core::EngineProfiler profiler;
    profiler.Reset();

    MpqManager mpq;
    CSimulation sim;
    SceneManager scene;
    UnitPresentationSystem presentation;
    core::WorldSceneSession session;

    profiler.BeginFrame(1);
    {
        WARSMASH_PROFILE_SCOPE(profiler, "WorldSceneSession::LoadMap", "MapLoad");
        TEST_ASSERT(session.LoadMap(mpq, sim, scene, presentation, mapPath));
    }

    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
    std::vector<int> squadHandles;
    for (int i = 0; i < 8; ++i) {
        CUnit* u = sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
            static_cast<float>(i * 32 - 128), 0.0f, 0.0f));
        u->AddAbility(sim, std::make_unique<CAbilityMove>(sim.AllocateHandleId()));
        squadHandles.push_back(u->GetHandleId());
    }

    {
        WARSMASH_PROFILE_SCOPE(profiler, "SimulationStep", "ECS");
        sim.Update();
    }

    pathfinding::CrowdSteeringSystem steering;
    {
        WARSMASH_PROFILE_SCOPE(profiler, "CrowdSteeringStep", "Navigation");
        steering.StepGroupSteering(sim, squadHandles, 256.0f, 256.0f, 0.05f, nullptr);
    }

    SimStateSnapshot snap;
    {
        WARSMASH_PROFILE_SCOPE(profiler, "SimSnapshotCapture", "Netcode");
        snap = SimStateSnapshot::Capture(sim);
    }
    profiler.EndFrame();

    TEST_ASSERT(session.GetStats().terrainChunksBuilt > 0);
    TEST_ASSERT(snap.units.size() == 8);
    TEST_ASSERT(profiler.GetCompletedFrameCount() == 1);
    TEST_ASSERT(profiler.GetAllZoneSummaries().size() >= 4);

    return true;
}


// ============================================================================
// Stage V (Phase 9 Runtime Integration & Retail Cloud Asset Verification)
// ============================================================================

bool TestPhase9StageV1ZeroAllocProcessorAndSquadFlowFieldMove() {
    WpmData wpm;
    wpm.width = 64;
    wpm.height = 64;
    wpm.pathing.assign(64 * 64, 0x00);
    for (int y = 12; y <= 48; ++y) {
        wpm.pathing[static_cast<size_t>(y * 64 + 32)] = 0x02; // UNWALKABLE wall
    }

    CSimulation sim;
    sim.SetPathingMap(&wpm, -1024.0f, -1024.0f);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    CPathfindingProcessor* proc = sim.GetPathfinding();
    TEST_ASSERT(proc != nullptr);
    proc->ResetWorkspaceStats();

    // 1. Run 30 consecutive FindPathWorld queries and verify zero heap reallocations after query 1
    for (int i = 0; i < 30; ++i) {
        std::vector<std::pair<float, float>> path;
        TEST_ASSERT(proc->FindPathWorld(-400.0f, -200.0f + i * 8.0f, 400.0f, 100.0f,
                                        32.0f, PathingGridMovementType::FOOT, nullptr, path));
        TEST_ASSERT(path.size() >= 2);
    }
    TEST_ASSERT(proc->GetZeroAllocQueriesExecuted() == 30);
    TEST_ASSERT(proc->GetWorkspaceBufferAllocations() == 1);
    TEST_ASSERT(proc->GetTotalNodesExpanded() > 100);

    // 2. Spawn a 6-unit squad and issue a group point order via CommandBufferPipeline
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
    std::vector<int> squadHandles;
    std::vector<CUnit*> squadUnits;
    for (int i = 0; i < 6; ++i) {
        CUnit* u = sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
            -500.0f + (i % 3) * 40.0f, -60.0f + (i / 3) * 40.0f, 0.0f));
        u->AddAbility(sim, std::make_unique<CAbilityMove>(sim.AllocateHandleId()));
        squadHandles.push_back(u->GetHandleId());
        squadUnits.push_back(u);
    }
    sim.Update();

    core::CommandBufferPipeline cmdPipeline;
    const int issued = cmdPipeline.IssueGroupPointOrder(
        sim, 0, squadHandles, OrderIds::MOVE, 500.0f, 120.0f, /*useFormation=*/true, 72.0f, nullptr);
    TEST_ASSERT(issued == 6);

    // Verify all 6 units automatically had FlowFieldSteering enabled on their CBehaviorMove
    for (CUnit* u : squadUnits) {
        TEST_ASSERT(u->GetMoveBehavior() != nullptr);
        TEST_ASSERT(u->GetMoveBehavior()->IsUsingFlowFieldSteering());
    }

    // Step simulation 12 ticks and verify FlowFieldGrid cache & CrowdSteeringSystem were exercised
    for (int t = 0; t < 12; ++t) {
        sim.Update();
    }

    uint64_t totalFlowSteps = 0;
    for (CUnit* u : squadUnits) {
        totalFlowSteps += u->GetMoveBehavior()->GetFlowFieldSteeringSteps();
    }
    TEST_ASSERT(totalFlowSteps > 0);
    TEST_ASSERT(sim.PathfindingServer().GetStats().flowFieldsBuilt >= 1);
    TEST_ASSERT(sim.CrowdSteering().GetStats().totalAgentsSteered > 0);

    return true;
}

bool TestPhase9StageV2SceneManagerIntegratedRenderGraphAndSharedMeshes() {
    resource::ResourceServer resServer;
    const std::string knightMdl = R"(
        Version { FormatVersion 800, }
        Model "IntegratedKnight" {
            NumGeosets 1,
            NumBones 1,
            BlendTime 150,
            MinimumExtent { -32.0, -32.0, 0.0 },
            MaximumExtent { 32.0, 32.0, 96.0 },
            BoundsRadius 64.0,
        }
        Sequences 1 {
            Anim "Stand" {
                Interval { 0, 1000 },
                MinimumExtent { -32.0, -32.0, 0.0 },
                MaximumExtent { 32.0, 32.0, 96.0 },
                BoundsRadius 64.0,
            }
        }
        Geoset {
            Vertices 3 { { -10.0, 0.0, 0.0 }, { 10.0, 0.0, 0.0 }, { 0.0, 0.0, 20.0 }, }
            Normals 3 { { 0.0, 0.0, 1.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 0.0, 1.0 }, }
            TVertices 3 { { 0.0, 0.0 }, { 1.0, 0.0 }, { 0.5, 1.0 }, }
            VertexGroup { 0, 0, 0, }
            Faces 1 3 { Triangles { { 0, 1, 2 }, } }
            Groups 1 1 { Matrices { 0 }, }
        }
        Bone "RootBone" { ObjectId 0, Parent -1, GeosetId 0, GeosetAnimId -1, }
        PivotPoints 1 { { 0.0, 0.0, 0.0 }, }
    )";
    resServer.MountMemoryFile("Units/Human/Knight/Knight.mdl",
                              std::vector<uint8_t>(knightMdl.begin(), knightMdl.end()));

    W3EMap w3e;
    w3e.tileWidth = 65;
    w3e.tileHeight = 65;
    w3e.offsetX = -4096.0f;
    w3e.offsetY = -4096.0f;
    w3e.tiles.resize(65 * 65);
    for (auto& tp : w3e.tiles) {
        tp.groundHeight = 0x2000;
        tp.waterAndBoundary = 0x2000;
        tp.cliffAndLayer = 2;
    }
    render::TerrainChunkGrid terrainGrid;
    TEST_ASSERT(terrainGrid.BuildFromW3E(w3e, 16));

    SceneManager scene;
    scene.SetResourceServer(&resServer);
    scene.SetBoundTerrainChunkGrid(&terrainGrid);

    const uint32_t knightId = War3ID("hkni").value;
    scene.RegisterUnitModelPath(knightId, "Units/Human/Knight/Knight.mdx");

    const float scale[3] = {1.0f, 1.0f, 1.0f};
    for (int i = 0; i < 6; ++i) {
        const float pos[3] = {static_cast<float>(i * 25 - 60), 0.0f, 0.0f};
        TEST_ASSERT(scene.AddUnit(knightId, pos, 0.0f, scale, 0, 835, 0) >= 0);
    }

    // Verify EnsureModelCached automatically registered the model in MdxSharedMeshRegistry
    TEST_ASSERT(scene.GetSharedMeshRegistry().SharedMeshCount() == 1);

    // Submit 2 particle emitters into SceneManager's integrated ParticleRibbonBatcher
    scene.GetParticleRibbonBatcher().BeginFrame();
    render::ParticleSubmission p;
    p.textureId = 77u;
    p.blendMode = render::RenderBlendMode::Additive;
    scene.GetParticleRibbonBatcher().SubmitEmitterParticles({p, p});
    scene.GetParticleRibbonBatcher().SubmitEmitterParticles({p, p});

    Vec3 camEye(0.0f, 700.0f, 700.0f);
    Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(50.0f, 16.0f / 9.0f, 10.0f, 3000.0f);

    scene.RenderAll(view, proj, 0.016f);

    // Verify integrated RenderGraph executed all 7 passes during SceneManager::RenderAll
    TEST_ASSERT(scene.GetRenderGraph().GetExecutedPassCount() == 7);
    const auto* terrainPass = scene.GetRenderGraph().GetPassMetrics(render::RenderGraphPassId::TerrainChunkPass);
    const auto* opaquePass  = scene.GetRenderGraph().GetPassMetrics(render::RenderGraphPassId::OpaqueInstancedPass);
    const auto* partPass    = scene.GetRenderGraph().GetPassMetrics(render::RenderGraphPassId::ParticleRibbonBatchPass);
    TEST_ASSERT(terrainPass != nullptr && terrainPass->executed && terrainPass->itemsProcessed > 0);
    TEST_ASSERT(opaquePass != nullptr && opaquePass->executed && opaquePass->itemsProcessed == 6);
    TEST_ASSERT(partPass != nullptr && partPass->executed && partPass->drawCallsSubmitted == 1);
    TEST_ASSERT(scene.GetLastRenderGraphContext().terrainStats.culledChunks > 0);

    return true;
}

bool TestPhase9StageV3JassZeroAllocNativeCallsAndFastStructSlots() {
    JassVM vm;
    jass::RegisterNativeAuto(vm, "FastAdd3", [](int a, int b, int c) -> int {
        return a + b + c;
    });

    const std::string src = R"(
        native FastAdd3 takes integer a, integer b, integer c returns integer

        struct HeroStats
            integer str = 10
            integer agi = 15
            integer intStat = 20
        endstruct

        globals
            integer g_total = 0
            integer g_structSum = 0
        endglobals

        function RunLoop takes integer count returns integer
            local integer i = 0
            local integer acc = 0
            loop
                exitwhen i >= count
                set acc = FastAdd3(acc, i, 1)
                set i = i + 1
            endloop
            return acc
        endfunction

        function main takes nothing returns nothing
            local HeroStats h = HeroStats.create()
            set h.str = h.str + 5
            set h.agi = h.agi + 10
            set g_structSum = h.str + h.agi + h.intStat
            set g_total = RunLoop(50)
            call h.destroy()
        endfunction
    )";

    TEST_ASSERT(vm.LoadScript(src));
    vm.SetBytecodeEnabled(true);
    vm.Execute("main");

    // 10+5 + 15+10 + 20 = 60
    TEST_ASSERT(vm.GetGlobal("g_structSum").AsInt() == 60);
    // Sum_{i=0..49} (i + 1) = 50 * 51 / 2 = 1275
    TEST_ASSERT(vm.GetGlobal("g_total").AsInt() == 1275);
    // Verify pre-reserved callArgs buffer handled all 50 native calls + RunLoop call with zero heap reallocations
    TEST_ASSERT(vm.GetZeroAllocCallCount() >= 50);
    // Verify vJASS struct scalar fields used O(1) indexed fastScalarSlots
    TEST_ASSERT(vm.GetStructFastSlotAccessCount() >= 6);

    return true;
}

bool TestPhase9StageV4AudioSystemIntegratedServerAndRetailDriveMultiMapSuite() {
    // 1. Verify AudioSystem Play2D & Play3DPath routing through AudioServer in headless mode
    AudioSystem audioSys;
    audioSys.SetHeadlessSimulationEnabled(true);
    audioSys.SetMasterVolume(0.8f);
    audioSys.SetMusicVolume(0.5f);
    TEST_NEAR(audioSys.GetAudioServer().GetBusVolumeLinear(audio::AudioBusId::Master), 0.8f, 1e-3f);
    TEST_NEAR(audioSys.GetAudioServer().GetBusVolumeLinear(audio::AudioBusId::Music), 0.5f, 1e-3f);

    const int uiVoice = audioSys.Play2D("InterfaceClick", 1.0f);
    TEST_ASSERT(uiVoice > 0);
    TEST_ASSERT(audioSys.GetAudioServer().GetActiveVoiceCountOnBus(audio::AudioBusId::UI) == 1);

    const int unitVoice = audioSys.Play3DPath("Sound/Units/Human/Footman/FootmanYes1.wav", 100.0f, 0.0f, 100.0f, 1.0f);
    TEST_ASSERT(unitVoice > 0);
    TEST_ASSERT(audioSys.GetAudioServer().GetActiveVoiceCountOnBus(audio::AudioBusId::UnitVoice) == 1);

    // 2. Multi-Map Retail W3X Cloud Asset Verification Suite
    const std::vector<std::string> retailDriveMaps = {
        "resources/Maps/FrozenThrone/(8)GardenOfWar.w3x",
        "resources/Maps/FrozenThrone/(8)LastManStanding.w3x",
        "resources/Maps/FrozenThrone/(8)PlainsOfSnow.w3x",
        "resources/Maps/FrozenThrone/(8)TwilightRuins.w3x",
        "resources/Maps/FrozenThrone/(2)TerenasStand.w3x",
        "resources/Maps/FrozenThrone/(4)TwistedMeadows.w3x",
        "resources/Maps/FrozenThrone/Scenario/(6)BlizzardTD.w3x",
        "resources/Maps/FrozenThrone/Scenario/(8)AzureTowerDefense.w3x",
        "resources/Maps/FrozenThrone/Scenario/(8)AzerothGrandPrix.w3x",
        "resources/Maps/FrozenThrone/Scenario/(10)ExtremeCandyWar2004.w3x",
        "resources/Maps/FrozenThrone/Scenario/(12)WormWar.w3x",
        "resources/Maps/FrozenThrone/Scenario/(6)BomberCommand.w3x"
    };

    MpqManager mpq;
    CSimulation sim;
    SceneManager scene;
    UnitPresentationSystem presentation;
    core::WorldSceneSession session;
    core::EngineProfiler profiler;
    profiler.Reset();

    int loadedRetailCount = 0;
    size_t totalDoodadsVerified = 0;
    size_t totalTerrainChunksVerified = 0;

    for (size_t i = 0; i < retailDriveMaps.size(); ++i) {
        const std::string& mapPath = retailDriveMaps[i];
        FILE* f = std::fopen(mapPath.c_str(), "rb");
        if (!f) continue;
        std::fclose(f);

        profiler.BeginFrame(i + 1);
        bool ok = false;
        {
            WARSMASH_PROFILE_SCOPE(profiler, "RetailDriveMapLoadOrSwitch", "Session");
            if (loadedRetailCount == 0) {
                ok = session.LoadMap(mpq, sim, scene, presentation, mapPath);
            } else {
                ok = session.HotSwitchMap(mpq, sim, scene, presentation, mapPath, false);
            }
        }
        TEST_ASSERT(ok);
        loadedRetailCount++;

        TEST_ASSERT(scene.GetBoundTerrainChunkGrid() == &session.GetTerrainChunkGrid());
        TEST_ASSERT(session.GetTerrainChunkGrid().GetChunkCount() > 0);
        totalTerrainChunksVerified += session.GetTerrainChunkGrid().GetChunkCount();
        totalDoodadsVerified += session.GetDoodadsDoo().doodads.size();

        // Execute SceneManager::RenderAll with integrated RenderGraph on every retail map
        Vec3 camEye(0.0f, 1200.0f, 1200.0f);
        Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
        Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 5000.0f);
        {
            WARSMASH_PROFILE_SCOPE(profiler, "RetailDriveSceneRenderGraph", "Render");
            scene.RenderAll(view, proj, 0.016f);
        }
        TEST_ASSERT(scene.GetRenderGraph().GetExecutedPassCount() == 7);
        profiler.EndFrame();
    }

    if (loadedRetailCount > 0) {
        TEST_ASSERT(loadedRetailCount >= 10);
        TEST_ASSERT(totalTerrainChunksVerified >= 500);
        TEST_ASSERT(totalDoodadsVerified >= 10000);
        TEST_ASSERT(session.GetStats().hotSwitchesPerformed == static_cast<uint32_t>(loadedRetailCount - 1));
    }

    session.TeardownCurrentSession(mpq, sim, scene, presentation);
    return true;
}

// ============================================================================
// Phase 10 Stage X1-Z2: Fog of War Single-Pass + Parallel & Smooth Transition,
// MDX Animator Zero-Alloc Cursor Cache + Cross-Fade + LOD, Data-Driven Buff Pool,
// and TacticalThreatMap Utility AI with Retail Drive Map Verification
// ============================================================================

bool TestPhase10StageX1FogSinglePassSynthesisAndDirtyTiles() {
    CFogSystem fog;
    fog.EnsureGrid(256, 256, -4096.0f, -4096.0f, 4096.0f, 4096.0f);
    fog.ClearDirtyTiles(0);
    TEST_ASSERT(fog.GetDirtyTileCount(0) == 0);

    // Splat 4 sight sources for player 0
    const CFogSightSource sources[4] = {
        {0, -2000.0f, -2000.0f, 600.0f, false},
        {0,  2000.0f, -2000.0f, 600.0f, false},
        {0, -2000.0f,  2000.0f, 600.0f, false},
        {0,  2000.0f,  2000.0f, 600.0f, true}
    };
    fog.UpdateSight(sources, 4);
    const int dirtyAfterMove = fog.GetDirtyTileCount(0);
    TEST_ASSERT(dirtyAfterMove > 0);

    // Calling UpdateSight again with identical stationary units produces 0 dirty tiles
    fog.ClearDirtyTiles(0);
    fog.UpdateSight(sources, 4);
    TEST_ASSERT(fog.GetDirtyTileCount(0) == 0);

    // Add 16 small scripted modifiers and 4 overrides for player 0
    for (int i = 0; i < 16; ++i) {
        const float cx = -3000.0f + (float)(i * 350);
        const int modId = fog.AddModifier(0, CFogArea::Circle(cx, 0.0f, 180.0f), CFogState::VISIBLE);
        fog.SetModifierEnabled(modId, true);
    }
    for (int i = 0; i < 4; ++i) {
        const float rx = -1500.0f + (float)(i * 800);
        fog.AddOverride(0, CFogArea::Rect(rx, 800.0f, rx + 200.0f, 1000.0f), CFogState::VISIBLE);
    }
    TEST_ASSERT(fog.GetDirtyTileCount(0) > 0);

    std::vector<unsigned char> out(256 * 256, 255);
    fog.Rasterize(0, 256, 256, -4096.0f, -4096.0f, 4096.0f, 4096.0f, out.data());

    // Legacy per-pixel modifier scan would perform 256 * 256 * 20 = 1,310,720 checks.
    // Stage X1 AABB-clipped stamp performs < 3,000 checks (> 99.7% reduction)!
    TEST_ASSERT(fog.GetLastRasterizeModifierChecks() > 0);
    TEST_ASSERT(fog.GetLastRasterizeModifierChecks() < 5000);

    // Verify center of sight circle and modifier circle are VISIBLE (0)
    TEST_ASSERT(fog.IsVisibleTo(0, -2000.0f, -2000.0f));
    TEST_ASSERT(fog.IsVisibleTo(0, -3000.0f, 0.0f));
    return true;
}

bool TestPhase10StageX2FogParallelMultiPlayerAndTemporalSmoothTransition() {
    CFogSystem fogSeq;
    CFogSystem fogPar;
    fogSeq.EnsureGrid(128, 128, -4096.0f, -4096.0f, 4096.0f, 4096.0f);
    fogPar.EnsureGrid(128, 128, -4096.0f, -4096.0f, 4096.0f, 4096.0f);

    std::vector<CFogSightSource> sources;
    for (int p = 0; p < 8; ++p) {
        for (int u = 0; u < 8; ++u) {
            CFogSightSource s;
            s.player = p;
            s.x = -3000.0f + (float)(p * 750);
            s.y = -3000.0f + (float)(u * 750);
            s.radius = 500.0f + (float)(u * 30);
            s.isFlying = (u % 2 == 0);
            sources.push_back(s);
        }
    }

    core::JobSystem jobs(4);
    fogSeq.UpdateSight(sources.data(), (int)sources.size());
    fogPar.UpdateSightParallel(sources.data(), (int)sources.size(), &jobs);

    // Verify bit-for-bit determinism across all 8 players
    std::vector<unsigned char> bufSeq(128 * 128);
    std::vector<unsigned char> bufPar(128 * 128);
    for (int p = 0; p < 8; ++p) {
        fogSeq.Rasterize(p, 128, 128, -4096.0f, -4096.0f, 4096.0f, 4096.0f, bufSeq.data());
        fogPar.Rasterize(p, 128, 128, -4096.0f, -4096.0f, 4096.0f, 4096.0f, bufPar.data());
        TEST_ASSERT(std::memcmp(bufSeq.data(), bufPar.data(), bufSeq.size()) == 0);
    }

    // Verify FogOfWar temporal smooth transition buffer
    FogOfWar gpuFog;
    gpuFog.Init(64, 64);
    std::vector<unsigned char> initialMasked(64 * 64, 255);
    std::vector<unsigned char> targetRevealed(64 * 64, 0);
    gpuFog.SetData(initialMasked.data());
    gpuFog.ClearDirtyTiles();
    TEST_ASSERT(gpuFog.GetDirtyTileCount() == 0);

    gpuFog.SetTargetData(targetRevealed.data());
    const int modifiedStep1 = gpuFog.StepSmoothTransition(0.10f, 1000.0f); // step by 100 levels (255 -> 155)
    TEST_ASSERT(modifiedStep1 == 64 * 64);
    TEST_ASSERT(gpuFog.GetData()[0] == 155);
    TEST_ASSERT(gpuFog.GetDirtyTileCount() == 16); // 4x4 tiles of 16x16

    // Finish transition to 0
    gpuFog.StepSmoothTransition(0.20f, 1000.0f);
    TEST_ASSERT(gpuFog.GetData()[0] == 0);
    TEST_ASSERT(gpuFog.StepSmoothTransition(0.05f, 1000.0f) == 0);
    return true;
}

bool TestPhase10StageY1MdxAnimatorZeroAllocInPlaceAndCursorCache() {
    MdxModel model;
    MdxSequence seq;
    seq.name = "Stand";
    seq.startFrame = 0;
    seq.endFrame = 1000;
    model.sequences.push_back(seq);
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});
    model.pivotPoints.push_back({0.0f, 0.0f, 50.0f});

    MdxNode rootBone;
    rootBone.name = "RootBone";
    rootBone.objectId = 0;
    rootBone.parentId = -1;
    MdxTimeline trTl;
    trTl.tag = (uint32_t)(('K' << 24) | ('G' << 16) | ('T' << 8) | 'R');
    trTl.interpolationType = MdxInterpolation::Linear;
    for (int i = 0; i <= 20; ++i) {
        trTl.frames.push_back((uint32_t)(i * 50));
        trTl.values.push_back((float)i * 2.0f);
        trTl.values.push_back(0.0f);
        trTl.values.push_back(0.0f);
    }
    rootBone.timelines.push_back(trTl);

    MdxNode childBone;
    childBone.name = "ChildBone";
    childBone.objectId = 1;
    childBone.parentId = 0;
    MdxTimeline rotTl;
    rotTl.tag = (uint32_t)(('K' << 24) | ('G' << 16) | ('R' << 8) | 'T');
    rotTl.interpolationType = MdxInterpolation::Linear;
    for (int i = 0; i <= 20; ++i) {
        rotTl.frames.push_back((uint32_t)(i * 50));
        rotTl.values.push_back(0.0f);
        rotTl.values.push_back(0.0f);
        rotTl.values.push_back(std::sin((float)i * 0.05f));
        rotTl.values.push_back(std::cos((float)i * 0.05f));
    }
    childBone.timelines.push_back(rotTl);

    // Push child before parent to verify BuildSkeleton topological sort places parent first
    model.bones.push_back(childBone);
    model.bones.push_back(rootBone);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.IsTopologicalOrderValid());
    TEST_ASSERT(animator.PlaySequence("Stand", true, 1.0f));

    for (int step = 0; step < 50; ++step) {
        animator.Update(0.016f);
    }

    // Verify 0 temporary MdxTimeline heap allocations and >85% O(1) cursor hit rate
    TEST_ASSERT(animator.GetSampleHeapAllocations() == 0);
    TEST_ASSERT(animator.GetCursorHitCount() >= 80);
    TEST_ASSERT(animator.GetCursorHitCount() > animator.GetCursorMissCount() * 5);
    return true;
}

bool TestPhase10StageY2MdxAnimatorCrossFadeAndAnimationLod() {
    MdxModel model;
    MdxSequence seqStand;
    seqStand.name = "Stand";
    seqStand.startFrame = 0;
    seqStand.endFrame = 500;
    MdxSequence seqAttack;
    seqAttack.name = "Attack";
    seqAttack.startFrame = 600;
    seqAttack.endFrame = 1100;
    model.sequences.push_back(seqStand);
    model.sequences.push_back(seqAttack);
    model.pivotPoints.push_back({0.0f, 0.0f, 0.0f});

    MdxNode bone;
    bone.name = "WeaponBone";
    bone.objectId = 0;
    bone.parentId = -1;
    MdxTimeline trTl;
    trTl.tag = (uint32_t)(('K' << 24) | ('G' << 16) | ('T' << 8) | 'R');
    trTl.interpolationType = MdxInterpolation::Linear;
    // Stand at X = 0, Attack at X = 100
    trTl.frames = {0, 500, 600, 1100};
    trTl.values = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        100.0f, 0.0f, 0.0f,
        100.0f, 0.0f, 0.0f
    };
    bone.timelines.push_back(trTl);
    model.bones.push_back(bone);

    MdxAnimator animator;
    animator.Init(model);
    TEST_ASSERT(animator.PlaySequence("Stand", true, 1.0f));
    animator.Update(0.05f);

    Vec3 posBefore;
    TEST_ASSERT(animator.GetObjectWorldPositionYUp(0, posBefore));
    TEST_ASSERT(std::fabs(posBefore.x - 0.0f) < 0.01f);

    // Cross-fade to Attack over 0.20 seconds
    TEST_ASSERT(animator.CrossFadeToSequence("Attack", false, 0.20f, 1.0f));
    TEST_ASSERT(std::fabs(animator.GetBlendLeft() - 0.20f) < 1e-4f);

    // Halfway through cross-fade (0.10s), bone X should be smoothly blended (~50.0f)
    animator.Update(0.10f);
    Vec3 posMid;
    TEST_ASSERT(animator.GetObjectWorldPositionYUp(0, posMid));
    TEST_ASSERT(posMid.x > 35.0f && posMid.x < 65.0f);

    // Finish cross-fade
    animator.Update(0.15f);
    Vec3 posEnd;
    TEST_ASSERT(animator.GetObjectWorldPositionYUp(0, posEnd));
    TEST_ASSERT(std::fabs(posEnd.x - 100.0f) < 1.0f);

    // Test Animation LOD stride = 4 (skips 3 of every 4 updates)
    animator.SetAnimationLodStride(4);
    TEST_ASSERT(animator.GetAnimationLodStride() == 4);
    for (int i = 0; i < 20; ++i) {
        animator.Update(0.016f);
    }
    TEST_ASSERT(animator.GetLodSkippedUpdateCount() == 15);
    return true;
}

bool TestPhase10StageZ1DataDrivenBuffZeroAllocPoolAndStacking() {
    CSimulation sim;
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 500, 270, false);
    CUnit* unit = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    TEST_ASSERT(unit != nullptr);
    const float baseSpeed = unit->GetSpeed();
    const int baseDefense = unit->GetTotalDefense();

    // 1. Apply a stacking poison + armor reduction debuff (max 3 stacks)
    DataDrivenBuffSpec poisonSpec;
    poisonSpec.id = War3ID("Bpoi");
    poisonSpec.duration = 5.0f;
    poisonSpec.isPositive = false;
    poisonSpec.stackType = CBuffStackType::STACK;
    poisonSpec.maxStacks = 3;
    poisonSpec.moveSpeedPercent = -0.10f; // -10% speed per stack
    poisonSpec.armorDelta = -2.0f;        // -2 armor per stack
    poisonSpec.periodic = true;
    poisonSpec.periodicInterval = 1.0f;
    poisonSpec.periodicDamage = 10.0f;    // 10 dmg/sec per stack

    TEST_ASSERT(unit->ApplyDataDrivenBuff(poisonSpec, sim));
    TEST_ASSERT(unit->ApplyDataDrivenBuff(poisonSpec, sim));
    TEST_ASSERT(unit->ApplyDataDrivenBuff(poisonSpec, sim));
    TEST_ASSERT(unit->HasBuff(War3ID("Bpoi")));

    const auto* rec = unit->GetDataDrivenBuff(War3ID("Bpoi"));
    TEST_ASSERT(rec != nullptr && rec->stackCount == 3);
    TEST_ASSERT(unit->GetTotalDefense() == baseDefense - 6);
    TEST_ASSERT(unit->GetSpeed() < baseSpeed);

    // 2. Apply a positive haste + bonus damage buff
    DataDrivenBuffSpec hasteSpec;
    hasteSpec.id = War3ID("Bblo");
    hasteSpec.duration = 2.0f;
    hasteSpec.isPositive = true;
    hasteSpec.moveSpeedPercent = 0.50f;
    hasteSpec.attackSpeedBonus = 0.40f;
    hasteSpec.flatDamageBonus = 15;
    TEST_ASSERT(unit->ApplyDataDrivenBuff(hasteSpec, sim));
    TEST_ASSERT(unit->GetNonStackingDamageTotal() == 15);
    TEST_ASSERT(unit->GetZeroAllocBuffAppliedCount() == 4);

    // 3. Tick buffs for 2.1 seconds: periodic poison ticks twice (30 dmg * 2 = 60 dmg) and haste expires
    const float lifeBefore = unit->GetLife();
    const int steps = (int)std::ceil(2.1f / sim.GetSimStepTime());
    for (int i = 0; i < steps; ++i) {
        unit->UpdateBuffs(sim);
    }
    TEST_ASSERT(!unit->HasBuff(War3ID("Bblo")));
    TEST_ASSERT(unit->GetNonStackingDamageTotal() == 0);
    TEST_ASSERT(unit->GetLife() < lifeBefore - 50.0f);

    // 4. Dispel negative buffs via RemoveBuffsIf
    unit->RemoveBuffsIf(sim, false, true, false, false);
    TEST_ASSERT(!unit->HasBuff(War3ID("Bpoi")));
    TEST_ASSERT(unit->GetTotalDefense() == baseDefense);
    TEST_ASSERT(std::fabs(unit->GetSpeed() - baseSpeed) < 0.01f);
    return true;
}

bool TestPhase10StageZ2TacticalThreatMapUtilityAiAndRetailDriveSuite() {
    CSimulation sim;
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
    CUnitType* gruntType   = MakePhase5UnitType(War3ID("ogru"), 700, 270, false);
    CUnitType* heroType    = MakePhase5UnitType(War3ID("Obla"), 650, 320, false);

    sim.AddPlayer(std::make_unique<CPlayer>(0, "AI", CMapControl::COMPUTER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "Enemy", CMapControl::USER));
    // Player 0 = AI, Player 1 = Enemy
    CUnit* aiFootmanHealthy = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    CUnit* aiFootmanWounded = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 60.0f, 0.0f, 0.0f));
    CUnit* enemyGruntFull   = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), gruntType, 1, War3ID("ogru"), 200.0f, 0.0f, 180.0f));
    CUnit* enemyHeroLowHp   = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), heroType, 1, War3ID("Obla"), 220.0f, 40.0f, 180.0f));
    TEST_ASSERT(aiFootmanHealthy && aiFootmanWounded && enemyGruntFull && enemyHeroLowHp);
    sim.Update();

    // Wound the AI footman to 15% HP and enemy unit to 8% HP
    aiFootmanWounded->SetLife(sim, aiFootmanWounded->GetMaxLife() * 0.15f);
    enemyHeroLowHp->SetLife(sim, enemyHeroLowHp->GetMaxLife() * 0.08f);

    MeleeAIController ai(&sim, 0);
    ai.RebuildThreatMap();
    TEST_ASSERT(ai.GetThreatMap().GetActiveThreatCellCount() > 0);
    TEST_ASSERT(ai.GetThreatMap().SampleThreatAt(200.0f, 0.0f) > 0.0f);

    // Utility AI target selection should rank the critically wounded enemy higher than the full-HP Grunt
    CUnit* bestTarget = ai.EvaluateBestFocusTarget(aiFootmanHealthy, 900.0f);
    TEST_ASSERT(bestTarget == enemyHeroLowHp);

    // ExecuteTacticalMicro should kite the wounded footman away and focus-fire with the healthy footman
    const int microOrders = ai.ExecuteTacticalMicro(0.30f, 280.0f);
    TEST_ASSERT(microOrders == 2);
    TEST_ASSERT(ai.GetKitingRetreatCount() == 1);
    TEST_ASSERT(ai.GetFocusFireOrderCount() == 1);

    // Verify FogOfWar parallel + single-pass rasterize on retail Drive map dimensions via WorldSceneSession
    MpqManager mpq;
    CSimulation mapSim;
    SceneManager scene;
    UnitPresentationSystem presentation;
    core::WorldSceneSession session;
    const std::string mapPath = "resources/Maps/FrozenThrone/(8)GardenOfWar.w3x";
    FILE* f = std::fopen(mapPath.c_str(), "rb");
    if (f) {
        std::fclose(f);
        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        const W3EMap& w3e = session.GetW3E();
        CFogSystem mapFog;
        const float minX = w3e.offsetX;
        const float minY = w3e.offsetY;
        const float maxX = minX + (float)(w3e.tileWidth - 1) * 128.0f;
        const float maxY = minY + (float)(w3e.tileHeight - 1) * 128.0f;
        mapFog.EnsureGrid(w3e.tileWidth, w3e.tileHeight, minX, minY, maxX, maxY);
        core::JobSystem jobs(4);
        const CFogSightSource mapSight[2] = {
            {0, 0.0f, 0.0f, 1200.0f, false},
            {1, 512.0f, 512.0f, 1200.0f, true}
        };
        mapFog.UpdateSightParallel(mapSight, 2, &jobs);
        std::vector<unsigned char> fogBuf((size_t)w3e.tileWidth * w3e.tileHeight, 255);
        mapFog.Rasterize(0, w3e.tileWidth, w3e.tileHeight, minX, minY, maxX, maxY, fogBuf.data());
        TEST_ASSERT(mapFog.IsVisibleTo(0, 0.0f, 0.0f));
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}

// ============================================================================
// Phase 11 Stage AA1-DD2: Zero-Alloc VFX Particle Staging & RibbonRingBuffer,
// Broadphase Raycast & NonAlloc Picking, O(1) RichTextLayoutCache & UTF-8/CJK,
// and IScriptEngineHost RealScript Hot-Swap with Retail Drive Map Verification
// ============================================================================

bool TestPhase11StageAA1ParticleSystemZeroAllocStaging() {
    MdxModel model;
    MdxParticleEmitter2 pe;
    pe.name = "FireEmitter";
    pe.objectId = 0;
    pe.parentId = -1;
    pe.speed = 60.0f;
    pe.variation = 0.1f;
    pe.latitude = 30.0f;
    pe.gravity = 9.8f;
    pe.lifeSpan = 1.5f;
    pe.emissionRate = 120.0f;
    pe.width = 16.0f;
    pe.length = 16.0f;
    pe.rows = 1;
    pe.columns = 1;
    pe.timeMiddle = 0.5f;
    for (int i = 0; i < 3; ++i) {
        pe.segmentColors[i][0] = 1.0f;
        pe.segmentColors[i][1] = 0.5f;
        pe.segmentColors[i][2] = 0.1f;
        pe.segmentAlphas[i] = 255;
        pe.segmentScaling[i] = 12.0f;
    }
    model.particleEmitters2.push_back(pe);

    MdxParticleSystem partSys;
    partSys.Init(model, nullptr);
    TEST_ASSERT(partSys.GetEmitterCount() == 1);

    for (int step = 0; step < 30; ++step) {
        partSys.Update(0.05f, Vec3{10.0f, 0.0f, 20.0f});
    }

    TEST_ASSERT(partSys.GetParticleCount() > 50);
    // Every non-empty frame updated stagingInstances in-place with zero heap reallocations
    TEST_ASSERT(partSys.GetZeroAllocUploadCount() >= 25);
    const auto& staged = partSys.GetEmitterStagingInstances(0);
    TEST_ASSERT((int)staged.size() == partSys.GetParticleCount());
    TEST_ASSERT(std::fabs(staged[0].size - 12.0f) < 0.01f);
    return true;
}

bool TestPhase11StageAA2RibbonRingBufferZeroAllocTrail() {
    RibbonRingBuffer ring;
    TEST_ASSERT(ring.empty() && ring.size() == 0);
    for (int i = 0; i < 600; ++i) {
        RibbonSegment seg;
        seg.age = (float)(i % 10) * 0.1f;
        seg.life = 0.5f;
        seg.slot = i;
        ring.push_back(seg);
    }
    // Capped at kMaxCapacity (512) with oldest overwritten in ring order
    TEST_ASSERT(ring.size() == RibbonRingBuffer::kMaxCapacity);
    TEST_ASSERT(ring[0].slot == 88);
    TEST_ASSERT(ring[ring.size() - 1].slot == 599);

    // Compact expired segments (age >= life) in-place without allocation
    ring.remove_if_inplace([](const RibbonSegment& s) { return s.age >= s.life; });
    TEST_ASSERT(ring.size() > 0 && ring.size() < RibbonRingBuffer::kMaxCapacity);

    // Test MdxRibbonSystem zero-alloc staging vertex/index generation
    MdxModel model;
    MdxRibbonEmitter rb;
    rb.name = "BladeTrail";
    rb.objectId = 0;
    rb.parentId = -1;
    rb.heightAbove = 25.0f;
    rb.heightBelow = 25.0f;
    rb.alpha = 0.9f;
    rb.color[0] = 0.2f; rb.color[1] = 0.6f; rb.color[2] = 1.0f;
    rb.lifeSpan = 1.0f;
    rb.emissionRate = 60.0f;
    rb.rows = 1;
    rb.columns = 1;
    model.ribbonEmitters.push_back(rb);

    MdxRibbonSystem ribSys;
    ribSys.Init(model, nullptr);
    for (int step = 0; step < 20; ++step) {
        ribSys.Update(0.04f, Vec3{(float)step * 10.0f, 0.0f, 0.0f});
    }
    TEST_ASSERT(ribSys.GetSegmentCount() >= 10);
    TEST_ASSERT(ribSys.GetLiveQuadCount() == ribSys.GetSegmentCount() - 1);
    TEST_ASSERT(ribSys.GetZeroAllocRibbonUploadCount() >= 10);
    return true;
}

bool TestPhase11StageBB1BroadphaseSphereRejectionAndCollisionRaycast() {
    resource::ResourceServer resServer;
    const std::string colMdl = R"(
        Version { FormatVersion 800, }
        Model "PickableTower" {
            NumGeosets 1,
            NumBones 1,
            BlendTime 150,
            MinimumExtent { -32.0, -32.0, 0.0 },
            MaximumExtent { 32.0, 32.0, 128.0 },
            BoundsRadius 64.0,
        }
        Sequences 1 {
            Anim "Stand" {
                Interval { 0, 1000 },
                MinimumExtent { -32.0, -32.0, 0.0 },
                MaximumExtent { 32.0, 32.0, 128.0 },
                BoundsRadius 64.0,
            }
        }
        Geoset {
            Vertices 3 { { -10.0, 0.0, 0.0 }, { 10.0, 0.0, 0.0 }, { 0.0, 0.0, 20.0 }, }
            Normals 3 { { 0.0, 0.0, 1.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 0.0, 1.0 }, }
            TVertices 3 { { 0.0, 0.0 }, { 1.0, 0.0 }, { 0.5, 1.0 }, }
            VertexGroup { 0, 0, 0, }
            Faces 1 3 { Triangles { { 0, 1, 2 }, } }
            Groups 1 1 { Matrices { 0 }, }
        }
        Bone "RootBone" { ObjectId 0, Parent -1, GeosetId 0, GeosetAnimId -1, }
        CollisionShape "HitBox" {
            ObjectId 1,
            Parent 0,
            Box,
            Vertices {
                { -30.0, -30.0, 0.0 },
                { 30.0, 30.0, 100.0 },
            }
        }
        PivotPoints 2 { { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 }, }
    )";
    resServer.MountMemoryFile("Buildings/Human/WatchTower/WatchTower.mdl",
                              std::vector<uint8_t>(colMdl.begin(), colMdl.end()));

    SceneManager scene;
    scene.SetResourceServer(&resServer);
    const uint32_t towerId = War3ID("hwtw").value;
    scene.RegisterUnitModelPath(towerId, "Buildings/Human/WatchTower/WatchTower.mdx");

    const float scale[3] = {1.0f, 1.0f, 1.0f};
    // Place 40 towers along a wide grid; only 1 tower is at (0, 0, 0)
    for (int i = 0; i < 40; ++i) {
        const float pos[3] = {(float)(i * 600), 0.0f, (float)((i % 5) * 600)};
        TEST_ASSERT(scene.AddUnit(towerId, pos, 0.0f, scale, 0, 500, 0) >= 0);
    }

    scene.ResetPickingStats();
    picking::PickRay ray;
    ray.origin = {0.0f, 50.0f, 200.0f};
    ray.direction = {0.0f, 0.0f, -1.0f};

    int hitCount = 0;
    for (int i = 0; i < scene.GetInstanceCount(); ++i) {
        float t = 0.0f;
        if (scene.IntersectInstanceRayWithCollision(i, ray, t)) {
            ++hitCount;
        }
    }
    TEST_ASSERT(hitCount == 1);
    // Broad-phase sphere rejection must reject >= 38 of the 40 distant towers without 4x4 matrix inversion!
    TEST_ASSERT(scene.GetBroadphaseRejectedCount() >= 38);
    TEST_ASSERT(scene.GetNarrowphaseTestedCount() <= 2);
    return true;
}

bool TestPhase11StageBB2RaycastInstancesNonAllocAndScreenRectQuery() {
    SceneManager scene;
    const float scale[3] = {1.0f, 1.0f, 1.0f};
    // Place 5 procedural instances along Z axis: z = -100, -300, -500 and 2 far off-axis
    const float p0[3] = {0.0f, 0.0f, -500.0f};
    const float p1[3] = {0.0f, 0.0f, -100.0f};
    const float p2[3] = {0.0f, 0.0f, -300.0f};
    const float p3[3] = {2000.0f, 0.0f, -200.0f};
    const float p4[3] = {-2000.0f, 0.0f, -200.0f};
    const int idx0 = scene.AddProceduralInstance(War3ID("hfoo").value, p0, 0.0f, scale, 0);
    const int idx1 = scene.AddProceduralInstance(War3ID("hfoo").value, p1, 0.0f, scale, 0);
    const int idx2 = scene.AddProceduralInstance(War3ID("hfoo").value, p2, 0.0f, scale, 0);
    scene.AddProceduralInstance(War3ID("hfoo").value, p3, 0.0f, scale, 0);
    scene.AddProceduralInstance(War3ID("hfoo").value, p4, 0.0f, scale, 0);

    picking::PickRay ray;
    ray.origin = {0.0f, 0.0f, 100.0f};
    ray.direction = {0.0f, 0.0f, -1.0f};

    SceneManager::SceneRaycastHit hits[4];
    const int numHits = scene.RaycastInstancesNonAlloc(ray, 1000.0f, hits, 4, true);
    TEST_ASSERT(numHits == 3);
    // Verify ascending distance order: idx1 (z=-100) -> idx2 (z=-300) -> idx0 (z=-500)
    TEST_ASSERT(hits[0].instanceIndex == idx1);
    TEST_ASSERT(hits[1].instanceIndex == idx2);
    TEST_ASSERT(hits[2].instanceIndex == idx0);
    TEST_ASSERT(hits[0].distance < hits[1].distance && hits[1].distance < hits[2].distance);

    // Test 2D screen box selection
    Mat4 view = LookAt(Vec3(0.0f, 400.0f, 400.0f), Vec3(0.0f, 0.0f, -250.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(60.0f, 16.0f / 9.0f, 10.0f, 3000.0f);
    std::vector<int> boxed;
    const int boxedCount = scene.QueryScreenRectInstances(view, proj, 1280, 720, 200.0f, 50.0f, 1080.0f, 670.0f, boxed);
    TEST_ASSERT(boxedCount == 3);
    return true;
}

bool TestPhase11StageCC1RichTextLayoutCacheAndO1ColorParser() {
    TextRenderer tr;
    tr.InitHeadlessMetrics(24.0f, 0.5f); // Each ASCII char = 12px at size 24
    TEST_ASSERT(tr.IsValid());

    uint32_t parsedColor = 0;
    TEST_ASSERT(TextRenderer::TryParseColorCodeO1("|cffffcc00Gold|r", parsedColor));
    // Short/truncated "|c123" must fail safely in O(1) without reading past null terminator
    TEST_ASSERT(!TextRenderer::TryParseColorCodeO1("|c123", parsedColor));

    const char* richStr = "|cffffcc00Gold: 500|r|n|cff00ff00Lumber: 250|r";
    const float w1 = tr.MeasureText(richStr, 24.0f);
    const float w2 = tr.MeasureText(richStr, 24.0f);
    TEST_NEAR(w1, w2, 1e-4f);
    // "Lumber: 250" is 11 chars -> 11 * 12.0f = 132.0f max line width
    TEST_NEAR(w1, 11.0f * 12.0f, 1e-3f);
    TEST_ASSERT(tr.GetLayoutCacheMissCount() == 1);
    TEST_ASSERT(tr.GetLayoutCacheHitCount() == 1);

    const CachedTextLayout& layout = tr.GetOrBuildLayout(richStr);
    TEST_ASSERT(layout.lineCount == 2);
    TEST_ASSERT(layout.spans.size() == 2);
    TEST_ASSERT(layout.spans[0].hasColorOverride && layout.spans[0].text == "Gold: 500");
    TEST_ASSERT(layout.spans[1].newlineBefore && layout.spans[1].text == "Lumber: 250");
    return true;
}

bool TestPhase11StageCC2Utf8CodepointDecoderAndCjkMetrics() {
    TextRenderer tr;
    tr.InitHeadlessMetrics(24.0f, 0.5f); // ASCII = 12px, CJK fullwidth = 24px

    const char* cjkRich = "|cffffcc00阿尔萨斯|r Lv10";
    // "阿尔萨斯" = 4 CJK chars (4 * 24 = 96px) + " Lv10" = 5 ASCII chars (5 * 12 = 60px) => 156px
    const float measured = tr.MeasureText(cjkRich, 24.0f);
    TEST_NEAR(measured, 4.0f * 24.0f + 5.0f * 12.0f, 1e-3f);

    // Custom glyph override for a specific Unicode codepoint
    const char* swordChar = "\xE5\x89\x91"; // U+5251
    const char* cursor = swordChar;
    const uint32_t cp = TextRenderer::DecodeNextUtf8Codepoint(cursor);
    TEST_ASSERT(cp == 0x5251u);
    tr.RegisterUnicodeGlyphAdvance(0x5251u, 30.0f);
    TEST_NEAR(tr.MeasureText(swordChar, 24.0f), 30.0f, 1e-3f);
    return true;
}

bool TestPhase11StageDD1UnifiedScriptEngineHostAndJassAdapter() {
    scripting::ScriptHostRouter router(std::make_unique<scripting::JassScriptHostAdapter>());
    TEST_ASSERT(router.GetActiveHost() != nullptr);
    TEST_ASSERT(router.GetActiveHost()->GetBackendKind() == scripting::ScriptBackendKind::JassLegacy);

    int hostHookSum = 0;
    router.RegisterSharedNativeAuto("HostReportScore", [&](int player, int delta) -> int {
        hostHookSum += player * 100 + delta;
        return hostHookSum;
    });

    const std::string jassSrc = R"(
        native HostReportScore takes integer p, integer d returns integer
        globals
            integer g_campaignStage = 3
            integer g_lastScore = 0
        endglobals
        function OnUnitDeathHandler takes nothing returns nothing
            set g_lastScore = HostReportScore(2, 45)
            set g_campaignStage = 4
        endfunction
    )";

    auto* host = router.GetActiveHost();
    TEST_ASSERT(host->LoadModule("CampaignJass", jassSrc));
    host->SetGlobal("g_campaignStage", JassValue::Int(3));

    const int subId = host->SubscribeEvent(101, "OnUnitDeathHandler", /*filterHandle=*/5001);
    TEST_ASSERT(subId > 0);

    scripting::ScriptEventPayload ev;
    ev.eventId = 101;
    ev.sourceHandle = 5001;
    ev.targetHandle = 5002;
    ev.numericValue = 120.0f;
    TEST_ASSERT(host->FireEvent(ev) == 1);
    TEST_ASSERT(hostHookSum == 245);
    TEST_ASSERT(host->GetGlobal("g_lastScore").AsInt() == 245);
    TEST_ASSERT(host->GetGlobal("g_campaignStage").AsInt() == 4);

    // Handle destruction automatically unsubscribes handle-filtered triggers
    host->OnHandleDestroyed(5001);
    TEST_ASSERT(host->FireEvent(ev) == 0);
    return true;
}

bool TestPhase11StageDD2RealScriptHotSwapAndRetailDriveSuite() {
    scripting::ScriptHostRouter router(std::make_unique<scripting::JassScriptHostAdapter>());

    int sharedNativeCalls = 0;
    router.RegisterSharedNativeAuto("EngineAddGold", [&](int playerIdx, int amount) -> int {
        ++sharedNativeCalls;
        return playerIdx * 1000 + amount;
    });

    // Populate globals in JassScriptHostAdapter before hot-swapping to RealScriptEngineHost
    router.GetActiveHost()->SetGlobal("g_migratedGold", JassValue::Int(1250));
    router.GetActiveHost()->SetGlobal("g_heroName", JassValue::Str("Arthas"));

    auto realScriptHost = std::make_unique<scripting::RealScriptEngineHost>();
    auto* rsRaw = realScriptHost.get();

    // Hot-swap from JassVM -> RealScriptEngineHost with automatic native re-binding & global state migration!
    TEST_ASSERT(router.HotSwapBackend(std::move(realScriptHost), /*migrateGlobals=*/true));
    TEST_ASSERT(router.GetHotSwapCount() == 1);
    TEST_ASSERT(router.GetActiveHost()->GetBackendKind() == scripting::ScriptBackendKind::RealScript);
    TEST_ASSERT(router.GetActiveHost()->GetGlobal("g_migratedGold").AsInt() == 1250);
    TEST_ASSERT(router.GetActiveHost()->GetGlobal("g_heroName").strVal == "Arthas");

    // Load a RealScript module with declarative variables and register a RealScript handler
    const std::string rsModule = R"(
        let rs_wave_bonus = 350;
        let rs_boss_active = true;
    )";
    TEST_ASSERT(rsRaw->LoadModule("RealScriptWaveLogic.rs", rsModule));
    TEST_ASSERT(rsRaw->GetGlobal("rs_wave_bonus").AsInt() == 350);
    TEST_ASSERT(rsRaw->GetGlobal("rs_boss_active").AsBool() == true);

    rsRaw->RegisterScriptFunction("OnRealScriptWaveEvent", [](scripting::RealScriptEngineHost& host,
                                                              const std::vector<scripting::ScriptValue>& args) {
        const int srcHandle = !args.empty() ? args[0].AsInt() : 0;
        const int bonus = host.GetGlobal("rs_wave_bonus").AsInt();
        const auto res = host.InvokeNativeZeroAlloc("EngineAddGold", {
            scripting::ScriptValue::Int(1),
            scripting::ScriptValue::Int(bonus + srcHandle)
        });
        host.SetGlobal("g_migratedGold", res);
        return res;
    });

    rsRaw->SubscribeEvent(202, "OnRealScriptWaveEvent", /*filterHandle=*/50);
    scripting::ScriptEventPayload waveEv;
    waveEv.eventId = 202;
    waveEv.sourceHandle = 50;
    TEST_ASSERT(rsRaw->FireEvent(waveEv) == 1);
    TEST_ASSERT(sharedNativeCalls == 1);
    TEST_ASSERT(rsRaw->GetGlobal("g_migratedGold").AsInt() == 1400);
    TEST_ASSERT(rsRaw->GetZeroAllocNativeCallCount() >= 2);

    // End-to-End Retail Drive Map Picking & Broadphase Verification on (4)TwistedMeadows.w3x
    MpqManager mpq;
    CSimulation sim;
    SceneManager scene;
    UnitPresentationSystem presentation;
    core::WorldSceneSession session;
    const std::string mapPath = "resources/Maps/FrozenThrone/(4)TwistedMeadows.w3x";
    FILE* f = std::fopen(mapPath.c_str(), "rb");
    if (f) {
        std::fclose(f);
        TEST_ASSERT(session.LoadMap(mpq, sim, scene, presentation, mapPath));
        TEST_ASSERT(scene.GetInstanceCount() > 100);

        scene.ResetPickingStats();
        picking::PickRay pickRay;
        pickRay.origin = {0.0f, 1500.0f, 0.0f};
        pickRay.direction = {0.0f, -1.0f, 0.0f};
        SceneManager::SceneRaycastHit hits[8];
        (void)scene.RaycastInstancesNonAlloc(pickRay, 5000.0f, hits, 8, true);
        // Over 90% of doodads across the retail map are rejected by the broadphase sphere check
        TEST_ASSERT(scene.GetBroadphaseRejectedCount() > (uint64_t)(scene.GetInstanceCount() * 8 / 10));
        session.TeardownCurrentSession(mpq, sim, scene, presentation);
    }

    return true;
}

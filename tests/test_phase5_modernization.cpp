#include "net/lockstep_host.h"
#include "net/lobby_state.h"
#include "net/tcp_transport.h"
#include "engine/dynamic_module_loader.h"
#include "game/war3/war3_game_module.h"
#include "engine/engine_kernel.h"
#include "data/slk_merge.h"
#include "data/object_mod_meta.h"
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
#include "data/unitdata.h"
#include "data/upgradedata.h"
#include "ecs/tech_tree.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_upgrade.h"
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


// ============================================================================
// Phase 12 Stage EE1-GG1: General-Purpose Engine Foundation (EngineKernel,
// SubsystemRegistry, FixedTimestepStepper, IGameModule) & De-Redundancy Suite
// ============================================================================

bool TestPhase12StageEE1EngineSubsystemRegistryAndPhaseLifecycle() {
    std::vector<std::string> initOrder;
    std::vector<std::string> fixedOrder;
    std::vector<std::string> shutdownOrder;

    class MockSubsystem : public engine::ISubsystem {
    public:
        MockSubsystem(std::string name, engine::SubsystemPhase phase,
                      std::vector<std::string>& iOrd,
                      std::vector<std::string>& fOrd,
                      std::vector<std::string>& sOrd)
            : name_(std::move(name)), phase_(phase),
              initOrd_(iOrd), fixedOrd_(fOrd), shutOrd_(sOrd) {}

        const char* GetName() const override { return name_.c_str(); }
        engine::SubsystemPhase GetPhase() const override { return phase_; }

        bool OnInit() override {
            initOrd_.push_back(name_);
            return true;
        }
        void OnFixedUpdate(float fixedDt) override {
            accumFixedDt += fixedDt;
            fixedOrd_.push_back(name_);
        }
        void OnFrameUpdate(float dt) override {
            accumFrameDt += dt;
        }
        void OnRender() override {
            ++renderCalls;
        }
        void OnShutdown() override {
            shutOrd_.push_back(name_);
        }

        float accumFixedDt = 0.0f;
        float accumFrameDt = 0.0f;
        int renderCalls = 0;

    private:
        std::string name_;
        engine::SubsystemPhase phase_;
        std::vector<std::string>& initOrd_;
        std::vector<std::string>& fixedOrd_;
        std::vector<std::string>& shutOrd_;
    };

    struct RenderTestSub final : MockSubsystem {
        using MockSubsystem::MockSubsystem;
    };
    struct PlatformTestSub final : MockSubsystem {
        using MockSubsystem::MockSubsystem;
    };
    struct SimTestSub final : MockSubsystem {
        using MockSubsystem::MockSubsystem;
    };
    struct TelemetryTestSub final : MockSubsystem {
        using MockSubsystem::MockSubsystem;
    };

    engine::SubsystemRegistry reg;
    // Register out of phase order -> registry must automatically sort by SubsystemPhase
    reg.Register<RenderTestSub>("Render", engine::SubsystemPhase::Render, initOrder, fixedOrder, shutdownOrder);
    reg.Register<TelemetryTestSub>("Telemetry", engine::SubsystemPhase::Telemetry, initOrder, fixedOrder, shutdownOrder);
    reg.Register<PlatformTestSub>("Platform", engine::SubsystemPhase::Platform, initOrder, fixedOrder, shutdownOrder);
    reg.Register<SimTestSub>("Simulation", engine::SubsystemPhase::Simulation, initOrder, fixedOrder, shutdownOrder);

    TEST_ASSERT(reg.Count() == 4);
    TEST_ASSERT(reg.TryGet<SimTestSub>() != nullptr);
    TEST_ASSERT(reg.FindByName("Render") == reg.TryGet<RenderTestSub>());

    // 1. InitializeAll executes strictly in ascending SubsystemPhase order
    TEST_ASSERT(reg.InitializeAll());
    TEST_ASSERT(initOrder.size() == 4);
    TEST_ASSERT(initOrder[0] == "Platform");
    TEST_ASSERT(initOrder[1] == "Simulation");
    TEST_ASSERT(initOrder[2] == "Render");
    TEST_ASSERT(initOrder[3] == "Telemetry");

    // 2. FixedUpdateAll, FrameUpdateAll, RenderAll respect enabled state
    reg.FixedUpdateAll(0.05f);
    TEST_ASSERT(fixedOrder.size() == 4);
    TEST_ASSERT(reg.SetSubsystemEnabled("Render", false));
    fixedOrder.clear();
    reg.FixedUpdateAll(0.05f);
    reg.FrameUpdateAll(0.016f);
    reg.RenderAll();
    TEST_ASSERT(fixedOrder.size() == 3); // Render skipped while disabled
    TEST_ASSERT(reg.TryGet<RenderTestSub>()->renderCalls == 0);
    TEST_ASSERT(reg.TryGet<SimTestSub>()->renderCalls == 1);
    TEST_NEAR(reg.TryGet<SimTestSub>()->accumFixedDt, 0.10f, 1e-4f);

    // 3. ShutdownAll executes strictly in reverse SubsystemPhase order
    reg.ShutdownAll();
    TEST_ASSERT(shutdownOrder.size() == 4);
    TEST_ASSERT(shutdownOrder[0] == "Telemetry");
    TEST_ASSERT(shutdownOrder[1] == "Render");
    TEST_ASSERT(shutdownOrder[2] == "Simulation");
    TEST_ASSERT(shutdownOrder[3] == "Platform");
    return true;
}

bool TestPhase12StageEE2FixedTimestepStepperAndInterpolationAlpha() {
    engine::FixedTimestepStepper stepper(0.05f, 3); // 20Hz fixed step (50ms), max 3 substeps/frame

    int fixedCalls = 0;
    // Frame 1: 20ms frame -> 0 fixed steps, accumulator = 20ms, interpolation alpha = 20/50 = 0.40
    float observedFixedDt = 0.0f;
    int steps1 = stepper.AdvanceFrame(0.020f, [&](float dt) {
        observedFixedDt = dt;
        ++fixedCalls;
    });
    TEST_ASSERT(steps1 == 0 && fixedCalls == 0);
    TEST_NEAR(stepper.GetInterpolationAlpha(), 0.40f, 1e-4f);

    // Frame 2: 45ms frame -> total 65ms -> 1 fixed step (50ms), remainder 15ms -> alpha = 15/50 = 0.30
    int steps2 = stepper.AdvanceFrame(0.045f, [&](float /*dt*/) { ++fixedCalls; });
    TEST_ASSERT(steps2 == 1 && fixedCalls == 1);
    TEST_NEAR(stepper.GetInterpolationAlpha(), 0.30f, 1e-4f);

    // Frame 3: 500ms lag spike -> clamped to maxSubStepsPerFrame (3 * 50ms = 150ms) -> 3 steps, remainder 0ms
    int steps3 = stepper.AdvanceFrame(0.500f, [&](float /*dt*/) { ++fixedCalls; });
    TEST_ASSERT(steps3 == 3 && fixedCalls == 4);
    TEST_ASSERT(stepper.GetSpiralClampCount() == 1);
    TEST_NEAR(stepper.GetInterpolationAlpha(), 0.0f, 1e-4f);
    TEST_ASSERT(stepper.GetTotalFixedTicks() == 4);
    TEST_ASSERT(stepper.GetTotalFrames() == 3);
    return true;
}

bool TestPhase12StageFF1EngineKernelPluggableGameModuleAndRetailDriveMap() {
    class ExperimentalRtsGameModule final : public engine::IGameModule {
    public:
        const char* GetModuleName() const override { return "ExperimentalWarcraft3Module"; }

        bool OnInit(engine::EngineKernel& kernel) override {
            if (kernel.Input()) {
                kernel.Input()->RegisterStandardWarcraftDefaults();
            }
            sim.AddPlayer(std::make_unique<CPlayer>(0, "Player0", CMapControl::USER));
            presentation.BindSimulationEvents(sim);

            const std::string mapPath = "resources/Maps/FrozenThrone/(2)EchoIsles.w3x";
            FILE* f = std::fopen(mapPath.c_str(), "rb");
            if (f) {
                std::fclose(f);
                if (!session.LoadMap(mpq, sim, scene, presentation, mapPath)) {
                    return false;
                }
                retailMapLoaded = true;
            }

            CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
            heroUnit = sim.AdoptUnit(std::make_unique<CUnit>(
                sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
            sim.Update();
            presentation.CaptureSimulationTick(sim);
            return heroUnit != nullptr;
        }

        void OnFixedTick(engine::EngineKernel& /*kernel*/, float /*fixedDt*/) override {
            ++fixedTicks;
            if (heroUnit) {
                heroUnit->SetPosition(static_cast<float>(fixedTicks * 50), 0.0f);
            }
            sim.Update();
            presentation.CaptureSimulationTick(sim);
        }

        void OnFrameUpdate(engine::EngineKernel& /*kernel*/, float /*dt*/, float alpha) override {
            ++frameUpdates;
            lastAlpha = alpha;
            presentation.SetInterpolationAlpha(alpha);
        }

        void OnRender(engine::EngineKernel& /*kernel*/, int /*screenW*/, int /*screenH*/) override {
            ++renderPasses;
            Vec3 camEye(0.0f, 900.0f, 900.0f);
            Mat4 view = LookAt(camEye, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
            Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);
            scene.RenderAll(view, proj, 0.016f);
        }

        void OnShutdown(engine::EngineKernel& /*kernel*/) override {
            if (retailMapLoaded) {
                session.TeardownCurrentSession(mpq, sim, scene, presentation);
            }
            shutDownCleanly = true;
        }

        MpqManager mpq;
        CSimulation sim;
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;
        CUnit* heroUnit = nullptr;
        bool retailMapLoaded = false;
        bool shutDownCleanly = false;
        int fixedTicks = 0;
        int frameUpdates = 0;
        int renderPasses = 0;
        float lastAlpha = 0.0f;
    };

    engine::EngineKernel kernel;
    TEST_ASSERT(kernel.BootstrapStandardSubsystems(2, 0.05f));
    TEST_ASSERT(kernel.Jobs() != nullptr && kernel.Jobs()->IsInitialized());
    TEST_ASSERT(kernel.Resources() != nullptr);
    TEST_ASSERT(kernel.Audio() != nullptr);
    TEST_ASSERT(kernel.RenderServer() != nullptr);
    TEST_ASSERT(kernel.RenderGraph() != nullptr);
    TEST_ASSERT(kernel.Input() != nullptr);
    TEST_ASSERT(kernel.Scripting() != nullptr);
    TEST_ASSERT(kernel.Profiler() != nullptr);

    auto mod = std::make_unique<ExperimentalRtsGameModule>();
    auto* modRaw = mod.get();
    TEST_ASSERT(kernel.MountGameModule(std::move(mod)));

    // Step 1 frame of 75ms -> triggers 1 fixed tick (50ms, hero moves 0 -> 50) + remainder 25ms (alpha = 0.50)
    TEST_ASSERT(kernel.StepFrame(0.075f, 1280, 720) == 1);
    TEST_ASSERT(modRaw->fixedTicks == 1);
    TEST_ASSERT(modRaw->frameUpdates == 1);
    TEST_ASSERT(modRaw->renderPasses == 1);
    TEST_NEAR(modRaw->lastAlpha, 0.50f, 1e-3f);

    // Sample interpolated hero position at alpha = 0.50 -> midpoint between 0 and 50 is 25.0f!
    render::RenderTransformSnapshot snap;
    TEST_ASSERT(modRaw->presentation.SampleInterpolatedUnit(modRaw->heroUnit->GetHandleId(), snap));
    TEST_NEAR(snap.position.x, 25.0f, 1e-2f);

    if (modRaw->retailMapLoaded) {
        TEST_ASSERT(modRaw->session.GetStats().terrainChunksBuilt > 0);
        TEST_ASSERT(modRaw->scene.GetRenderGraph().GetExecutedPassCount() == 7);
    }

    const engine::EngineKernelStats kStats = kernel.GetStats();
    TEST_ASSERT(kStats.framesExecuted == 1);
    TEST_ASSERT(kStats.fixedTicksExecuted == 1);
    TEST_ASSERT(kStats.hasMountedGameModule);
    TEST_ASSERT(kStats.activeGameModuleName == "ExperimentalWarcraft3Module");
    TEST_ASSERT(kStats.activeSubsystems == 7);

    kernel.Shutdown();
    return true;
}

bool TestPhase12StageGG1UnifiedSlkMergeAndColdUnitScriptOverridesPool() {
    // 1. Verify unified slk_merge.h & ObjectMetaTable deduplication
    const std::string metaSlkText =
        "ID;PWXL;N;E\n"
        "B;X6;Y3;D0\n"
        "C;X1;Y1;K\"ID\"\n"
        "C;X2;Y1;K\"slk\"\n"
        "C;X3;Y1;K\"field\"\n"
        "C;X4;Y1;K\"data\"\n"
        "C;X5;Y1;K\"repeat\"\n"
        "C;X6;Y1;K\"index\"\n"
        "C;X1;Y2;K\"unam\"\n"
        "C;X2;Y2;K\"UnitUI\"\n"
        "C;X3;Y2;K\"Name\"\n"
        "C;X4;Y2;K\"0\"\n"
        "C;X5;Y2;K\"0\"\n"
        "C;X6;Y2;K\"-1\"\n"
        "C;X1;Y3;K\"DataA\"\n"
        "C;X2;Y3;K\"AbilityData\"\n"
        "C;X3;Y3;K\"Data\"\n"
        "C;X4;Y3;K\"1\"\n"
        "C;X5;Y3;K\"3\"\n"
        "C;X6;Y3;K\"-1\"\n"
        "E\n";
    SlkTable metaSlk;
    TEST_ASSERT(metaSlk.Load(reinterpret_cast<const uint8_t*>(metaSlkText.data()), metaSlkText.size()));

    ObjectMetaTable metaTbl;
    TEST_ASSERT(metaTbl.Load(metaSlk));

    ObjectModification mod1;
    // Little-endian FourCC matching binary w3u/w3a reader so mod1.id.ToString() == "unam"
    mod1.id = War3ID(static_cast<uint32_t>('u') |
                     (static_cast<uint32_t>('n') << 8) |
                     (static_cast<uint32_t>('a') << 16) |
                     (static_cast<uint32_t>('m') << 24));
    mod1.varType = ObjectModification::STRING;
    mod1.stringValue = "Elite Footman";

    std::string outSlk, outField;
    TEST_ASSERT(ResolveObjectModField(metaTbl, mod1, outSlk, outField));
    TEST_ASSERT(outSlk == "UnitUI" && outField == "Name");

    std::string singleField;
    TEST_ASSERT(ResolveObjectModField(metaTbl.accessor, mod1, singleField));
    TEST_ASSERT(singleField == "Name");

    // Verify ObjectMetaTable copy/move rebinds accessor.table to ownedSlk
    ObjectMetaTable copiedMeta = metaTbl;
    TEST_ASSERT(copiedMeta.accessor.table == &copiedMeta.ownedSlk);
    std::string copiedSlk, copiedField;
    TEST_ASSERT(ResolveObjectModField(copiedMeta, mod1, copiedSlk, copiedField));
    TEST_ASSERT(copiedSlk == "UnitUI" && copiedField == "Name");

    ObjectMetaTable movedMeta = std::move(copiedMeta);
    TEST_ASSERT(movedMeta.accessor.table == &movedMeta.ownedSlk);
    std::string movedSlk, movedField;
    TEST_ASSERT(ResolveObjectModField(movedMeta, mod1, movedSlk, movedField));
    TEST_ASSERT(movedSlk == "UnitUI" && movedField == "Name");

    // 2. Verify Cold UnitScriptOverridesComponent lazy pooling on CUnit
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);

    std::vector<CUnit*> standardUnits;
    for (int i = 0; i < 20; ++i) {
        standardUnits.push_back(sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
            static_cast<float>(i * 20), 0.0f, 0.0f)));
    }
    sim.Update();

    // Standard units never touch sharedVision or unitTypeFlags -> 0 entries in UnitScriptOverridesComponent pool!
    TEST_ASSERT(sim.Registry().Pool<UnitScriptOverridesComponent>().Size() == 0);
    for (const CUnit* u : standardUnits) {
        TEST_ASSERT(u->SharedVisionPlayers().empty());
        TEST_ASSERT(!u->HasUnitTypeFlag(7));
    }
    // Reading const getters still leaves the pool at 0 allocations!
    TEST_ASSERT(sim.Registry().Pool<UnitScriptOverridesComponent>().Size() == 0);

    // Mutate script overrides on only 1 of the 20 units -> pool size becomes 1
    CUnit* specialUnit = standardUnits[0];
    specialUnit->SetSharesVisionWith(2, true);
    specialUnit->AddUnitTypeFlag(42);
    TEST_ASSERT(sim.Registry().Pool<UnitScriptOverridesComponent>().Size() == 1);
    TEST_ASSERT(specialUnit->SharedVisionPlayers().count(2) == 1);
    TEST_ASSERT(specialUnit->HasUnitTypeFlag(42));

    // Removing the unit automatically purges its UnitScriptOverridesComponent entry
    sim.RemoveUnit(specialUnit);
    sim.Update();
    TEST_ASSERT(sim.Registry().Pool<UnitScriptOverridesComponent>().Size() == 0);

    return true;
}


// ============================================================================
// Phase 13 Stage HH1-II2: Independent War3GameModule Decomposition,
// Module-Scoped Ability Catalog Mount/Unload & Dynamic Shared Library (.so) Loader
// ============================================================================

bool TestPhase13StageHH1War3GameModuleDecoupledUnitAndAbilityAssembly() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Populate synthetic unit types in TxtTables: Human Peasant (hpea), Town Hall (htow), Paladin Hero (Hpal), Undead Acolyte (uaco)
    udTxt.SetString("hpea", "race", "human");
    udTxt.SetString("hpea", "isbldg", "0");
    ubTxt.SetString("hpea", "HP", "220");
    ubTxt.SetString("hpea", "spd", "190");
    ubTxt.SetString("hpea", "fused", "1");
    ubTxt.SetString("hpea", "builds", "htow,hbar");
    uwTxt.SetString("hpea", "weapsOn", "1");
    uwTxt.SetString("hpea", "atkType1", "normal");
    uaTxt.SetString("hpea", "abilList", "Ahar,Arep");

    udTxt.SetString("uaco", "race", "undead");
    udTxt.SetString("uaco", "isbldg", "0");
    ubTxt.SetString("uaco", "HP", "220");
    ubTxt.SetString("uaco", "spd", "220");
    ubTxt.SetString("uaco", "fused", "1");

    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");
    ubTxt.SetString("htow", "fmade", "12");
    ubTxt.SetString("htow", "trains", "hpea");
    ubTxt.SetString("htow", "Upgrade", "hkee");

    udTxt.SetString("Hpal", "race", "human");
    udTxt.SetString("Hpal", "isbldg", "0");
    ubTxt.SetString("Hpal", "HP", "650");
    ubTxt.SetString("Hpal", "spd", "270");
    ubTxt.SetString("Hpal", "fused", "5");
    ubTxt.SetString("Hpal", "isHero", "1");
    ubTxt.SetString("Hpal", "primary", "STR");
    uaTxt.SetString("Hpal", "abilList", "AInv");
    uaTxt.SetString("Hpal", "heroAbilList", "AHhb,AHds,AHad,AHre");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    // 1. Test Pure Engine Mode (NullSimulationModule): CSimulation creates bare ECS CUnits with 0 War3 abilities!
    CSimulation pureSim;
    pureSim.SetUnitData(&unitData);
    pureSim.AddPlayer(std::make_unique<CPlayer>(0, "PureP0", CMapControl::USER));
    pureSim.SetPureEngineMode(true);
    TEST_ASSERT(pureSim.IsPureEngineMode());
    TEST_ASSERT(std::string(pureSim.GetSimulationModule()->GetModuleId()) == "NullPureEngineModule");

    CUnit* barePeasant = pureSim.CreateUnit(War3ID("hpea"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* bareHall    = pureSim.CreateUnit(War3ID("htow"), 0, 200.0f, 0.0f, 0.0f);
    TEST_ASSERT(barePeasant != nullptr && bareHall != nullptr);
    TEST_ASSERT(barePeasant->GetAbilities().empty());
    TEST_ASSERT(bareHall->GetAbilities().empty());
    pureSim.Update();
    // In Pure Engine Mode, War3 food & techtree rules are not applied
    TEST_ASSERT(pureSim.GetPlayer(0)->GetFoodUsed() == 0);
    TEST_ASSERT(pureSim.GetPlayer(0)->GetFoodCap() == 0);

    // 2. Mount War3GameModule dynamically onto CSimulation and verify full War3 archetype assembly!
    CSimulation war3Sim;
    war3Sim.SetUnitData(&unitData);
    war3Sim.AddPlayer(std::make_unique<CPlayer>(0, "War3P0", CMapControl::USER));
    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    war3Sim.SetSimulationModule(war3Mod);
    TEST_ASSERT(!war3Sim.IsPureEngineMode());
    TEST_ASSERT(war3Sim.GetSimulationModule() == war3Mod.get());

    CUnit* peasant = war3Sim.CreateUnit(War3ID("hpea"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* acolyte = war3Sim.CreateUnit(War3ID("uaco"), 0, 64.0f, 0.0f, 0.0f);
    CUnit* hall    = war3Sim.CreateUnit(War3ID("htow"), 0, 256.0f, 0.0f, 0.0f);
    CUnit* paladin = war3Sim.CreateUnit(War3ID("Hpal"), 0, -128.0f, 0.0f, 0.0f);
    TEST_ASSERT(peasant && acolyte && hall && paladin);

    // Verify War3GameModule assembled peasant's Attack, Move, Harvest, Human Build (AHbu), and Repair (Arep)
    TEST_ASSERT(peasant->GetAbility(War3ID("Aatk")) != nullptr);
    TEST_ASSERT(peasant->GetAbility(War3ID("Amov")) != nullptr);
    TEST_ASSERT(peasant->GetAbility(War3ID("Ahar")) != nullptr);
    TEST_ASSERT(peasant->GetAbility(War3ID("AHbu")) != nullptr);
    TEST_ASSERT(peasant->GetAbility(War3ID("Arep")) != nullptr);

    // Verify Undead Acolyte got auto-harvest with 0 lumber capacity
    auto* acoHarvest = dynamic_cast<CAbilityHarvest*>(acolyte->GetAbility(War3ID("Ahar")));
    TEST_ASSERT(acoHarvest != nullptr && acoHarvest->GetLumberCapacity() == 0);

    // Verify Town Hall got Queue (Aque), Rally (ARal), Upgrade (Aupg), and ReturnResources (Aret)
    TEST_ASSERT(hall->GetAbility(War3ID("Aque")) != nullptr);
    TEST_ASSERT(hall->GetAbility(War3ID("ARal")) != nullptr);
    TEST_ASSERT(hall->GetAbility(War3ID("Aupg")) != nullptr);
    TEST_ASSERT(hall->GetAbility(War3ID("Aret")) != nullptr);

    // Verify Paladin got Hero (AHer) and Inventory (AInv)
    TEST_ASSERT(paladin->GetAbility(War3ID("AHer")) != nullptr);
    TEST_ASSERT(paladin->GetAbility(War3ID("AInv")) != nullptr);

    war3Sim.Update();
    // Food used = 1 (peasant) + 1 (acolyte) + 5 (paladin) = 7; Food cap = 12 (town hall)
    TEST_ASSERT(war3Sim.GetPlayer(0)->GetFoodUsed() == 7);
    TEST_ASSERT(war3Sim.GetPlayer(0)->GetFoodCap() == 12);

    const engine::SimulationModuleStats& mStats = war3Mod->GetModuleStats();
    TEST_ASSERT(mStats.unitsAssembled == 4);
    TEST_ASSERT(mStats.abilitiesAttached >= 14);
    TEST_ASSERT(mStats.unitsCommitted == 4);

    // Remove peasant and step -> War3GameModule::OnUnitRemovedFromWorld refunds 1 food
    war3Sim.RemoveUnit(peasant);
    war3Sim.Update();
    TEST_ASSERT(war3Sim.GetPlayer(0)->GetFoodUsed() == 6);
    TEST_ASSERT(war3Mod->GetModuleStats().unitsRemoved == 1);

    return true;
}

bool TestPhase13StageHH2ModuleScopedAbilityCatalogMountAndUnload() {
    // 1. Verify War3GameModule owns the ~190 standard WarCraft 3 ability creators
    AbilityFactory::EnsureDefaultCatalogRegistered();
    const size_t war3Count = AbilityFactory::GetModuleRegisteredCount(war3::War3GameModule::kModuleId);
    TEST_ASSERT(war3Count >= 180);
    TEST_ASSERT(AbilityFactory::GetCreatorModuleId(War3ID("AHbz")) == war3::War3GameModule::kModuleId);
    TEST_ASSERT(AbilityFactory::GetCreatorModuleId(War3ID("Aatk")) == war3::War3GameModule::kModuleId);

    // 2. Register a custom expansion module's abilities ("SciFiTerranModule")
    const std::string customMod = "SciFiTerranModule";
    const War3ID stimCode("Stmp");
    const War3ID empCode("EmpB");
    AbilityFactory::RegisterModuleSpellDescriptor(
        customMod,
        ABSpellDescriptor::MakeAreaHealAndDispel(empCode, 300.0f, 150.0f, true, 600.0f, 12.0f, 75));
    AbilityFactory::RegisterModuleAlias(customMod, stimCode, War3ID("Absk"));

    TEST_ASSERT(AbilityFactory::GetModuleRegisteredCount(customMod) == 2);
    TEST_ASSERT(AbilityFactory::GetCreatorModuleId(empCode) == customMod);
    TEST_ASSERT(AbilityFactory::GetCreatorModuleId(stimCode) == customMod);

    auto empAbil = AbilityFactory::CreateAbility(9001, empCode);
    TEST_ASSERT(empAbil != nullptr && empAbil->GetAlias() == empCode);

    // 3. Unload "SciFiTerranModule" -> its 2 creators are cleanly removed while War3GameModule remains intact
    const size_t removedCustom = AbilityFactory::UnregisterModuleCreators(customMod);
    TEST_ASSERT(removedCustom == 2);
    TEST_ASSERT(AbilityFactory::GetModuleRegisteredCount(customMod) == 0);
    TEST_ASSERT(AbilityFactory::GetModuleRegisteredCount(war3::War3GameModule::kModuleId) == war3Count);

    // 4. Test unloading and re-mounting War3GameModule's catalog with auto-bootstrap disabled
    AbilityFactory::SetAutoBootstrapDefaultCatalog(false);
    const size_t unloadedWar3 = war3::War3GameModule::UnregisterWar3AbilityCatalog();
    TEST_ASSERT(unloadedWar3 == war3Count);
    TEST_ASSERT(AbilityFactory::GetModuleRegisteredCount(war3::War3GameModule::kModuleId) == 0);

    // Re-register War3GameModule catalog and restore auto-bootstrap
    war3::War3GameModule::RegisterWar3AbilityCatalog();
    AbilityFactory::SetAutoBootstrapDefaultCatalog(true);
    TEST_ASSERT(AbilityFactory::GetModuleRegisteredCount(war3::War3GameModule::kModuleId) == war3Count);

    return true;
}

bool TestPhase13StageII1DynamicModuleLoaderSharedLibraryDlopenAndHotReload() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();

    // 1. Verify built-in factory registration for "War3GameModule"
    TEST_ASSERT(loader.HasModule(war3::War3GameModule::kModuleId));
    auto builtInSimMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
    auto builtInGameMod = loader.CreateGameModule(war3::War3GameModule::kModuleId);
    TEST_ASSERT(builtInSimMod != nullptr && builtInGameMod != nullptr);
    TEST_ASSERT(std::string(builtInSimMod->GetModuleId()) == war3::War3GameModule::kModuleId);
    TEST_ASSERT(std::string(builtInGameMod->GetModuleName()) == war3::War3GameModule::kModuleId);

    // 2. Load the compiled shared library plugin ./libwar3_module_plugin.so via OS dlopen/dlsym
    std::string soPath = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3DynamicPluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath));
        TEST_ASSERT(loader.IsSharedLibraryLoaded(pluginKey));
        TEST_ASSERT(loader.GetModuleGeneration(pluginKey) == 1u);

        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            auto dynGameMod = loader.CreateGameModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr && dynGameMod != nullptr);
            TEST_ASSERT(std::string(dynSimMod->GetModuleId()) == war3::War3GameModule::kModuleId);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 13u);
            TEST_ASSERT(dynGameMod->GetCustomDestroyFn() != nullptr);

            // Attach dynamically loaded .so module to a live CSimulation and spawn a Footman
            CSimulation sim;
            sim.AddPlayer(std::make_unique<CPlayer>(0, "DynPlayer", CMapControl::USER));
            sim.SetSimulationModule(dynSimMod);

            CUnitType* footmanType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
            CUnit u(sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f);
            dynSimMod->OnAssembleUnitAbilities(sim, u, *footmanType);
            TEST_ASSERT(u.GetAbility(War3ID("Aatk")) != nullptr);
            TEST_ASSERT(u.GetAbility(War3ID("Amov")) != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleStats().unitsAssembled == 1);
            sim.SetPureEngineMode(true); // Detach before unloading .so
        }

        // Perform live hot-reload of the shared library and verify generation bumps 1 -> 2
        TEST_ASSERT(loader.HotReloadSharedLibrary(pluginKey));
        TEST_ASSERT(loader.GetModuleGeneration(pluginKey) == 2u);
        TEST_ASSERT(loader.GetStats().totalHotReloads >= 1);

        // Unload shared library cleanly via dlclose
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
        TEST_ASSERT(!loader.IsSharedLibraryLoaded(pluginKey));
    }

    return true;
}

bool TestPhase13StageII2War3GameModuleRetailDriveMapsEndToEndSuite() {
    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TurtleRock.w3x",
        "resources/Maps/FrozenThrone/(8)GardenOfWar.w3x",
        "resources/Maps/FrozenThrone/Scenario/(6)BlizzardTD.w3x",
        "resources/Maps/FrozenThrone/Scenario/(8)AzerothGrandPrix.w3x"
    };

    auto& loader = engine::DynamicModuleLoader::Instance();
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();

    int mapsVerified = 0;
    for (const char* mapPath : kCloudMaps) {
        FILE* f = std::fopen(mapPath, "rb");
        if (!f) continue;
        std::fclose(f);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        auto gameMod = loader.CreateGameModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod != nullptr && gameMod != nullptr);

        MpqManager mpq;
        CSimulation sim;
        sim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, sim, scene, presentation, mapPath));

        auto* war3GameMod = dynamic_cast<war3::War3GameModule*>(gameMod.get());
        TEST_ASSERT(war3GameMod != nullptr);
        war3GameMod->BindSimulation(&sim);

        engine::EngineKernel kernel;
        TEST_ASSERT(kernel.BootstrapStandardSubsystems(2, 0.05f));
        TEST_ASSERT(kernel.MountGameModule(std::move(gameMod)));

        const int ticksBefore = sim.GetGameTurnTick();
        TEST_ASSERT(kernel.StepFrame(0.10f, 1280, 720) == 2);
        TEST_ASSERT(sim.GetGameTurnTick() == ticksBefore + 2);

        session.TeardownCurrentSession(mpq, sim, scene, presentation);
        kernel.Shutdown();
        ++mapsVerified;
    }

    return true;
}

// ============================================================================
// Phase 14 Tests (Stage JJ1, JJ2, KK1, KK2):
// CPlayer Techtree/Upgrade System & Victory/Defeat Rules Extracted into War3GameModule
// ============================================================================

bool TestPhase14StageJJ1War3TechtreeTierEquivalencesAndBuildingMorph() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Town Hall (htow) -> Keep (hkee) -> Castle (hcas)
    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");
    ubTxt.SetString("htow", "fmade", "12");
    ubTxt.SetString("htow", "Upgrade", "hkee");

    udTxt.SetString("hkee", "race", "human");
    udTxt.SetString("hkee", "isbldg", "1");
    ubTxt.SetString("hkee", "HP", "1750");
    ubTxt.SetString("hkee", "fmade", "12");
    ubTxt.SetString("hkee", "goldcost", "320");
    ubTxt.SetString("hkee", "lumbercost", "210");
    ubTxt.SetString("hkee", "bldtm", "2");
    ubTxt.SetString("hkee", "Upgrade", "hcas");

    udTxt.SetString("hcas", "race", "human");
    udTxt.SetString("hcas", "isbldg", "1");
    ubTxt.SetString("hcas", "HP", "2000");
    ubTxt.SetString("hcas", "fmade", "12");
    ubTxt.SetString("hcas", "goldcost", "360");
    ubTxt.SetString("hcas", "lumbercost", "210");
    ubTxt.SetString("hcas", "bldtm", "2");

    // Workshop (harm) requires Keep (hkee); Gryphon Aviary (hgra) requires Castle (hcas)
    udTxt.SetString("harm", "race", "human");
    udTxt.SetString("harm", "isbldg", "1");
    udTxt.SetString("harm", "Requires", "hkee");
    ubTxt.SetString("harm", "HP", "1200");

    udTxt.SetString("hgra", "race", "human");
    udTxt.SetString("hgra", "isbldg", "1");
    udTxt.SetString("hgra", "Requires", "hcas");
    ubTxt.SetString("hgra", "HP", "1200");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    auto playerPtr = std::make_unique<CPlayer>(0, "HumanP0", CMapControl::USER);
    playerPtr->SetGold(5000);
    playerPtr->SetLumber(5000);
    sim.AddPlayer(std::move(playerPtr));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    CUnit* hall = sim.CreateUnit(War3ID("htow"), 0, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT(hall != nullptr);
    sim.Update();

    CPlayer* p0 = sim.GetPlayer(0);
    TEST_ASSERT(p0->GetTechLevel(War3ID("htow")) == 1);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("htow")) == 1);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("hkee")) == 0);
    TEST_ASSERT(!TechTree::MeetsRequirements(sim, 0, War3ID("harm")));
    TEST_ASSERT(!TechTree::MeetsRequirements(sim, 0, War3ID("hgra")));

    // 1. Upgrade Town Hall (htow) -> Keep (hkee) via CAbilityUpgrade
    auto* upgAbil = dynamic_cast<CAbilityUpgrade*>(hall->GetAbility(War3ID("Aupg")));
    TEST_ASSERT(upgAbil != nullptr);
    upgAbil->BeginNoTarget(sim, hall, static_cast<int>(War3ID("hkee").value));
    for (int i = 0; i < 45; ++i) {
        sim.Update();
    }
    TEST_ASSERT(hall->GetTypeId() == War3ID("hkee"));
    TEST_ASSERT(war3Mod->GetModuleStats().unitsMorphed >= 1);

    // Direct tech level for htow is now 0, hkee is 1, but EffectiveTechLevel(htow) is still 1 via War3 equivalence!
    TEST_ASSERT(p0->GetTechLevel(War3ID("htow")) == 0);
    TEST_ASSERT(p0->GetTechLevel(War3ID("hkee")) == 1);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("htow")) == 1);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("hkee")) == 1);
    TEST_ASSERT(TechTree::MeetsRequirements(sim, 0, War3ID("harm")));
    TEST_ASSERT(!TechTree::MeetsRequirements(sim, 0, War3ID("hgra")));

    // 2. Morph Keep (hkee) -> Castle (hcas)
    const CUnitType* oldKeepType = hall->GetUnitType();
    CUnitType* castleType = unitData.GetUnitType(War3ID("hcas"));
    hall->MorphTo(castleType, War3ID("hcas"));
    war3Mod->OnUnitMorphed(sim, *hall, oldKeepType, War3ID("hkee").value, castleType, War3ID("hcas").value);

    // Castle (hcas) satisfies requirements for Castle, Keep, AND Town Hall!
    TEST_ASSERT(p0->GetTechLevel(War3ID("htow")) == 0);
    TEST_ASSERT(p0->GetTechLevel(War3ID("hkee")) == 0);
    TEST_ASSERT(p0->GetTechLevel(War3ID("hcas")) == 1);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("htow")) == 1);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("hkee")) == 1);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("hcas")) == 1);
    TEST_ASSERT(TechTree::MeetsRequirements(sim, 0, War3ID("harm")));
    TEST_ASSERT(TechTree::MeetsRequirements(sim, 0, War3ID("hgra")));

    // 3. Switch to PureEngineMode (NullSimulationModule) -> War3 equivalences are disabled
    sim.SetPureEngineMode(true);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("hkee")) == 0);
    sim.SetSimulationModule(war3Mod);
    TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("hkee")) == 1);

    return true;
}

bool TestPhase14StageJJ2War3UpgradeEffectEngineLiveAndSpawnedUnits() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Footman (hfoo) uses upgrades Rhme (melee attack), Rhar (armor), Rhac (HP + speed)
    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "420");
    ubTxt.SetString("hfoo", "spd", "270");
    ubTxt.SetString("hfoo", "def", "2");
    ubTxt.SetString("hfoo", "defUp", "2");
    ubTxt.SetString("hfoo", "upgrades", "Rhme,Rhar,Rhac");
    uwTxt.SetString("hfoo", "weapsOn", "1");
    uwTxt.SetString("hfoo", "atkType1", "normal");
    uwTxt.SetString("hfoo", "dmgplus1", "11");
    uwTxt.SetString("hfoo", "dice1", "1");
    uwTxt.SetString("hfoo", "sides1", "2");
    uwTxt.SetString("hfoo", "dmgUp1", "2");

    // Priest (hmpr) uses Rhpt (mana + heal ability level)
    udTxt.SetString("hmpr", "race", "human");
    udTxt.SetString("hmpr", "isbldg", "0");
    ubTxt.SetString("hmpr", "HP", "290");
    ubTxt.SetString("hmpr", "mana0", "200");
    ubTxt.SetString("hmpr", "manaN", "200");
    ubTxt.SetString("hmpr", "spd", "270");
    ubTxt.SetString("hmpr", "upgrades", "Rhpt");
    uaTxt.SetString("hmpr", "abilList", "Ahea");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);
    CUpgradeData upgradeData;

    // 1. Register Iron Plating (Rhar): rarm (+2 armor per level)
    {
        auto upg = std::make_unique<CUpgradeType>();
        upg->id = War3ID("Rhar");
        upg->name = "Iron Plating";
        upg->upgradeClass = "armor";
        upg->maxLevel = 3;
        upg->race = 1;
        upg->effects.push_back(CUpgradeEffectSpec{"rarm", 2.0f, 0.0f, ""});
        const War3ID upgId = upg->id; upgradeData.RegisterUpgrade(upgId, std::move(upg));
    }
    // 2. Register Iron Forged Swords (Rhme): ratx (+3 dmg at L1, +6 at L2 -> cumulative +9)
    {
        auto upg = std::make_unique<CUpgradeType>();
        upg->id = War3ID("Rhme");
        upg->name = "Iron Forged Swords";
        upg->upgradeClass = "melee";
        upg->maxLevel = 3;
        upg->race = 1;
        upg->effects.push_back(CUpgradeEffectSpec{"ratx", 3.0f, 3.0f, ""});
        const War3ID upgId = upg->id; upgradeData.RegisterUpgrade(upgId, std::move(upg));
    }
    // 3. Register War Training (Rhac): rhpx (+150 HP) + rmvx (+0.20 speed modifier)
    {
        auto upg = std::make_unique<CUpgradeType>();
        upg->id = War3ID("Rhac");
        upg->name = "War Training";
        upg->maxLevel = 1;
        upg->race = 1;
        upg->effects.push_back(CUpgradeEffectSpec{"rhpx", 150.0f, 0.0f, ""});
        upg->effects.push_back(CUpgradeEffectSpec{"rmvx", 0.20f, 0.0f, ""});
        const War3ID upgId = upg->id; upgradeData.RegisterUpgrade(upgId, std::move(upg));
    }
    // 4. Register Priest Adept Training (Rhpt): rmnx (+100 mana per level) + rlev (Ahea)
    {
        auto upg = std::make_unique<CUpgradeType>();
        upg->id = War3ID("Rhpt");
        upg->name = "Priest Adept Training";
        upg->maxLevel = 2;
        upg->race = 1;
        upg->effects.push_back(CUpgradeEffectSpec{"rmnx", 100.0f, 0.0f, ""});
        upg->effects.push_back(CUpgradeEffectSpec{"rlev", 1.0f, 0.0f, "Ahea"});
        const War3ID upgId = upg->id; upgradeData.RegisterUpgrade(upgId, std::move(upg));
    }

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.SetUpgradeData(&upgradeData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "Upgrader", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    sim.SetSimulationModule(war3Mod);

    CUnit* foot1 = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* priest = sim.CreateUnit(War3ID("hmpr"), 0, 64.0f, 0.0f, 0.0f);
    TEST_ASSERT(foot1 != nullptr && priest != nullptr);
    sim.Update();

    TEST_ASSERT(std::fabs(foot1->GetMaxLife() - 420.0f) < 0.01f);
    TEST_ASSERT(std::fabs(static_cast<float>(foot1->GetBonusArmor()) - 0.0f) < 0.01f);
    TEST_ASSERT(foot1->GetNonStackingDamageTotal() == 0);
    TEST_ASSERT(priest->GetMaxMana() == 200);

    // Research Rhar level 1 & level 2 -> foot1 gains +4 perm defense bonus (2 + 2)
    sim.SetPlayerTechLevel(0, War3ID("Rhar"), 1);
    TEST_ASSERT(std::fabs(static_cast<float>(foot1->GetBonusArmor()) - 2.0f) < 0.01f);
    sim.SetPlayerTechLevel(0, War3ID("Rhar"), 2);
    TEST_ASSERT(std::fabs(static_cast<float>(foot1->GetBonusArmor()) - 4.0f) < 0.01f);

    // Research Rhme level 2 -> foot1 gains +9 attack damage bonus (3 + 6)
    sim.SetPlayerTechLevel(0, War3ID("Rhme"), 2);
    TEST_ASSERT(foot1->GetNonStackingDamageTotal() == 9);

    // Research Rhac level 1 -> foot1 gains +150 max HP (420 -> 570) and +20% speed (270 -> 324)
    sim.SetPlayerTechLevel(0, War3ID("Rhac"), 1);
    TEST_ASSERT(std::fabs(foot1->GetMaxLife() - 570.0f) < 0.01f);
    TEST_ASSERT(std::fabs(foot1->GetLife() - 570.0f) < 0.01f);
    TEST_ASSERT(std::fabs(foot1->GetSpeed() - 324.0f) < 0.5f);

    // Research Rhpt level 1 -> priest gains +100 max mana (200 -> 300) and Ahea level 2 (foot1 unaffected!)
    sim.SetPlayerTechLevel(0, War3ID("Rhpt"), 1);
    TEST_ASSERT(priest->GetMaxMana() == 300);
    TEST_ASSERT(priest->GetAbility(War3ID("Ahea")) != nullptr);
    TEST_ASSERT(priest->GetAbility(War3ID("Ahea"))->GetLevel() == 2);
    TEST_ASSERT(std::fabs(foot1->GetMaxLife() - 570.0f) < 0.01f);

    // Spawn a new Footman (foot2) AFTER upgrades are researched -> automatically receives Rhar(L2), Rhme(L2), Rhac(L1)!
    CUnit* foot2 = sim.CreateUnit(War3ID("hfoo"), 0, 128.0f, 0.0f, 0.0f);
    TEST_ASSERT(foot2 != nullptr);
    sim.Update();
    TEST_ASSERT(std::fabs(static_cast<float>(foot2->GetBonusArmor()) - 4.0f) < 0.01f);
    TEST_ASSERT(foot2->GetNonStackingDamageTotal() == 9);
    TEST_ASSERT(std::fabs(foot2->GetMaxLife() - 570.0f) < 0.01f);
    TEST_ASSERT(std::fabs(foot2->GetSpeed() - 324.0f) < 0.5f);
    TEST_ASSERT(war3Mod->GetModuleStats().unitUpgradesApplied >= 7);

    return true;
}

bool TestPhase14StageKK1War3MeleeAndAlliedVictoryDefeatAndCrippledRules() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");

    udTxt.SetString("hbar", "race", "human");
    udTxt.SetString("hbar", "isbldg", "1");
    ubTxt.SetString("hbar", "HP", "1200");

    udTxt.SetString("hpea", "race", "human");
    udTxt.SetString("hpea", "isbldg", "0");
    ubTxt.SetString("hpea", "HP", "220");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);

    // 3 competitors: Player 0 & Player 1 (mutually allied with alliedVictory=true) vs Player 2 (enemy)
    sim.AddPlayer(std::make_unique<CPlayer>(0, "Ally0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "Ally1", CMapControl::COMPUTER));
    sim.AddPlayer(std::make_unique<CPlayer>(2, "Enemy2", CMapControl::COMPUTER));

    CPlayer* p0 = sim.GetPlayer(0);
    CPlayer* p1 = sim.GetPlayer(1);
    CPlayer* p2 = sim.GetPlayer(2);
    p0->SetAlliance(1, CAllianceType::ALLIED, true);
    p1->SetAlliance(0, CAllianceType::ALLIED, true);
    p0->SetAlliedVictory(true);
    p1->SetAlliedVictory(true);

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->SetMeleeVictoryRulesEnabled(true);
    sim.SetSimulationModule(war3Mod);

    int defeatedEvents = 0;
    int victoriousEvents = 0;
    int crippledEvents = 0;
    bool gameEndedSignal = false;
    sim.Events().onPlayerDefeated.Connect([&](int) { ++defeatedEvents; });
    sim.Events().onPlayerVictorious.Connect([&](int) { ++victoriousEvents; });
    sim.Events().onPlayerCrippled.Connect([&](int, bool c) { if (c) ++crippledEvents; });
    sim.Events().onGameEnd.Connect([&](bool v, int) { if (v) gameEndedSignal = true; });

    CUnit* p0Hall = sim.CreateUnit(War3ID("htow"), 0, -512.0f, 0.0f, 0.0f);
    CUnit* p1Hall = sim.CreateUnit(War3ID("htow"), 1, 0.0f, 0.0f, 0.0f);
    CUnit* p2Hall = sim.CreateUnit(War3ID("htow"), 2, 512.0f, 0.0f, 0.0f);
    CUnit* p2Barracks = sim.CreateUnit(War3ID("hbar"), 2, 640.0f, 0.0f, 0.0f);
    TEST_ASSERT(p0Hall && p1Hall && p2Hall && p2Barracks);
    sim.Update();

    TEST_ASSERT(!sim.IsPlayerCrippled(2));
    TEST_ASSERT(!war3Mod->IsMatchEnded());

    // 1. Destroy Player 2 Town Hall while leaving Player 2 Barracks standing (and 0 gold/workers)
    // -> Player 2 becomes Crippled, but is not yet Defeated!
    p2Hall->SetLife(sim, 0.0f);
    sim.Update();
    TEST_ASSERT(p2->IsCrippled());
    TEST_ASSERT(sim.IsPlayerCrippled(2));
    TEST_ASSERT(!p2->IsDefeated());
    TEST_ASSERT(crippledEvents == 1);
    TEST_ASSERT(!war3Mod->IsMatchEnded());

    // 2. Destroy Player 2 remaining Barracks -> Player 2 has 0 living structures -> DEFEAT!
    // And since surviving players P0 and P1 are mutually allied, both achieve VICTORY!
    p2Barracks->SetLife(sim, 0.0f);
    sim.Update();

    TEST_ASSERT(p2->IsDefeated());
    TEST_ASSERT(p0->IsVictorious());
    TEST_ASSERT(p1->IsVictorious());
    TEST_ASSERT(defeatedEvents == 1);
    TEST_ASSERT(victoriousEvents == 2);
    TEST_ASSERT(gameEndedSignal);
    TEST_ASSERT(war3Mod->IsMatchEnded());
    TEST_ASSERT(war3Mod->GetWinningPlayerIndex() == 0);

    return true;
}

bool TestPhase14StageKK2DynamicPluginHotReloadAndCloudRetailMapsPhase14Suite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();

    std::string soPath = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase14PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath));

        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 14u);

            CSimulation sim;
            sim.AddPlayer(std::make_unique<CPlayer>(0, "PluginP0", CMapControl::USER));
            sim.AddPlayer(std::make_unique<CPlayer>(1, "PluginP1", CMapControl::COMPUTER));
            sim.SetSimulationModule(dynSimMod);

            // Verify upkeep tax and techtree equivalence across the dynamic .so boundary
            CPlayer* p0 = sim.GetPlayer(0);
            p0->SetFoodUsed(60); // Low Upkeep (30% tax)
            TEST_ASSERT(p0->GetUpkeepLevel(&sim) == 1);
            TEST_ASSERT(p0->ApplyUpkeepGoldTax(100, &sim) == 70);

            p0->SetTechLevel(sim, War3ID("hcas"), 1);
            TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("htow")) == 1);
            TEST_ASSERT(sim.GetEffectiveTechLevel(0, War3ID("hkee")) == 1);

            sim.SetPureEngineMode(true);
            TEST_ASSERT(p0->GetUpkeepLevel(&sim) == 0);
            TEST_ASSERT(p0->ApplyUpkeepGoldTax(100, &sim) == 100);
        }

        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    return true;
}

// ============================================================================
// Phase 15 Tests (Stage LL1, LL2, MM1, MM2):
// CCombatSystem Armor/Attack Multiplier Matrix & MeleeAIController Tactical AI
// Decoupled into War3GameModule Plugin
// ============================================================================

bool TestPhase15StageLL1War3CombatMatrixArmorCurveAndEtherealVsPureEngine() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // 1. Rifleman (hrif): Pierce attack, Medium armor
    udTxt.SetString("hrif", "race", "human");
    udTxt.SetString("hrif", "isbldg", "0");
    ubTxt.SetString("hrif", "HP", "535");
    ubTxt.SetString("hrif", "def", "0");
    ubTxt.SetString("hrif", "defType", "medium");
    uwTxt.SetString("hrif", "weapsOn", "1");
    uwTxt.SetString("hrif", "atkType1", "pierce");
    uwTxt.SetString("hrif", "dmgplus1", "20");

    // 2. Gryphon Rider (hgry): Magic attack, Light armor (takes 200% from Pierce!)
    udTxt.SetString("hgry", "race", "human");
    udTxt.SetString("hgry", "isbldg", "0");
    ubTxt.SetString("hgry", "HP", "825");
    ubTxt.SetString("hgry", "def", "0");
    ubTxt.SetString("hgry", "defType", "light");
    uwTxt.SetString("hgry", "weapsOn", "1");
    uwTxt.SetString("hgry", "atkType1", "magic");
    uwTxt.SetString("hgry", "dmgplus1", "50");

    // 3. Footman (hfoo): Normal attack, Heavy armor with 10 defense (takes 200% from Magic; 10 armor = 37.5% physical reduction)
    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "1000");
    ubTxt.SetString("hfoo", "def", "10");
    ubTxt.SetString("hfoo", "defType", "heavy");
    uwTxt.SetString("hfoo", "weapsOn", "1");
    uwTxt.SetString("hfoo", "atkType1", "normal");
    uwTxt.SetString("hfoo", "dmgplus1", "12");

    // 4. Scout Tower (hwtw): Fortified armor (takes 35% from Pierce, 150% from Siege)
    udTxt.SetString("hwtw", "race", "human");
    udTxt.SetString("hwtw", "isbldg", "1");
    ubTxt.SetString("hwtw", "HP", "1000");
    ubTxt.SetString("hwtw", "def", "0");
    ubTxt.SetString("hwtw", "defType", "fortified");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "AtkP0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "DefP1", CMapControl::COMPUTER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    sim.SetSimulationModule(war3Mod);

    CUnit* rifle   = sim.CreateUnit(War3ID("hrif"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* gryphon = sim.CreateUnit(War3ID("hgry"), 1, 100.0f, 0.0f, 0.0f);
    CUnit* footman = sim.CreateUnit(War3ID("hfoo"), 1, 200.0f, 0.0f, 0.0f);
    CUnit* tower   = sim.CreateUnit(War3ID("hwtw"), 1, 300.0f, 0.0f, 0.0f);
    TEST_ASSERT(rifle && gryphon && footman && tower);
    sim.Update();

    CCombatSystem* combat = sim.GetCombatSystem();
    TEST_ASSERT(combat != nullptr);

    // A. War3GameModule active: verify canonical 7x8 counter multipliers & 0.06 armor curve
    // Pierce (100 base) vs Light (gryphon, 0 armor) -> 200.0 damage (2.0x)
    DamageResult rPierceVsLight = combat->CalculateDamage(
        rifle, gryphon, true, true, CAttackType::PIERCE, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    TEST_ASSERT(std::fabs(rPierceVsLight.multiplier - 2.0f) < 0.01f);
    TEST_ASSERT(std::fabs(rPierceVsLight.damageDealt - 200.0f) < 0.1f);

    // Pierce (100 base) vs Fortified (tower, 0 armor) -> 35.0 damage (0.35x)
    DamageResult rPierceVsFort = combat->CalculateDamage(
        rifle, tower, true, true, CAttackType::PIERCE, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    TEST_ASSERT(std::fabs(rPierceVsFort.multiplier - 0.35f) < 0.01f);
    TEST_ASSERT(std::fabs(rPierceVsFort.damageDealt - 35.0f) < 0.1f);

    // Magic (100 base) vs Heavy (footman, 10 armor: reduction = 0.6 / 1.6 = 37.5%)
    // Rolled = 100 * 2.0 (Magic vs Heavy) = 200; after 37.5% armor reduction = 125.0 damage!
    DamageResult rMagicVsHeavy = combat->CalculateDamage(
        gryphon, footman, true, true, CAttackType::MAGIC, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    TEST_ASSERT(std::fabs(rMagicVsHeavy.multiplier - 2.0f) < 0.01f);
    TEST_ASSERT(std::fabs(rMagicVsHeavy.damageDealt - 125.0f) < 0.1f);
    TEST_ASSERT(war3Mod->GetModuleStats().combatDamageEvaluations >= 3);

    // B. Switch to PureEngineMode (NullSimulationModule): all attack/defense multipliers become 1.0x and armor reduction is 0!
    sim.SetPureEngineMode(true);
    DamageResult rPurePierceVsLight = combat->CalculateDamage(
        rifle, gryphon, true, true, CAttackType::PIERCE, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    DamageResult rPureMagicVsHeavy = combat->CalculateDamage(
        gryphon, footman, true, true, CAttackType::MAGIC, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    TEST_ASSERT(std::fabs(rPurePierceVsLight.multiplier - 1.0f) < 0.01f);
    TEST_ASSERT(std::fabs(rPurePierceVsLight.damageDealt - 100.0f) < 0.1f);
    TEST_ASSERT(std::fabs(rPureMagicVsHeavy.multiplier - 1.0f) < 0.01f);
    TEST_ASSERT(std::fabs(rPureMagicVsHeavy.damageDealt - 100.0f) < 0.1f);

    return true;
}

bool TestPhase15StageLL2CustomCombatMatrixHotOverrideAndDynamicPluginCombat() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "1000");
    ubTxt.SetString("hfoo", "def", "0");
    ubTxt.SetString("hfoo", "defType", "heavy");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    CUnit* atkUnit = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* defUnit = sim.CreateUnit(War3ID("hfoo"), 1, 64.0f, 0.0f, 0.0f);
    TEST_ASSERT(atkUnit && defUnit);
    sim.Update();

    // 1. Default Normal vs Heavy multiplier is 1.0x
    CCombatSystem* combat = sim.GetCombatSystem();
    DamageResult baseRes = combat->CalculateDamage(
        atkUnit, defUnit, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    TEST_ASSERT(std::fabs(baseRes.damageDealt - 100.0f) < 0.1f);

    // 2. Customize Normal vs Heavy multiplier on War3GameModule to 2.5x -> immediately takes effect in CCombatSystem!
    war3Mod->SetAttackDefenseMultiplier(CAttackType::NORMAL, CDefenseType::HEAVY, 2.5f);
    DamageResult customRes = combat->CalculateDamage(
        atkUnit, defUnit, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    TEST_ASSERT(std::fabs(customRes.multiplier - 2.5f) < 0.01f);
    TEST_ASSERT(std::fabs(customRes.damageDealt - 250.0f) < 0.1f);

    // Reset back to canonical War3 defaults
    war3Mod->ResetCombatRulesToCanonicalDefaults();
    DamageResult resetRes = combat->CalculateDamage(
        atkUnit, defUnit, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
    TEST_ASSERT(std::fabs(resetRes.damageDealt - 100.0f) < 0.1f);

    // 3. Verify dynamically loaded ./libwar3_module_plugin.so exports Version 15 and evaluates War3 combat rules
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase15CombatPluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 15u);
            sim.SetSimulationModule(dynSimMod);

            DamageResult pluginRes = combat->CalculateDamage(
                atkUnit, defUnit, true, false, CAttackType::MAGIC, CDamageType::NORMAL, 100.0f, 0.0f, 0, 0, false);
            TEST_ASSERT(std::fabs(pluginRes.multiplier - 2.0f) < 0.01f);
            TEST_ASSERT(std::fabs(pluginRes.damageDealt - 200.0f) < 0.1f);
            sim.SetPureEngineMode(true);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    return true;
}

bool TestPhase15StageMM1War3ModuleTacticalAiLifecycleAndArmorCounterScoring() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Human Town Hall (htow), Peasant (hpea), Farm (hhou), Barracks (hbar), Rifleman (hrif)
    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");
    ubTxt.SetString("htow", "fmade", "12");
    ubTxt.SetString("htow", "trains", "hpea");

    udTxt.SetString("hpea", "race", "human");
    udTxt.SetString("hpea", "isbldg", "0");
    ubTxt.SetString("hpea", "HP", "220");
    ubTxt.SetString("hpea", "goldcost", "75");
    uaTxt.SetString("hpea", "abilList", "Ahar");

    udTxt.SetString("hhou", "race", "human");
    udTxt.SetString("hhou", "isbldg", "1");
    ubTxt.SetString("hhou", "HP", "500");
    ubTxt.SetString("hhou", "fmade", "6");
    ubTxt.SetString("hhou", "goldcost", "80");

    udTxt.SetString("hbar", "race", "human");
    udTxt.SetString("hbar", "isbldg", "1");
    ubTxt.SetString("hbar", "HP", "1200");
    ubTxt.SetString("hbar", "trains", "hrif");

    // Rifleman (Pierce attack)
    udTxt.SetString("hrif", "race", "human");
    udTxt.SetString("hrif", "isbldg", "0");
    ubTxt.SetString("hrif", "HP", "535");
    ubTxt.SetString("hrif", "goldcost", "205");
    ubTxt.SetString("hrif", "defType", "medium");
    uwTxt.SetString("hrif", "weapsOn", "1");
    uwTxt.SetString("hrif", "atkType1", "pierce");
    uwTxt.SetString("hrif", "dmgplus1", "21");
    uwTxt.SetString("hrif", "range1", "400");

    // Two enemy targets at identical distance & HP ratio:
    // - Enemy Light Armor unit (hgry: takes 200% from Pierce!)
    // - Enemy Fortified Armor unit (hfort: takes only 35% from Pierce!)
    udTxt.SetString("hgry", "race", "orc");
    udTxt.SetString("hgry", "isbldg", "0");
    ubTxt.SetString("hgry", "HP", "500");
    ubTxt.SetString("hgry", "defType", "light");
    uwTxt.SetString("hgry", "weapsOn", "1");
    uwTxt.SetString("hgry", "atkType1", "magic");
    uwTxt.SetString("hgry", "dmgplus1", "20");

    udTxt.SetString("hcrt", "race", "orc");
    udTxt.SetString("hcrt", "isbldg", "0");
    ubTxt.SetString("hcrt", "HP", "500");
    ubTxt.SetString("hcrt", "defType", "fortified");
    uwTxt.SetString("hcrt", "weapsOn", "1");
    uwTxt.SetString("hcrt", "atkType1", "siege");
    uwTxt.SetString("hcrt", "dmgplus1", "20");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "AiP0", CMapControl::COMPUTER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "EnemyP1", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    sim.SetSimulationModule(war3Mod);

    CUnit* hall  = sim.CreateUnit(War3ID("htow"), 0, -400.0f, 0.0f, 0.0f);
    CUnit* rifle = sim.CreateUnit(War3ID("hrif"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* enemyFort  = sim.CreateUnit(War3ID("hcrt"), 1, 250.0f, 0.0f, 0.0f);
    CUnit* enemyLight = sim.CreateUnit(War3ID("hgry"), 1, 250.0f, 0.0f, 0.0f);
    TEST_ASSERT(hall && rifle && enemyFort && enemyLight);
    sim.Update();

    // 1. Create Player 0 AI through CSimulation -> War3GameModule
    TEST_ASSERT(sim.CreatePlayerAI(0, 2)); // INSANE difficulty
    TEST_ASSERT(war3Mod->GetActiveAICount() == 1);
    MeleeAIController* ai = war3Mod->GetPlayerAI(0);
    TEST_ASSERT(ai != nullptr);

    // Step AI ticks -> discovers Human race build plan via War3GameModule::DiscoverRaceBuildPlan
    for (int i = 0; i < 12; ++i) {
        sim.TickPlayerAIs();
    }
    TEST_ASSERT(ai->HasPlan());
    TEST_ASSERT(ai->GetPlan().townHall == War3ID("htow"));
    TEST_ASSERT(ai->GetPlan().worker == War3ID("hpea"));
    TEST_ASSERT(ai->GetPlan().farm == War3ID("hhou"));
    TEST_ASSERT(ai->GetPlan().barracks == War3ID("hbar"));
    TEST_ASSERT(ai->GetPlan().armyUnit == War3ID("hrif"));
    TEST_ASSERT(war3Mod->GetModuleStats().aiControllerTicks >= 12);

    // 2. Verify Armor-Counter Tactical Target Scoring:
    // Rifleman (Pierce) scores Light-armored enemy (2.0x counter -> +90 bonus) much higher than Fortified enemy (0.35x -> -58.5 penalty)!
    ai->RebuildThreatMap();
    const float scoreLight = ai->GetThreatMap().ScoreTargetUtility(rifle, enemyLight);
    const float scoreFort  = ai->GetThreatMap().ScoreTargetUtility(rifle, enemyFort);
    TEST_ASSERT(scoreLight > scoreFort + 100.0f);

    CUnit* bestFocus = ai->EvaluateBestFocusTarget(rifle, 600.0f);
    TEST_ASSERT(bestFocus == enemyLight);
    TEST_ASSERT(war3Mod->GetModuleStats().tacticalTargetEvaluations >= 2);

    return true;
}

bool TestPhase15StageMM2RetailDriveMapsCombatAndTacticalAiEndToEndSuite() {
    const std::string mapPath = "resources/Maps/FrozenThrone/(2)EchoIsles.w3x";
    FILE* f = std::fopen(mapPath.c_str(), "rb");
    if (!f) {
        return true;
    }
    std::fclose(f);

    auto& loader = engine::DynamicModuleLoader::Instance();
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();

    auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
    auto gameMod = loader.CreateGameModule(war3::War3GameModule::kModuleId);
    TEST_ASSERT(simMod != nullptr && gameMod != nullptr);
    TEST_ASSERT(simMod->GetModuleVersion() >= 15u);

    MpqManager mpq;
    CSimulation sim;
    sim.SetSimulationModule(simMod);
    SceneManager scene;
    UnitPresentationSystem presentation;
    core::WorldSceneSession session;

    TEST_ASSERT(session.LoadMap(mpq, sim, scene, presentation, mapPath));

    auto* war3GameMod = dynamic_cast<war3::War3GameModule*>(gameMod.get());
    TEST_ASSERT(war3GameMod != nullptr);
    war3GameMod->BindSimulation(&sim);
    TEST_ASSERT(war3GameMod->CreatePlayerAI(sim, 0, 1));
    TEST_ASSERT(war3GameMod->GetActiveAICount() == 1);

    engine::EngineKernel kernel;
    TEST_ASSERT(kernel.BootstrapStandardSubsystems(2, 0.05f));
    TEST_ASSERT(kernel.MountGameModule(std::move(gameMod)));

    // Step 4 fixed ticks (200ms) -> EngineKernel ticks War3GameModule::OnFixedTick -> TickPlayerAIs + sim.Update()
    const int ticksBefore = sim.GetGameTurnTick();
    TEST_ASSERT(kernel.StepFrame(0.10f, 1280, 720) + kernel.StepFrame(0.10f, 1280, 720) == 4);
    TEST_ASSERT(sim.GetGameTurnTick() == ticksBefore + 4);
    TEST_ASSERT(war3GameMod->GetModuleStats().aiControllerTicks == 4);

    session.TeardownCurrentSession(mpq, sim, scene, presentation);
    kernel.Shutdown();
    return true;
}


#include "ecs/hero_carry.h"
#include "ecs/item_bonuses.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/spells4.h"
#include "ecs/abilities/spells5.h"
#include "ecs/abilities/spells10.h"
#include "ecs/abilities/spells25.h"

// ============================================================================
// Phase 16 (Stage NN & Stage OO):
//   - Stage NN1: CUnit 16-bit UnitStateFlag bitmask packing, snapshot/restore,
//                and Resistant Skin (Brsk) / Spell Shield (BNss) module hooks
//   - Stage NN2: War3GameModule Combat Reactions (Mana Shield, Howl, Avatar,
//                Defend) & Passive Procs (Thorns/Spiked Carapace, Vampiric,
//                Cleave, Wind Walk Backstab) vs PureEngineMode
//   - Stage OO1: War3GameModule Bounty Dice, Hero XP Split, Item Stat Routing,
//                and Campaign Hero Carry vs PureEngineMode
//   - Stage OO2: Dynamic Plugin SO Version 16 & Cloud Retail W3X Maps Suite
// ============================================================================

bool TestPhase16StageNN1UnitStateBitfieldPackingAndZeroAbilityHeaderCoupling() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "500");
    ubTxt.SetString("hfoo", "manaN", "200");
    ubTxt.SetString("hfoo", "mana0", "200");
    ubTxt.SetString("hfoo", "spd", "270");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "Player0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "Player1", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    sim.SetSimulationModule(war3Mod);

    CUnit* u = sim.CreateUnit(War3ID("hfoo"), 0, 100.0f, 200.0f, 0.0f);
    CUnit* enemy = sim.CreateUnit(War3ID("hfoo"), 1, 250.0f, 200.0f, 180.0f);
    TEST_ASSERT(u && enemy);
    sim.Update();

    // 1. Verify default UnitStateFlag bitmask has PathingEnabled | CanSleep | CanSleepPerm | UseFood
    TEST_ASSERT(u->IsPathingEnabled());
    TEST_ASSERT(u->CanSleep());
    TEST_ASSERT(u->CanSleepPerm());
    TEST_ASSERT(!u->IsHidden());
    TEST_ASSERT(!u->IsInvulnerable());
    TEST_ASSERT(!u->IsEthereal());
    TEST_ASSERT(!u->IsMagicImmune());
    TEST_ASSERT(!u->IsIllusion());
    TEST_ASSERT(u->HasStateFlag(UnitStateFlag::PathingEnabled));
    TEST_ASSERT(u->HasStateFlag(UnitStateFlag::CanSleep));
    TEST_ASSERT(u->HasStateFlag(UnitStateFlag::CanSleepPerm));

    // Toggle flags and verify packed 16-bit mask round-trips through SimStateSnapshot
    u->SetEthereal(true);
    u->SetMagicImmune(true);
    u->SetIsIllusion(true);
    u->SetRooted(true);
    u->SetInvisible(true);
    u->SetDetector(true);
    u->SetIgnoreAlarm(true);
    const uint16_t expectedMask = u->GetStateFlagsBitmask();
    TEST_ASSERT(u->IsEthereal() && u->IsMagicImmune() && u->IsIllusion() &&
                u->IsRooted() && u->IsInvisible() && u->IsDetector() && u->IsAlarmIgnored());

    SimStateSnapshot snap = SimStateSnapshot::Capture(sim);
    u->SetEthereal(false);
    u->SetMagicImmune(false);
    u->SetIsIllusion(false);
    u->SetRooted(false);
    u->SetInvisible(false);
    TEST_ASSERT(!u->IsEthereal() && !u->IsRooted());

    TEST_ASSERT(snap.Restore(sim));
    TEST_ASSERT(u->GetStateFlagsBitmask() == expectedMask);
    TEST_ASSERT(u->IsEthereal() && u->IsMagicImmune() && u->IsIllusion() &&
                u->IsRooted() && u->IsInvisible() && u->IsDetector() && u->IsAlarmIgnored());

    // Reset combat-blocking flags for buff & spell shield tests
    u->SetEthereal(false);
    u->SetMagicImmune(false);
    u->SetIsIllusion(false);
    u->SetRooted(false);
    u->SetInvisible(false);

    // 2. Verify Resistant Skin (Brsk) reduces negative buff duration to 25% in War3GameModule,
    //    whereas PureEngineMode leaves duration at 100%.
    u->AddBuff(std::make_unique<CBuffResistant>(), sim);
    TEST_ASSERT(u->HasBuff(War3ID("Brsk")));
    u->AddBuff(std::make_unique<CBuffSlow>(12.0f, 0.5f, enemy->GetHandleId()), sim);
    const CBuff* slowWar3 = u->GetBuff(War3ID("Bslo"));
    TEST_ASSERT(slowWar3 != nullptr);
    TEST_NEAR(slowWar3->GetDuration(), 3.0f, 0.01f);
    u->RemoveBuff(War3ID("Bslo"), sim);

    sim.SetPureEngineMode(true);
    u->AddBuff(std::make_unique<CBuffSlow>(12.0f, 0.5f, enemy->GetHandleId()), sim);
    const CBuff* slowPure = u->GetBuff(War3ID("Bslo"));
    TEST_ASSERT(slowPure != nullptr);
    TEST_NEAR(slowPure->GetDuration(), 12.0f, 0.01f);
    u->RemoveBuff(War3ID("Bslo"), sim);

    // 3. Verify Spell Shield (CBuffSpellShield `BNss`) blocks hostile spell in War3GameModule
    //    (returns false = spell blocked & consumes BNss), whereas PureEngineMode allows it.
    sim.SetSimulationModule(war3Mod);
    u->AddBuff(std::make_unique<CBuffSpellShield>(30.0f), sim);
    TEST_ASSERT(u->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!u->CheckSpellEffectReaction(sim, enemy));
    TEST_ASSERT(!u->HasBuff(War3ID("BNss")));

    return true;
}

bool TestPhase16StageNN2War3ModuleCombatReactionsAndPassiveProcsVsPureEngine() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "1000");
    ubTxt.SetString("hfoo", "manaN", "500");
    ubTxt.SetString("hfoo", "mana0", "500");
    ubTxt.SetString("hfoo", "defType", "heavy");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "Player0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "Player1", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    sim.SetSimulationModule(war3Mod);

    CUnit* attacker = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* defender = sim.CreateUnit(War3ID("hfoo"), 1, 100.0f, 0.0f, 180.0f);
    CUnit* bystander = sim.CreateUnit(War3ID("hfoo"), 1, 120.0f, 40.0f, 180.0f);
    TEST_ASSERT(attacker && defender && bystander);
    sim.Update();

    // 1. Pre-mitigation: Howl of Terror (BNht 0.70x damage factor) + Mana Shield (BNms absorbs incoming damage via mana)
    attacker->AddBuff(std::make_unique<CBuffHowl>(10.0f, 0.70f), sim);
    defender->AddBuff(std::make_unique<CBuffManaShield>(30.0f), sim);
    defender->SetMana(40.0f);
    defender->SetLife(sim, 1000.0f);

    // 100 weapon damage -> Mana Shield absorbs 40 (drains all 40 mana, leaving 60) -> Howl scales 60 * 0.70 = 42 damage -> 958 life
    defender->Damage(sim, attacker, true, true, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f);
    TEST_NEAR(defender->GetMana(), 0.0f, 0.1f);
    TEST_NEAR(defender->GetLife(), 958.0f, 0.1f);
    TEST_ASSERT(war3Mod->GetModuleStats().preMitigationEvaluations >= 1);

    attacker->RemoveBuff(War3ID("BNht"), sim);
    defender->RemoveBuff(War3ID("BNms"), sim);

    // 2. Post-mitigation: Defend (CBuffDefend 0.5x weapon attack reduction)
    defender->SetLife(sim, 1000.0f);
    defender->AddBuff(std::make_unique<CBuffDefend>(30.0f), sim);
    defender->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f);
    TEST_NEAR(defender->GetLife(), 950.0f, 0.1f);
    TEST_ASSERT(war3Mod->GetModuleStats().postMitigationEvaluations >= 1);
    defender->RemoveBuff(War3ID("Bdef"), sim);

    // 3. Post-hit passives: Vampiric Aura lifesteal + Thorns reflection + Cleave splash + Wind Walk backstab
    attacker->SetLife(sim, 500.0f);
    attacker->AddBuff(std::make_unique<CBuffLifesteal>(30.0f, 0.20f), sim); // 20% lifesteal
    defender->AddBuff(std::make_unique<CBuffThorns>(30.0f, 0.10f), sim);    // 10% melee reflection
    attacker->AddAbility(sim, std::make_unique<CAbilityCleavingAttack>(99001)); // 50% splash in 200 radius
    attacker->AddBuff(std::make_unique<CBuffWindWalk>(20.0f, 50.0f), sim);  // +50 backstab damage

    defender->SetLife(sim, 1000.0f);
    bystander->SetLife(sim, 1000.0f);

    defender->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f);
    // Attacker gained 20 HP from Vampiric (500 -> 520) and took 10 reflected damage from Thorns (520 -> 510)
    TEST_NEAR(attacker->GetLife(), 510.0f, 0.5f);
    // Defender took 100 weapon + 50 Wind Walk backstab = 150 damage (1000 -> 850), and BOwk buff was consumed
    TEST_NEAR(defender->GetLife(), 850.0f, 0.5f);
    TEST_ASSERT(!attacker->HasBuff(War3ID("BOwk")));
    // Bystander took 50% Cleave splash = 50 damage (1000 -> 950)
    TEST_NEAR(bystander->GetLife(), 950.0f, 0.5f);
    TEST_ASSERT(war3Mod->GetModuleStats().postDamageReactions >= 1);

    // 4. Switch to PureEngineMode -> verify War3 combat reactions & passives are completely bypassed
    sim.SetPureEngineMode(true);
    attacker->SetLife(sim, 500.0f);
    defender->SetLife(sim, 1000.0f);
    bystander->SetLife(sim, 1000.0f);
    defender->AddBuff(std::make_unique<CBuffDefend>(30.0f), sim);
    defender->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f);
    // In PureEngineMode: no Defend 0.5x reduction, no Vampiric heal, no Thorns reflect, no Cleave splash
    TEST_NEAR(defender->GetLife(), 900.0f, 0.1f);
    TEST_NEAR(attacker->GetLife(), 500.0f, 0.1f);
    TEST_NEAR(bystander->GetLife(), 1000.0f, 0.1f);

    return true;
}

bool TestPhase16StageOO1War3ModuleBountyDiceHeroXpSplitItemBonusesAndHeroCarry() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Paladin Hero (Hpal)
    udTxt.SetString("Hpal", "race", "human");
    udTxt.SetString("Hpal", "isbldg", "0");
    ubTxt.SetString("Hpal", "isHero", "1");
    ubTxt.SetString("Hpal", "HP", "700");
    ubTxt.SetString("Hpal", "manaN", "300");
    ubTxt.SetString("Hpal", "mana0", "300");
    ubTxt.SetString("Hpal", "STR", "22");
    ubTxt.SetString("Hpal", "AGI", "13");
    ubTxt.SetString("Hpal", "INT", "17");
    ubTxt.SetString("Hpal", "STRplus", "2.7");
    ubTxt.SetString("Hpal", "AGIplus", "1.5");
    ubTxt.SetString("Hpal", "INTplus", "1.8");
    ubTxt.SetString("Hpal", "primary", "STR");
    uaTxt.SetString("Hpal", "heroAbilList", "AHhb,AHds");

    // Creep (nogr): Level 3, Bounty 25 + 1d1 = 26 gold
    udTxt.SetString("nogr", "race", "creeps");
    udTxt.SetString("nogr", "isbldg", "0");
    ubTxt.SetString("nogr", "HP", "400");
    ubTxt.SetString("nogr", "level", "3");
    ubTxt.SetString("nogr", "bountyGoldDice", "1");
    ubTxt.SetString("nogr", "bountyGoldSides", "1");
    ubTxt.SetString("nogr", "bountyGoldBase", "25");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    constants.grantNormalXp = {25, 40, 60, 85, 115};
    CSimulation sim;
    sim.SetGameplayConstants(constants);
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "HumanPlayer", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "CreepPlayer", CMapControl::COMPUTER));
    sim.GetPlayer(1)->SetGivesBounty(true);

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    sim.SetSimulationModule(war3Mod);

    CUnit* hero = sim.CreateUnit(War3ID("Hpal"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* creep = sim.CreateUnit(War3ID("nogr"), 1, 100.0f, 0.0f, 180.0f);
    TEST_ASSERT(hero && creep);
    sim.Update();

    auto* heroAbil = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
    TEST_ASSERT(heroAbil != nullptr);

    // 1. Verify War3GameModule assembled hero stats & heroAbilList from Hpal CUnitType
    TEST_ASSERT(heroAbil->GetStrength().base == 22);
    TEST_ASSERT(heroAbil->GetLearnableSkills().count(War3ID("AHhb")) == 1);

    // 2. Configure 2.0x custom bounty & XP multipliers on War3GameModule and kill the creep
    war3Mod->SetBountyMultiplier(2.0f);
    war3Mod->SetExperienceMultiplier(2.0f);
    const int goldBefore = sim.GetPlayer(0)->GetGold();
    const int xpBefore = heroAbil->GetXP();

    creep->Damage(sim, hero, true, false, CAttackType::HERO, CDamageType::NORMAL, "", 500.0f);
    TEST_ASSERT(creep->IsDead());
    // Base bounty = 26 * 2.0 = 52 gold; Level 3 creep base XP = 60 * 2.0 = 120 XP
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == goldBefore + 52);
    TEST_ASSERT(heroAbil->GetXP() == xpBefore + 120);
    TEST_ASSERT(war3Mod->GetModuleStats().bountiesAwarded >= 1);
    TEST_ASSERT(war3Mod->GetModuleStats().heroXpDistributions >= 1);

    war3Mod->SetBountyMultiplier(1.0f);
    war3Mod->SetExperienceMultiplier(1.0f);

    // 3. Verify Item Bonus routing: +10 STR adds +10 Hero Strength (+250 MaxHP via War3 hero formula)
    const int maxLifeBefore = hero->GetMaxLife();
    std::vector<CItemBonus> strBonus = {{CItemBonusType::STRENGTH, 10}};
    ApplyItemBonuses(sim, hero, strBonus);
    TEST_ASSERT(heroAbil->GetStrength().bonus == 10);
    TEST_ASSERT(hero->GetMaxLife() == maxLifeBefore + 250);
    TEST_ASSERT(war3Mod->GetModuleStats().itemBonusesApplied >= 1);
    RemoveItemBonuses(sim, hero, strBonus);
    TEST_ASSERT(heroAbil->GetStrength().bonus == 0);
    TEST_ASSERT(hero->GetMaxLife() == maxLifeBefore);

    // 4. Verify Campaign Hero Carry snapshot via War3GameModule vs PureEngineMode
    heroAbil->SetProperName("Uther the Lightbringer");
    TEST_ASSERT(IsCarriableHero(hero, sim));
    CarriedHero carried = SnapshotHero(hero, sim);
    TEST_ASSERT(carried.typeId == War3ID("Hpal"));
    TEST_ASSERT(carried.xp == 120);
    TEST_ASSERT(carried.properName == "Uther the Lightbringer");
    // Respawn carried hero at (300, 400)
    CUnit* respawned = RespawnCarriedHero(sim, carried, 300.0f, 400.0f);
    TEST_ASSERT(war3Mod->GetModuleStats().heroesCarriedOver >= 1);
    TEST_ASSERT(respawned != nullptr);
    auto* respawnedHeroAbil = dynamic_cast<CAbilityHero*>(respawned->GetAbility(War3ID("AHer")));
    TEST_ASSERT(respawnedHeroAbil != nullptr);
    TEST_ASSERT(respawnedHeroAbil->GetXP() == 120);
    TEST_ASSERT(respawnedHeroAbil->GetProperName() == "Uther the Lightbringer");

    // In PureEngineMode, IsCarriableHero returns false (no War3 hero carry rule mounted)
    sim.SetPureEngineMode(true);
    TEST_ASSERT(!IsCarriableHero(hero, sim));

    return true;
}

bool TestPhase16StageOO2DynamicPluginVersion16AndRetailDriveMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath = candidate;
            break;
        }
    }

    FILE* f = std::fopen(soPath.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase16PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 16u);

            CGameplayConstants constants;
            SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
            TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;
            udTxt.SetString("hfoo", "race", "human");
            udTxt.SetString("hfoo", "isbldg", "0");
            ubTxt.SetString("hfoo", "HP", "420");
            ubTxt.SetString("hfoo", "bountyGoldDice", "1");
            ubTxt.SetString("hfoo", "bountyGoldSides", "1");
            ubTxt.SetString("hfoo", "bountyGoldBase", "20");
            CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

            CSimulation sim;
            sim.SetUnitData(&unitData);
            sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
            sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
            sim.GetPlayer(1)->SetGivesBounty(true);
            sim.SetSimulationModule(dynSimMod);

            CUnit* killer = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
            CUnit* victim = sim.CreateUnit(War3ID("hfoo"), 1, 64.0f, 0.0f, 180.0f);
            TEST_ASSERT(killer && victim);
            sim.Update();

            const int gold0 = sim.GetPlayer(0)->GetGold();
            victim->Damage(sim, killer, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 500.0f);
            TEST_ASSERT(victim->IsDead());
            TEST_ASSERT(sim.GetPlayer(0)->GetGold() == gold0 + 21);

            sim.SetPureEngineMode(true);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    // End-to-end verification across cloud retail W3X maps downloaded from Google Drive
    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TurtleRock.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();

    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        auto gameMod = loader.CreateGameModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && gameMod);
        TEST_ASSERT(simMod->GetModuleVersion() >= 16u);

        MpqManager mpq;
        CSimulation sim;
        sim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, sim, scene, presentation, mapPath));

        CUnitType* footType = MakePhase5UnitType(War3ID("hfoo"), 420, 270, false);
        CUnit* u0 = sim.CreateUnit(War3ID("hfoo"), 0, 128.0f, 128.0f, 0.0f);
        if (!u0) {
            auto uPtr = std::make_unique<CUnit>(sim.AllocateHandleId(), footType, 0, War3ID("hfoo"), 128.0f, 128.0f, 0.0f);
            u0 = uPtr.get();
            simMod->OnAssembleUnitAbilities(sim, *u0, *footType);
        }
        sim.Update();

        // Capture full state snapshot including packed uint16_t stateFlagsBitmask and verify deterministic restore
        SimStateSnapshot snap = SimStateSnapshot::Capture(sim);
        sim.Update();
        sim.Update();
        TEST_ASSERT(snap.Restore(sim));

        session.TeardownCurrentSession(mpq, sim, scene, presentation);
    }

    return true;
}

#include "ecs/abilities/spells.h"
#include "ecs/abilities/spells3.h"
#include "ecs/abilities/spells7.h"
#include "ecs/abilities/spells9.h"
#include "ecs/abilities/spells13.h"
#include "ecs/abilities/spells20.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/behaviors/behavior_return_resources.h"
#include "ecs/behaviors/behavior_repair.h"
#include "ecs/behaviors/behavior_item.h"
#include "ecs/behaviors/behavior_spell.h"
#include "ecs/unit_text_utils.h"
#include "ecs/animation_utils.h"

// ============================================================================
// Phase 17 (Stage PP & Stage QQ):
//   - Stage PP1: Generic CAbility virtual capability contracts (harvest, dropoff,
//                mine, repair, working state, inventory, spell casting) and
//                zero concrete ability header coupling across src/ecs/behaviors/
//   - Stage PP2: Declarative CAbilitySpellBase effect primitives (wave damage,
//                line damage+stun, self/target/area buffs, timed spawner) and
//                shared CAttackBonusDamageEffect
//   - Stage QQ1: Unified CAbilityItemStatBonus StatSlot (DAMAGE, ARMOR, MAX_LIFE,
//                MAX_MANA) and War3GameModule::RegisterCustomSpellDescriptor
//   - Stage QQ2: Dynamic Plugin SO Version 17 & Cloud Retail W3X Maps Suite
// ============================================================================

bool TestPhase17StagePP1GenericCAbilityCapabilityContractAndZeroBehaviorHeaderCoupling() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Peasant (hpea), Town Hall (htow), Gold Mine (ngol)
    udTxt.SetString("hpea", "race", "human");
    udTxt.SetString("hpea", "isbldg", "0");
    ubTxt.SetString("hpea", "HP", "220");
    ubTxt.SetString("hpea", "spd", "190");
    uaTxt.SetString("hpea", "abilList", "Ahar,Arep,AInv");

    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");
    ubTxt.SetString("htow", "fmade", "12");
    ubTxt.SetString("htow", "trains", "hpea");
    uaTxt.SetString("htow", "abilList", "Aret");

    udTxt.SetString("ngol", "race", "other");
    udTxt.SetString("ngol", "isbldg", "1");
    ubTxt.SetString("ngol", "HP", "1500");
    uaTxt.SetString("ngol", "abilList", "Agld");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    auto p0 = std::make_unique<CPlayer>(0, "HumanP0", CMapControl::USER);
    p0->SetGold(500);
    p0->SetLumber(500);
    sim.AddPlayer(std::move(p0));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    CUnit* peasant = sim.CreateUnit(War3ID("hpea"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* hall    = sim.CreateUnit(War3ID("htow"), 0, 100.0f, 0.0f, 0.0f);
    CUnit* mine    = sim.CreateUnit(War3ID("ngol"), 0, -100.0f, 0.0f, 0.0f);
    TEST_ASSERT(peasant && hall && mine);
    sim.Update();

    // 1. Verify generic CAbility harvest & dropoff capability queries (without casting to CAbilityHarvest / CAbilityReturnResources)
    CAbility* harvAbil = peasant->GetAbility(War3ID("Ahar"));
    CAbility* retAbil  = hall->GetAbility(War3ID("Aret"));
    CAbility* mineAbil = mine->GetAbility(War3ID("Agld"));
    CAbility* repAbil  = peasant->GetAbility(War3ID("Arep"));
    CAbility* queAbil  = hall->GetAbility(War3ID("Aque"));
    TEST_ASSERT(harvAbil && retAbil && mineAbil && repAbil && queAbil);

    TEST_ASSERT(!harvAbil->IsCarryingResources());
    TEST_ASSERT(unit_text_utils::GetUnitCarriedResourceType(peasant) == CResourceType::NONE);
    harvAbil->SetCarried(CResourceType::GOLD, 10);
    TEST_ASSERT(harvAbil->IsCarryingResources());
    TEST_ASSERT(harvAbil->GetCarriedResourceType() == CResourceType::GOLD);
    TEST_ASSERT(harvAbil->GetCarriedAmount() == 10);
    TEST_ASSERT(unit_text_utils::GetUnitCarriedResourceType(peasant) == CResourceType::GOLD);

    TEST_ASSERT(retAbil->AcceptsResourceDropoff(CResourceType::GOLD));
    TEST_ASSERT(retAbil->AcceptsResourceDropoff(CResourceType::LUMBER));
    TEST_ASSERT(!retAbil->AcceptsResourceDropoff(CResourceType::NONE));

    // 2. Verify generic CAbility mine & working state queries
    TEST_ASSERT(mineAbil->IsResourceMine());
    TEST_ASSERT(mineAbil->GetGold() == 12500);
    TEST_ASSERT(mineAbil->CanAddMiner());
    TEST_ASSERT(!mineAbil->IsActivelyWorking());
    TEST_ASSERT(!animation_utils::IsUnitWorkingFromPassiveState(mine));

    // Queue a peasant at the Town Hall -> IsActivelyWorking() becomes true on CAbilityQueue!
    TEST_ASSERT(!queAbil->IsActivelyWorking());
    TEST_ASSERT(!animation_utils::IsUnitWorkingFromPassiveState(hall));
    queAbil->CheckBeforeQueueNoTarget(sim, hall, static_cast<int>(War3ID("hpea").value));
    TEST_ASSERT(queAbil->IsActivelyWorking());
    TEST_ASSERT(animation_utils::IsUnitWorkingFromPassiveState(hall));

    // 3. Verify CBehaviorReturnResources deposits carried gold via CAbility virtual interface
    const int goldBefore = sim.GetPlayer(0)->GetGold();
    auto retBeh = std::make_unique<CBehaviorReturnResources>();
    retBeh->SetUnit(peasant);
    retBeh->SetHarvestAbility(harvAbil);
    retBeh->SetTarget(hall);
    peasant->SetCurrentBehavior(std::move(retBeh));
    sim.Update();
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == goldBefore + 10);
    TEST_ASSERT(!harvAbil->IsCarryingResources());

    return true;
}

bool TestPhase17StagePP2DeclarativeSpellBasePrimitivesWaveLineAndAreaBuffs() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "1000");
    ubTxt.SetString("hfoo", "manaN", "500");
    ubTxt.SetString("hfoo", "mana0", "500");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    CUnit* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* ally   = sim.CreateUnit(War3ID("hfoo"), 0, 40.0f, 0.0f, 0.0f);
    CUnit* enemy  = sim.CreateUnit(War3ID("hfoo"), 1, 150.0f, 0.0f, 180.0f);
    TEST_ASSERT(caster && ally && enemy);
    sim.Update();

    // 1. Test CastPointDamageWaves via Flame Strike (AHfs: 3 waves x 80 = 240 damage)
    CAbilityFlameStrike flame(sim.AllocateHandleId());
    flame.DoEffect(sim, caster, 150.0f, 0.0f);
    TEST_NEAR(enemy->GetLife(), 760.0f, 0.5f);

    // 2. Test CastLineDamageAndStun via Impale (AUim: 80 damage + 2s stun) & Stampede (ANst: 120 damage + 2s stun, enemy-only)
    enemy->SetLife(sim, 1000.0f);
    ally->SetLife(sim, 1000.0f);
    CAbilityImpale impale(sim.AllocateHandleId());
    impale.DoEffect(sim, caster, enemy);
    TEST_NEAR(enemy->GetLife(), 920.0f, 0.5f);
    TEST_ASSERT(enemy->HasBuff(War3ID("Bstn")));
    enemy->RemoveBuff(War3ID("Bstn"), sim);
    ally->RemoveBuff(War3ID("Bstn"), sim);
    ally->SetLife(sim, 1000.0f);

    CAbilityStampede stampede(sim.AllocateHandleId());
    stampede.DoEffect(sim, caster, 200.0f, 0.0f);
    TEST_NEAR(enemy->GetLife(), 800.0f, 0.5f);
    TEST_ASSERT(enemy->HasBuff(War3ID("Bstn")));
    TEST_NEAR(ally->GetLife(), 1000.0f, 0.5f);
    enemy->RemoveBuff(War3ID("Bstn"), sim);

    // 3. Test CastCasterAreaBuff via Roar (Aroa: applies Broa to same-player units) & Big Bad Voodoo (AOvd: applies Bdsb)
    CAbilityRoar roar(sim.AllocateHandleId());
    roar.DoEffect(sim, caster);
    TEST_ASSERT(ally->HasBuff(War3ID("Broa")));
    TEST_ASSERT(!enemy->HasBuff(War3ID("Broa")));

    CAbilityBigBadVoodoo voodoo(sim.AllocateHandleId());
    voodoo.DoEffect(sim, caster);
    TEST_ASSERT(caster->IsInvulnerable());
    TEST_ASSERT(ally->IsInvulnerable());
    caster->RemoveBuff(War3ID("Bdsb"), sim);
    ally->RemoveBuff(War3ID("Bdsb"), sim);

    // 4. Test shared CAttackBonusDamageEffect via Orb of Fire (AIfb: +15 bonus fire damage on weapon hit)
    enemy->SetLife(sim, 1000.0f);
    auto orb = std::make_unique<CAbilityOrbOfFire>(sim.AllocateHandleId());
    caster->AddAbility(sim, std::move(orb));
    enemy->Damage(sim, caster, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 50.0f);
    // 50 normal + 15 Orb of Fire proc = 65 damage -> 935 HP
    TEST_NEAR(enemy->GetLife(), 935.0f, 0.5f);

    return true;
}

bool TestPhase17StageQQ1UnifiedItemStatBonusSlotsAndWar3CustomSpellDescriptor() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "500");
    ubTxt.SetString("hfoo", "manaN", "200");
    ubTxt.SetString("hfoo", "mana0", "200");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->ResetModuleStats();
    sim.SetSimulationModule(war3Mod);

    CUnit* u = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* target = sim.CreateUnit(War3ID("hfoo"), 1, 100.0f, 0.0f, 180.0f);
    TEST_ASSERT(u && target);
    sim.Update();

    // 1. Verify unified CAbilityItemStatBonus (DAMAGE, ARMOR, MAX_LIFE, MAX_MANA) add & remove
    auto dmgAbil  = std::make_unique<CAbilityItemDamageBonus>(sim.AllocateHandleId());
    auto armAbil  = std::make_unique<CAbilityItemArmorBonus>(sim.AllocateHandleId());
    auto lifeAbil = std::make_unique<CAbilityItemLifeBonus>(sim.AllocateHandleId());
    auto manaAbil = std::make_unique<CAbilityItemManaPoolBonus>(sim.AllocateHandleId());

    const int dmgH  = dmgAbil->GetHandleId();
    const int armH  = armAbil->GetHandleId();
    const int lifeH = lifeAbil->GetHandleId();
    const int manaH = manaAbil->GetHandleId();

    u->AddAbility(sim, std::move(dmgAbil));
    u->AddAbility(sim, std::move(armAbil));
    u->AddAbility(sim, std::move(lifeAbil));
    u->AddAbility(sim, std::move(manaAbil));

    TEST_NEAR(u->GetBonusDamage(), 3.0f, 0.01f);
    TEST_ASSERT(u->GetBonusArmor() == 2);
    TEST_NEAR(u->GetMaxLife(), 650.0f, 0.01f);
    TEST_ASSERT(u->GetMaxMana() == 300);

    u->RemoveAbility(sim, dmgH);
    u->RemoveAbility(sim, armH);
    u->RemoveAbility(sim, lifeH);
    u->RemoveAbility(sim, manaH);

    TEST_NEAR(u->GetBonusDamage(), 0.0f, 0.01f);
    TEST_ASSERT(u->GetBonusArmor() == 0);
    TEST_NEAR(u->GetMaxLife(), 500.0f, 0.01f);
    TEST_ASSERT(u->GetMaxMana() == 200);

    // 2. Register a custom data-driven spell descriptor via War3GameModule::RegisterCustomSpellDescriptor
    ABSpellDescriptor customNuke;
    customNuke.code = War3ID("ACp7");
    customNuke.targetType = static_cast<uint8_t>(CSpellTargetType::TARGET_UNIT);
    customNuke.defaultRange = 600.0f;
    customNuke.defaultManaCost = 40;
    customNuke.defaultData[0] = 175.0f;
    customNuke.onEffect = [](const ABSpellEffectContext& ctx, CSimulation& game,
                             CUnit* caster, CWidget* tgt, bool, float, float) {
        ABActionDamageTarget hit(ctx.data[0], CAttackType::SPELLS, CDamageType::LIGHTNING);
        hit.Run(game, caster, tgt, false, 0.0f, 0.0f);
    };
    war3Mod->RegisterCustomSpellDescriptor(customNuke);
    TEST_ASSERT(war3Mod->GetModuleStats().customSpellsRegistered == 1);

    auto spawnedSpell = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("ACp7"));
    TEST_ASSERT(spawnedSpell != nullptr);
    u->AddAbility(sim, std::move(spawnedSpell));

    target->SetLife(sim, 500.0f);
    u->GetAbility(War3ID("ACp7"))->DoEffect(sim, u, target);
    TEST_NEAR(target->GetLife(), 325.0f, 0.5f);

    return true;
}

bool TestPhase17StageQQ2DynamicPluginVersion17AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath = candidate;
            break;
        }
    }

    FILE* f = std::fopen(soPath.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase17PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 17u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    // Verify cloud retail W3X maps load and step cleanly with War3GameModule v17
    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TwistedMeadows.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();

    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        auto gameMod = loader.CreateGameModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && gameMod);
        TEST_ASSERT(simMod->GetModuleVersion() >= 17u);

        MpqManager mpq;
        CSimulation sim;
        sim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, sim, scene, presentation, mapPath));
        sim.Update();
        session.TeardownCurrentSession(mpq, sim, scene, presentation);
    }

    return true;
}

#include "ecs/abilities/spells6.h"
#include "ecs/abilities/spells8.h"
#include "ecs/abilities/spells21.h"
#include "ecs/abilities/spells26.h"
#include "ecs/abilities/spells27.h"
#include "ecs/abilities/spells28.h"
#include "ecs/abilities/spells29.h"

// ============================================================================
// Phase 18 (Stage RR & Stage SS):
//   - Stage RR1: Compact CBuff/CAbility/CAbilitySpellBase bitfield memory layout
//                and CBuffCloneable<Derived, Base> CRTP deep clone fidelity
//   - Stage RR2: Unified CBuff state-restoration counters (Invulnerable, Rooted,
//                Invisible, MagicImmune) and DamageEnemyUnitsInRadius AoE helpers
//   - Stage SS1: SetPassiveSpell(true), CastPointAreaHeal, ForEachPassiveAllyInRange,
//                and CAbilityBlightedGoldMine virtual resource mine contract
//   - Stage SS2: War3GameModule Version 18 custom autocast ability registration
//                (RegisterAutocastAbility) and Dynamic Plugin SO v18 + cloud maps
// ============================================================================

bool TestPhase18StageRR1CompactCBuffCAbilityLayoutAndCRTPCloneFidelity() {
    // 1. Verify compact memory footprint after Phase 18 bitfield & int16_t packing
    static_assert(sizeof(CAbility) <= 32, "CAbility must fit within 32 bytes (down from 40)");
    static_assert(sizeof(CBuff) <= 40, "CBuff must fit within 40 bytes (down from 56)");
    static_assert(sizeof(CAbilitySpellBase) <= 56, "CAbilitySpellBase must fit within 56 bytes (down from 72)");
    TEST_ASSERT(sizeof(CAbility) <= 32);
    TEST_ASSERT(sizeof(CBuff) <= 40);
    TEST_ASSERT(sizeof(CAbilitySpellBase) <= 56);

    // 2. Verify CAbility state flag bitmask round-trip
    CAbilitySpellBase spell(9001, War3ID("AHbz"), CAbilityCategory::SPELL);
    spell.SetPhysical(true);
    spell.SetPermanent(true);
    spell.SetIconShowing(false);
    spell.SetAutocastActive(true);
    spell.SetPassiveSpell(true);
    const uint8_t abilMask = spell.GetAbilityStateFlagsBitmask();
    TEST_ASSERT((abilMask & static_cast<uint8_t>(AbilityStateFlag::PHYSICAL)) != 0);
    TEST_ASSERT((abilMask & static_cast<uint8_t>(AbilityStateFlag::PERMANENT)) != 0);
    TEST_ASSERT((abilMask & static_cast<uint8_t>(AbilityStateFlag::ICON_SHOWING)) == 0);
    TEST_ASSERT((abilMask & static_cast<uint8_t>(AbilityStateFlag::AUTOCAST_ACTIVE)) != 0);
    TEST_ASSERT((abilMask & static_cast<uint8_t>(AbilityStateFlag::PASSIVE_SPELL)) != 0);

    CAbilitySpellBase restoredSpell(9002, War3ID("AHbz"), CAbilityCategory::SPELL);
    restoredSpell.SetAbilityStateFlagsBitmask(abilMask);
    TEST_ASSERT(restoredSpell.IsPhysical() && restoredSpell.IsPermanent() &&
                !restoredSpell.IsIconShowing() && restoredSpell.IsAutocastActive() &&
                restoredSpell.IsPassiveSpell());

    // 3. Verify CBuff state flag bitmask & CBuffCloneable CRTP deep cloning across single & multi-level hierarchies
    CBuffSlow slow(12.0f, 0.45f, 777);
    slow.SetRemaining(7.5f);
    slow.SetSuspended(true);
    std::unique_ptr<CBuff> clonedSlow(slow.Clone());
    TEST_ASSERT(clonedSlow != nullptr);
    TEST_ASSERT(clonedSlow->GetId() == War3ID("Bslo"));
    TEST_NEAR(clonedSlow->GetDuration(), 12.0f, 0.001f);
    TEST_NEAR(clonedSlow->GetRemaining(), 7.5f, 0.001f);
    TEST_ASSERT(clonedSlow->GetCasterHandleId() == 777);
    TEST_ASSERT(clonedSlow->IsSuspended());

    // Multi-level CRTP inheritance: CBuffPolymorph -> CBuffCloneable<CBuffPolymorph, CBuffHex>
    CBuffPolymorph poly(25.0f);
    poly.SetRemaining(14.0f);
    std::unique_ptr<CBuff> clonedPoly(poly.Clone());
    TEST_ASSERT(clonedPoly != nullptr);
    TEST_ASSERT(clonedPoly->GetId() == War3ID("Bply"));
    TEST_NEAR(clonedPoly->GetRemaining(), 14.0f, 0.001f);
    TEST_ASSERT(dynamic_cast<CBuffHex*>(clonedPoly.get()) != nullptr);

    // Multi-level CRTP inheritance: CBuffCyclone -> CBuffCloneable<CBuffCyclone, CBuffStun>
    CBuffCyclone cyc(6.0f);
    cyc.SetRemaining(3.25f);
    std::unique_ptr<CBuff> clonedCyc(cyc.Clone());
    TEST_ASSERT(clonedCyc != nullptr);
    TEST_ASSERT(clonedCyc->GetId() == War3ID("Bcyc"));
    TEST_NEAR(clonedCyc->GetRemaining(), 3.25f, 0.001f);
    TEST_ASSERT(dynamic_cast<CBuffStun*>(clonedCyc.get()) != nullptr);

    return true;
}

bool TestPhase18StageRR2UnifiedBuffStateRestorationAndPeriodicAoEHelpers() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "1000");
    ubTxt.SetString("hfoo", "manaN", "500");
    ubTxt.SetString("hfoo", "mana0", "500");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());

    CUnit* hero  = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* ally  = sim.CreateUnit(War3ID("hfoo"), 0, 60.0f, 0.0f, 0.0f);
    CUnit* enemy = sim.CreateUnit(War3ID("hfoo"), 1, 60.0f, 0.0f, 180.0f);
    TEST_ASSERT(hero && ally && enemy);
    sim.Update();

    // 1. Overlapping Invulnerable & Rooted buffs (DivineShield + StoneForm)
    hero->AddBuff(std::make_unique<CBuffDivineShield>(10.0f), sim);
    hero->AddBuff(std::make_unique<CBuffStoneForm>(10.0f), sim);
    TEST_ASSERT(hero->IsInvulnerable() && hero->IsRooted());

    // Removing DivineShield must keep hero invulnerable while StoneForm remains active
    hero->RemoveBuff(War3ID("Bdsh"), sim);
    TEST_ASSERT(hero->IsInvulnerable() && hero->IsRooted());

    // Adding Ensnare then removing StoneForm clears Invulnerable but keeps Rooted
    hero->AddBuff(std::make_unique<CBuffEnsnare>(10.0f), sim);
    hero->RemoveBuff(War3ID("Bstm"), sim);
    TEST_ASSERT(!hero->IsInvulnerable() && hero->IsRooted());
    hero->RemoveBuff(War3ID("Bena"), sim);
    TEST_ASSERT(!hero->IsRooted());

    // 2. Overlapping MagicImmune buffs (CBuffMagicImmune + CBuffWhirlwind)
    hero->AddBuff(std::make_unique<CBuffMagicImmune>(10.0f), sim);
    hero->AddBuff(std::make_unique<CBuffWhirlwind>(10.0f, 50.0f, 200.0f), sim);
    TEST_ASSERT(hero->IsMagicImmune());
    hero->RemoveBuff(War3ID("Bam2"), sim);
    TEST_ASSERT(hero->IsMagicImmune());
    hero->RemoveBuff(War3ID("BOww"), sim);
    TEST_ASSERT(!hero->IsMagicImmune());

    // 3. Shared DamageEnemyUnitsInRadius helper via CBuffPhoenixFire & CBuffExplode
    enemy->SetLife(sim, 1000.0f);
    ally->SetLife(sim, 1000.0f);
    hero->AddBuff(std::make_unique<CBuffPhoenixFire>(5.0f, 30.0f, 200.0f, 45.0f), sim);
    // Initial damage (45) hits enemy immediately on apply, sparing ally
    TEST_NEAR(enemy->GetLife(), 955.0f, 0.5f);
    TEST_NEAR(ally->GetLife(), 1000.0f, 0.5f);

    return true;
}

bool TestPhase18StageSS1PassiveSpellFlagAreaHealAndBlightedMineVirtualContract() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "1000");
    ubTxt.SetString("hfoo", "manaN", "500");
    ubTxt.SetString("hfoo", "mana0", "500");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    auto p0 = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
    p0->SetGold(100);
    sim.AddPlayer(std::move(p0));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());

    CUnit* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* ally   = sim.CreateUnit(War3ID("hfoo"), 0, 100.0f, 0.0f, 0.0f);
    CUnit* enemy  = sim.CreateUnit(War3ID("hfoo"), 1, 100.0f, 0.0f, 180.0f);
    TEST_ASSERT(caster && ally && enemy);
    sim.Update();

    // 1. Verify SetPassiveSpell(true) on CAbilityItemSpellResist / CAbilityItemMixedDefense / CAbilityChaos
    CAbilityItemSpellResist spellResist(sim.AllocateHandleId());
    CAbilityItemMixedDefense mixedDef(sim.AllocateHandleId());
    CAbilityChaos chaos(sim.AllocateHandleId());
    TEST_ASSERT(spellResist.IsPassiveSpell() && mixedDef.IsPassiveSpell() && chaos.IsPassiveSpell());
    TEST_ASSERT(!spellResist.CheckCanTarget(sim, caster, 0, enemy));
    TEST_ASSERT(!spellResist.CheckCanTargetPoint(sim, caster, 0, 0.0f, 0.0f));
    TEST_ASSERT(!spellResist.CheckCanTargetNoTarget(sim, caster, 0));
    TEST_ASSERT(!mixedDef.CheckCanTargetNoTarget(sim, caster, 0));
    TEST_ASSERT(!chaos.CheckCanTargetNoTarget(sim, caster, 0));

    // 2. Verify CastPointAreaHeal via Healing Spray (ANhs) & ItemHealGroup (AIha)
    caster->SetLife(sim, 600.0f);
    ally->SetLife(sim, 600.0f);
    enemy->SetLife(sim, 600.0f);
    CAbilityHealingSpray spray(sim.AllocateHandleId());
    spray.DoEffect(sim, caster, 50.0f, 0.0f);
    TEST_ASSERT(caster->GetLife() > 600.0f);
    TEST_ASSERT(ally->GetLife() > 600.0f);
    TEST_NEAR(enemy->GetLife(), 600.0f, 0.5f);

    // 3. Verify ForEachPassiveAllyInRange via CAbilitySpellShieldArea & CAbilityReplenish
    CAbilitySpellShieldArea shieldArea(sim.AllocateHandleId());
    shieldArea.DoEffect(sim, caster, 50.0f, 0.0f);
    TEST_ASSERT(caster->HasBuff(War3ID("BNss")));
    TEST_ASSERT(ally->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!enemy->HasBuff(War3ID("BNss")));

    // 4. Verify CAbilityBlightedGoldMine implements CAbility virtual resource mine contract
    auto blightedMine = std::make_unique<CAbilityBlightedGoldMine>(sim.AllocateHandleId());
    CAbility* genericMine = blightedMine.get();
    TEST_ASSERT(genericMine->IsResourceMine());
    TEST_ASSERT(genericMine->GetGold() == 12500);
    genericMine->SetGold(5000);
    TEST_ASSERT(genericMine->GetGold() == 5000);
    TEST_ASSERT(genericMine->CanAddMiner());
    TEST_ASSERT(!genericMine->IsActivelyWorking());
    TEST_ASSERT(blightedMine->AddMiner());
    TEST_ASSERT(genericMine->IsActivelyWorking());

    return true;
}

bool TestPhase18StageSS2CustomAutocastSpellRegistrationAndDynamicPluginV18Suite() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "1000");
    ubTxt.SetString("hfoo", "manaN", "500");
    ubTxt.SetString("hfoo", "mana0", "500");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 18u);
    sim.SetSimulationModule(war3Mod);

    // 1. Register a custom data-driven single-target heal spell (ACh8) & register it for LOWEST_HP_ALLY autocast
    ABSpellDescriptor customHeal;
    customHeal.code = War3ID("ACh8");
    customHeal.targetType = static_cast<uint8_t>(CSpellTargetType::TARGET_UNIT);
    customHeal.defaultRange = 600.0f;
    customHeal.defaultCastPoint = 0.0f;
    customHeal.defaultCooldown = 2.0f;
    customHeal.defaultManaCost = 25;
    customHeal.defaultData[0] = 200.0f;
    customHeal.onEffect = [](const ABSpellEffectContext& ctx, CSimulation& game,
                              CUnit* caster, CWidget* tgt, bool, float, float) {
        ABActionHealTarget heal(ctx.data[0]);
        heal.Run(game, caster, tgt, false, 0.0f, 0.0f);
    };
    war3Mod->RegisterCustomSpellDescriptor(customHeal);
    war3Mod->RegisterAutocastAbility(War3ID("ACh8"), static_cast<int>(CAutocastKind::LOWEST_HP_ALLY));
    TEST_ASSERT(war3Mod->GetModuleStats().customAutocastRegistered == 1);
    TEST_ASSERT(war3Mod->GetAbilityAutocastKind(War3ID("ACh8").value) ==
                static_cast<int>(CAutocastKind::LOWEST_HP_ALLY));

    CUnit* healer  = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* wounded = sim.CreateUnit(War3ID("hfoo"), 0, 120.0f, 0.0f, 0.0f);
    TEST_ASSERT(healer && wounded);
    sim.Update();

    auto healAbil = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("ACh8"));
    TEST_ASSERT(healAbil != nullptr);
    healAbil->SetAutocastActive(true);
    healer->AddAbility(sim, std::move(healAbil));

    wounded->SetLife(sim, 400.0f);
    for (int i = 0; i < 5; ++i) {
        sim.Update();
    }
    TEST_ASSERT(war3Mod->GetModuleStats().autocastOrdersIssued >= 1);
    TEST_NEAR(wounded->GetLife(), 600.0f, 1.0f);

    // 2. Verify Dynamic Plugin SO Version 18 & Cloud Retail W3X Maps
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath18 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths18[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths18) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath18 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath18.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase18PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath18));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 18u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TurtleRock.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 18u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}


#include "scripting/jass_natives_internal.h"

// ============================================================================
// Phase 19 (Stage TT & Stage UU):
//   - Stage TT1: Generic CAbility virtual contracts for Production Queue,
//                Build Menu, Building Upgrade, Repair, and Rally Point
//   - Stage TT2: Generic CAbility virtual contracts for Hero Progression/Stats
//                and Shop Stock Management
//   - Stage UU1: Decoupled JASS Natives (Hero, Inventory, Shop, Cooldowns)
//                via ResolveJassUnit / FindHeroAbility / FindUnitInventory / FindUnitShop
//   - Stage UU2: War3GameModule Version 19 OnSnapBuildPosition &
//                IsBuildLocationObstructed + Dynamic Plugin SO v19 + Cloud Maps
// ============================================================================

bool TestPhase19StageTT1GenericCAbilityQueueBuildUpgradeAndRallyContracts() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Peasant (hpea), Town Hall (htow -> upgrades to hkee, trains hpea)
    udTxt.SetString("hpea", "race", "human");
    udTxt.SetString("hpea", "isbldg", "0");
    ubTxt.SetString("hpea", "HP", "220");
    ubTxt.SetString("hpea", "goldcost", "75");
    ubTxt.SetString("hpea", "fused", "1");
    ubTxt.SetString("hpea", "bldtm", "10");
    ubTxt.SetString("hpea", "builds", "htow,hbar");
    uaTxt.SetString("hpea", "abilList", "Ahar,Arep");

    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");
    ubTxt.SetString("htow", "fmade", "12");
    ubTxt.SetString("htow", "trains", "hpea");
    ubTxt.SetString("htow", "Upgrade", "hkee");

    udTxt.SetString("hkee", "race", "human");
    udTxt.SetString("hkee", "isbldg", "1");
    ubTxt.SetString("hkee", "HP", "2000");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    auto p0 = std::make_unique<CPlayer>(0, "HumanP0", CMapControl::USER);
    p0->SetGold(1000);
    p0->SetLumber(1000);
    sim.AddPlayer(std::move(p0));
    sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());

    CUnit* peasant = sim.CreateUnit(War3ID("hpea"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* hall    = sim.CreateUnit(War3ID("htow"), 0, 200.0f, 0.0f, 0.0f);
    TEST_ASSERT(peasant && hall);
    sim.Update();

    // 1. Inspect Build & Repair abilities purely via CAbility* virtual contract
    CAbility* buildAbil = nullptr;
    CAbility* repAbil   = nullptr;
    for (CAbility* ab : peasant->GetAbilities()) {
        if (!ab) continue;
        if (ab->GetCategory() == CAbilityCategory::BUILD && !ab->IsRepairAbility()) buildAbil = ab;
        if (ab->IsRepairAbility()) repAbil = ab;
    }
    TEST_ASSERT(buildAbil != nullptr && repAbil != nullptr);
    TEST_ASSERT(buildAbil->GetBuildableStructures().count(War3ID("htow")) > 0);
    TEST_ASSERT(buildAbil->GetBuildableStructures().count(War3ID("hbar")) > 0);

    // 2. Inspect Queue, Upgrade, and Rally abilities on Town Hall purely via CAbility*
    CAbility* queueAbil = hall->GetAbility(War3ID("Aque"));
    CAbility* upgAbil   = hall->GetAbility(War3ID("Aupg"));
    CAbility* rallyAbil = hall->GetAbility(War3ID("ARal"));
    TEST_ASSERT(queueAbil && upgAbil && rallyAbil);
    TEST_ASSERT(queueAbil->GetTrainedUnits().count(War3ID("hpea")) > 0);
    TEST_ASSERT(upgAbil->GetUpgradesTo().count(War3ID("hkee")) > 0);
    TEST_ASSERT(!rallyAbil->IsExplicitlySet());

    // Queue a peasant via CAbility* virtual QueueUnit and cancel it via CAbility* virtual CancelQueueItem
    queueAbil->QueueUnit(sim, hall, War3ID("hpea"));
    TEST_ASSERT(queueAbil->GetQueueCount() == 1);
    TEST_ASSERT(queueAbil->GetQueuedItemType(0) == CQueueItemType::UNIT);
    TEST_ASSERT(queueAbil->GetQueuedItem(0) == War3ID("hpea"));
    TEST_ASSERT(queueAbil->GetBuildTime() > 0.0f);
    queueAbil->CancelQueueItem(sim, hall, 0);
    TEST_ASSERT(queueAbil->GetQueueCount() == 0);

    return true;
}

bool TestPhase19StageTT2GenericCAbilityHeroProgressionAndShopStockContracts() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Paladin Hero (Hpal) & Goblin Merchant Shop (ngme)
    udTxt.SetString("Hpal", "race", "human");
    udTxt.SetString("Hpal", "isbldg", "0");
    ubTxt.SetString("Hpal", "HP", "650");
    ubTxt.SetString("Hpal", "STR", "22");
    ubTxt.SetString("Hpal", "AGI", "13");
    ubTxt.SetString("Hpal", "INT", "17");
    ubTxt.SetString("Hpal", "isHero", "1");
    ubTxt.SetString("Hpal", "primary", "STR");
    uaTxt.SetString("Hpal", "heroAbilList", "AHhb,AHds,AHad,AHre");

    udTxt.SetString("ngme", "race", "other");
    udTxt.SetString("ngme", "isbldg", "1");
    ubTxt.SetString("ngme", "HP", "1500");
    uaTxt.SetString("ngme", "abilList", "Asei");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());

    CUnit* paladin = sim.CreateUnit(War3ID("Hpal"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* shop    = sim.CreateUnit(War3ID("ngme"), 0, 200.0f, 0.0f, 0.0f);
    TEST_ASSERT(paladin && shop);
    sim.Update();

    // 1. Operate Hero ability exclusively through generic CAbility* pointer
    CAbility* heroAbil = paladin->GetAbility(War3ID("AHer"));
    TEST_ASSERT(heroAbil != nullptr);
    TEST_ASSERT(heroAbil->GetHeroLevel() == 1);
    TEST_ASSERT(heroAbil->GetSkillPoints() == 1);
    TEST_ASSERT(heroAbil->GetStrength().GetCurrent(1) == 22);
    TEST_ASSERT(heroAbil->GetAgility().GetCurrent(1) == 13);
    TEST_ASSERT(heroAbil->GetIntelligence().GetCurrent(1) == 17);

    heroAbil->SetProperName("Uther the Lightbringer");
    TEST_ASSERT(heroAbil->GetProperName() == "Uther the Lightbringer");

    heroAbil->SetHeroLevel(sim, paladin, 3);
    TEST_ASSERT(heroAbil->GetHeroLevel() == 3);
    TEST_ASSERT(heroAbil->GetSkillPoints() == 3);
    TEST_ASSERT(heroAbil->SelectHeroSkill(sim, paladin, War3ID("AHhb")));
    TEST_ASSERT(heroAbil->GetSkillPoints() == 2);

    heroAbil->AddStrengthBonus(sim, paladin, 8);
    TEST_ASSERT(heroAbil->GetStrength().bonus == 8);
    TEST_ASSERT(heroAbil->StripLevels(sim, paladin, 1));
    TEST_ASSERT(heroAbil->GetHeroLevel() == 2);

    // 2. Operate Shop stock ability exclusively through generic CAbility* pointer
    CAbility* shopAbil = shop->GetAbility(War3ID("Asei"));
    TEST_ASSERT(shopAbil != nullptr);
    shopAbil->SetStockMax(3);
    shopAbil->SetStockReplenishInterval(15.0f);
    shopAbil->AddStockItem(War3ID("phea"), 2, 3);
    shopAbil->AddStockUnit(War3ID("hfoo"), 1, 2);
    TEST_ASSERT(shopAbil->RemoveStock(War3ID("phea")));
    TEST_ASSERT(shopAbil->RemoveStock(War3ID("hfoo")));

    return true;
}

bool TestPhase19StageUU1DecoupledJassNativesHeroInventoryShopAndCooldowns() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("Hpal", "race", "human");
    udTxt.SetString("Hpal", "isbldg", "0");
    ubTxt.SetString("Hpal", "HP", "650");
    ubTxt.SetString("Hpal", "STR", "20");
    ubTxt.SetString("Hpal", "AGI", "15");
    ubTxt.SetString("Hpal", "INT", "18");
    ubTxt.SetString("Hpal", "isHero", "1");
    ubTxt.SetString("Hpal", "primary", "STR");
    uaTxt.SetString("Hpal", "abilList", "AInv,AHhb");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());

    JassVM vm;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto callNative = [&](const std::string& name, const std::vector<JassValue>& args) -> JassValue {
        const auto& table = vm.GetRegisteredNatives();
        auto it = table.find(name);
        return it != table.end() ? it->second(args) : JassValue::Null();
    };

    CUnit* hero = sim.CreateUnit(War3ID("Hpal"), 0, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT(hero != nullptr);
    sim.Update();

    const int heroJassHandle = natives.GetData()->AllocHandle(JassHandleInfo::UNIT, hero->GetHandleId());
    TEST_ASSERT(ResolveJassUnit(natives.GetData(), &sim, heroJassHandle) == hero);
    TEST_ASSERT(FindHeroAbility(&sim, hero->GetHandleId()) == hero->GetAbility(War3ID("AHer")));
    TEST_ASSERT(FindUnitInventory(natives.GetData(), &sim, heroJassHandle) == hero->GetAbility(War3ID("AInv")));

    // Exercise JASS Hero & Inventory & Cooldown natives through VM
    TEST_ASSERT(callNative("GetHeroLevel", {JassValue::Int(heroJassHandle)}).intVal == 1);
    callNative("SetHeroLevel", {JassValue::Int(heroJassHandle), JassValue::Int(4), JassValue::Bool(false)});
    TEST_ASSERT(callNative("GetHeroLevel", {JassValue::Int(heroJassHandle)}).intVal == 4);

    // ModifyHeroStat: stat 0 (STR), method 1 (ADD), value 10
    callNative("ModifyHeroStat", {
        JassValue::Int(heroJassHandle),
        JassValue::Int(0),
        JassValue::Int(1),
        JassValue::Int(10)
    });
    TEST_ASSERT(callNative("GetHeroStr", {JassValue::Int(heroJassHandle), JassValue::Bool(true)}).intVal == 30);

    // Inventory size & UnitAddItemById
    TEST_ASSERT(callNative("UnitInventorySize", {JassValue::Int(heroJassHandle)}).intVal == 6);
    const int itemJassHandle = callNative("UnitAddItemById", {
        JassValue::Int(heroJassHandle),
        JassValue::Int(static_cast<int>(War3ID("rat9").value))
    }).intVal;
    TEST_ASSERT(itemJassHandle != 0);
    TEST_ASSERT(callNative("UnitHasItem", {
        JassValue::Int(heroJassHandle),
        JassValue::Int(itemJassHandle)
    }).boolVal);

    // Cooldown start & end via CAbility virtual cooldown interface
    callNative("StartUnitAbilityDefaultCooldown", {
        JassValue::Int(heroJassHandle),
        JassValue::Int(static_cast<int>(War3ID("AHhb").value))
    });
    CAbility* holyLight = hero->GetAbility(War3ID("AHhb"));
    TEST_ASSERT(holyLight != nullptr);
    holyLight->SetCooldown(5.0f);
    holyLight->StartCooldown();
    TEST_NEAR(holyLight->GetCooldownRemaining(), 5.0f, 0.01f);
    callNative("EndUnitAbilityCooldown", {
        JassValue::Int(heroJassHandle),
        JassValue::Int(static_cast<int>(War3ID("AHhb").value))
    });
    TEST_NEAR(holyLight->GetCooldownRemaining(), 0.0f, 0.01f);

    return true;
}

bool TestPhase19StageUU2War3GameModuleV19BuildSnapObstructionAndDynamicPluginSuite() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 19u);
    sim.SetSimulationModule(war3Mod);

    // 1. Test OnSnapBuildPosition & IsBuildLocationObstructed on War3GameModule v19
    const CUnitType* hallType = unitData.GetUnitType(War3ID("htow"));
    float snapX = 115.0f, snapY = -70.0f;
    sim.GetSimulationModule()->OnSnapBuildPosition(hallType, snapX, snapY);
    TEST_ASSERT(std::fmod(std::fabs(snapX), 32.0f) < 0.01f);
    TEST_ASSERT(std::fmod(std::fabs(snapY), 32.0f) < 0.01f);

    TEST_ASSERT(!sim.GetSimulationModule()->IsBuildLocationObstructed(
        sim, 512.0f, 512.0f, 0, War3ID("htow").value));

    // 2. Test Dynamic Plugin SO Version 19 & Cloud Retail W3X Maps
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath19 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths19[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths19) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath19 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath19.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase19PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath19));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 19u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TwistedMeadows.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 19u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}


// ============================================================================
// Phase 20 Modernization Tests:
//   - Stage VV1: Decoupled MeleeAIController (OnDiscoverAIRacePlan & build
//                dispatch) + Cross-Ability Virtual Contracts (Inventory,
//                SellItems, ReviveHero)
//   - Stage VV2: Declarative CAbilityAura (Bitfields, Stat Bonuses, SetAuraBuff)
//                & War3GameModule::RegisterCustomAura
//   - Stage WW1: Direct O(1) Value COrder Dispatch & Deduplicated AbilityBuilder
//                Actions (DamageArea, HealArea, HealTarget, ReviveTarget,
//                CreateUnitAtTarget, TransferToCaster)
//   - Stage WW2: War3GameModule Version 20 Dynamic Order Registration
//                (RegisterCustomOrder) + Dynamic Plugin SO v20 + Cloud Maps
// ============================================================================

bool TestPhase20StageVV1DecoupledMeleeAIAndCrossAbilityVirtualContracts() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    // Human Town Hall (htow), Peasant (hpea), Altar of Kings (halt), Paladin (Hpal), Gold Mine (ngol)
    udTxt.SetString("htow", "race", "human");
    udTxt.SetString("htow", "isbldg", "1");
    ubTxt.SetString("htow", "HP", "1500");
    ubTxt.SetString("htow", "fmade", "12");
    ubTxt.SetString("htow", "trains", "hpea");

    udTxt.SetString("hpea", "race", "human");
    udTxt.SetString("hpea", "isbldg", "0");
    ubTxt.SetString("hpea", "HP", "220");
    ubTxt.SetString("hpea", "goldcost", "75");
    ubTxt.SetString("hpea", "fused", "1");
    ubTxt.SetString("hpea", "bldtm", "10");
    ubTxt.SetString("hpea", "builds", "htow,halt");
    uaTxt.SetString("hpea", "abilList", "Ahar,Arep");

    udTxt.SetString("halt", "race", "human");
    udTxt.SetString("halt", "isbldg", "1");
    ubTxt.SetString("halt", "HP", "1200");
    ubTxt.SetString("halt", "goldcost", "180");
    ubTxt.SetString("halt", "lumbercost", "50");
    ubTxt.SetString("halt", "trains", "Hpal");
    uaTxt.SetString("halt", "abilList", "Arev");

    udTxt.SetString("Hpal", "race", "human");
    udTxt.SetString("Hpal", "isbldg", "0");
    ubTxt.SetString("Hpal", "isHero", "1");
    ubTxt.SetString("Hpal", "HP", "650");
    ubTxt.SetString("Hpal", "STR", "22");
    ubTxt.SetString("Hpal", "AGI", "13");
    ubTxt.SetString("Hpal", "INT", "17");
    ubTxt.SetString("Hpal", "primary", "STR");
    uaTxt.SetString("Hpal", "abilList", "AInv");

    udTxt.SetString("ngol", "race", "other");
    udTxt.SetString("ngol", "isbldg", "1");
    ubTxt.SetString("ngol", "HP", "1500");
    uaTxt.SetString("ngol", "abilList", "Agld");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    auto p0 = std::make_unique<CPlayer>(0, "AIPlayer", CMapControl::COMPUTER);
    p0->SetGold(1500);
    p0->SetLumber(1000);
    sim.AddPlayer(std::move(p0));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    CUnit* hall    = sim.CreateUnit(War3ID("htow"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* peasant = sim.CreateUnit(War3ID("hpea"), 0, 128.0f, 0.0f, 0.0f);
    CUnit* altar   = sim.CreateUnit(War3ID("halt"), 0, -256.0f, 0.0f, 0.0f);
    CUnit* paladin = sim.CreateUnit(War3ID("Hpal"), 0, -128.0f, 128.0f, 0.0f);
    CUnit* mine    = sim.CreateUnit(War3ID("ngol"), 12, 384.0f, 0.0f, 0.0f);
    TEST_ASSERT(hall && peasant && altar && paladin && mine);
    sim.Update();

    // 1. Verify ISimulationModule::OnDiscoverAIRacePlan and MeleeAIController without concrete ability headers
    AIRacePlan discovered{};
    TEST_ASSERT(sim.GetSimulationModule()->OnDiscoverAIRacePlan(sim, 0, discovered));
    TEST_ASSERT(discovered.valid);
    TEST_ASSERT(discovered.townHall == War3ID("htow"));
    TEST_ASSERT(discovered.worker == War3ID("hpea"));

    TEST_ASSERT(sim.GetSimulationModule()->CreatePlayerAI(sim, 0, 1));
    sim.GetSimulationModule()->TickPlayerAIs(sim, 1.0f);

    // 2. Verify CAbilityInventory virtual FindEmptySlot and RemoveItemAt
    CAbility* inv = paladin->GetAbility(War3ID("AInv"));
    TEST_ASSERT(inv != nullptr);
    TEST_ASSERT(inv->FindEmptySlot() == 0);
    CItem* claw = sim.CreateItem(War3ID("rat9"), 0.0f, 0.0f);
    TEST_ASSERT(claw != nullptr);
    TEST_ASSERT(inv->PickUpItem(sim, paladin, claw));
    TEST_ASSERT(inv->FindEmptySlot() == 1);
    TEST_ASSERT(inv->RemoveItemAt(sim, paladin, 0));
    TEST_ASSERT(inv->GetItemCount() == 0);

    // 3. Verify decoupled CAbilityReviveHero queueing via generic CAbility*
    paladin->SetLife(sim, 0.0f);
    TEST_ASSERT(paladin->IsDead());
    CAbility* revAbil = altar->GetAbility(War3ID("Arev"));
    CAbility* altarQueue = altar->GetAbility(War3ID("Aque"));
    TEST_ASSERT(revAbil && altarQueue);
    revAbil->BeginNoTarget(sim, altar, paladin->GetHandleId());
    TEST_ASSERT(altarQueue->GetQueueCount() == 1);
    TEST_ASSERT(altarQueue->GetQueuedItemType(0) == CQueueItemType::HERO_REVIVE);

    return true;
}

bool TestPhase20StageVV2DeclarativeCAbilityAuraAndSetAuraBuffEngine() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "420");
    ubTxt.SetString("hfoo", "def", "2");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    auto p0 = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
    auto p1 = std::make_unique<CPlayer>(1, "P1", CMapControl::USER);
    p0->SetAlliance(1, CAllianceType::ALLIED, false);
    p1->SetAlliance(0, CAllianceType::ALLIED, false);
    sim.AddPlayer(std::move(p0));
    sim.AddPlayer(std::move(p1));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    // Register a custom declarative stat aura ("ACa0": +5 flat armor in 600 radius)
    war3Mod->RegisterCustomAura(
        War3ID("ACa0"), 600.0f, true, true, false,
        {{CAuraEffectType::ARMOR, 5.0f, false}});
    TEST_ASSERT(war3Mod->GetModuleStats().customAurasRegistered >= 1u);

    CUnit* auraBearer = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* enemyUnit  = sim.CreateUnit(War3ID("hfoo"), 1, 100.0f, 0.0f, 0.0f);
    TEST_ASSERT(auraBearer && enemyUnit);
    sim.Update();

    const int baseDef = auraBearer->GetTotalDefense();
    auto customAura = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("ACa0"));
    TEST_ASSERT(customAura != nullptr);
    auraBearer->AddAbility(sim, std::move(customAura));

    // Attach declarative Thorns Aura (BEah) and Disease Cloud (Bapl)
    auraBearer->AddAbility(sim, AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("AEah")));
    auraBearer->AddAbility(sim, AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("Aapl")));

    sim.Update();
    TEST_ASSERT(auraBearer->GetTotalDefense() == baseDef + 5);
    TEST_ASSERT(auraBearer->HasBuff(War3ID("BEah")));
    TEST_ASSERT(!auraBearer->HasBuff(War3ID("Bapl")));
    TEST_ASSERT(enemyUnit->HasBuff(War3ID("Bapl")));

    TEST_ASSERT(war3Mod->UnregisterCustomAura(War3ID("ACa0")));
    return true;
}

bool TestPhase20StageWW1DirectValueCOrderDispatchAndDeduplicatedAbilityBuilderActions() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "500");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());

    CUnit* u0 = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    CUnit* u1 = sim.CreateUnit(War3ID("hfoo"), 1, 100.0f, 0.0f, 0.0f);
    TEST_ASSERT(u0 && u1);
    sim.Update();

    // 1. Verify O(1) COrder value accessors across all 3 target kinds
    COrderNoTarget noTgt(OrderIds::STOP);
    TEST_ASSERT(noTgt.GetOrderId() == OrderIds::STOP);
    TEST_ASSERT(noTgt.GetTargetKind() == COrder::TargetKind::NONE);

    COrderTargetPoint ptOrd(OrderIds::MOVE, 250.0f, -125.0f, 16.0f);
    TEST_ASSERT(ptOrd.GetOrderId() == OrderIds::MOVE);
    TEST_ASSERT(ptOrd.GetTargetKind() == COrder::TargetKind::POINT);
    TEST_NEAR(ptOrd.GetX(), 250.0f, 0.01f);
    TEST_NEAR(ptOrd.GetY(), -125.0f, 0.01f);
    TEST_NEAR(ptOrd.GetZ(), 16.0f, 0.01f);

    COrderTargetWidget wgOrd(OrderIds::ATTACK, u1);
    TEST_ASSERT(wgOrd.GetOrderId() == OrderIds::ATTACK);
    TEST_ASSERT(wgOrd.GetTargetKind() == COrder::TargetKind::WIDGET);
    TEST_ASSERT(wgOrd.GetTarget() == u1);

    TEST_ASSERT(sim.IssueUnitOrder(u0->GetHandleId(), std::make_unique<COrderTargetPoint>(OrderIds::MOVE, 200.0f, 0.0f)));
    TEST_ASSERT(u0->GetLastOrderId() == OrderIds::MOVE);

    // 2. Verify deduplicated AbilityBuilder actions (DamageArea, HealArea, HealTarget, TransferToCaster, ReviveTarget)
    ABActionDamageArea dmgArea(200.0f, 150.0f);
    dmgArea.Run(sim, u0, nullptr, false, 0.0f, 0.0f);
    TEST_ASSERT(u1->GetLife() < 500.0f);

    const float afterDmg = u1->GetLife();
    ABActionHealTarget healTgt(50.0f);
    healTgt.Run(sim, u0, u1, false, 0.0f, 0.0f);
    TEST_NEAR(u1->GetLife(), afterDmg + 50.0f, 0.1f);

    ABActionHealArea healArea(200.0f, 500.0f);
    healArea.Run(sim, u0, nullptr, false, 0.0f, 0.0f);
    TEST_NEAR(u1->GetLife(), 500.0f, 0.1f);

    ABActionTransferToCaster transfer;
    transfer.Run(sim, u0, u1, false, 0.0f, 0.0f);
    TEST_ASSERT(u1->GetPlayerIndex() == 0);

    u1->SetLife(sim, 0.0f);
    TEST_ASSERT(u1->IsDead());
    ABActionReviveTarget revive;
    revive.Run(sim, u0, u1, false, 0.0f, 0.0f);
    TEST_ASSERT(!u1->IsDead());

    return true;
}

bool TestPhase20StageWW2War3GameModuleV20DynamicOrdersCustomAurasAndPluginSuite() {
    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 20u);

    // 1. Test dynamic custom order string & ability order mapping (including toggle secondary order)
    ClearCustomOrderMappings();
    war3Mod->RegisterCustomOrder("customnova", 899101, War3ID("ACn0").value, 899102);
    TEST_ASSERT(war3Mod->GetModuleStats().customOrdersRegistered == 1u);
    TEST_ASSERT(OrderIdFromString("CustomNova") == 899101);
    TEST_ASSERT(OrderStringFromId(899101) == "customnova");
    TEST_ASSERT(SpellBaseOrderId(War3ID("ACn0")) == 899101);
    TEST_ASSERT(SpellMatchesOrder(War3ID("ACn0"), 899101));
    TEST_ASSERT(SpellMatchesOrder(War3ID("ACn0"), 899102));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("ACn0"), 899999));
    ClearCustomOrderMappings();

    // 2. Test Dynamic Plugin SO Version 20 & Cloud Retail W3X Maps
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath20 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths20[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths20) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath20 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath20.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase20PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath20));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 20u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TurtleRock.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 20u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}

// ============================================================================
// Phase 21 Modernization Tests:
//   - Stage XX1: Unified MpqManager::ReadSingleArchiveFile & ReadJassScriptFile
//                (10 external files decoupled from <StormLib.h>)
//   - Stage XX2: Layered Map ObjectMods Loader & Case-Insensitive SlkMerge
//                Helpers (EqualsIgnoreCase, TryGetTxtValueCaseInsensitive)
//   - Stage YY1: War3GameModule Version 21 In-Memory Custom Data Archetypes
//                (RegisterCustomItemArchetype, RegisterCustomUpgradeArchetype,
//                 RegisterCustomAbilityDef)
//   - Stage YY2: Dynamic Plugin SO Version 21 & Cloud Retail Maps Suite
// ============================================================================

bool TestPhase21StageXX1UnifiedMpqSingleArchiveReaderAndJassScriptResolver() {
    const char* mapPath = "resources/Maps/FrozenThrone/(2)EchoIsles.w3x";
    FILE* f = std::fopen(mapPath, "rb");
    if (f) {
        std::fclose(f);
        std::vector<uint8_t> w3iBytes;
        std::vector<uint8_t> w3eBytes;
        std::vector<uint8_t> wpmBytes;
        std::vector<uint8_t> jassBytes;

        TEST_ASSERT(MpqManager::ReadSingleArchiveFile(mapPath, "war3map.w3i", w3iBytes));
        TEST_ASSERT(!w3iBytes.empty());

        TEST_ASSERT(MpqManager::ReadSingleArchiveFile(mapPath, "war3map.w3e", w3eBytes));
        TEST_ASSERT(!w3eBytes.empty());

        TEST_ASSERT(MpqManager::ReadSingleArchiveFile(mapPath, "war3map.wpm", wpmBytes));
        TEST_ASSERT(!wpmBytes.empty());

        MpqManager mpq;
        TEST_ASSERT(mpq.ReadJassScriptFile(mapPath, "Scripts\\war3map.j", jassBytes));
        TEST_ASSERT(!jassBytes.empty());
    }

    // Verify graceful false return on missing archive or missing internal file
    std::vector<uint8_t> missing{1u, 2u, 3u};
    TEST_ASSERT(!MpqManager::ReadSingleArchiveFile("nonexistent_archive.w3x", "war3map.w3i", missing));
    TEST_ASSERT(missing.empty());

    MpqManager emptyMpq;
    missing = {4u, 5u};
    TEST_ASSERT(!emptyMpq.ReadFromGameArchivesOnly("Scripts\\common.j", missing));
    TEST_ASSERT(missing.empty());

    return true;
}

bool TestPhase21StageXX2LayeredMapObjectModsAndCaseInsensitiveSlkMergeHelpers() {
    TEST_ASSERT(EqualsIgnoreCase("Buttonpos", "buttonpos"));
    TEST_ASSERT(EqualsIgnoreCase("GoldCost", "GOLDCOST"));
    TEST_ASSERT(!EqualsIgnoreCase("GoldCost", "LumberCost"));

    TxtTable txt;
    txt.SetString("hfoo", "Buttonpos", "2,1");
    txt.SetString("hfoo", "GoldCost", "135");

    std::string outVal;
    TEST_ASSERT(TryGetTxtValueCaseInsensitive(txt, "hfoo", "buttonpos", outVal));
    TEST_ASSERT(outVal == "2,1");
    TEST_ASSERT(TryGetTxtValueCaseInsensitive(txt, "hfoo", "goldcost", outVal));
    TEST_ASSERT(outVal == "135");

    // Verify unified layered object-mod loaders on Scenario map with custom object data
    const char* tdMap = "resources/Maps/FrozenThrone/Scenario/(6)BlizzardTD.w3x";
    FILE* f = std::fopen(tdMap, "rb");
    if (f) {
        std::fclose(f);
        ObjectModificationSet unitMods;
        if (LoadWar3MapUnitMods(tdMap, unitMods)) {
            TEST_ASSERT(!unitMods.original.empty() || !unitMods.custom.empty());
        }
    }

    return true;
}

bool TestPhase21StageYY1War3GameModuleV21CustomItemUpgradeAndAbilityDataArchetypes() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("Hpal", "race", "human");
    udTxt.SetString("Hpal", "isbldg", "0");
    ubTxt.SetString("Hpal", "isHero", "1");
    ubTxt.SetString("Hpal", "HP", "650");
    ubTxt.SetString("Hpal", "def", "2");
    ubTxt.SetString("Hpal", "STR", "20");
    ubTxt.SetString("Hpal", "AGI", "12");
    ubTxt.SetString("Hpal", "INT", "16");
    ubTxt.SetString("Hpal", "primary", "STR");
    uaTxt.SetString("Hpal", "abilList", "AInv,ACs1");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);
    CItemData itemData;
    CUpgradeData upgradeData;
    CAbilityData abilityData;

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.SetItemData(&itemData);
    sim.SetUpgradeData(&upgradeData);
    sim.SetAbilityData(&abilityData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 21u);
    sim.SetSimulationModule(war3Mod);

    // 1. Register custom ability definition & descriptor ("ACs1")
    war3Mod->RegisterCustomSpellDescriptor(
        ABSpellDescriptor::MakeSelfInvulnerableBuff(War3ID("ACs1"), 8.0f, 12.0f, 35));
    TEST_ASSERT(war3Mod->RegisterCustomAbilityDef(
        sim, War3ID("ACs1"), "Aegis Shield", 3, 20.0f, 50, 0.0f, 0.0f, 10.0f));
    TEST_ASSERT(abilityData.HasAbility(War3ID("ACs1")));

    // 2. Register custom item archetype ("Ics1") and custom upgrade archetype ("Rcs1": +4 armor per level)
    TEST_ASSERT(war3Mod->RegisterCustomItemArchetype(
        sim, War3ID("Ics1"), "Blade of Valor", 350, 50, 2, {War3ID("AIat")}));
    TEST_ASSERT(itemData.HasItem(War3ID("Ics1")));

    TEST_ASSERT(war3Mod->RegisterCustomUpgradeArchetype(
        sim, War3ID("Rcs1"), "Mithril Plating", 3, 100, 75, "rarm", 4.0f, 0.0f));
    TEST_ASSERT(upgradeData.HasUpgrade(War3ID("Rcs1")));
    TEST_ASSERT(war3Mod->GetModuleStats().customDataArchetypesRegistered == 3u);

    // 3. Spawn hero, verify custom ability def applied, pick up custom item, and apply custom upgrade
    CUnit* hero = sim.CreateUnit(War3ID("Hpal"), 0, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT(hero != nullptr);
    sim.Update();

    CAbility* customSpell = hero->GetAbility(War3ID("ACs1"));
    TEST_ASSERT(customSpell != nullptr);
    TEST_NEAR(customSpell->GetCooldown(), 20.0f, 0.01f);
    TEST_ASSERT(customSpell->GetManaCost() == 50);

    CItem* customItem = sim.CreateItem(War3ID("Ics1"), 0.0f, 0.0f);
    TEST_ASSERT(customItem != nullptr);
    TEST_ASSERT(customItem->GetName() == "Blade of Valor");
    TEST_ASSERT(customItem->GetGoldValue() == 350);
    TEST_ASSERT(customItem->GetCharges() == 2);

    const int defBeforeUpgrade = hero->GetTotalDefense();
    sim.GetPlayer(0)->SetTechLevel(sim, War3ID("Rcs1"), 2);
    TEST_ASSERT(hero->GetTotalDefense() == defBeforeUpgrade + 8);

    return true;
}

bool TestPhase21StageYY2DynamicPluginSOV21AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath21 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths21[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths21) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath21 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath21.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase21PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath21));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 21u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(8)GardenOfWar.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 21u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}


// ============================================================================
// Phase 22 Modernization Tests:
//   - Stage ZZ1: 8-bit CBuff State Bitfields (TIMED_LIFE, HEX, SILENCE, SLEEP,
//                STEALTH) & Virtual Combat Modifier Contract (GetMissChance,
//                GetDamageDealtFactor, GetWeaponDamageTakenMultiplier,
//                GetAttackTypeDamageTakenMultiplier, GetReflectPercent,
//                GetStealPercent, GetBackstabDamage) with sizeof(CBuff) == 40
//   - Stage ZZ2: Complete CAbility Base Virtual Contract (Zero Cross-Ability
//                Headers & Zero dynamic_cast<CAbility*> Across src/)
//   - Stage AAA1: War3GameModule Version 22 Dynamic Custom Buff Factory &
//                 Declarative DataDrivenBuffSpec Registry + JASS CreateJassBuff
//                 Live Unit Attachment
//   - Stage AAA2: Dynamic Plugin SO Version 22 & Cloud Retail Maps Suite
// ============================================================================

bool TestPhase22StageZZ1CompactCBuff8BitStateFlagsAndVirtualCombatModifierContract() {
    // 1. Verify sizeof(CBuff) remains 40 bytes after packing all 8 status flags into 1 byte
    TEST_ASSERT(sizeof(CBuff) == 40);

    // 2. Verify O(1) bitfield flags on CBuff subclasses without dynamic_cast
    CBuffTimedLife timed(War3ID("BTLF"), 30.0f);
    TEST_ASSERT(timed.IsTimedLife());
    TEST_ASSERT((timed.GetBuffStateFlagsBitmask() & static_cast<uint8_t>(BuffStateFlag::TIMED_LIFE)) != 0);

    CBuffHex hex(10.0f);
    TEST_ASSERT(hex.IsHexBuff());
    CBuffPolymorph poly(10.0f);
    TEST_ASSERT(poly.IsHexBuff());

    CBuffSilence sil(8.0f);
    TEST_ASSERT(sil.IsSilenceBuff());

    CBuffSleep sleep(12.0f);
    TEST_ASSERT(sleep.IsSleepBuff());

    CBuffWindWalk walk(20.0f, 85.0f);
    TEST_ASSERT(walk.IsStealthBuff());
    TEST_NEAR(walk.GetBackstabDamage(), 85.0f, 1e-4f);

    // 3. Verify virtual combat modifier getters on CBuff* base pointer
    const CBuff* hazePtr = &timed;
    TEST_NEAR(hazePtr->GetMissChance(), 0.0f, 1e-4f);

    CBuffHaze haze(10.0f, 0.45f);
    CBuffCurse curse(10.0f, 0.33f);
    CBuffHowl howl(10.0f, 0.70f);
    CBuffDefend defend(10.0f);
    CBuffBerserk berserk(10.0f, 1.50f, 1.20f);
    CBuffDamageMod dmgMod(War3ID("Bdm1"), 0.67f, 0.80f, 0.50f, 0.25f);
    CBuffThorns thorns(10.0f, 0.30f);
    CBuffLifesteal leech(10.0f, 0.15f);

    const CBuff* bHaze = &haze;
    const CBuff* bCurse = &curse;
    const CBuff* bHowl = &howl;
    const CBuff* bDef = &defend;
    const CBuff* bBrk = &berserk;
    const CBuff* bMod = &dmgMod;
    const CBuff* bThr = &thorns;
    const CBuff* bLch = &leech;

    TEST_NEAR(bHaze->GetMissChance(), 0.45f, 1e-4f);
    TEST_NEAR(bCurse->GetMissChance(), 0.33f, 1e-4f);
    TEST_NEAR(bHowl->GetDamageDealtFactor(), 0.70f, 1e-4f);
    TEST_NEAR(bDef->GetWeaponDamageTakenMultiplier(), 0.50f, 1e-4f);
    TEST_NEAR(bBrk->GetWeaponDamageTakenMultiplier(), 1.20f, 1e-4f);
    TEST_NEAR(bMod->GetAttackDealtAdd(), 0.25f, 1e-4f);
    TEST_NEAR(bMod->GetAttackTypeDamageTakenMultiplier(static_cast<int>(CAttackType::SPELLS)), 0.67f, 1e-4f);
    TEST_NEAR(bMod->GetAttackTypeDamageTakenMultiplier(static_cast<int>(CAttackType::MAGIC)), 0.80f, 1e-4f);
    TEST_NEAR(bMod->GetAttackTypeDamageTakenMultiplier(static_cast<int>(CAttackType::PIERCE)), 0.50f, 1e-4f);
    TEST_NEAR(bThr->GetReflectPercent(), 0.30f, 1e-4f);
    TEST_NEAR(bLch->GetStealPercent(), 0.15f, 1e-4f);

    return true;
}

bool TestPhase22StageZZ2CompleteCAbilityVirtualContractZeroCrossAbilityCasts() {
    TEST_ASSERT(sizeof(CAbility) == 24);

    CSimulation sim;
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("egol"), std::make_unique<CUnitType>(*MakePhase5UnitType(War3ID("egol"), 1000, 0, true)));
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.SetSimulationModule(std::make_shared<war3::War3GameModule>());

    // 1. Verify GENERIC_PLACEHOLDER bitfield on unknown ability without dynamic_cast
    auto unknownAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("Z999"));
    TEST_ASSERT(unknownAb != nullptr);
    TEST_ASSERT(unknownAb->IsGenericPlaceholder());

    // 2. Verify passive combat proc virtual getters through CAbility* base pointer
    auto bashAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("AHbh"));
    auto cleaveAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("ANca"));
    auto pulvAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("Awar"));
    auto caraAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("AUts"));
    TEST_ASSERT(bashAb && cleaveAb && pulvAb && caraAb);
    TEST_ASSERT(!bashAb->IsGenericPlaceholder());
    TEST_ASSERT(bashAb->GetProcChance() > 0.0f && bashAb->GetProcDuration() > 0.0f);
    TEST_ASSERT(cleaveAb->GetSplashFraction() > 0.0f && cleaveAb->GetSplashRadius() > 0.0f);
    TEST_ASSERT(pulvAb->GetProcChance() > 0.0f && pulvAb->GetSplashRadius() > 0.0f);
    TEST_ASSERT(caraAb->GetReflectPercent() > 0.0f);

    // 3. Verify Entangle Gold Mine (Aent), Overlayed Mine (Agl2), and CargoHold (Aenc) via CAbility* virtual contract
    CUnitType* treeType = MakePhase5UnitType(War3ID("etol"), 1500, 0, true);
    CUnitType* mineType = MakePhase5UnitType(War3ID("ngol"), 1500, 0, true);
    CUnitType* wispType = MakePhase5UnitType(War3ID("ewsp"), 120, 270, false);

    CUnit* mineUnit = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), mineType, 0, War3ID("ngol"), 0.0f, 0.0f, 0.0f));
    CUnit* treeUnit = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), treeType, 0, War3ID("etol"), 64.0f, 0.0f, 0.0f));
    CUnit* wispUnit = sim.AdoptUnit(std::make_unique<CUnit>(sim.AllocateHandleId(), wispType, 0, War3ID("ewsp"), 32.0f, 0.0f, 0.0f));
    mineUnit->AddAbility(sim, AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("Agld")));
    CAbility* baseGold = mineUnit->GetAbility(War3ID("Agld"));
    TEST_ASSERT(baseGold != nullptr);
    baseGold->SetGold(250);

    auto entangleAb = AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("Aent"));
    TEST_ASSERT(entangleAb != nullptr);
    entangleAb->DoEffect(sim, treeUnit, mineUnit);
    sim.Update();

    // Find spawned egol replacement unit and operate its cargo & overlay purely via CAbility*
    CUnit* egol = nullptr;
    for (CUnit* u : sim.GetUnits()) {
        if (u && u->GetTypeId() == War3ID("egol")) {
            egol = u;
            break;
        }
    }
    TEST_ASSERT(egol != nullptr);
    CAbility* overlay = egol->GetAbility(War3ID("Agl2"));
    CAbility* cargo = egol->GetAbility(War3ID("Aenc"));
    TEST_ASSERT(overlay != nullptr && cargo != nullptr);
    TEST_ASSERT(overlay->GetGold() == 250);
    TEST_ASSERT(cargo->GetCargoCapacity() == 5);
    TEST_ASSERT(cargo->TryLoad(egol, wispUnit));

    return true;
}

bool TestPhase22StageAAA1War3GameModuleV22DynamicBuffFactoryAndJassBuffAttachment() {
    CGameplayConstants constants;
    SlkTable udSlk, uaSlk, ubSlk, uwSlk, uuiSlk;
    TxtTable udTxt, uaTxt, ubTxt, uwTxt, uuiTxt;

    udTxt.SetString("hfoo", "race", "human");
    udTxt.SetString("hfoo", "isbldg", "0");
    ubTxt.SetString("hfoo", "HP", "500");
    ubTxt.SetString("hfoo", "def", "2");
    ubTxt.SetString("hfoo", "spd", "270");

    CUnitData unitData(constants, udSlk, udTxt, uaSlk, uaTxt, ubSlk, ubTxt, uwSlk, uwTxt, uuiSlk, uuiTxt);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 22u);
    sim.SetSimulationModule(war3Mod);

    // 1. Register a declarative DataDrivenBuffSpec ("BCs1": +6 armor, +20 flat damage for 15s)
    DataDrivenBuffSpec customSpec;
    customSpec.id = War3ID("BCs1");
    customSpec.duration = 15.0f;
    customSpec.isPositive = true;
    customSpec.armorDelta = 6.0f;
    customSpec.flatDamageBonus = 20;
    war3Mod->RegisterCustomBuffSpec(customSpec);
    TEST_ASSERT(war3Mod->HasCustomBuff(War3ID("BCs1")));

    // 2. Register a custom CBuffCreatorFn ("BCs2": custom Silence buff)
    war3Mod->RegisterCustomBuffCreator(War3ID("BCs2"),
        [](float duration, int /*casterHandleId*/) -> std::unique_ptr<CBuff> {
            return std::make_unique<CBuffSilence>(duration > 0.0f ? duration : 8.0f);
        });
    TEST_ASSERT(war3Mod->HasCustomBuff(War3ID("BCs2")));
    TEST_ASSERT(war3Mod->GetModuleStats().customBuffsRegistered == 2u);

    CUnit* footman = sim.CreateUnit(War3ID("hfoo"), 0, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT(footman != nullptr);
    sim.Update();

    // 3. Apply custom buffs via JASS CreateJassBuff + UnitAddAbility and UnitApplyTimedLife
    JassVM vm;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto callNative = [&](const std::string& name, const std::vector<JassValue>& args) -> JassValue {
        const auto& table = vm.GetRegisteredNatives();
        auto it = table.find(name);
        return it != table.end() ? it->second(args) : JassValue::Null();
    };

    const int uHandle = natives.GetData()->AllocHandle(JassHandleInfo::UNIT, footman->GetHandleId());
    const int baseDef = footman->GetTotalDefense();

    const int buffHandle1 = callNative("CreateJassBuff", {
        JassValue::Int(static_cast<int>(War3ID("BCs1").value)),
        JassValue::Int(static_cast<int>(War3ID("BCs1").value))
    }).intVal;
    TEST_ASSERT(buffHandle1 != 0);
    TEST_ASSERT(callNative("UnitAddAbility", {
        JassValue::Int(uHandle),
        JassValue::Int(buffHandle1)
    }).boolVal);
    TEST_ASSERT(footman->HasBuff(War3ID("BCs1")));
    TEST_ASSERT(footman->GetTotalDefense() == baseDef + 6);
    TEST_ASSERT(footman->GetNonStackingDamageTotal() == 20);

    // Apply custom creator buff "BCs2" directly by rawcode via UnitAddAbility
    TEST_ASSERT(!footman->IsSilenced());
    TEST_ASSERT(callNative("UnitAddAbility", {
        JassValue::Int(uHandle),
        JassValue::Int(static_cast<int>(War3ID("BCs2").value))
    }).boolVal);
    TEST_ASSERT(footman->IsSilenced());
    TEST_ASSERT(war3Mod->GetModuleStats().customBuffsApplied == 2u);

    // Verify UnitApplyTimedLife & UnitPauseTimedLife route through ISimulationModule::CreateTimedLifeBuff
    TEST_ASSERT(!footman->IsSummoned());
    callNative("UnitApplyTimedLife", {
        JassValue::Int(uHandle),
        JassValue::Int(static_cast<int>(War3ID("BTLF").value)),
        JassValue::Real(25.0f)
    });
    TEST_ASSERT(footman->IsSummoned());
    callNative("UnitPauseTimedLife", {
        JassValue::Int(uHandle),
        JassValue::Bool(true)
    });
    TEST_ASSERT(footman->GetBuff(War3ID("BTLF"))->IsSuspended());

    TEST_ASSERT(war3Mod->UnregisterCustomBuff(War3ID("BCs1")));
    TEST_ASSERT(war3Mod->UnregisterCustomBuff(War3ID("BCs2")));
    return true;
}

bool TestPhase22StageAAA2DynamicPluginSOV22AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath22 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths22[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths22) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath22 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath22.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase22PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath22));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 22u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TwistedMeadows.w3x",
        "resources/Maps/FrozenThrone/Scenario/(6)BlizzardTD.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 22u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}

// ============================================================================
#include "ecs/behaviors/behavior_move.h"
#include "ecs/behaviors/behavior_root.h"
// Phase 23 Modernization Tests: Engine-Level Pluggable CProjectile & CUnitBehavior
// ============================================================================

bool TestPhase23StageBBB1CompactCProjectileLayoutMakeAttackProjectileAndModuleHooks() {
    // 1. Verify compact CProjectile layout (1-byte bitfield state flags save 16 bytes vs 6 bools on 64-bit targets)
    TEST_ASSERT(sizeof(CProjectile) <= sizeof(std::string) + sizeof(std::function<void(CSimulation&, CUnit*, CWidget*)>) + 104);

    CProjectile p;
    TEST_ASSERT(p.isAttack && p.isRanged && !p.groundTarget && !p.checkEffectReaction && !p.pierceReflected && !p.customImpactOnly);
    TEST_ASSERT(p.projectileTypeId.value == 0);

    // 2. Verify CProjectile::MakeAttackProjectile unified factory for homing, bounce, splash, line, and artillery
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    auto* shooter = sim.AdoptUnit(std::make_unique<CUnit>(101, MakePhase5UnitType(War3ID("hrif"), 500, 0, false), 0, War3ID("hrif"), 0.0f, 0.0f, 0.0f));
    auto* target1 = sim.AdoptUnit(std::make_unique<CUnit>(102, MakePhase5UnitType(War3ID("ogru"), 800, 0, false), 1, War3ID("ogru"), 200.0f, 0.0f, 0.0f));
    auto* target2 = sim.AdoptUnit(std::make_unique<CUnit>(103, MakePhase5UnitType(War3ID("ogru"), 800, 0, false), 1, War3ID("ogru"), 320.0f, 0.0f, 0.0f));
    sim.Update();

    CUnitAttack atk;
    atk.weaponType = CWeaponType::MISSILE_BOUNCE;
    atk.attackType = CAttackType::PIERCE;
    atk.projectileSpeed = 1000.0f;
    atk.areaHalf = 300.0f;
    atk.targetCount = 1;
    atk.damageLossFactor = 0.5f;

    CProjectile bounceShot = CProjectile::MakeAttackProjectile(shooter, atk, 40.0f, "Rifle", target1, 200.0f, 0.0f, false);
    TEST_ASSERT(!bounceShot.groundTarget);
    TEST_ASSERT(bounceShot.targetHandleId == target1->GetHandleId());
    TEST_ASSERT(bounceShot.bouncesLeft == 1);
    TEST_NEAR(bounceShot.bounceLossFactor, 0.5f, 1e-4f);

    // 3. Verify Dragon Scale (BIdd) pierce projectile reflection via ISimulationModule::OnProjectilePreImpact
    target1->AddBuff(std::make_unique<CBuffDamageMod>(War3ID("BIdd"), 1.0f, 1.0f, 0.5f, 0.0f), sim);
    CProjectile reflectShot = CProjectile::MakeAttackProjectile(shooter, atk, 40.0f, "", target1, 200.0f, 0.0f, false);
    reflectShot.bouncesLeft = 0;
    sim.SpawnProjectile(std::move(reflectShot));

    const float shooterLifeBefore = shooter->GetLife();
    const float target1LifeBefore = target1->GetLife();
    for (int i = 0; i < 40 && !sim.GetProjectiles().empty(); ++i) {
        sim.Update();
    }
    TEST_ASSERT(sim.GetProjectiles().empty());
    // Target1 reflected the pierce shot back to shooter!
    TEST_NEAR(target1->GetLife(), target1LifeBefore, 1e-3f);
    TEST_ASSERT(shooter->GetLife() < shooterLifeBefore);

    return true;
}

bool TestPhase23StageBBB2CompleteCBehaviorPolymorphicContractAndZeroDynamicCast() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* worker = sim.AdoptUnit(std::make_unique<CUnit>(201, MakePhase5UnitType(War3ID("hpea"), 250, 0, false), 0, War3ID("hpea"), 0.0f, 0.0f, 0.0f));
    sim.Update();

    // 1. Verify polymorphic CBehavior slot attachment and FlowField steering queries via CBehavior*
    auto moveBeh = std::make_unique<CBehaviorMove>();
    moveBeh->SetUseFlowFieldSteering(true);
    worker->SetMoveBehavior(std::move(moveBeh));
    TEST_ASSERT(worker->GetMoveBehavior() != nullptr);
    TEST_ASSERT(worker->GetMoveBehavior()->IsUsingFlowFieldSteering());
    TEST_ASSERT(worker->GetMoveBehavior()->TracksPropellingState());

    // 2. Verify CBehaviorUproot inherits CBehaviorRoot and supports virtual duration/remaining
    auto uprootBeh = std::make_unique<CBehaviorUproot>(0.4f);
    TEST_ASSERT(uprootBeh->GetCategory() == CBehaviorCategory::ROOT);
    TEST_NEAR(uprootBeh->GetRemaining(), 0.4f, 1e-4f);
    uprootBeh->SetRemaining(0.2f);
    TEST_NEAR(uprootBeh->GetRemaining(), 0.2f, 1e-4f);

    // 3. Verify CBehaviorStun refresh through CBuffStun::OnRefresh without dynamic_cast
    worker->AddBuff(std::make_unique<CBuffStun>(1.5f), sim);
    TEST_ASSERT(worker->GetCurrentBehavior() != nullptr);
    TEST_ASSERT(worker->GetCurrentBehavior()->GetCategory() == CBehaviorCategory::STUN);
    TEST_NEAR(worker->GetCurrentBehavior()->GetRemaining(), 1.5f, 1e-4f);
    worker->AddBuff(std::make_unique<CBuffStun>(3.0f), sim);
    TEST_NEAR(worker->GetCurrentBehavior()->GetRemaining(), 3.0f, 1e-4f);

    return true;
}

bool TestPhase23StageCCC1War3GameModuleV23CustomProjectilesAndCustomBehaviors() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 23u);
    sim.SetSimulationModule(war3Mod);

    auto* caster = sim.AdoptUnit(std::make_unique<CUnit>(301, MakePhase5UnitType(War3ID("Hamg"), 700, 300, false), 0, War3ID("Hamg"), 0.0f, 0.0f, 0.0f));
    auto* enemy1 = sim.AdoptUnit(std::make_unique<CUnit>(302, MakePhase5UnitType(War3ID("ogru"), 600, 0, false), 1, War3ID("ogru"), 180.0f, 0.0f, 0.0f));
    auto* enemy2 = sim.AdoptUnit(std::make_unique<CUnit>(303, MakePhase5UnitType(War3ID("ogru"), 600, 0, false), 1, War3ID("ogru"), 300.0f, 0.0f, 0.0f));
    sim.Update();

    // 1. Register a custom buff to be applied on projectile impact
    DataDrivenBuffSpec impactBuff;
    impactBuff.id = War3ID("BCsP");
    impactBuff.duration = 5.0f;
    impactBuff.isPositive = false;
    impactBuff.armorDelta = -5.0f;
    war3Mod->RegisterCustomBuffSpec(impactBuff);

    // 2. Register a declarative CustomProjectileSpec with bounce + impact buff + callback
    int impactCallbacks = 0;
    war3::War3GameModule::CustomProjectileSpec projSpec;
    projSpec.code = War3ID("PCs1");
    projSpec.speed = 1200.0f;
    projSpec.damage = 60.0f;
    projSpec.attackType = CAttackType::SPELLS;
    projSpec.damageType = CDamageType::MAGIC;
    projSpec.bounces = 1;
    projSpec.bounceRange = 350.0f;
    projSpec.bounceLossFactor = 0.5f;
    projSpec.impactBuffCode = War3ID("BCsP");
    projSpec.impactBuffDuration = 5.0f;
    projSpec.onImpact = [&](CSimulation&, CUnit*, CWidget*, float, float) {
        ++impactCallbacks;
    };
    war3Mod->RegisterCustomProjectileSpec(projSpec);
    TEST_ASSERT(war3Mod->HasCustomProjectile(War3ID("PCs1")));

    const float e1LifeBefore = enemy1->GetLife();
    const float e2LifeBefore = enemy2->GetLife();
    TEST_ASSERT(war3Mod->SpawnCustomProjectile(sim, War3ID("PCs1").value, caster, enemy1));
    TEST_ASSERT(war3Mod->GetModuleStats().customProjectilesSpawned == 1u);

    for (int i = 0; i < 30 && !sim.GetProjectiles().empty(); ++i) {
        sim.Update();
    }
    TEST_ASSERT(sim.GetProjectiles().empty());
    TEST_ASSERT(impactCallbacks == 2); // hit enemy1 and bounced to enemy2!
    TEST_ASSERT(enemy1->GetLife() < e1LifeBefore);
    TEST_ASSERT(enemy2->GetLife() < e2LifeBefore);
    TEST_ASSERT(enemy1->HasBuff(War3ID("BCsP")));
    TEST_ASSERT(enemy2->HasBuff(War3ID("BCsP")));

    // 3. Register a declarative CustomBehaviorSpec and dispatch via CUnit::IssueOrder
    constexpr int kCustomSurveyOrderId = 852991;
    int beginCount = 0;
    int tickCount = 0;
    int endCount = 0;

    war3::War3GameModule::CustomBehaviorSpec behSpec;
    behSpec.code = War3ID("BhSv");
    behSpec.orderId = kCustomSurveyOrderId;
    behSpec.category = CBehaviorCategory::BUILD;
    behSpec.range = 80.0f;
    behSpec.duration = 0.5f;
    behSpec.tickInterval = 0.15f;
    behSpec.onBegin = [&](CSimulation&, CUnit*, CWidget*, float, float) { ++beginCount; };
    behSpec.onTick = [&](CSimulation&, CUnit*, CWidget*, float, float, float) -> bool {
        ++tickCount;
        return false;
    };
    behSpec.onEnd = [&](CSimulation&, CUnit*) { ++endCount; };
    war3Mod->RegisterCustomBehaviorSpec(behSpec);
    TEST_ASSERT(war3Mod->HasCustomBehavior(kCustomSurveyOrderId));

    caster->IssueOrder(std::make_unique<COrderTargetPoint>(kCustomSurveyOrderId, 160.0f, 0.0f));
    sim.Update();
    TEST_ASSERT(caster->GetCurrentBehavior() != nullptr);
    TEST_ASSERT(caster->GetCurrentBehavior()->GetCustomBehaviorCode() == War3ID("BhSv"));
    TEST_ASSERT(caster->GetCurrentBehavior()->IsMovingToWorkSite());

    // Step until caster walks to (160, 0), executes onBegin + ticks, and finishes
    for (int i = 0; i < 60 && caster->GetCurrentBehavior() != nullptr; ++i) {
        sim.Update();
    }
    TEST_ASSERT(beginCount == 1);
    TEST_ASSERT(tickCount >= 2);
    TEST_ASSERT(endCount == 1);
    TEST_ASSERT(war3Mod->GetModuleStats().customBehaviorsDispatched == 1u);

    TEST_ASSERT(war3Mod->UnregisterCustomProjectile(War3ID("PCs1")));
    TEST_ASSERT(war3Mod->UnregisterCustomBehavior(kCustomSurveyOrderId));
    TEST_ASSERT(war3Mod->UnregisterCustomBuff(War3ID("BCsP")));
    return true;
}

bool TestPhase23StageCCC2DynamicPluginSOV23AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath23 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths23[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths23) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath23 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath23.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase23PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath23));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 23u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(8)GardenOfWar.w3x",
        "resources/Maps/FrozenThrone/Scenario/(8)AzerothGrandPrix.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 23u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}

// ============================================================================
// Phase 24 Modernization Tests: Engine-Level Behavior Factories, Compact
// CItem/CDestructable Bitfields, and War3GameModule v24 Resource Dropoff Rules
// ============================================================================

bool TestPhase24StageDDD1EngineBehaviorStaticFactoriesAndZeroAbilityBehaviorIncludes() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(401, MakePhase5UnitType(War3ID("hpea"), 250, 100, false), 0, War3ID("hpea"), 0.0f, 0.0f, 0.0f));
    auto* target = sim.AdoptUnit(std::make_unique<CUnit>(402, MakePhase5UnitType(War3ID("hfoo"), 420, 0, true), 0, War3ID("hfoo"), 64.0f, 0.0f, 0.0f));
    sim.Update();

    // 1. Verify CBehavior::Create* static factories construct valid polymorphic CBehavior instances
    auto mBeh = CBehavior::CreateMove();
    auto fBeh = CBehavior::CreateFollow();
    auto pBeh = CBehavior::CreatePatrol();
    auto hBeh = CBehavior::CreateHoldPosition();
    auto sBeh = CBehavior::CreateStop();
    auto aBeh = CBehavior::CreateAttack();
    auto amBeh = CBehavior::CreateAttackMove();
    auto stBeh = CBehavior::CreateStun(2.25f);

    TEST_ASSERT(mBeh && mBeh->GetCategory() == CBehaviorCategory::MOVE);
    TEST_ASSERT(fBeh && fBeh->GetCategory() == CBehaviorCategory::FOLLOW);
    TEST_ASSERT(pBeh && pBeh->GetCategory() == CBehaviorCategory::PATROL);
    TEST_ASSERT(hBeh && hBeh->GetCategory() == CBehaviorCategory::HOLD_POSITION);
    TEST_ASSERT(sBeh && sBeh->GetCategory() == CBehaviorCategory::STOP);
    TEST_ASSERT(aBeh && aBeh->GetCategory() == CBehaviorCategory::ATTACK);
    TEST_ASSERT(amBeh && amBeh->GetCategory() == CBehaviorCategory::ATTACK_MOVE);
    TEST_ASSERT(stBeh && stBeh->GetCategory() == CBehaviorCategory::STUN);
    TEST_NEAR(stBeh->GetRemaining(), 2.25f, 1e-4f);

    // 2. Verify order-specific raw behavior factories (RootMorph, LoadPassenger, BoardTransport, DropCargo, Repair)
    std::unique_ptr<CBehavior> rootBeh(CBehavior::CreateRootMorph(0.6f, false));
    std::unique_ptr<CBehavior> uprootBeh(CBehavior::CreateRootMorph(0.8f, true));
    std::unique_ptr<CBehavior> loadBeh(CBehavior::CreateLoadPassenger(256.0f, target));
    std::unique_ptr<CBehavior> boardBeh(CBehavior::CreateBoardTransport(256.0f, 8, target));
    std::unique_ptr<CBehavior> dropBeh(CBehavior::CreateDropCargo(128.0f, 128.0f, 0.5f));
    std::unique_ptr<CBehavior> repBeh(CBehavior::CreateRepair(unit, nullptr, target));

    TEST_ASSERT(rootBeh && rootBeh->GetCategory() == CBehaviorCategory::ROOT && std::fabs(rootBeh->GetRemaining() - 0.6f) < 1e-4f);
    TEST_ASSERT(uprootBeh && uprootBeh->GetCategory() == CBehaviorCategory::ROOT && std::fabs(uprootBeh->GetRemaining() - 0.8f) < 1e-4f);
    TEST_ASSERT(loadBeh && loadBeh->GetCategory() == CBehaviorCategory::BOARD_TRANSPORT);
    TEST_ASSERT(boardBeh && boardBeh->GetCategory() == CBehaviorCategory::BOARD_TRANSPORT && boardBeh->GetTarget() == target);
    TEST_ASSERT(dropBeh && dropBeh->GetCategory() == CBehaviorCategory::SPELL);
    TEST_ASSERT(repBeh && repBeh->GetCategory() == CBehaviorCategory::BUILD && repBeh->GetTarget() == target);

    return true;
}

bool TestPhase24StageDDD2CompactCItemCDestructableBitfieldsAndWorkerNavigation() {
    // 1. Verify CItem 16-bit bitfield state flags & int8_t inventorySlot_
    CItem item(501, War3ID("rat9"), 100.0f, 200.0f);
    TEST_ASSERT(!item.IsPowerup() && !item.IsPermanent() && !item.IsCharged() && !item.IsPerishable());
    TEST_ASSERT(!item.IsUseAutomatically() && !item.IsHidden() && !item.IsInvulnerable());
    TEST_ASSERT(item.IsDroppable() && item.IsPawnable() && item.IsDroppedOnDeath() && !item.IsInInventory());
    TEST_ASSERT(item.GetInventorySlot() == -1);

    item.SetPowerup(true);
    item.SetCharged(true);
    item.SetPerishable(true);
    item.SetInInventory(true);
    item.SetInventorySlot(4);
    item.SetInvulnerable(true);
    TEST_ASSERT(item.IsPowerup() && item.IsCharged() && item.IsPerishable() && item.IsInInventory() && item.IsInvulnerable());
    TEST_ASSERT(item.GetInventorySlot() == 4);

    // 2. Verify CDestructable 8-bit bitfield state flags (invulnerable, hidden, blighted)
    CDestructable dest(502, War3ID("LTlt"), 300.0f, 400.0f, 50.0f);
    TEST_ASSERT(!dest.IsInvulnerable() && !dest.IsHidden() && !dest.IsBlighted());
    dest.SetInvulnerable(true);
    dest.SetHidden(true);
    dest.SetBlighted(true);
    TEST_ASSERT(dest.IsInvulnerable() && dest.IsHidden() && dest.IsBlighted());

    return true;
}

bool TestPhase24StageEEE1War3GameModuleV24CustomDropoffsAndBehaviorCategoryOverrides() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 24u);
    sim.SetSimulationModule(war3Mod);

    // 1. Register a custom dropoff unit type ("hcdp": custom mobile dropoff wagon without Aret ability)
    war3Mod->RegisterCustomDropoffUnitType(War3ID("hcdp"), true, false);
    TEST_ASSERT(war3Mod->HasCustomDropoffUnitType(War3ID("hcdp")));

    auto* worker = sim.AdoptUnit(std::make_unique<CUnit>(601, MakePhase5UnitType(War3ID("hpea"), 250, 0, false), 0, War3ID("hpea"), 0.0f, 0.0f, 0.0f));
    auto* customDropoff = sim.AdoptUnit(std::make_unique<CUnit>(602, MakePhase5UnitType(War3ID("hcdp"), 600, 0, false), 0, War3ID("hcdp"), 96.0f, 0.0f, 0.0f));
    sim.Update();

    worker->AddAbility(sim, AbilityFactory::CreateAbility(sim.AllocateHandleId(), War3ID("Ahar")));
    CAbility* harv = worker->GetAbility(War3ID("Ahar"));
    TEST_ASSERT(harv != nullptr);
    harv->SetCarried(CResourceType::GOLD, 10);

    // Verify CanUnitAcceptResourceDropoff and OnFindNearestResourceDropoff recognize customDropoff for GOLD (but not LUMBER)
    TEST_ASSERT(war3Mod->CanUnitAcceptResourceDropoff(*customDropoff, static_cast<int>(CResourceType::GOLD)));
    TEST_ASSERT(!war3Mod->CanUnitAcceptResourceDropoff(*customDropoff, static_cast<int>(CResourceType::LUMBER)));
    TEST_ASSERT(war3Mod->OnFindNearestResourceDropoff(sim, *worker, static_cast<int>(CResourceType::GOLD)) == customDropoff);

    // Issue RETURN_RESOURCES order to worker and verify it deposits 10 gold at customDropoff
    const int goldBefore = sim.GetPlayer(0)->GetGold();
    worker->IssueOrder(std::make_unique<COrderNoTarget>(OrderIds::RETURN_RESOURCES));
    for (int i = 0; i < 15 && harv->IsCarryingResources(); ++i) {
        sim.Update();
    }
    TEST_ASSERT(!harv->IsCarryingResources());
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == goldBefore + 10);

    // 2. Verify RegisterCategoryBehaviorOverride on STOP category
    int customStopTriggered = 0;
    war3Mod->RegisterCategoryBehaviorOverride(CBehaviorCategory::STOP, [&]() -> CBehavior* {
        class CCustomStopBehavior final : public CBehavior {
        public:
            explicit CCustomStopBehavior(int& counter) : counter_(counter) {}
            CBehaviorCategory GetCategory() const override { return CBehaviorCategory::STOP; }
            void Begin(CSimulation&) override { ++counter_; if (unit_) unit_->ClearOrders(); }
            bool Update(CSimulation&) override { return true; }
        private:
            int& counter_;
        };
        return new CCustomStopBehavior(customStopTriggered);
    });
    TEST_ASSERT(war3Mod->HasCategoryBehaviorOverride(CBehaviorCategory::STOP));

    worker->IssueOrder(std::make_unique<COrderNoTarget>(OrderIds::STOP));
    sim.Update();
    TEST_ASSERT(customStopTriggered == 1);

    TEST_ASSERT(war3Mod->UnregisterCategoryBehaviorOverride(CBehaviorCategory::STOP));
    TEST_ASSERT(war3Mod->UnregisterCustomDropoffUnitType(War3ID("hcdp")));
    return true;
}

bool TestPhase24StageEEE2DynamicPluginSOV24AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath24 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths24[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths24) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath24 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath24.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase24PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath24));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 24u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TurtleRock.w3x",
        "resources/Maps/FrozenThrone/Scenario/(10)ExtremeCandyWar2004.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 24u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}

// ============================================================================
// Phase 25 Modernization Tests: CFogSystem Bitmask Sight & RAII FogOfWar,
// CWorldCollision/CPathfindingProcessor Pluggable Passability, and
// SimStateSnapshot POD Projectile & Module State Serialization
// ============================================================================

bool TestPhase25StageFFF1BitmaskSharedVisionSightAndModuleVisionHooks() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    sim.AddPlayer(std::make_unique<CPlayer>(2, "P2", CMapControl::COMPUTER));
    sim.GetPlayer(0)->SetAlliance(1, CAllianceType::SHARED_VISION, true);

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 25u);
    sim.SetSimulationModule(war3Mod);

    // 1. Verify CFogSightSource 32-bit playerMask serves multiple players from a single struct
    CFogSightSource multiSrc;
    multiSrc.player = 0;
    multiSrc.playerMask = (1u << 0) | (1u << 1) | (1u << 2);
    multiSrc.x = 512.0f;
    multiSrc.y = 512.0f;
    multiSrc.radius = 600.0f;
    multiSrc.isFlying = true;
    sim.GetFogSystem()->UpdateSight(&multiSrc, 1);

    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 512.0f, 512.0f));
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(1, 512.0f, 512.0f));
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(2, 512.0f, 512.0f));
    TEST_ASSERT(!sim.GetFogSystem()->IsVisibleTo(3, 512.0f, 512.0f));

    // 2. Verify War3GameModule sight multiplier & custom radar sight source injection
    war3Mod->SetPlayerSightMultiplier(0, 2.0f);
    TEST_NEAR(war3Mod->GetPlayerSightMultiplier(0), 2.0f, 1e-4f);
    war3Mod->RegisterCustomSightSource(777, 0, -1024.0f, -1024.0f, 500.0f, true);
    TEST_ASSERT(war3Mod->HasCustomSightSource(777));

    auto* scout = sim.AdoptUnit(std::make_unique<CUnit>(701, MakePhase5UnitType(War3ID("hfoo"), 420, 0, false), 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    scout->SetSharesVisionWith(2, true);
    for (int i = 0; i < 12; ++i) sim.Update();

    // Player 0, allied Player 1 (SHARED_VISION), and Player 2 (UnitShareVision) all see scout at (0,0)
    // and Player 0 + allied Player 1 also see custom radar sight source at (-1024, -1024)!
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, 0.0f, 0.0f));
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(1, 0.0f, 0.0f));
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(2, 0.0f, 0.0f));
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(0, -1024.0f, -1024.0f));
    TEST_ASSERT(sim.GetFogSystem()->IsVisibleTo(1, -1024.0f, -1024.0f));

    TEST_ASSERT(war3Mod->UnregisterCustomSightSource(777));
    return true;
}

bool TestPhase25StageFFF2WorldCollisionProximityAndPluggableCellPassability() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    auto* u1 = sim.AdoptUnit(std::make_unique<CUnit>(801, MakePhase5UnitType(War3ID("hfoo"), 420, 0, false), 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    auto* u2 = sim.AdoptUnit(std::make_unique<CUnit>(802, MakePhase5UnitType(War3ID("ogru"), 700, 0, false), 1, War3ID("ogru"), 180.0f, 0.0f, 0.0f));
    auto* u3 = sim.AdoptUnit(std::make_unique<CUnit>(803, MakePhase5UnitType(War3ID("ogru"), 700, 0, false), 1, War3ID("ogru"), 420.0f, 0.0f, 0.0f));
    sim.Update();

    // 1. Verify CWorldCollision SpatialGrid binding, FindNearestUnit, and CountUnitsInRange
    auto* wc = sim.GetWorldCollision();
    TEST_ASSERT(wc != nullptr);
    TEST_ASSERT(wc->GetSpatialGrid() == &sim.SpatialGrid());
    wc->ResetTelemetry();

    TEST_ASSERT(wc->CountUnitsInRange(0.0f, 0.0f, 250.0f) == 2u);
    CUnit* nearestEnemy = wc->FindNearestUnit(0.0f, 0.0f, 500.0f, [&](const CUnit* cand) {
        return cand != u1 && cand->GetPlayerIndex() == 1;
    });
    TEST_ASSERT(nearestEnemy == u2);
    TEST_ASSERT(wc->GetRangeQueriesPerformed() >= 2u);

    // 2. Verify pluggable custom cell passability rules on War3GameModule -> CPathfindingProcessor
    auto* pf = sim.GetPathfinding();
    TEST_ASSERT(pf != nullptr);
    TEST_ASSERT(pf->IsCellPathableFor(10, 10, PathingGridMovementType::FOOT));

    // Block ground movement on cell (10, 10) while keeping air flyable
    war3Mod->SetCustomCellPassabilityRule(10, 10, false, true);
    TEST_ASSERT(!pf->IsCellPathableFor(10, 10, PathingGridMovementType::FOOT));
    TEST_ASSERT(pf->IsCellPathableFor(10, 10, PathingGridMovementType::FLY));

    TEST_ASSERT(war3Mod->RemoveCustomCellPassabilityRule(10, 10));
    TEST_ASSERT(pf->IsCellPathableFor(10, 10, PathingGridMovementType::FOOT));
    return true;
}

bool TestPhase25StageGGG1SafePodProjectileSnapshotAndModuleStateRoundTrip() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->SetBountyMultiplier(2.5f);
    war3Mod->SetExperienceMultiplier(3.0f);
    war3Mod->SetDefenseArmorFactor(0.08f);
    war3Mod->SetMeleeVictoryRulesEnabled(true);
    war3Mod->SetPlayerSightMultiplier(0, 1.75f);
    war3Mod->RegisterCustomSightSource(888, 0, 256.0f, 256.0f, 400.0f, true);
    war3Mod->SetCustomCellPassabilityRule(12, 14, false, true);
    sim.SetSimulationModule(war3Mod);

    auto* archer = sim.AdoptUnit(std::make_unique<CUnit>(901, MakePhase5UnitType(War3ID("earc"), 245, 0, false), 0, War3ID("earc"), 0.0f, 0.0f, 0.0f));
    auto*target = sim.AdoptUnit(std::make_unique<CUnit>(902, MakePhase5UnitType(War3ID("ogru"), 700, 0, false), 1, War3ID("ogru"), 600.0f, 0.0f, 0.0f));
    sim.Update();

    // Spawn an in-flight projectile with non-empty weaponSound and custom projectileTypeId
    CProjectile shot;
    shot.attackerHandleId = archer->GetHandleId();
    shot.targetHandleId = target->GetHandleId();
    shot.x = 120.0f;
    shot.y = 40.0f;
    shot.speed = 900.0f;
    shot.damage = 35.0f;
    shot.projectileTypeId = War3ID("PCs9");
    shot.attackType = CAttackType::PIERCE;
    shot.damageType = CDamageType::NORMAL;
    shot.weaponSound = "MetalMediumSlice";
    shot.bouncesLeft = 2;
    shot.bounceRange = 400.0f;
    shot.maxTravel = 1200.0f;
    sim.SpawnProjectile(std::move(shot));

    // Capture full snapshot, serialize to binary wire format, and deserialize
    SimStateSnapshot snap = SimStateSnapshot::Capture(sim);
    TEST_ASSERT(snap.projectiles.size() == 1u);
    TEST_ASSERT(!snap.moduleStateBytes.empty());

    std::vector<uint8_t> bytes = snap.SerializeBinary();
    SimStateSnapshot decoded;
    TEST_ASSERT(SimStateSnapshot::DeserializeBinary(bytes.data(), bytes.size(), decoded));
    TEST_ASSERT(decoded.projectiles.size() == 1u);
    TEST_ASSERT(decoded.projectiles[0].projectileTypeId == War3ID("PCs9"));
    TEST_ASSERT(decoded.projectiles[0].weaponSound == "MetalMediumSlice");
    TEST_ASSERT(decoded.projectiles[0].bouncesLeft == 2);
    TEST_NEAR(decoded.projectiles[0].damage, 35.0f, 1e-4f);

    // Mutate live simulation and module settings, then restore from decoded binary snapshot
    war3Mod->SetBountyMultiplier(1.0f);
    war3Mod->SetExperienceMultiplier(1.0f);
    war3Mod->SetDefenseArmorFactor(0.06f);
    war3Mod->SetMeleeVictoryRulesEnabled(false);
    war3Mod->SetPlayerSightMultiplier(0, 1.0f);
    war3Mod->UnregisterCustomSightSource(888);
    war3Mod->RemoveCustomCellPassabilityRule(12, 14);
    sim.Update();
    sim.Update();

    TEST_ASSERT(decoded.Restore(sim));
    TEST_NEAR(war3Mod->GetBountyMultiplier(), 2.5f, 1e-4f);
    TEST_NEAR(war3Mod->GetExperienceMultiplier(), 3.0f, 1e-4f);
    TEST_NEAR(war3Mod->GetDefenseArmorFactor(), 0.08f, 1e-4f);
    TEST_ASSERT(war3Mod->IsMeleeVictoryRulesEnabled());
    TEST_NEAR(war3Mod->GetPlayerSightMultiplier(0), 1.75f, 1e-4f);
    TEST_ASSERT(war3Mod->HasCustomSightSource(888));
    TEST_ASSERT(war3Mod->OnCheckCustomCellPassability(12, 14, static_cast<int>(PathingGridMovementType::FOOT)) == 0);
    TEST_ASSERT(war3Mod->OnCheckCustomCellPassability(12, 14, static_cast<int>(PathingGridMovementType::FLY)) == 1);
    TEST_ASSERT(sim.GetProjectiles().size() == 1u);
    TEST_ASSERT(sim.GetProjectiles()[0].weaponSound == "MetalMediumSlice");

    return true;
}

bool TestPhase25StageGGG2DynamicPluginSOV25AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath25 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths25[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths25) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath25 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath25.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase25PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath25));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 25u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(8)TwilightRuins.w3x",
        "resources/Maps/FrozenThrone/Scenario/(12)WormWar.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 25u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}


// ============================================================================
// Phase 26 Modernization Tests: Lockstep Command Bus & Memory Replay Stream,
// TurnManager Seat Bitfield & History Cap, LockstepHost Fast Keyframe Resync,
// LobbyState Skirmish Generalization, and War3GameModule Version 26
// ============================================================================

bool TestPhase26StageHHH1ReplayMemoryStreamAndPlayerCommandExecution() {
    // 1. Verify shared LE wire helpers & ReplayLog zero-disk-IO memory serialization
    ReplayLog log;
    log.Begin(0xCAFEBABEu);
    SimCommand c1{1, 0, 101, 851971, 1, -1, 256.0f, 512.0f, 0.0f};
    SimCommand c2{2, 0, -1, war3::War3GameModule::kOrderTributeGold, 0, 1, 150.0f, 0.0f, 0.0f};
    TEST_ASSERT(!c1.IsGlobalPlayerCommand());
    TEST_ASSERT(c2.IsGlobalPlayerCommand());
    log.Record(c1);
    log.Record(c2);

    std::vector<uint8_t> memWire = log.SerializeMemory();
    TEST_ASSERT(memWire.size() == 16 + 2 * SimCommand::kWireSize);

    ReplayLog decodedLog;
    TEST_ASSERT(decodedLog.DeserializeMemory(memWire.data(), memWire.size()));
    TEST_ASSERT(decodedLog.Seed() == 0xCAFEBABEu);
    TEST_ASSERT(decodedLog.Commands().size() == 2u);
    TEST_ASSERT(decodedLog.Commands()[0] == c1);
    TEST_ASSERT(decodedLog.Commands()[1] == c2);

    // Trailing byte must be rejected
    std::vector<uint8_t> corrupted = memWire;
    corrupted.push_back(0xFFu);
    ReplayLog badLog;
    TEST_ASSERT(!badLog.DeserializeMemory(corrupted.data(), corrupted.size()));

    // 2. Verify ApplyBatch & ExecuteSimCommand dispatch both unit orders and global player commands
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.GetPlayer(0)->SetGold(500);
    sim.GetPlayer(1)->SetGold(200);

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 26u);
    sim.SetSimulationModule(war3Mod);

    auto* footman = sim.AdoptUnit(std::make_unique<CUnit>(101, MakePhase5UnitType(War3ID("hfoo"), 420, 0, false), 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    (void)footman;
    sim.Update();
    sim.ClearCommandLog();

    TurnBatch batch;
    batch.turn = sim.GetGameTurnTick();
    batch.commands.push_back(c1);
    batch.commands.push_back(c2);

    const int applied = ApplyBatch(sim, batch);
    TEST_ASSERT(applied == 2);
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 350);
    TEST_ASSERT(sim.GetPlayer(1)->GetGold() == 350);
    TEST_ASSERT(sim.LoggedCommands().size() == 2u);
    return true;
}

bool TestPhase26StageHHH2TurnManagerHistoryCapLockstepHostFastKeyframeResyncAndLobbySkirmish() {
    // 1. Verify TurnManager compact seatFlags_ & bounded history ring cap
    TurnManager tm(2);
    tm.SetMaxHistoryTurns(4);
    TEST_ASSERT(tm.GetMaxHistoryTurns() == 4u);

    for (int t = 0; t < 10; ++t) {
        SimCommand cmd{t, 0, 101, 851971, 1, -1, float(t * 10), 0.0f, 0.0f};
        TEST_ASSERT(tm.SubmitPlayerCommands(0, {cmd}));
        TEST_ASSERT(tm.SubmitPlayerCommands(1, {}));
        TEST_ASSERT(tm.IsComplete());
        TurnBatch merged = tm.TakeBatch();
        TEST_ASSERT(merged.turn == t);
    }
    TEST_ASSERT(tm.History().size() == 4u);
    TEST_ASSERT(tm.GetPrunedTurnCount() == 6u);
    TEST_ASSERT(tm.History().front().turn == 6);
    TEST_ASSERT(tm.History().back().turn == 9);

    // 2. Verify LobbyState configurable maxRaceId and CanStartSkirmish (1 Human + 1 AI)
    LobbyState lobby;
    TEST_ASSERT(lobby.GetMaxRaceId() == 4);
    LobbySlot customFactionSlot;
    customFactionSlot.name = "CustomSciFiPlayer";
    customFactionSlot.race = 6;
    customFactionSlot.controller = LobbySlotController::USER;
    customFactionSlot.ready = true;
    TEST_ASSERT(!lobby.SetSlot(0, customFactionSlot)); // rejected under default maxRaceId=4
    lobby.SetMaxRaceId(8);
    TEST_ASSERT(lobby.SetSlot(0, customFactionSlot));  // accepted under maxRaceId=8
    TEST_ASSERT(!lobby.IsReadyToStart());              // < 2 humans
    TEST_ASSERT(!lobby.CanStartSkirmish());            // no opponent yet

    LobbySlot aiOpponent;
    aiOpponent.name = "Bot";
    aiOpponent.race = 2;
    aiOpponent.controller = LobbySlotController::COMPUTER;
    TEST_ASSERT(lobby.SetSlot(1, aiOpponent));
    TEST_ASSERT(lobby.CanStartSkirmish());             // 1 ready human + 1 AI opponent can start skirmish!

    // 3. Verify LockstepHost TCP FastKeyframeResync transfer to a spectator / peer
    CSimulation hostSim;
    hostSim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    hostSim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    hostSim.AdoptUnit(std::make_unique<CUnit>(301, MakePhase5UnitType(War3ID("hfoo"), 420, 0, false), 0, War3ID("hfoo"), 128.0f, 256.0f, 0.0f));
    for (int i = 0; i < 5; ++i) hostSim.Update();

    SimStateSnapshot keyframe = SimStateSnapshot::Capture(hostSim);
    std::vector<TurnBatch> fullHistory;
    for (int i = 0; i < 3; ++i) {
        TurnBatch tb;
        tb.turn = hostSim.GetGameTurnTick();
        fullHistory.push_back(tb);
        hostSim.Update();
    }
    net::FastResyncKeyframePackage fastPkg;
    TEST_ASSERT(net::FastResyncKeyframePackage::Build(
        keyframe, fullHistory, hostSim.GetGameTurnTick(), hostSim.HashState(), fastPkg));

    TcpTransport::GlobalInit();
    LockstepHost host(2);
    TEST_ASSERT(host.Listen(0, true));
    const uint16_t port = host.Port();
    TEST_ASSERT(port > 0);

    const int clientFd = TcpTransport::Connect("127.0.0.1", port, 1000);
    TEST_ASSERT(clientFd != TcpTransport::kInvalidFd);
    const int watcherIdx = host.AcceptSpectator(1000);
    TEST_ASSERT(watcherIdx == 0);

    TEST_ASSERT(host.SendFastKeyframeResyncToWatcher(watcherIdx, fastPkg));
    net::FastResyncKeyframePackage recvPkg;
    TEST_ASSERT(RecvFastKeyframeResync(clientFd, recvPkg, 1000));
    TcpTransport::Close(clientFd);
    host.Close();
    TcpTransport::GlobalShutdown();

    CSimulation watcherSim;
    watcherSim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    watcherSim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    TEST_ASSERT(recvPkg.ApplyToSimulation(watcherSim));
    TEST_ASSERT(watcherSim.GetGameTurnTick() == hostSim.GetGameTurnTick());
    TEST_ASSERT(watcherSim.HashState() == hostSim.HashState());
    return true;
}

bool TestPhase26StageIII1War3ModuleLockstepValidationAlliedTributeAndStateHash() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.GetPlayer(0)->SetGold(1000);
    sim.GetPlayer(0)->SetLumber(500);
    sim.GetPlayer(1)->SetGold(100);
    sim.GetPlayer(1)->SetLumber(100);

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 26u);
    sim.SetSimulationModule(war3Mod);

    auto* u0 = sim.AdoptUnit(std::make_unique<CUnit>(401, MakePhase5UnitType(War3ID("hfoo"), 420, 0, false), 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    sim.Update();

    // 1. Verify player command muting blocks both unit orders and global player commands
    war3Mod->SetPlayerCommandMuted(0, true);
    TEST_ASSERT(war3Mod->IsPlayerCommandMuted(0));
    TEST_ASSERT(!sim.IssueUnitOrder(u0->GetHandleId(), std::make_unique<COrderTargetPoint>(851971, 300.0f, 300.0f, 0.0f)));
    TEST_ASSERT(!sim.IssuePlayerCommand(0, war3::War3GameModule::kOrderTributeGold, 0, 1, 200.0f));
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 1000);

    war3Mod->SetPlayerCommandMuted(0, false);
    TEST_ASSERT(!war3Mod->IsPlayerCommandMuted(0));

    // 2. Verify built-in allied gold & lumber tribute commands and custom global command registration
    const uint64_t hashBeforeTribute = sim.HashState();
    TEST_ASSERT(sim.IssuePlayerCommand(0, war3::War3GameModule::kOrderTributeGold, 0, 1, 250.0f));
    TEST_ASSERT(sim.IssuePlayerCommand(0, war3::War3GameModule::kOrderTributeLumber, 0, 1, 120.0f));
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 750);
    TEST_ASSERT(sim.GetPlayer(1)->GetGold() == 350);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 380);
    TEST_ASSERT(sim.GetPlayer(1)->GetLumber() == 220);
    TEST_ASSERT(war3Mod->GetTributeCommandsExecuted() == 2u);

    const uint64_t hashAfterTribute = sim.HashState();
    TEST_ASSERT(hashAfterTribute != hashBeforeTribute);

    // 3. Verify custom global command registration & module state hash sensitivity
    constexpr int32_t kCustomPingCmd = 0x50494E47; // "PING"
    float lastPingX = 0.0f;
    float lastPingY = 0.0f;
    war3Mod->RegisterCustomGlobalCommand(kCustomPingCmd, [&](CSimulation& /*s*/, const SimCommand& cmd) {
        lastPingX = cmd.x;
        lastPingY = cmd.y;
        return true;
    });
    TEST_ASSERT(war3Mod->HasCustomGlobalCommand(kCustomPingCmd));
    TEST_ASSERT(sim.IssuePlayerCommand(1, kCustomPingCmd, 1, -1, 768.0f, -512.0f));
    TEST_NEAR(lastPingX, 768.0f, 1e-4f);
    TEST_NEAR(lastPingY, -512.0f, 1e-4f);

    const uint64_t hashBeforeModChange = sim.HashState();
    war3Mod->SetBountyMultiplier(4.0f);
    const uint64_t hashAfterModChange = sim.HashState();
    TEST_ASSERT(hashAfterModChange != hashBeforeModChange);

    // 4. Verify HashState sensitivity to Phase 25 sight multipliers & passability rules,
    //    and reject cross-player spoofed unit SimCommand & non-finite tribute amounts
    war3Mod->SetPlayerSightMultiplier(0, 1.75f);
    const uint64_t hashAfterSightMult = sim.HashState();
    TEST_ASSERT(hashAfterSightMult != hashAfterModChange);

    SimCommand spoofedUnitCmd{sim.GetGameTurnTick(), 1, u0->GetHandleId(), 851971, 1, -1, 100.0f, 100.0f, 0.0f};
    TEST_ASSERT(!sim.ExecuteSimCommand(spoofedUnitCmd));
    TEST_ASSERT(!sim.IssuePlayerCommand(0, war3::War3GameModule::kOrderTributeGold, 0, 1, std::numeric_limits<float>::quiet_NaN()));
    TEST_ASSERT(!sim.IssuePlayerCommand(0, war3::War3GameModule::kOrderTributeGold, 0, 1, 1e20f));

    TEST_ASSERT(war3Mod->UnregisterCustomGlobalCommand(kCustomPingCmd));
    return true;
}

bool TestPhase26StageIII2DynamicPluginSOV26AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath26 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths26[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths26) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath26 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath26.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase26PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath26));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 26u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TurtleRock.w3x",
        "resources/Maps/FrozenThrone/Scenario/(6)BlizzardTD.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 26u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}


// ============================================================================
// Phase 27 Modernization Tests: ScriptEngineHost Shared Event Bus & HotSwap
// Subscription Migration, Bit-Packed PathQueryWorkspace & Weighted FlowField,
// Multi-Shape Formations & Role-Aware Frontline/Backline Sorting, and Plugin v27
// ============================================================================

bool TestPhase27StageJJJ1ScriptEngineHostBaseEventBusAndHotSwapSubscriptionMigration() {
    scripting::ScriptHostRouter router(std::make_unique<scripting::JassScriptHostAdapter>());
    TEST_ASSERT(router.GetActiveBackendKind() == scripting::ScriptBackendKind::JassLegacy);

    router.ActiveHost().SetGlobal("g_matchScore", scripting::ScriptValue::Int(42));
    const int sub1 = router.ActiveHost().SubscribeEvent(100, "OnUnitSpawnedScript", 501);
    const int sub2 = router.ActiveHost().SubscribeEvent(200, "OnTimerExpiredScript", -1);
    const int sub3 = router.ActiveHost().SubscribeEvent(100, "OnTransientUnitScript", 999);
    TEST_ASSERT(sub1 > 0 && sub2 > 0 && sub3 > 0);
    TEST_ASSERT(router.ActiveHost().GetActiveSubscriptionCount() == 3u);

    // Destroy handle 999 before hot-swapping -> only 2 active subscriptions should migrate
    router.ActiveHost().OnHandleDestroyed(999);
    TEST_ASSERT(router.ActiveHost().GetActiveSubscriptionCount() == 2u);

    auto realScriptHost = std::make_unique<scripting::RealScriptEngineHost>();
    int spawnCalls = 0;
    int timerCalls = 0;
    realScriptHost->RegisterScriptFunction("OnUnitSpawnedScript",
        [&](scripting::RealScriptEngineHost& host, const std::vector<scripting::ScriptValue>& args) {
            spawnCalls++;
            const int cur = host.GetGlobal("g_matchScore").intVal;
            host.SetGlobal("g_matchScore", scripting::ScriptValue::Int(cur + (args.empty() ? 1 : args[0].intVal)));
            return scripting::ScriptValue::Bool(true);
        });
    realScriptHost->RegisterScriptFunction("OnTimerExpiredScript",
        [&](scripting::RealScriptEngineHost& /*host*/, const std::vector<scripting::ScriptValue>& /*args*/) {
            timerCalls++;
            return scripting::ScriptValue::Bool(true);
        });

    // Hot-swap from JassVM to RealScript and verify both globals AND event subscriptions migrated!
    TEST_ASSERT(router.HotSwapBackend(std::move(realScriptHost), /*migrateGlobals=*/true));
    TEST_ASSERT(router.GetActiveBackendKind() == scripting::ScriptBackendKind::RealScript);
    TEST_ASSERT(router.ActiveHost().GetGlobal("g_matchScore").intVal == 42);
    TEST_ASSERT(router.ActiveHost().GetActiveSubscriptionCount() == 2u);

    // Fire migrated events on the new RealScript backend
    scripting::ScriptEventPayload evSpawn{100, 501, -1, 0, 10.0f, "spawn"};
    TEST_ASSERT(router.ActiveHost().FireEvent(evSpawn) == 1);
    TEST_ASSERT(spawnCalls == 1);
    TEST_ASSERT(router.ActiveHost().GetGlobal("g_matchScore").intVal == 42 + 501);

    scripting::ScriptEventPayload evTimer{200, -1, -1, 0, 1.0f, "timer"};
    TEST_ASSERT(router.ActiveHost().FireEvent(evTimer) == 1);
    TEST_ASSERT(timerCalls == 1);

    return true;
}

bool TestPhase27StageJJJ2BitPackedPathQueryWorkspaceFlatBfsFlowFieldAndWeightedTerrainCost() {
    CSimulation sim;
    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 27u);
    sim.SetSimulationModule(war3Mod);

    WpmData wpm;
    wpm.width = 32;
    wpm.height = 32;
    wpm.pathing.assign(static_cast<size_t>(wpm.width * wpm.height), 0u);
    sim.SetPathingMap(&wpm, 0.0f, 0.0f);

    auto* pf = sim.GetPathfinding();
    TEST_ASSERT(pf != nullptr);

    // 1. Solve baseline path from cell (2, 10) to cell (10, 10)
    pathfinding::PathQueryWorkspace ws;
    pathfinding::PathRequest req;
    req.requestId = 1;
    req.startWorldX = pf->CellToWorldX(2);
    req.startWorldY = pf->CellToWorldY(10);
    req.goalWorldX  = pf->CellToWorldX(10);
    req.goalWorldY  = pf->CellToWorldY(10);
    req.movementType = PathingGridMovementType::FOOT;

    pathfinding::PathResult baseRes;
    TEST_ASSERT(ws.SolveAStar(*pf, req, baseRes));
    TEST_ASSERT(baseRes.found);
    TEST_ASSERT(ws.GetBufferAllocationCount() == 1u);

    // 2. Paint high traversal cost (marsh = 12.0x) along the direct horizontal row y=10 (x=4..8)
    for (int x = 4; x <= 8; ++x) {
        war3Mod->SetCustomCellCostMultiplier(x, 10, 12.0f);
        war3Mod->SetCustomCellCostMultiplier(x, 9,  12.0f);
    }
    TEST_NEAR(war3Mod->GetCustomCellCostMultiplier(5, 10), 12.0f, 1e-4f);

    // Solve again in the same workspace (0 new buffer allocations due to bit-packed generation stamp!)
    pathfinding::PathResult detourRes;
    TEST_ASSERT(ws.SolveAStar(*pf, req, detourRes));
    TEST_ASSERT(detourRes.found);
    TEST_ASSERT(ws.GetBufferAllocationCount() == 1u);

    // Verify the weighted A* path detoured around the high-cost marsh strip (y != 10 at x=6)
    bool detouredAroundMarsh = false;
    for (const auto& [wx, wy] : detourRes.waypoints) {
        (void)wx;
        if ( pf->WorldToCellY(wy) > 10 ) {
            detouredAroundMarsh = true;
            break;
        }
    }
    TEST_ASSERT(detouredAroundMarsh);

    // 3. Verify flat-queue FlowFieldGrid reflects weighted terrain cost at cell (5, 10) vs normal cell (10, 5)
    pathfinding::FlowFieldGrid ff;
    TEST_ASSERT(ff.Build(*pf, pf->CellToWorldX(10), pf->CellToWorldY(10), 12, PathingGridMovementType::FOOT));
    const uint32_t marshIntegrationCost = ff.GetIntegrationCostAtCell(5, 10);
    const uint32_t clearIntegrationCost = ff.GetIntegrationCostAtCell(10, 5);
    TEST_ASSERT(marshIntegrationCost > clearIntegrationCost);

    // 4. Verify value-sensitive module hash and snapshot round-trip for customCellCostMultipliers_ & customUnitFormationRanks_
    war3Mod->SetCustomUnitFormationRank(War3ID("hfoo"), 0);
    const uint64_t hash12x = sim.HashState();
    war3Mod->SetCustomCellCostMultiplier(5, 10, 24.0f);
    const uint64_t hash24x = sim.HashState();
    TEST_ASSERT(hash12x != hash24x);

    const SimStateSnapshot snap = SimStateSnapshot::Capture(sim);
    war3Mod->ClearCustomCellCostMultipliers();
    war3Mod->RemoveCustomUnitFormationRank(War3ID("hfoo"));
    TEST_NEAR(war3Mod->GetCustomCellCostMultiplier(5, 10), 1.0f, 1e-4f);
    TEST_ASSERT(war3Mod->GetCustomUnitFormationRank(War3ID("hfoo")) == -1);
    TEST_ASSERT(snap.Restore(sim));
    TEST_NEAR(war3Mod->GetCustomCellCostMultiplier(5, 10), 24.0f, 1e-4f);
    TEST_ASSERT(war3Mod->GetCustomUnitFormationRank(War3ID("hfoo")) == 0);
    TEST_ASSERT(sim.HashState() == hash24x);

    war3Mod->ClearCustomCellCostMultipliers();
    war3Mod->RemoveCustomUnitFormationRank(War3ID("hfoo"));
    return true;
}

bool TestPhase27StageKKK1MultiShapeFormationsAndRoleAwareFrontlineBacklineSorting() {
    // 1. Verify Line, Circle, and Wedge formation geometry
    auto lineOffsets = core::CommandBufferPipeline::ComputeFormationOffsets(
        4, core::FormationShape::Line, 100.0f, 0.0f);
    TEST_ASSERT(lineOffsets.size() == 4u);
    // Facing 0 rad (+X forward): line slots spread along Y (lateral), depth dx == 0
    for (const auto& off : lineOffsets) {
        TEST_NEAR(off.dx, 0.0f, 1e-3f);
    }
    TEST_ASSERT(lineOffsets[0].dy != lineOffsets[3].dy);

    auto circleOffsets = core::CommandBufferPipeline::ComputeFormationOffsets(
        6, core::FormationShape::Circle, 80.0f, 0.0f);
    TEST_ASSERT(circleOffsets.size() == 6u);
    const float r0 = std::hypot(circleOffsets[0].dx, circleOffsets[0].dy);
    const float r3 = std::hypot(circleOffsets[3].dx, circleOffsets[3].dy);
    TEST_NEAR(r0, r3, 1e-3f);

    auto wedgeOffsets = core::CommandBufferPipeline::ComputeFormationOffsets(
        6, core::FormationShape::Wedge, 80.0f, 0.0f);
    TEST_ASSERT(wedgeOffsets.size() == 6u);
    // Apex leader (index 0) is at front (dx == 0), row 1 (indices 1,2) is behind (dx == -80), row 2 (indices 3,4,5) at dx == -160
    TEST_NEAR(wedgeOffsets[0].dx, 0.0f, 1e-3f);
    TEST_NEAR(wedgeOffsets[1].dx, -80.0f, 1e-3f);
    TEST_NEAR(wedgeOffsets[3].dx, -160.0f, 1e-3f);

    // 2. Verify role-aware frontline/backline sorting in IssueGroupPointOrder
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);

    // Register custom formation ranks: priest (hmpr) = backline (2), archer (earc) = midline (1), footman (hfoo) = frontline (0)
    war3Mod->SetCustomUnitFormationRank(War3ID("hmpr"), 2);
    war3Mod->SetCustomUnitFormationRank(War3ID("earc"), 1);
    war3Mod->SetCustomUnitFormationRank(War3ID("hfoo"), 0);

    // Deliberately create Priest with LOWEST handle ID (501), Archer (502), Footman with HIGHEST handle ID (503)
    auto* priest  = sim.AdoptUnit(std::make_unique<CUnit>(501, MakePhase5UnitType(War3ID("hmpr"), 290, 200, false), 0, War3ID("hmpr"), 0.0f, 0.0f, 0.0f));
    auto* archer  = sim.AdoptUnit(std::make_unique<CUnit>(502, MakePhase5UnitType(War3ID("earc"), 245, 0,   false), 0, War3ID("earc"), 0.0f, 0.0f, 0.0f));
    auto* footman = sim.AdoptUnit(std::make_unique<CUnit>(503, MakePhase5UnitType(War3ID("hfoo"), 420, 0,   false), 0, War3ID("hfoo"), 0.0f, 0.0f, 0.0f));
    sim.Update();
    sim.ClearCommandLog();

    core::CommandBufferPipeline pipeline;
    ReplayLog replay;
    replay.Begin(12345u);

    // Issue Wedge formation move along +X toward (1000, 0):
    // Footman (rank 0) should be placed at the front apex (x = 1000), while Archer (rank 1) and Priest (rank 2) take rear slots (x = 920)!
    const int count = pipeline.IssueGroupPointOrder(
        sim, 0,
        {priest->GetHandleId(), archer->GetHandleId(), footman->GetHandleId()},
        OrderIds::MOVE, 1000.0f, 0.0f,
        /*useFormation=*/true, /*spacing=*/80.0f, &replay, core::FormationShape::Wedge);
    TEST_ASSERT(count == 3);
    TEST_ASSERT(replay.Commands().size() == 3u);
    // First command in formation order is the frontline Footman (handle 503) at the wedge apex (1000, 0)
    TEST_ASSERT(replay.Commands()[0].unit == footman->GetHandleId());
    TEST_NEAR(replay.Commands()[0].x, 1000.0f, 1e-2f);
    TEST_ASSERT(replay.Commands()[2].unit == priest->GetHandleId());
    TEST_ASSERT(replay.Commands()[2].x < 1000.0f);

    return true;
}

bool TestPhase27StageKKK2DynamicPluginSOV27AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath27 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths27[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths27) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath27 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath27.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase27PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath27));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 27u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(8)GardenOfWar.w3x",
        "resources/Maps/FrozenThrone/Scenario/(8)AzerothGrandPrix.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 27u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}


// ============================================================================
// Phase 28 Modernization Tests: Zero-Alloc RenderInterpolationBuffer Swap &
// BonePaletteAtlas Fast Reset, AudioServer Precomputed Bus Gains & Pluggable
// Combat Sound Routing, UIDataBinder Custom Reactive Properties & Plugin v28
// ============================================================================

bool TestPhase28StageLLL1ZeroAllocRenderInterpolationSwapAndBoneAtlasFastReset() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    UnitPresentationSystem presentation;
    presentation.BindSimulationEvents(sim);

    auto* u1 = sim.AdoptUnit(std::make_unique<CUnit>(601, MakePhase5UnitType(War3ID("hfoo"), 420, 0, false), 0, War3ID("hfoo"), 100.0f, 200.0f, 0.0f));
    auto* u2 = sim.AdoptUnit(std::make_unique<CUnit>(602, MakePhase5UnitType(War3ID("ogru"), 700, 0, false), 0, War3ID("ogru"), 300.0f, 400.0f, 0.0f));
    sim.Update();

    presentation.CaptureSimulationTick(sim);
    TEST_ASSERT(presentation.InterpolationBuffer().TrackedEntityCount() == 2u);
    TEST_ASSERT(presentation.InterpolationBuffer().PreviousSnapshotCount() == 2u);

    u1->SetPosition(200.0f, 400.0f);
    presentation.CaptureSimulationTick(sim);

    // Mid-tick interpolation alpha = 0.5f should sample (150, 0, 300)
    presentation.SetInterpolationAlpha(0.5f);
    render::RenderTransformSnapshot midSnap;
    TEST_ASSERT(presentation.SampleInterpolatedUnit(u1->GetHandleId(), midSnap));
    TEST_NEAR(midSnap.position.x, 150.0f, 1e-3f);
    TEST_NEAR(midSnap.position.z, 300.0f, 1e-3f);

    // Remove u2 -> reactive OnUnitRemoved immediately purges u2 from both currSnapshots_ and prevSnapshots_
    const int u2Handle = u2->GetHandleId();
    sim.RemoveUnit(u2);
    sim.Update();
    TEST_ASSERT(presentation.InterpolationBuffer().TrackedEntityCount() == 1u);
    TEST_ASSERT(presentation.InterpolationBuffer().PreviousSnapshotCount() == 1u);
    TEST_ASSERT(!presentation.SampleInterpolatedUnit(u2Handle, midSnap));

    // Verify BonePaletteAtlas O(1) ResetFrame preserves Row 0 identity matrices without corrupting subsequent rows
    render::BonePaletteAtlas atlas(16);
    std::vector<Mat4> unitBones(4, Mat4::Translate(10.0f, 20.0f, 30.0f));
    const int row1 = atlas.AllocateAndPackRow(unitBones);
    TEST_ASSERT(row1 == 1);
    TEST_ASSERT(atlas.GetActiveRowCount() == 2);

    atlas.ResetFrame(16);
    TEST_ASSERT(atlas.GetActiveRowCount() == 1);
    const Mat4 row0Bone0 = atlas.SampleMatrix(0, 0);
    TEST_NEAR(row0Bone0.m[0], 1.0f, 1e-5f);
    TEST_NEAR(row0Bone0.m[12], 0.0f, 1e-5f);

    return true;
}

bool TestPhase28StageLLL2AudioServerPrecomputedBusGainsAndPluggableCombatSoundRouting() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    auto war3Mod = std::make_shared<war3::War3GameModule>();
    TEST_ASSERT(war3Mod->GetModuleVersion() >= 28u);
    war3Mod->RegisterCustomUnitSoundOverride(
        War3ID("hfoo"),
        "Sound/Custom/FootmanLaserHit",
        "Sound/Custom/FootmanLaserDeath");
    TEST_ASSERT(war3Mod->HasCustomUnitSoundOverride(War3ID("hfoo")));
    sim.SetSimulationModule(war3Mod);

    auto& audioSrv = audio::AudioServer::Instance();
    audioSrv.Reset();
    audioSrv.SetListenerPosition(Vec3(0.0f, 0.0f, 0.0f));
    audioSrv.BindSimulationEvents(sim);

    auto* footman = sim.AdoptUnit(std::make_unique<CUnit>(611, MakePhase5UnitType(War3ID("hfoo"), 100, 0, false), 0, War3ID("hfoo"), 64.0f, 64.0f, 0.0f));
    sim.Update();

    // Emit damage event -> verify custom hit clip routed via War3GameModule::OnResolveUnitCombatSound
    sim.Events().onUnitDamage.Emit(footman->GetHandleId(), -1, 25.0f);
    TEST_ASSERT(audioSrv.GetStats().simDamageSoundsTriggered == 1);
    bool foundCustomHit = false;
    for (const auto& v : audioSrv.GetActiveVoices()) {
        if (v.clipKey.find("Sound/Custom/FootmanLaserHit") != std::string::npos) {
            foundCustomHit = true;
            break;
        }
    }
    TEST_ASSERT(foundCustomHit);

    // Emit death event -> verify custom death clip routed on UnitVoice bus and sidechain ducks SFX voice!
    sim.Events().onUnitDeath.Emit(footman->GetHandleId(), -1, 0);
    TEST_ASSERT(audioSrv.GetStats().simDeathSoundsTriggered == 1);
    bool foundCustomDeath = false;
    for (const auto& v : audioSrv.GetActiveVoices()) {
        if (v.clipKey.find("Sound/Custom/FootmanLaserDeath") != std::string::npos) {
            foundCustomDeath = true;
            break;
        }
    }
    TEST_ASSERT(foundCustomDeath);

    audioSrv.UnbindSimulationEvents(sim);
    TEST_ASSERT(war3Mod->UnregisterCustomUnitSoundOverride(War3ID("hfoo")));
    return true;
}

bool TestPhase28StageMMM1UIDataBinderCustomReactivePropertiesAndUpkeepFormatter() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.GetPlayer(0)->SetGold(500);
    sim.GetPlayer(0)->SetLumber(250);
    sim.GetPlayer(0)->SetFoodUsed(55);
    sim.GetPlayer(0)->SetFoodCap(80);
    auto war3Mod = std::make_shared<war3::War3GameModule>();
    war3Mod->SetCustomUpkeepLabel(1, "30% Tax Tier");
    sim.SetSimulationModule(war3Mod);

    GameUI gui;
    UIFrame root("RootHUD");
    TextFrame goldTxt("GoldText");
    TextFrame upkeepTxt("UpkeepText");
    TextFrame waveTxt("WaveTimerText");

    TextFrame* goldPtr = &goldTxt;
    TextFrame* upkeepPtr = &upkeepTxt;
    TextFrame* wavePtr = &waveTxt;

    root.AddChild(goldPtr);
    root.AddChild(upkeepPtr);
    root.AddChild(wavePtr);
    gui.rootFrame = &root;

    ui::UIDataBinder binder;
    binder.BindResourceFrames(goldPtr, nullptr, nullptr, upkeepPtr);

    int waveNumber = 1;
    binder.BindCustomTextProperty("wave_status", wavePtr,
        [&](const CSimulation& /*s*/, int /*playerIdx*/, int /*selHandle*/) {
            return "Wave " + std::to_string(waveNumber);
        });
    TEST_ASSERT(binder.GetCustomPropertyCount() == 1u);

    // 1. Initial sync updates all bound fields including custom upkeep label and custom property
    TEST_ASSERT(binder.SyncAndRelayoutIfChanged(sim, 0, -1, gui));
    TEST_ASSERT(goldPtr->GetText() == "500");
    TEST_ASSERT(upkeepPtr->GetText() == "30% Tax Tier");
    TEST_ASSERT(wavePtr->GetText() == "Wave 1");

    // 2. Second sync with no changes skips all updates (zero dirty updates)
    const uint64_t dirtyBefore = binder.GetStats().dirtyFieldUpdates;
    TEST_ASSERT(!binder.SyncAndRelayoutIfChanged(sim, 0, -1, gui));
    TEST_ASSERT(binder.GetStats().dirtyFieldUpdates == dirtyBefore);

    // 3. Mutate only custom wave property -> only wavePtr updates!
    waveNumber = 2;
    TEST_ASSERT(binder.SyncAndRelayoutIfChanged(sim, 0, -1, gui));
    TEST_ASSERT(wavePtr->GetText() == "Wave 2");
    TEST_ASSERT(binder.GetStats().dirtyFieldUpdates == dirtyBefore + 1u);

    TEST_ASSERT(binder.UnbindCustomTextProperty("wave_status"));
    return true;
}

bool TestPhase28StageMMM2DynamicPluginSOV28AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath28 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths28[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths28) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath28 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath28.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase28PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath28));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() >= 28u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(4)TwistedMeadows.w3x",
        "resources/Maps/FrozenThrone/Scenario/(10)ExtremeCandyWar2004.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() >= 28u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}


// ============================================================================
// Phase 29 Modernization Tests: EngineProfiler O(1) Ring-Buffer & RenderGraph
// Dynamic Sorted Passes, ResourceServer Generic Cache Kernel & VFS Path Aliases,
// and War3GameModule Version 29
// ============================================================================

bool TestPhase29StageNNN1EngineProfilerO1RingBufferAndRenderGraphDynamicSortedPasses() {
    // 1. Verify EngineProfiler O(1) circular ring buffer beyond kMaxRollingSamplesPerZone (256)
    core::EngineProfiler prof;
    prof.Reset();

    // Push 300 samples: first 44 are 10us, latest 256 are 100..355us
    for (int i = 0; i < 44; ++i) {
        prof.RecordZoneSample("SimStep", "Simulation", static_cast<uint64_t>(i * 10), 10);
    }
    for (int i = 0; i < 256; ++i) {
        prof.RecordZoneSample("SimStep", "Simulation", static_cast<uint64_t>((44 + i) * 100), static_cast<uint64_t>(100 + i));
    }

    core::ProfileZoneSummary summary;
    TEST_ASSERT(prof.GetZoneSummary("SimStep", summary));
    TEST_ASSERT(summary.callCount == 300u);
    TEST_NEAR(summary.minMicros, 10.0, 1e-4);
    TEST_NEAR(summary.maxMicros, 355.0, 1e-4);
    // Rolling window of latest 256 samples is [100..355], so P50 must be >= 227 (old 10us samples overwritten!)
    TEST_ASSERT(summary.p50Micros >= 227.0 && summary.p50Micros <= 229.0);

    // 2. Verify RenderGraph RegisterCustomPass topological ordering and RemovePass
    render::RenderGraph graph;
    TEST_ASSERT(graph.GetPassCount() == 7u);

    const auto customMinimapPassId = static_cast<render::RenderGraphPassId>(550); // Between Portrait (500) and UI (600)
    bool customPassRan = false;
    graph.RegisterCustomPass(customMinimapPassId, "CustomMinimapFogPass",
        [&](render::RenderGraphContext& /*ctx*/, render::RenderGraphPassMetrics& m) {
            customPassRan = true;
            m.itemsProcessed = 4;
            m.drawCallsSubmitted = 2;
        });
    TEST_ASSERT(graph.GetPassCount() == 8u);

    render::RenderGraphContext ctx;
    ctx.uiOverlayCommandCount = 12;
    graph.ExecuteFrame(ctx);
    TEST_ASSERT(customPassRan);

    const auto& allMetrics = graph.GetAllPassMetrics();
    TEST_ASSERT(allMetrics.size() == 8u);
    // Pass at index 6 should be CustomMinimapFogPass (550), right before UiOverlayPass (600) at index 7!
    TEST_ASSERT(allMetrics[6].passId == customMinimapPassId);
    TEST_ASSERT(allMetrics[6].drawCallsSubmitted == 2);
    TEST_ASSERT(allMetrics[7].passId == render::RenderGraphPassId::UiOverlayPass);

    TEST_ASSERT(graph.RemovePass(customMinimapPassId));
    TEST_ASSERT(graph.GetPassCount() == 7u);
    return true;
}

bool TestPhase29StageNNN2ResourceServerGenericCacheKernelAndVfsPathAliases() {
    resource::ResourceServer srv;
    std::vector<uint8_t> hdBytes = {0x48, 0x44, 0x5F, 0x4D, 0x45, 0x53, 0x48}; // "HD_MESH"
    srv.MountMemoryFile("Units/Human/Footman/Footman_HD.bin", hdBytes);

    // Register a VFS path alias redirecting legacy Footman.bin -> Footman_HD.bin
    srv.RegisterPathAlias("mpq://Units/Human/Footman/Footman.bin", "Units/Human/Footman/Footman_HD.bin");
    TEST_ASSERT(srv.GetPathAliasCount() == 1u);
    TEST_ASSERT(srv.FileExists("Units\\Human\\Footman\\footman.bin"));

    auto binRes1 = srv.LoadRawBinary("Units/Human/Footman/Footman.bin");
    TEST_ASSERT(binRes1 != nullptr);
    TEST_ASSERT(binRes1->bytes == hdBytes);

    // Second load hits generic LoadOrInsertCached<T> cache
    const uint64_t hitsBefore = srv.GetStats().cacheHits;
    auto binRes2 = srv.LoadRawBinary("mpq://units\\human\\footman\\FOOTMAN.BIN");
    TEST_ASSERT(binRes2 == binRes1);
    TEST_ASSERT(srv.GetStats().cacheHits == hitsBefore + 1u);
    TEST_ASSERT(srv.FindCached("Units/Human/Footman/Footman.bin", resource::ResourceKind::RawBinary) == binRes1);

    // Chained alias resolution (Footman_Legacy.bin -> Footman.bin -> Footman_HD.bin)
    srv.RegisterPathAlias("Units/Human/Footman/Footman_Legacy.bin", "Units/Human/Footman/Footman.bin");
    TEST_ASSERT(srv.ResolvePathAlias("Units/Human/Footman/Footman_Legacy.bin") == "Units/Human/Footman/Footman_HD.bin");
    TEST_ASSERT(srv.UnregisterPathAlias("Units/Human/Footman/Footman_Legacy.bin"));

    TEST_ASSERT(srv.UnregisterPathAlias("Units/Human/Footman/Footman.bin"));
    TEST_ASSERT(srv.GetPathAliasCount() == 0u);
    return true;
}

bool TestPhase29StageOOO1War3GameModuleV29AssetPathAliasesAndSoundModelRedirection() {
    resource::ResourceServer srv;
    const std::string slkText = "ID;P\nC;X1;Y1;K\"unitID\"\nC;X1;Y2;K\"hfoo\"\nE\n";
    std::vector<uint8_t> customSlkBytes(slkText.begin(), slkText.end());
    srv.MountMemoryFile("CustomMod/UnitBalance_Custom.slk", customSlkBytes);

    CSimulation sim;
    auto war3Mod = std::make_shared<war3::War3GameModule>();
    sim.SetSimulationModule(war3Mod);
    TEST_ASSERT(war3Mod->GetModuleVersion() == 29u);
    const uint64_t hashBefore = war3Mod->OnComputeModuleStateHash(sim);

    war3Mod->RegisterAssetPathAlias(
        "Units/UnitBalance.slk",
        "CustomMod/UnitBalance_Custom.slk",
        &srv);
    TEST_ASSERT(war3Mod->HasAssetPathAlias("units\\unitbalance.slk"));
    const uint64_t hashWithAlias = war3Mod->OnComputeModuleStateHash(sim);
    TEST_ASSERT(hashWithAlias != hashBefore);

    std::string redirected;
    TEST_ASSERT(war3Mod->OnResolveAssetPathAlias("mpq://Units/UnitBalance.slk", redirected));
    TEST_ASSERT(redirected == "CustomMod/UnitBalance_Custom.slk");

    // Loading Units/UnitBalance.slk from srv transparently loads CustomMod/UnitBalance_Custom.slk!
    auto slkRes = srv.LoadSlkTable("Units/UnitBalance.slk");
    TEST_ASSERT(slkRes != nullptr);
    TEST_ASSERT(slkRes->GetState() == resource::ResourceLoadState::Loaded);

    // Verify WM29 module snapshot round-trip preserves customAssetPathAliases_
    std::vector<uint8_t> modSnap;
    war3Mod->OnCaptureModuleSnapshot(sim, modSnap);
    TEST_ASSERT(war3Mod->UnregisterAssetPathAlias("Units/UnitBalance.slk", &srv));
    TEST_ASSERT(!war3Mod->HasAssetPathAlias("Units/UnitBalance.slk"));
    TEST_ASSERT(war3Mod->OnRestoreModuleSnapshot(sim, modSnap.data(), modSnap.size()));
    TEST_ASSERT(war3Mod->HasAssetPathAlias("Units/UnitBalance.slk"));
    TEST_ASSERT(war3Mod->OnComputeModuleStateHash(sim) == hashWithAlias);

    TEST_ASSERT(war3Mod->UnregisterAssetPathAlias("Units/UnitBalance.slk", &srv));
    TEST_ASSERT(!war3Mod->HasAssetPathAlias("Units/UnitBalance.slk"));
    return true;
}

bool TestPhase29StageOOO2DynamicPluginSOV29AndCloudRetailMapsEndToEndSuite() {
    auto& loader = engine::DynamicModuleLoader::Instance();
    std::string soPath29 = "./libwar3_module_plugin.so";
    const char* kCandidatePluginPaths29[] = {
        "./libwar3_module_plugin.so",
        "./war3_module_plugin.dll",
        "./build/tests/warsmash_tests.exe",
        "./warsmash_tests.exe"
    };
    for (const char* candidate : kCandidatePluginPaths29) {
        if (FILE* probe = std::fopen(candidate, "rb")) {
            std::fclose(probe);
            soPath29 = candidate;
            break;
        }
    }
    FILE* f = std::fopen(soPath29.c_str(), "rb");
    if (f) {
        std::fclose(f);
        const std::string pluginKey = "War3Phase29PluginSO";
        TEST_ASSERT(loader.LoadSharedLibrary(pluginKey, soPath29));
        {
            auto dynSimMod = loader.CreateSimulationModule(pluginKey);
            TEST_ASSERT(dynSimMod != nullptr);
            TEST_ASSERT(dynSimMod->GetModuleVersion() == 29u);
        }
        TEST_ASSERT(loader.UnloadSharedLibrary(pluginKey));
    }

    const char* kCloudMaps[] = {
        "resources/Maps/FrozenThrone/(2)EchoIsles.w3x",
        "resources/Maps/FrozenThrone/(2)TerenasStand.w3x",
        "resources/Maps/FrozenThrone/Scenario/(12)WormWar.w3x"
    };
    war3::War3GameModule::EnsureRegisteredInDynamicLoader();
    for (const char* mapPath : kCloudMaps) {
        FILE* mf = std::fopen(mapPath, "rb");
        if (!mf) continue;
        std::fclose(mf);

        auto simMod = loader.CreateSimulationModule(war3::War3GameModule::kModuleId);
        TEST_ASSERT(simMod && simMod->GetModuleVersion() == 29u);

        MpqManager mpq;
        CSimulation mapSim;
        mapSim.SetSimulationModule(simMod);
        SceneManager scene;
        UnitPresentationSystem presentation;
        core::WorldSceneSession session;

        TEST_ASSERT(session.LoadMap(mpq, mapSim, scene, presentation, mapPath));
        mapSim.Update();
        session.TeardownCurrentSession(mpq, mapSim, scene, presentation);
    }

    return true;
}

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

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "render/camera.h"
#include "render/math.h"
#include "render/render_server.h"

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond << std::endl; \
            return false; \
        } \
    } while (0)

#define TEST_NEAR(a, b, eps) \
    do { \
        if (std::fabs((a) - (b)) > (eps)) { \
            std::cerr << "NEAR @" << __LINE__ << ": " << (a) << " vs " << (b) << std::endl; \
            return false; \
        } \
    } while (0)

namespace {

CUnitType* MakePhase4UnitType(War3ID code, int maxLife = 500, int speed = 270, bool building = false) {
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

bool TestRenderServerMaterialsAndFrustumCulling() {
    using namespace render;

    // 1. Test MDX Layer -> ShaderMaterial conversion and 64-bit PipelineKey ordering
    ShaderMaterial opaqueMat = ShaderMaterial::FromMdxLayer(0, 0, 101, true, false);
    ShaderMaterial alphaTestMat = ShaderMaterial::FromMdxLayer(1, kShadingTwoSided, 101, true, false);
    ShaderMaterial blendMat = ShaderMaterial::FromMdxLayer(2, 0, 102, true, false);
    ShaderMaterial additiveMat = ShaderMaterial::FromMdxLayer(3, kShadingNoDepthSet | kShadingUnshaded, 103, false, false);

    TEST_ASSERT(!opaqueMat.IsTransparent());
    TEST_ASSERT(opaqueMat.depthState == RenderDepthState::ReadWrite);
    TEST_ASSERT(opaqueMat.cullMode == RenderCullMode::Back);

    TEST_ASSERT(!alphaTestMat.IsTransparent());
    TEST_ASSERT(alphaTestMat.cullMode == RenderCullMode::None);

    TEST_ASSERT(blendMat.IsTransparent());
    TEST_ASSERT(blendMat.depthState == RenderDepthState::ReadOnly);

    TEST_ASSERT(additiveMat.IsTransparent());
    TEST_ASSERT((additiveMat.shadingFlags & kShadingUnshaded) != 0);

    // Opaque pipeline keys always sort before transparent pipeline keys (bit 63)
    TEST_ASSERT(opaqueMat.ComputePipelineKey() < blendMat.ComputePipelineKey());
    TEST_ASSERT(alphaTestMat.ComputePipelineKey() < additiveMat.ComputePipelineKey());

    // 2. Test MaterialPropertyBlock per-instance overrides
    MaterialPropertyBlock props;
    props.teamColorIndex = 3;
    props.SetFloat("u_rimPower", 2.75f);
    props.SetVec4("u_customGlow", Vec4(0.2f, 0.5f, 0.9f, 1.0f));
    TEST_ASSERT(props.HasCustomOverrides());
    TEST_NEAR(props.GetFloat("u_rimPower"), 2.75f, 1e-5f);
    TEST_NEAR(props.GetFloat("u_missing", 1.0f), 1.0f, 1e-5f);
    TEST_NEAR(props.GetVec4("u_customGlow").z, 0.9f, 1e-5f);

    // 3. Test Gribb-Hartmann 6-Plane Frustum Culling
    Vec3 eye(0.0f, 500.0f, 500.0f);
    Vec3 center(0.0f, 0.0f, 0.0f);
    Vec3 up(0.0f, 1.0f, 0.0f);
    Mat4 view = LookAt(eye, center, up);
    Mat4 proj = Perspective(60.0f, 16.0f / 9.0f, 10.0f, 3000.0f);
    Frustum frustum = Frustum::FromViewProj(proj * view);

    // Origin is directly at camera target -> visible
    TEST_ASSERT(frustum.TestSphere(Vec3(0.0f, 0.0f, 0.0f), 32.0f));
    TEST_ASSERT(frustum.TestAABB(Vec3(-20.0f, -20.0f, -20.0f), Vec3(20.0f, 20.0f, 20.0f)));

    // Far behind the camera -> culled
    TEST_ASSERT(!frustum.TestSphere(Vec3(0.0f, 1500.0f, 1500.0f), 32.0f));
    // Way off to the side -> culled
    TEST_ASSERT(!frustum.TestSphere(Vec3(8000.0f, 0.0f, 0.0f), 32.0f));
    TEST_ASSERT(!frustum.TestAABB(Vec3(7900.0f, -10.0f, -10.0f), Vec3(8100.0f, 10.0f, 10.0f)));

    return true;
}

bool TestRenderServerInstancingAndSimulationInterpolation() {
    using namespace render;

    // 1. Verify shortest-arc angular interpolation across 360/0 boundary
    TEST_NEAR(RenderInterpolationBuffer::LerpAngleDeg(350.0f, 10.0f, 0.5f), 0.0f, 1e-4f);
    TEST_NEAR(RenderInterpolationBuffer::LerpAngleDeg(10.0f, 350.0f, 0.25f), 5.0f, 1e-4f);

    // 2. Setup CSimulation with multiple Footmen ("hfoo") and Riflemen ("hrif")
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));

    CUnitType* footmanType = MakePhase4UnitType(War3ID("hfoo"), 420, 270.0f);
    CUnitType* riflemanType = MakePhase4UnitType(War3ID("hrif"), 535, 270.0f);

    std::vector<CUnit*> footmen;
    for (int i = 0; i < 12; ++i) {
        CUnit* u = sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"),
            static_cast<float>(i * 20 - 100), 0.0f, 350.0f));
        u->SetScale(1.2f);
        footmen.push_back(u);
    }

    std::vector<CUnit*> riflemen;
    for (int i = 0; i < 8; ++i) {
        CUnit* u = sim.AdoptUnit(std::make_unique<CUnit>(
            sim.AllocateHandleId(), riflemanType, 1, War3ID("hrif"),
            static_cast<float>(i * 20 - 80), 40.0f, 90.0f));
        riflemen.push_back(u);
    }

    // Add 2 translucent units (e.g. Wind Walk fade, vertex alpha < 255) at different distances
    CUnit* ghostNear = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 0.0f, 100.0f, 0.0f));
    ghostNear->SetVertexColor(255, 255, 255, 128);

    CUnit* ghostFar = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), riflemanType, 1, War3ID("hrif"), 0.0f, -200.0f, 0.0f));
    ghostFar->SetVertexColor(200, 200, 255, 100);

    // Add 1 unit far outside the camera frustum (should be culled)
    CUnit* offscreenUnit = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), footmanType, 0, War3ID("hfoo"), 25000.0f, 25000.0f, 0.0f));
    (void)offscreenUnit;

    sim.Update();

    // Capture Tick 0 snapshot
    RenderInterpolationBuffer interpBuffer;
    interpBuffer.CaptureSimulationTick(sim);
    TEST_ASSERT(interpBuffer.TrackedEntityCount() == 23);

    // Advance first footman in Tick 1 to test sub-tick interpolation
    footmen[0]->SetPosition(0.0f, 100.0f); // moved from (-100, 0) to (0, 100)
    footmen[0]->SetFacing(10.0f);          // turned from 350 to 10 degrees
    footmen[0]->SetFlyingHeight(80.0f);    // rose from 0 to 80
    interpBuffer.CaptureSimulationTick(sim);

    RenderTransformSnapshot midSnap;
    TEST_ASSERT(interpBuffer.SampleEntity(footmen[0]->GetHandleId(), 0.5f, midSnap));
    TEST_NEAR(midSnap.position.x, -50.0f, 1e-3f);
    TEST_NEAR(midSnap.position.y, 40.0f, 1e-3f);  // interpolated flyHeight
    TEST_NEAR(midSnap.position.z, 50.0f, 1e-3f);  // interpolated simulation Y
    TEST_NEAR(midSnap.facingDeg, 0.0f, 1e-3f);    // shortest arc 350 -> 10 at 0.5 is 0

    // 3. Submit all simulation units to RenderServer and verify GPU Instancing batch reduction
    RenderServer server;
    uint32_t registeredId = server.RegisterMaterial(
        "StandardUnitOpaque", ShaderMaterial::FromMdxLayer(0, 0, 1, true, false));
    TEST_ASSERT(registeredId > 0);
    TEST_ASSERT(server.FindMaterialByName("StandardUnitOpaque") != nullptr);

    Vec3 camPos(0.0f, 800.0f, 800.0f);
    Mat4 view = LookAt(camPos, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Perspective(55.0f, 16.0f / 9.0f, 10.0f, 4000.0f);

    server.BeginFrame(view, proj, camPos);
    server.SubmitSimulationUnits(sim, interpBuffer, 0.5f, 48.0f);
    server.BuildAndSortBatches();

    const RenderFrameStats& stats = server.GetStats();
    // 23 units submitted: 1 off-screen unit culled, 22 visible
    TEST_ASSERT(stats.submittedItems == 23);
    TEST_ASSERT(stats.culledByFrustum == 1);
    TEST_ASSERT(stats.visibleItems == 22);

    // 12 opaque Footmen -> 1 instanced batch; 8 opaque Riflemen -> 1 instanced batch
    TEST_ASSERT(stats.opaqueBatchCount == 2);
    // 2 translucent ghosts (different models) -> 2 transparent batches
    TEST_ASSERT(stats.transparentBatchCount == 2);
    TEST_ASSERT(stats.totalDrawCalls == 4);
    // 22 visible items drawn in 4 instanced draw calls -> 18 draw calls saved!
    TEST_ASSERT(stats.instancedDrawCallsSaved == 18);

    // Verify transparent batches are sorted back-to-front (furthest from camera first)
    const auto& transBatches = server.GetTransparentBatches();
    TEST_ASSERT(transBatches.size() == 2);
    TEST_ASSERT(transBatches[0].sortDepthSq >= transBatches[1].sortDepthSq);
    TEST_ASSERT(transBatches[0].entityHandles[0] == ghostFar->GetHandleId());
    TEST_ASSERT(transBatches[1].entityHandles[0] == ghostNear->GetHandleId());

    // Verify ExecuteBatches invokes callback for all 4 batches totaling 22 instances
    int executedBatches = 0;
    int executedInstances = 0;
    server.ExecuteBatches([&](const RenderBatchGroup& group) {
        executedBatches++;
        executedInstances += static_cast<int>(group.instances.size());
    });
    TEST_ASSERT(executedBatches == 4);
    TEST_ASSERT(executedInstances == 22);

    return true;
}

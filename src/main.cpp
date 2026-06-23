#include <iostream>
#include <filesystem>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_set>

#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_gfx.h"
#include "third_party/sokol/sokol_glue.h"
#include "third_party/sokol/sokol_log.h"

#include "asset/stb_image_write.h"
#include "platform/platform.h"
#include "render/renderer.h"
#include "render/camera.h"
#include "render/terrain.h"
#include "render/textured_terrain.h"
#include "resource/mpq_storm.h"
#include "asset/Texture.h"
#include "asset/TileSet.h"
#include "map/w3e.h"
#include "map/w3x_storm.h"
#include "map/w3i.h"
#include "map/wpm.h"
#include "map/w3r.h"
#include "map/wts.h"
#include "map/w3d.h"
#include "map/unitsdoo.h"
#include "model/mdx.h"
#include "render/mdx_viewer.h"
#include "render/scene.h"
#include "render/shadow_manager.h"
#include "render/fog_of_war.h"
#include "render/splat_model.h"
#include "render/software_mesh.h"
#include "render/shapes.h"
#include "render/minimap.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/orders/order.h"
#include "data/unitdata.h"
#include "data/gameplay_constants.h"
#include "ui/MainUI.h"
#include "audio/AudioSystem.h"
#include "scripting/JassVM.h"
#include "scripting/JassNatives.h"
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>
#include "render/daynight.h"

namespace fs = std::filesystem;

// --- Global state (for callbacks) ---
static struct {
    bool initialized = false;
    bool running = true;
    int frameCount = 0;
    float lastTime = 0;

    MpqManager mpq;
    SceneManager scene;
    TerrainRenderer simpleTerrain;
    TexturedTerrain texturedTerrain;
    bool terrainLoaded = false;
    W3EMap map;
    unsigned minimapTex = 0;
    W3iInfo w3i;
    TileSet tileset;
    std::string mapPath;

    OrbitCamera camera;
    Minimap minimap;
    MdxViewer mdxViewer;

    bool showMinimap = true;
    bool showPathing = false;
    bool showRegions = false;
    bool wireframe = false;

    std::string screenshotPath;
    int screenshotFrame = -1;

    float mouseDX = 0, mouseDY = 0, mouseScroll = 0;
    bool mouseDown[3] = {};

    // JASS VM
    JassVM jass;

    // Audio
    AudioSystem audio;

    // Day/Night cycle
    DayNightCycle dayNight;

    // UI
    MainUI ui;

    // JASS natives (heap-allocated for lifetime safety)
    std::unique_ptr<JassNatives> jassNatives;

    // ECS / Game Logic
    struct UnitVisualSeed {
        float scale[3] = {1.0f, 1.0f, 1.0f};
        uint32_t skinId = 0;
        int hitpoints = 0;
        int flags = 0;
    };

    struct PendingUnitAnimation {
        std::string sequenceName;
        int sequenceIndex = -1;
        bool useSequenceIndex = false;
    };

    CSimulation simulation;
    CUnitData unitData;
    WpmData wpm;
    bool simulationInitialized = false;
    std::unordered_map<int, UnitVisualSeed> simUnitVisualSeeds;
    std::unordered_map<int, PendingUnitAnimation> simUnitPendingAnimations;
    std::unordered_map<int, int> simUnitToVisualIdx; // handleId → visual instance index
    std::unordered_set<int> simUnitVisualRegistrationFailed;
    std::vector<int> selectedUnitHandles;
    std::unordered_set<int> selectedUnitHandleSet;
    int selectedUnitHandle = -1;
    War3ID selectedUnitTypeId;
    bool selectionPressActive = false;
    bool selectionDragVisible = false;
    float selectionStartX = 0.0f;
    float selectionStartY = 0.0f;
    float selectionCurrentX = 0.0f;
    float selectionCurrentY = 0.0f;
} g;

// Project a 3D world position to 2D screen coordinates (sokol top-left origin)
static bool WorldToScreen(const Vec3& worldPos, const Mat4& view, const Mat4& proj,
                          int screenW, int screenH, float& outX, float& outY) {
    HMM_Vec4 clip = HMM_MulM4V4(proj, HMM_MulM4V4(view, HMM_V4(worldPos.x, worldPos.y, worldPos.z, 1.0f)));
    if (clip.W <= 0.0f) return false;
    float ndcX = clip.X / clip.W;
    float ndcY = clip.Y / clip.W;
    outX = (ndcX * 0.5f + 0.5f) * screenW;
    outY = (1.0f - (ndcY * 0.5f + 0.5f)) * screenH; // flip Y for sokol top-left
    return true;
}

// Unproject a screen point to a world position on the terrain plane
static Vec3 ScreenToWorld(float screenX, float screenY, const Mat4& view, const Mat4& proj,
                            int screenW, int screenH) {
    float ndcX = 2.0f * screenX / screenW - 1.0f;
    float ndcY = 1.0f - 2.0f * screenY / screenH;
    HMM_Mat4 vp = HMM_MulM4((HMM_Mat4)proj, (HMM_Mat4)view);
    HMM_Mat4 invVP = HMM_InvGeneralM4(vp);
    HMM_Vec4 nearP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, -1.0f, 1.0f));
    HMM_Vec4 farP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, 1.0f, 1.0f));
    if (nearP.W != 0) { nearP.X /= nearP.W; nearP.Y /= nearP.W; nearP.Z /= nearP.W; }
    if (farP.W != 0) { farP.X /= farP.W; farP.Y /= farP.W; farP.Z /= farP.W; }
    float dx = farP.X - nearP.X, dy = farP.Y - nearP.Y, dz = farP.Z - nearP.Z;
    if (fabsf(dy) < 0.0001f) return Vec3(nearP.X, 0, nearP.Z);
    float t = -nearP.Y / dy;
    return Vec3(nearP.X + t * dx, 0, nearP.Z + t * dz);
}

struct PickRay {
    Vec3 origin;
    Vec3 direction;
};

static PickRay BuildScreenPickRay(float screenX, float screenY, const Mat4& view, const Mat4& proj,
                                  int screenW, int screenH) {
    float ndcX = 2.0f * screenX / screenW - 1.0f;
    float ndcY = 1.0f - 2.0f * screenY / screenH;
    HMM_Mat4 vp = HMM_MulM4((HMM_Mat4)proj, (HMM_Mat4)view);
    HMM_Mat4 invVP = HMM_InvGeneralM4(vp);
    HMM_Vec4 nearP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, -1.0f, 1.0f));
    HMM_Vec4 farP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, 1.0f, 1.0f));
    if (nearP.W != 0) { nearP.X /= nearP.W; nearP.Y /= nearP.W; nearP.Z /= nearP.W; }
    if (farP.W != 0) { farP.X /= farP.W; farP.Y /= farP.W; farP.Z /= farP.W; }

    PickRay ray;
    ray.origin = Vec3(nearP.X, nearP.Y, nearP.Z);
    ray.direction = Normalize(Vec3(farP.X - nearP.X, farP.Y - nearP.Y, farP.Z - nearP.Z));
    return ray;
}

static float GetTerrainHeightAt(float worldX, float worldY) {
    return g.texturedTerrain.IsValid() ? g.texturedTerrain.GetGroundHeight(worldX, worldY) : 0.0f;
}

static float ComputeUnitVisualHeight(const CUnit& unit) {
    float height = GetTerrainHeightAt(unit.GetX(), unit.GetY());
    switch (unit.GetMovementType()) {
        case PathingGridMovementType::FLY:
        case PathingGridMovementType::HOVER:
        case PathingGridMovementType::FLOAT:
            height += unit.GetFlyHeight();
            break;
        default:
            break;
    }
    return height;
}

static float DistanceToSegmentSquared(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float abLenSq = abx * abx + aby * aby;
    if (abLenSq <= 0.0001f) {
        const float dx = px - ax;
        const float dy = py - ay;
        return dx * dx + dy * dy;
    }
    const float t = std::clamp((apx * abx + apy * aby) / abLenSq, 0.0f, 1.0f);
    const float cx = ax + abx * t;
    const float cy = ay + aby * t;
    const float dx = px - cx;
    const float dy = py - cy;
    return dx * dx + dy * dy;
}

static float ComputeUnitSelectionWorldRadius(const CUnit& unit) {
    float radius = unit.GetCollisionSize();
    if (radius <= 1.0f) {
        radius = unit.IsBuilding() ? 96.0f : 32.0f;
    }
    if (unit.IsBuilding()) {
        radius = std::max(radius, 72.0f);
    }
    else {
        radius = std::max(radius, 24.0f);
    }
    return radius;
}

static float ComputeUnitSelectionVisualHeight(const CUnit& unit) {
    const CUnitType* unitType = unit.GetUnitType();
    float height = unitType ? unitType->GetImpactZ() : 0.0f;
    const float radius = ComputeUnitSelectionWorldRadius(unit);
    if (unit.IsBuilding()) {
        height = std::max(height, radius * 2.6f);
        height = std::max(height, 170.0f);
    }
    else {
        height = std::max(height, radius * 2.0f);
        height = std::max(height, 95.0f);
    }
    return height;
}

static bool IntersectSphere(const PickRay& ray, const Vec3& center, float radius, float& outT) {
    const Vec3 oc = ray.origin - center;
    const float a = Dot(ray.direction, ray.direction);
    const float b = 2.0f * Dot(oc, ray.direction);
    const float c = Dot(oc, oc) - radius * radius;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return false;
    }

    const float sqrtDisc = std::sqrt(discriminant);
    const float inv2A = 0.5f / a;
    const float t0 = (-b - sqrtDisc) * inv2A;
    const float t1 = (-b + sqrtDisc) * inv2A;
    if (t0 >= 0.0f) {
        outT = t0;
        return true;
    }
    if (t1 >= 0.0f) {
        outT = t1;
        return true;
    }
    return false;
}

static bool ComputeUnitSelectionScreenShape(const CUnit& unit,
                                            const Mat4& view,
                                            const Mat4& proj,
                                            int screenW,
                                            int screenH,
                                            float& outFootX,
                                            float& outFootY,
                                            float& outHeadX,
                                            float& outHeadY,
                                            float& outRadiusPixels) {
    const float baseHeight = ComputeUnitVisualHeight(unit);
    const float worldRadius = ComputeUnitSelectionWorldRadius(unit);
    const float visualHeight = ComputeUnitSelectionVisualHeight(unit);

    const Vec3 footPos(unit.GetX(), baseHeight + 8.0f, unit.GetY());
    const Vec3 headPos(unit.GetX(), baseHeight + visualHeight, unit.GetY());
    if (!WorldToScreen(footPos, view, proj, screenW, screenH, outFootX, outFootY)) {
        return false;
    }
    if (!WorldToScreen(headPos, view, proj, screenW, screenH, outHeadX, outHeadY)) {
        return false;
    }

    const float midHeight = baseHeight + visualHeight * 0.5f;
    const Vec3 centerPos(unit.GetX(), midHeight, unit.GetY());
    float centerX = 0.0f;
    float centerY = 0.0f;
    if (!WorldToScreen(centerPos, view, proj, screenW, screenH, centerX, centerY)) {
        centerX = (outFootX + outHeadX) * 0.5f;
        centerY = (outFootY + outHeadY) * 0.5f;
    }

    float radiusPixels = 0.0f;
    auto updateRadiusFromPoint = [&](const Vec3& point) {
        float sx = 0.0f;
        float sy = 0.0f;
        if (!WorldToScreen(point, view, proj, screenW, screenH, sx, sy)) {
            return;
        }
        const float dx = sx - centerX;
        const float dy = sy - centerY;
        radiusPixels = std::max(radiusPixels, std::sqrt(dx * dx + dy * dy));
    };
    updateRadiusFromPoint(Vec3(unit.GetX() + worldRadius, midHeight, unit.GetY()));
    updateRadiusFromPoint(Vec3(unit.GetX(), midHeight, unit.GetY() + worldRadius));

    outRadiusPixels = std::max(radiusPixels, unit.IsBuilding() ? 28.0f : 14.0f);
    return true;
}

static CUnit* PickSimulationUnitAtScreenPoint(float screenX,
                                              float screenY,
                                              const Mat4& view,
                                              const Mat4& proj,
                                              int screenW,
                                              int screenH,
                                              int ignoreHandle = -1) {
    const PickRay ray = BuildScreenPickRay(screenX, screenY, view, proj, screenW, screenH);
    CUnit* bestRayUnit = nullptr;
    CUnit* bestFallbackUnit = nullptr;
    float bestRayT = std::numeric_limits<float>::infinity();
    float bestFallbackScore = std::numeric_limits<float>::infinity();
    float bestFallbackDistSq = std::numeric_limits<float>::infinity();

    for (auto* unit : g.simulation.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (ignoreHandle >= 0 && unit->GetHandleId() == ignoreHandle) continue;

        float sphereRadius = ComputeUnitSelectionWorldRadius(*unit);
        Vec3 sphereCenter(unit->GetX(),
                          ComputeUnitVisualHeight(*unit) + ComputeUnitSelectionVisualHeight(*unit) * 0.5f,
                          unit->GetY());

        auto visualIt = g.simUnitToVisualIdx.find(unit->GetHandleId());
        if (visualIt != g.simUnitToVisualIdx.end()) {
            if (MdxViewer* viewer = g.scene.GetViewer(visualIt->second)) {
                const Vec3 viewerCenter = viewer->GetBoundsCenter();
                sphereCenter = Vec3(unit->GetX() + viewerCenter.x,
                                    ComputeUnitVisualHeight(*unit) + viewerCenter.y,
                                    unit->GetY() + viewerCenter.z);
                const float viewerRadius = viewer->GetBoundsRadius();
                if (viewerRadius > 1.0f) {
                    sphereRadius = std::max(sphereRadius, viewerRadius);
                }
            }
        }
        sphereRadius = std::max(sphereRadius, unit->IsBuilding() ? 72.0f : 28.0f);

        float hitT = 0.0f;
        if (IntersectSphere(ray, sphereCenter, sphereRadius, hitT)) {
            if (hitT < bestRayT) {
                bestRayT = hitT;
                bestRayUnit = unit;
            }
            continue;
        }

        float footX = 0.0f;
        float footY = 0.0f;
        float headX = 0.0f;
        float headY = 0.0f;
        float radiusPixels = 0.0f;
        if (!ComputeUnitSelectionScreenShape(*unit, view, proj, screenW, screenH,
                                             footX, footY, headX, headY, radiusPixels)) {
            continue;
        }

        const float distSq = DistanceToSegmentSquared(screenX, screenY, footX, footY, headX, headY);
        if (distSq > radiusPixels * radiusPixels) {
            continue;
        }

        const float score = distSq / std::max(1.0f, radiusPixels * radiusPixels);
        if ((score < bestFallbackScore) || ((score == bestFallbackScore) && (distSq < bestFallbackDistSq))) {
            bestFallbackScore = score;
            bestFallbackDistSq = distSq;
            bestFallbackUnit = unit;
        }
    }

    return bestRayUnit ? bestRayUnit : bestFallbackUnit;
}

static std::string DescribeUnitType(War3ID typeId) {
    auto* unitType = g.unitData.GetUnitType(typeId);
    if (!unitType) {
        return typeId.ToString();
    }
    return unitType->GetName() + " [" + typeId.ToString() + "]";
}

static void ApplyPendingUnitAnimation(int handleId, MdxViewer* viewer) {
    if (!viewer || !viewer->GetAnimator()) return;

    const auto pendingIt = g.simUnitPendingAnimations.find(handleId);
    if (pendingIt == g.simUnitPendingAnimations.end()) return;

    if (pendingIt->second.useSequenceIndex) {
        viewer->GetAnimator()->SetSequence(pendingIt->second.sequenceIndex);
    } else if (!pendingIt->second.sequenceName.empty()) {
        viewer->GetAnimator()->PlaySequence(pendingIt->second.sequenceName);
    }

    g.simUnitPendingAnimations.erase(pendingIt);
}

static bool EnsureSimulationUnitVisual(CUnit* unit) {
    if (!unit) return false;

    const int handleId = unit->GetHandleId();
    if (g.simUnitToVisualIdx.find(handleId) != g.simUnitToVisualIdx.end()) return true;
    if (g.simUnitVisualRegistrationFailed.find(handleId) != g.simUnitVisualRegistrationFailed.end()) return false;

    static const float defaultScale[3] = {1.0f, 1.0f, 1.0f};
    const auto seedIt = g.simUnitVisualSeeds.find(handleId);
    const float* scale = (seedIt != g.simUnitVisualSeeds.end()) ? seedIt->second.scale : defaultScale;
    const uint32_t skinId = (seedIt != g.simUnitVisualSeeds.end()) ? seedIt->second.skinId : 0u;
    const int hitpoints = (seedIt != g.simUnitVisualSeeds.end())
        ? seedIt->second.hitpoints
        : (int)std::lround(unit->GetLife());
    const int flags = (seedIt != g.simUnitVisualSeeds.end()) ? seedIt->second.flags : 0;
    float pos[3] = {unit->GetX(), unit->GetY(), ComputeUnitVisualHeight(*unit)};
    int visIdx = g.scene.InstanceCount();
    bool visualAdded = g.scene.AddUnit(unit->GetTypeId().value,
                                       pos,
                                       unit->GetFacing(),
                                       scale,
                                       unit->GetPlayerIndex(),
                                       hitpoints,
                                       flags,
                                       skinId,
                                       false);
    if (!visualAdded) {
        g.simUnitVisualRegistrationFailed.insert(handleId);
        std::cout << "SceneUnitSync: visual registration failed for sim unit "
                  << DescribeUnitType(unit->GetTypeId())
                  << " handle=" << handleId << std::endl;
        return false;
    }

    g.simUnitToVisualIdx[handleId] = visIdx;
    ApplyPendingUnitAnimation(handleId, g.scene.GetViewer(visIdx));
    std::cout << "SceneUnitSync: registered on-demand visual for sim unit "
              << DescribeUnitType(unit->GetTypeId())
              << " handle=" << handleId << std::endl;
    return true;
}

static void SyncSimulationUnitVisual(CUnit* unit) {
    if (!EnsureSimulationUnitVisual(unit)) return;

    auto it = g.simUnitToVisualIdx.find(unit->GetHandleId());
    if (it == g.simUnitToVisualIdx.end()) return;

    float pos[3] = {
        unit->GetX(),
        unit->GetY(),
        ComputeUnitVisualHeight(*unit)
    };
    g.scene.UpdateInstancePosition(it->second, pos, unit->GetFacing());
}

static void SyncAllSimulationUnitVisuals() {
    for (auto* unit : g.simulation.GetUnits()) {
        SyncSimulationUnitVisual(unit);
    }
}

static void FlushStartupSimulationState() {
    if (!g.simulationInitialized) return;

    // Make editor and JASS-created units visible before the first render frame.
    SyncAllSimulationUnitVisuals();

    if (!g.jassNatives) return;

    constexpr int maxStartupWarmupTicks = 4;
    for (int i = 0; i < maxStartupWarmupTicks; ++i) {
        const size_t unitCountBefore = g.simulation.GetUnits().size();
        const int firedTimers = g.jassNatives->UpdateTimers(CSimulation::SIM_STEP_TIME);
        g.simulation.Update();
        SyncAllSimulationUnitVisuals();
        if (firedTimers == 0 && g.simulation.GetUnits().size() == unitCountBefore) {
            break;
        }
    }
}

static bool IsUnitSelected(int handleId) {
    return g.selectedUnitHandleSet.find(handleId) != g.selectedUnitHandleSet.end();
}

static bool IsShiftSelectionModifierDown() {
    return Platform::IsKeyDown(SAPP_KEYCODE_LEFT_SHIFT) || Platform::IsKeyDown(SAPP_KEYCODE_RIGHT_SHIFT);
}

static ScreenRect MakeNormalizedScreenRect(float x0, float y0, float x1, float y1) {
    ScreenRect rect;
    rect.x = std::min(x0, x1);
    rect.y = std::min(y0, y1);
    rect.w = std::fabs(x1 - x0);
    rect.h = std::fabs(y1 - y0);
    return rect;
}

static bool RectanglesOverlap(const ScreenRect& a, const ScreenRect& b) {
    if (!a.IsValid() || !b.IsValid()) return false;
    return !(a.x + a.w < b.x || b.x + b.w < a.x || a.y + a.h < b.y || b.y + b.h < a.y);
}

static bool ComputeUnitSelectionScreenBounds(const CUnit& unit,
                                             const Mat4& view,
                                             const Mat4& proj,
                                             int screenW,
                                             int screenH,
                                             ScreenRect& outRect) {
    float footX = 0.0f;
    float footY = 0.0f;
    float headX = 0.0f;
    float headY = 0.0f;
    float radiusPixels = 0.0f;
    if (!ComputeUnitSelectionScreenShape(unit, view, proj, screenW, screenH,
                                         footX, footY, headX, headY, radiusPixels)) {
        return false;
    }

    const float minX = std::min(footX, headX) - radiusPixels;
    const float maxX = std::max(footX, headX) + radiusPixels;
    const float minY = std::min(footY, headY) - radiusPixels;
    const float maxY = std::max(footY, headY) + radiusPixels;
    outRect.x = minX;
    outRect.y = minY;
    outRect.w = maxX - minX;
    outRect.h = maxY - minY;
    return outRect.IsValid();
}

static std::vector<CUnit*> CollectUnitsInScreenRect(const ScreenRect& rect,
                                                    const Mat4& view,
                                                    const Mat4& proj,
                                                    int screenW,
                                                    int screenH) {
    std::vector<CUnit*> result;
    if (!rect.IsValid()) return result;

    for (auto* unit : g.simulation.GetUnits()) {
        if (!unit || unit->IsDead()) continue;

        ScreenRect unitRect;
        if (!ComputeUnitSelectionScreenBounds(*unit, view, proj, screenW, screenH, unitRect)) {
            continue;
        }
        if (RectanglesOverlap(rect, unitRect)) {
            result.push_back(unit);
        }
    }
    return result;
}

static std::vector<CUnit*> GetSelectedSimulationUnits() {
    std::vector<CUnit*> units;
    units.reserve(g.selectedUnitHandles.size());
    for (int handleId : g.selectedUnitHandles) {
        CUnit* unit = g.simulation.GetUnit(handleId);
        if (!unit || unit->IsDead()) continue;
        units.push_back(unit);
    }
    return units;
}

static std::vector<MultiSelectUnitInfo> BuildMultiSelectionInfo(const std::vector<CUnit*>& units) {
    static const uint32_t kPlayerColors[12] = {
        0xFFFF0000, 0xFF0042FF, 0xFF1CE6B6, 0xFF540081,
        0xFFFFFC00, 0xFFFE8A0E, 0xFF20C000, 0xFFE55BB0,
        0xFF959697, 0xFF7EBFF1, 0xFF106246, 0xFF4F2B05
    };

    std::vector<MultiSelectUnitInfo> info;
    info.reserve(units.size());
    for (auto* unit : units) {
        if (!unit) continue;
        MultiSelectUnitInfo item;
        item.handleId = unit->GetHandleId();
        if (auto* unitType = unit->GetUnitType()) {
            item.name = unitType->GetName();
            item.level = unitType->GetLevel();
            item.isHero = unitType->IsHero();
        }
        item.hpRatio = unit->GetMaxLife() > 0.0f ? unit->GetLife() / unit->GetMaxLife() : 1.0f;
        item.mpRatio = unit->GetMaxMana() > 0 ? unit->GetMana() / (float)unit->GetMaxMana() : 0.0f;
        const int playerIndex = unit->GetPlayerIndex();
        if (playerIndex >= 0 && playerIndex < 12) {
            item.playerColor = kPlayerColors[playerIndex];
        }
        info.push_back(std::move(item));
    }
    return info;
}

static void RefreshSelectionUi(bool rebuildCommandCard) {
    const int previousPrimaryHandle = g.selectedUnitHandle;
    const uint32_t previousPrimaryTypeValue = g.selectedUnitTypeId.value;
    std::vector<CUnit*> selectedUnits = GetSelectedSimulationUnits();
    g.selectedUnitHandles.clear();
    g.selectedUnitHandleSet.clear();
    for (auto* unit : selectedUnits) {
        g.selectedUnitHandles.push_back(unit->GetHandleId());
        g.selectedUnitHandleSet.insert(unit->GetHandleId());
    }

    if (selectedUnits.empty()) {
        g.selectedUnitHandle = -1;
        g.selectedUnitTypeId = War3ID();
        g.ui.SetSelectedUnit(-1);
        g.ui.SetMultiSelection({});
        g.ui.ClearInfoPanel();
        g.ui.HideHeroPanel();
        g.ui.ClearPendingCommand();
        return;
    }

    CUnit* primaryUnit = nullptr;
    for (auto* unit : selectedUnits) {
        if (unit->GetHandleId() == g.selectedUnitHandle) {
            primaryUnit = unit;
            break;
        }
    }
    if (!primaryUnit) {
        primaryUnit = selectedUnits.front();
    }

    g.selectedUnitHandle = primaryUnit->GetHandleId();
    g.selectedUnitTypeId = primaryUnit->GetTypeId();
    g.ui.SetSelectedUnit(g.selectedUnitHandle);

    if (!rebuildCommandCard) {
        rebuildCommandCard = (g.selectedUnitHandle != previousPrimaryHandle)
            || (g.selectedUnitTypeId.value != previousPrimaryTypeValue);
    }

    if (rebuildCommandCard) {
        g.ui.SetUnitData(&g.unitData);
        g.ui.PopulateCommandCardForUnitType(g.selectedUnitTypeId);
    }

    if (selectedUnits.size() > 1) {
        g.ui.SetMultiSelection(BuildMultiSelectionInfo(selectedUnits));
    } else {
        g.ui.SetMultiSelection({});
    }

    auto* unitType = primaryUnit->GetUnitType();
    std::string unitName = unitType ? unitType->GetName() : "Unknown";
    int unitLevel = unitType ? unitType->GetLevel() : 1;
    bool isHero = unitType ? unitType->IsHero() : false;
    float hp = primaryUnit->GetLife();
    float maxHp = primaryUnit->GetMaxLife();
    float mp = primaryUnit->GetMana();
    float maxMp = (float)primaryUnit->GetMaxMana();
    if (isHero) {
        int strBase = unitType->GetStartingStrength();
        int agiBase = unitType->GetStartingAgility();
        int intBase = unitType->GetStartingIntelligence();
        int heroLvl = unitLevel < 1 ? 1 : unitLevel;
        g.ui.ShowHeroPanel(unitName, heroLvl, 0, 100,
                           strBase, 0, agiBase, 0, intBase, 0);
    } else {
        g.ui.HideHeroPanel();
        g.ui.UpdateInfoPanel(g.selectedUnitTypeId, unitName, unitLevel, hp, maxHp, mp, maxMp);
    }
}

static void SetSelectedUnits(const std::vector<CUnit*>& units, int primaryHandle = -1, bool rebuildCommandCard = true) {
    g.selectedUnitHandles.clear();
    g.selectedUnitHandleSet.clear();

    for (auto* unit : units) {
        if (!unit || unit->IsDead()) continue;
        const int handleId = unit->GetHandleId();
        if (g.selectedUnitHandleSet.insert(handleId).second) {
            g.selectedUnitHandles.push_back(handleId);
        }
    }

    if (g.selectedUnitHandles.empty()) {
        g.selectedUnitHandle = -1;
    } else if (primaryHandle >= 0 && g.selectedUnitHandleSet.find(primaryHandle) != g.selectedUnitHandleSet.end()) {
        g.selectedUnitHandle = primaryHandle;
    } else {
        g.selectedUnitHandle = g.selectedUnitHandles.front();
    }

    RefreshSelectionUi(rebuildCommandCard);
}

static void SelectSingleUnit(CUnit* unit, bool additive) {
    if (!unit || unit->IsDead()) {
        if (!additive) {
            SetSelectedUnits({}, -1);
        }
        return;
    }

    if (!additive) {
        SetSelectedUnits({unit}, unit->GetHandleId());
        return;
    }

    std::vector<CUnit*> units = GetSelectedSimulationUnits();
    bool alreadySelected = false;
    for (auto* selected : units) {
        if (selected && selected->GetHandleId() == unit->GetHandleId()) {
            alreadySelected = true;
            break;
        }
    }
    if (!alreadySelected) {
        units.push_back(unit);
    }
    SetSelectedUnits(units, unit->GetHandleId());
}

static void SelectUnitsInRect(const ScreenRect& rect, bool additive) {
    int winW = sapp_width();
    int winH = sapp_height();
    float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
    Mat4 view = g.camera.GetViewMatrix();
    Mat4 proj = g.camera.GetProjectionMatrix(aspect);

    std::vector<CUnit*> units = CollectUnitsInScreenRect(rect, view, proj, winW, winH);
    if (additive) {
        std::vector<CUnit*> existing = GetSelectedSimulationUnits();
        existing.insert(existing.end(), units.begin(), units.end());
        SetSelectedUnits(existing);
    } else {
        SetSelectedUnits(units);
    }
}

static bool IsHumanTownHallType(War3ID typeId) {
    return typeId == War3ID("htow") || typeId == War3ID("hkee") || typeId == War3ID("hcas");
}

static int CountPlayerUnitsOfType(int playerIndex, War3ID typeId) {
    int count = 0;
    for (auto* unit : g.simulation.GetUnits()) {
        if (unit && unit->GetPlayerIndex() == playerIndex && unit->GetTypeId() == typeId) {
            ++count;
        }
    }
    return count;
}

static int CountPlayerTownHalls(int playerIndex) {
    int count = 0;
    for (auto* unit : g.simulation.GetUnits()) {
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) {
            ++count;
        }
    }
    return count;
}

static bool FindPlayerTownHallPosition(int playerIndex, float& outX, float& outY) {
    for (auto* unit : g.simulation.GetUnits()) {
        if (unit && unit->GetPlayerIndex() == playerIndex && IsHumanTownHallType(unit->GetTypeId())) {
            outX = unit->GetX();
            outY = unit->GetY();
            return true;
        }
    }
    return false;
}

static bool FindPlayerStartLocationPosition(int playerIndex, float& outX, float& outY) {
    if (!g.jassNatives) return false;
    auto data = g.jassNatives->GetData();
    if (!data) return false;
    if (playerIndex < 0 || playerIndex >= (int)data->playerMapConfig.size()) return false;

    const auto& playerConfig = data->playerMapConfig[playerIndex];
    const int startIndex = playerConfig.startLocationIndex;
    if (startIndex < 0 || startIndex >= (int)data->startLocations.size()) return false;

    const auto& start = data->startLocations[startIndex];
    if (!start.defined) return false;

    outX = start.x;
    outY = start.y;
    return true;
}

static bool FindInitialSpawnFocusPosition(float& outX, float& outY) {
    if (FindPlayerTownHallPosition(0, outX, outY)) {
        return true;
    }
    if (FindPlayerStartLocationPosition(0, outX, outY)) {
        return true;
    }

    if (!g.jassNatives) return false;
    auto data = g.jassNatives->GetData();
    if (!data) return false;

    int playerLimit = data->configuredPlayerCount > 0
        ? std::min(data->configuredPlayerCount, (int)data->playerMapConfig.size())
        : (int)data->playerMapConfig.size();
    for (int playerIndex = 0; playerIndex < playerLimit; ++playerIndex) {
        if (FindPlayerTownHallPosition(playerIndex, outX, outY)) {
            return true;
        }
        if (FindPlayerStartLocationPosition(playerIndex, outX, outY)) {
            return true;
        }
    }
    return false;
}

static void FocusCameraAndMinimapOnInitialSpawn() {
    float focusX = 0.0f;
    float focusY = 0.0f;
    if (!FindInitialSpawnFocusPosition(focusX, focusY)) {
        return;
    }

    const float groundHeight = GetTerrainHeightAt(focusX, focusY);
    g.camera.SetTarget(focusX, groundHeight + 100.0f, focusY);
    g.camera.SetDistance(2200.0f);
    g.camera.SetMaxDistance(5000.0f);
    g.camera.SaveHomeState();

    g.minimap.ClearMarkers();
    g.minimap.AddMarker(focusX, focusY, 0xFFD54AFFu, 10.0f);

    std::cout << "InitialFocus: spawn at (" << focusX << ", " << focusY << ")" << std::endl;
}

static void EnsureHumanMeleeStartForPlayer(int playerIndex, float startX, float startY) {
    if (!g.unitData.HasUnitType(War3ID("htow")) || !g.unitData.HasUnitType(War3ID("hpea"))) {
        return;
    }

    constexpr float townHallFacing = 270.0f;
    constexpr float peasantFacing = 90.0f;
    static const float peasantOffsets[][2] = {
        {-220.0f,  120.0f},
        {-220.0f,  -20.0f},
        {-220.0f, -160.0f},
        {-80.0f,   180.0f},
        {-80.0f,  -220.0f},
    };

    if (CountPlayerTownHalls(playerIndex) == 0) {
        g.simulation.CreateUnit(War3ID("htow"), playerIndex, startX, startY, townHallFacing);
        std::cout << "MeleeFallback: created Town Hall for player " << playerIndex
                  << " at (" << startX << ", " << startY << ")" << std::endl;
    }

    float townHallX = startX;
    float townHallY = startY;
    FindPlayerTownHallPosition(playerIndex, townHallX, townHallY);

    int existingPeasants = CountPlayerUnitsOfType(playerIndex, War3ID("hpea"));
    for (int i = existingPeasants; i < 5; ++i) {
        const float px = townHallX + peasantOffsets[i][0];
        const float py = townHallY + peasantOffsets[i][1];
        g.simulation.CreateUnit(War3ID("hpea"), playerIndex, px, py, peasantFacing);
    }
    if (existingPeasants < 5) {
        std::cout << "MeleeFallback: created " << (5 - existingPeasants)
                  << " Peasants for player " << playerIndex << std::endl;
    }
}

static void EnsureHumanMeleeStartsFromMapConfig() {
    if (!g.jassNatives || !g.simulationInitialized) return;

    auto data = g.jassNatives->GetData();
    if (!data) return;

    bool createdAny = false;
    int playerLimit = data->configuredPlayerCount > 0
        ? std::min(data->configuredPlayerCount, (int)data->playerMapConfig.size())
        : (int)data->playerMapConfig.size();

    for (int playerIndex = 0; playerIndex < playerLimit; ++playerIndex) {
        const auto& playerConfig = data->playerMapConfig[playerIndex];
        if (playerConfig.startLocationIndex < 0) continue;
        if (!playerConfig.slotActive && CountPlayerTownHalls(playerIndex) > 0) continue;

        const int startIndex = playerConfig.startLocationIndex;
        if (startIndex < 0 || startIndex >= (int)data->startLocations.size()) continue;

        const auto& start = data->startLocations[startIndex];
        if (!start.defined) continue;

        if (auto* player = g.simulation.GetPlayer(playerIndex)) {
            player->SetStartLocation(start.x, start.y);
        }

        const int beforeTownHalls = CountPlayerTownHalls(playerIndex);
        const int beforePeasants = CountPlayerUnitsOfType(playerIndex, War3ID("hpea"));
        EnsureHumanMeleeStartForPlayer(playerIndex, start.x, start.y);
        if (CountPlayerTownHalls(playerIndex) > beforeTownHalls ||
            CountPlayerUnitsOfType(playerIndex, War3ID("hpea")) > beforePeasants) {
            createdAny = true;
        }
    }

    if (createdAny) {
        g.simulation.Update();
    }
}

static W3EMap GenerateTestTerrain() {
    W3EMap map;
    map.version = 12;
    map.tileset = 'A';
    map.customTilesets = 0;
    map.tileWidth = 32;
    map.tileHeight = 32;
    map.offsetX = -2048.0f;
    map.offsetY = -2048.0f;
    map.groundTilePalette.push_back(0x7472644C);

    int tileCount = map.tileWidth * map.tileHeight;
    map.tiles.resize(tileCount);
    int stride = map.tileWidth;
    for (int row = 0; row < map.tileHeight; ++row) {
        for (int col = 0; col < map.tileWidth; ++col) {
            int idx = row * stride + col;
            W3ETileCorner& c = map.tiles[idx];
            float nx = (col - 16) / 16.0f, ny = (row - 16) / 16.0f;
            float dist = std::sqrt(nx * nx + ny * ny);
            float height = 0;
            if (dist < 4.0f) height = 4.0f + std::cos(dist * 1.5f) * 2.0f;
            else if (dist < 8.0f) {
                float t = (dist - 4.0f) / 4.0f;
                height = (4.0f + std::cos(4.0f * 1.5f) * 2.0f) * (1.0f - t) - 2.0f * t;
            } else height = -2.0f + std::sin(nx * 0.5f) * 0.5f + std::cos(ny * 0.7f) * 0.5f;
            c.groundHeight = (uint16_t)std::clamp(height * 512.0f + 8192.0f, 0.0f, 65535.0f);
            c.waterAndBoundary = 0;
            c.textureBits = (height < -1.0f) ? 1 : 0;
            c.variationBits = 0;
            c.cliffAndLayer = 1;
        }
    }
    return map;
}

static bool SaveScreenshot(const char* filename) {
    int w = sapp_width(), h = sapp_height();
    std::vector<unsigned char> pixels(w * h * 3);
    // Use sokol_gfx to read pixels? Not directly supported. Skip for now.
    (void)filename;
    return false;
}

// --- Event callback ---
static void event_cb(const sapp_event* ev) {
    Platform::HandleEvent(ev);

    switch (ev->type) {
    case SAPP_EVENTTYPE_MOUSE_DOWN: {
        if (g.ui.HandleEvent(ev))
            break;

        // Check if UI consumes this click first
        int btn = (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) ? 0 :
                  (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 1 : 2;
        if (g.ui.GetGameUI().rootFrame && g.ui.OnMouseDown(ev->mouse_x, ev->mouse_y, btn))
            break;

        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g.mouseDown[0] = true;
            g.selectionPressActive = false;
            g.selectionDragVisible = false;
            g.selectionStartX = ev->mouse_x;
            g.selectionStartY = ev->mouse_y;
            g.selectionCurrentX = ev->mouse_x;
            g.selectionCurrentY = ev->mouse_y;
            g.ui.ClearSelectionBox();
            // Check if click is on minimap
            if (g.minimap.IsValid() && g.minimap.HitTest(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height())) {
                float wx, wy;
                g.minimap.ScreenToWorld(ev->mouse_x, ev->mouse_y, sapp_width(), sapp_height(), &wx, &wy);
                float gh = g.texturedTerrain.GetGroundHeight(wx, wy);
                g.camera.SetTarget(wx, gh + 100.0f, wy);
            } else if (g.simulationInitialized && !g.simulation.GetUnits().empty()) {
                g.selectionPressActive = true;
            }
        }
        else if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
            bool issuedOrder = false;
            if (g.simulationInitialized && !g.selectedUnitHandles.empty()) {
                std::vector<CUnit*> selectedUnits = GetSelectedSimulationUnits();
                if (!selectedUnits.empty()) {
                    int winW = sapp_width(), winH = sapp_height();
                    float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                    Mat4 view = g.camera.GetViewMatrix();
                    Mat4 proj = g.camera.GetProjectionMatrix(aspect);
                    CUnit* targetUnit = PickSimulationUnitAtScreenPoint(ev->mouse_x, ev->mouse_y,
                                                                        view, proj, winW, winH,
                                                                        g.selectedUnitHandle);

                    int pendingCmd = g.ui.GetPendingCommand();
                    if (pendingCmd >= 0) {
                        g.ui.ClearPendingCommand();
                        Vec3 wPos = ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH);
                        for (auto* selectedUnit : selectedUnits) {
                            if (!selectedUnit) continue;
                            if (targetUnit) {
                                selectedUnit->IssueOrder(std::make_unique<COrderTargetWidget>(pendingCmd, targetUnit));
                            } else {
                                selectedUnit->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
                            }
                        }
                        issuedOrder = true;
                    } else if (targetUnit) {
                        for (auto* selectedUnit : selectedUnits) {
                            if (!selectedUnit) continue;
                            selectedUnit->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::ATTACK, targetUnit));
                        }
                        issuedOrder = true;
                    } else {
                        Vec3 wPos = ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH);
                        for (auto* selectedUnit : selectedUnits) {
                            if (!selectedUnit) continue;
                            selectedUnit->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::MOVE, wPos.x, wPos.z));
                        }
                        issuedOrder = true;
                    }
                }
            }
            if (!issuedOrder) {
                g.mouseDown[2] = true;
                sapp_lock_mouse(true);
            }
        } else {
            sapp_lock_mouse(true);
        }
        break;
    }
    case SAPP_EVENTTYPE_MOUSE_UP: {
        if (!g.selectionPressActive && g.ui.HandleEvent(ev))
            break;

        int btn = (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) ? 0 :
                  (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) ? 1 : 2;
        if (!g.selectionPressActive && g.ui.OnMouseUp(ev->mouse_x, ev->mouse_y, btn))
            break;

        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g.mouseDown[0] = false;
            if (g.selectionPressActive) {
                const bool additive = IsShiftSelectionModifierDown();
                const ScreenRect rect = MakeNormalizedScreenRect(g.selectionStartX, g.selectionStartY,
                                                                 g.selectionCurrentX, g.selectionCurrentY);
                if (g.selectionDragVisible && rect.w >= 4.0f && rect.h >= 4.0f) {
                    SelectUnitsInRect(rect, additive);
                } else if (g.simulationInitialized && !g.simulation.GetUnits().empty()) {
                    int winW = sapp_width(), winH = sapp_height();
                    float aspect = (winH > 0) ? (float)winW / winH : 1.0f;
                    Mat4 view = g.camera.GetViewMatrix();
                    Mat4 proj = g.camera.GetProjectionMatrix(aspect);
                    CUnit* bestUnit = PickSimulationUnitAtScreenPoint(ev->mouse_x, ev->mouse_y,
                                                                      view, proj, winW, winH);
                    if (bestUnit) {
                        if (g.audio.IsValid() && g.frameCount > 10) {
                            g.audio.Play2D("ui_click", 0.5f);
                        }
                        SelectSingleUnit(bestUnit, additive);
                        std::cout << "SelectedUnit: "
                                  << DescribeUnitType(bestUnit->GetTypeId())
                                  << " handle=" << bestUnit->GetHandleId()
                                  << " count=" << g.selectedUnitHandles.size()
                                  << std::endl;
                    } else if (!additive) {
                        SetSelectedUnits({}, -1);
                    }
                }
            }
            g.selectionPressActive = false;
            g.selectionDragVisible = false;
            g.ui.ClearSelectionBox();
        }
        else if (ev->mouse_button == SAPP_MOUSEBUTTON_RIGHT) g.mouseDown[2] = false;
        sapp_lock_mouse(false);
        break;
    }
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        if (!sapp_mouse_locked() && g.ui.HandleEvent(ev))
            break;
        if (g.mouseDown[0] && g.selectionPressActive && !sapp_mouse_locked()) {
            g.selectionCurrentX = ev->mouse_x;
            g.selectionCurrentY = ev->mouse_y;
            const ScreenRect rect = MakeNormalizedScreenRect(g.selectionStartX, g.selectionStartY,
                                                             g.selectionCurrentX, g.selectionCurrentY);
            if (rect.w >= 4.0f || rect.h >= 4.0f) {
                g.selectionDragVisible = true;
                g.ui.SetSelectionBox(rect);
            }
        }
        if (sapp_mouse_locked()) {
            g.mouseDX += ev->mouse_dx;
            g.mouseDY += ev->mouse_dy;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        if (g.ui.HandleEvent(ev))
            break;
        g.mouseScroll += ev->scroll_y;
        break;
    case SAPP_EVENTTYPE_CHAR:
        g.ui.HandleEvent(ev);
        break;
    case SAPP_EVENTTYPE_RESIZED:
        g.ui.HandleEvent(ev);
        break;
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (g.ui.HandleEvent(ev))
            break;
        switch (ev->key_code) {
        case SAPP_KEYCODE_P:
            g.showPathing = !g.showPathing;
            g.texturedTerrain.SetShowPathing(g.showPathing);
            break;
        case SAPP_KEYCODE_R:
            g.showRegions = !g.showRegions;
            g.texturedTerrain.SetShowRegions(g.showRegions);
            break;
        case SAPP_KEYCODE_F1:
            g.wireframe = !g.wireframe;
            break;
        case SAPP_KEYCODE_M:
            g.showMinimap = !g.showMinimap;
            break;
        case SAPP_KEYCODE_N: {
            // Toggle music
            static bool musicOn = false;
            if (!musicOn) {
                bool ok = g.audio.PlayMusic("Sound\\Music\\mp3\\MainMenu.mp3", &g.mpq, 0.5f, true);
                if (!ok) g.audio.PlayMusic("Sound\\Music\\mp3\\Human1.mp3", &g.mpq, 0.5f, true);
                if (ok || g.audio.IsMusicPlaying()) {
                    std::cout << "Music: on" << std::endl;
                    musicOn = true;
                }
            } else {
                g.audio.StopMusic();
                std::cout << "Music: off" << std::endl;
                musicOn = false;
            }
            break;
        }
        case SAPP_KEYCODE_F2: {
            g.dayNight.SetEnabled(!g.dayNight.IsEnabled());
            std::cout << "Day/night: " << (g.dayNight.IsEnabled() ? "on" : "off") << std::endl;
            break;
        }
        case SAPP_KEYCODE_F3: {
            // Speed up time
            static float speed = 1.0f;
            speed = (speed >= 8.0f) ? 1.0f : speed * 2.0f;
            g.dayNight.SetDayLength(480.0f / speed);
            std::cout << "Day/night speed: " << speed << "x" << std::endl;
            break;
        }
        case SAPP_KEYCODE_D: {
            int mode = (g.texturedTerrain.DebugMode() + 1) % 5;
            g.texturedTerrain.SetDebugMode(mode);
            const char* labels[] = {"normal", "hasTex(R/G)", "UV", "rawTex", "base-texture-only"};
            std::cout << "Debug mode: " << labels[mode] << std::endl;
            break;
        }
        default: break;
        }
        break;
    default:
        break;
    }
}

// --- Frame callback ---
static void frame_cb() {
    float dt = 0.016f;
    float now = (float)sapp_frame_duration();
    if (g.lastTime > 0) dt = now;
    g.lastTime = now;

    // Update camera from mouse state
    if (g.mouseDown[0] || g.mouseDown[2]) {
        g.camera.OnMouseMove(g.mouseDX, g.mouseDY, g.mouseDown[0], g.mouseDown[2]);
    }
    if (g.mouseScroll != 0.0f) {
        g.camera.OnMouseWheel(g.mouseScroll);
    }
    g.mouseDX = 0; g.mouseDY = 0; g.mouseScroll = 0;

    // Keyboard WASD / arrow panning
    float panSpeed = std::max(100.0f, g.camera.GetPosition().y * 0.5f) * dt;
    float panX = 0, panZ = 0;
    if (Platform::IsKeyDown(SAPP_KEYCODE_W) || Platform::IsKeyDown(SAPP_KEYCODE_UP))    panZ += panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_S) || Platform::IsKeyDown(SAPP_KEYCODE_DOWN))  panZ -= panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_A) || Platform::IsKeyDown(SAPP_KEYCODE_LEFT))  panX -= panSpeed;
    if (Platform::IsKeyDown(SAPP_KEYCODE_D) || Platform::IsKeyDown(SAPP_KEYCODE_RIGHT)) panX += panSpeed;
    if (panX != 0 || panZ != 0) g.camera.Pan(panX, panZ);

    // Player start position jump (keys 1-4)
    for (int k = SAPP_KEYCODE_1; k <= SAPP_KEYCODE_4; k++) {
        if (Platform::WasKeyPressed(k)) {
            int pi = k - SAPP_KEYCODE_1;
            float fx = 0.25f + (pi % 2) * 0.5f;  // 0.25 or 0.75
            float fy = 0.25f + (pi / 2) * 0.5f;  // 0.25 or 0.75
            float wx = g.w3i.cameraBounds[0] + (g.w3i.cameraBounds[1] - g.w3i.cameraBounds[0]) * fx;
            float wy = g.w3i.cameraBounds[2] + (g.w3i.cameraBounds[3] - g.w3i.cameraBounds[2]) * fy;
            wx *= 128.0f; wy *= 128.0f;
            float gh = g.texturedTerrain.GetGroundHeight(wx, wy);
            g.camera.SetTarget(wx, gh + 100.0f, wy);
            g.camera.ResetToHome();
        }
    }

    g.camera.Update(dt);

    // Update audio listener from camera
    if (g.audio.IsValid()) {
        Vec3 cp = g.camera.GetPosition();
        g.audio.SetListenerPosition(cp.x, cp.y, cp.z);
        float yaw = g.camera.GetYaw(), pitch = g.camera.GetPitch();
        float fx = std::sin(yaw) * std::cos(pitch);
        float fy = std::sin(pitch);
        float fz = std::cos(yaw) * std::cos(pitch);
        g.audio.SetListenerOrientation(fx, fy, fz, 0.0f, 1.0f, 0.0f);
        g.audio.Update();
    }

    // Day/night cycle
    g.dayNight.Update(dt);
    if (g.texturedTerrain.IsValid()) {
        g.dayNight.Apply(g.texturedTerrain);
    }

    // Update resource bar time display
    if (g.ui.GetResourceBar()) {
        g.ui.GetResourceBar()->SetTimeOfDay(g.dayNight.GetTimeOfDay());
    }

    // Run simulation step
    if (g.simulationInitialized) {
        if (g.jassNatives) {
            g.jassNatives->UpdateTimers(CSimulation::SIM_STEP_TIME);
        }

        g.simulation.Update();

        // Sync simulation positions to visual models after timer-driven spawns and orders.
        SyncAllSimulationUnitVisuals();

        RefreshSelectionUi(false);
    }

    int winW = sapp_width(), winH = sapp_height();
    float aspect = (winH > 0) ? (float)winW / winH : 1.0f;

    Mat4 view = g.camera.GetViewMatrix();
    Mat4 proj = g.camera.GetProjectionMatrix(aspect);

    Renderer::BeginFrame();
    Renderer::SetWireframe(g.wireframe);

    if (g.texturedTerrain.IsValid())
        g.texturedTerrain.Render(view, proj, g.frameCount * 0.016f);
    else if (g.simpleTerrain.IsValid())
        g.simpleTerrain.Render(view, proj);

    sg_reset_state_cache();
    sg_apply_viewport(0, 0, winW, winH, true);
    sg_apply_scissor_rect(0, 0, winW, winH, true);
    g.scene.RenderAll(view, proj);

    g.ui.Update(winW, winH);
    ScreenRect minimapRect = g.ui.GetMinimapRect();
    if (minimapRect.IsValid()) {
        g.minimap.SetScreenRect(minimapRect.x, minimapRect.y, minimapRect.w, minimapRect.h);
    }

    // Build in-world unit overlay data (health bars, selection indicators)
    static const uint32_t kPlayerColors[12] = {
        0xFFFF0000, 0xFF0042FF, 0xFF1CE6B6, 0xFF540081,
        0xFFFFFC00, 0xFFFE8A0E, 0xFF20C000, 0xFFE55BB0,
        0xFF959697, 0xFF7EBFF1, 0xFF106246, 0xFF4F2B05
    };

    std::vector<UnitOverlayInfo> overlays;
    if (g.simulationInitialized) {
        for (auto* unit : g.simulation.GetUnits()) {
            if (unit->IsDead()) continue;

            float gndH = 0;
            if (g.texturedTerrain.IsValid())
                gndH = g.texturedTerrain.GetGroundHeight(unit->GetX(), unit->GetY());

            UnitOverlayInfo info;
            info.handleId = unit->GetHandleId();
            info.isSelected = IsUnitSelected(unit->GetHandleId());
            info.isAlive = !unit->IsDead();
            info.hpRatio = unit->GetMaxLife() > 0 ? unit->GetLife() / unit->GetMaxLife() : 1.0f;
            info.mpRatio = unit->GetMaxMana() > 0 ? unit->GetMana() / (float)unit->GetMaxMana() : 0.0f;

            // Head screen position
            Vec3 headPos(unit->GetX(), gndH + 140.0f, unit->GetY());
            WorldToScreen(headPos, view, proj, winW, winH, info.screenX, info.screenY);

            // Foot screen position
            Vec3 footPos(unit->GetX(), gndH + 10.0f, unit->GetY());
            WorldToScreen(footPos, view, proj, winW, winH, info.footX, info.footY);

            // Unit name
            auto* unitType = g.unitData.GetUnitType(unit->GetTypeId());
            info.name = unitType ? unitType->GetName() : "?";

            // Player color by index
            int pi = unit->GetPlayerIndex();
            if (pi >= 0 && pi < 12) info.playerColor = kPlayerColors[pi];
            else info.playerColor = 0xFF888888;

            overlays.push_back(info);
        }
    }
    g.ui.SetOverlayData(overlays);

    // UI overlay (Noesis shell + world overlays)
    g.ui.Render();

    if (g.showMinimap && g.minimap.IsValid()) {
        Vec3 cp = g.camera.GetPosition();
        g.minimap.Render(winW, winH, cp.x, cp.z,
                         g.camera.GetYaw(), 0, 0, 45.0f, aspect);
    }

    Renderer::EndFrame();
    Platform::ProcessInput();
    g.frameCount++;

    if (g.screenshotFrame >= 0 && g.frameCount >= g.screenshotFrame) {
        std::cout << "Screenshot triggered but not implemented with sokol_gfx" << std::endl;
        g.screenshotFrame = -1;
    }
}

// Default map when no --run argument given
static const char* DEFAULT_MAP = "Maps/FrozenThrone/(4)Avalanche.w3x";

// --- Init callback ---
static void init_cb() {
    std::cout << "Warsmash C++ Recreation - Initializing..." << std::endl;

    if (!Renderer::Initialize()) return;

    fs::path project_root = PROJECT_SOURCE_DIR;
    fs::path resources_dir = project_root / "resources";

    for (const auto& name : {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"}) {
        fs::path p = resources_dir / name;
        if (fs::exists(p)) g.mpq.Open(p.string());
    }

    if (g.mpq.ArchiveCount() > 0) g.scene.Init(g.mpq);

    // Initialize audio
    g.audio.Init();
    if (g.audio.IsValid() && g.mpq.ArchiveCount() > 0) {
        g.audio.SetMpqManager(&g.mpq);
        g.audio.LoadSound("ui_click", "UI\\Mouse\\HumanClick1.wav", &g.mpq);
        g.audio.LoadSound("ui_error", "UI\\Mouse\\HumanClickError.wav", &g.mpq);
    }

    // Initialize UI
    g.ui.Init(&g.mpq, sapp_width(), sapp_height());

    // Find and load map
    std::vector<fs::path> mapFiles;
    if (fs::exists(resources_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(resources_dir)) {
            std::string ext = entry.path().extension().string();
            if (ext == ".w3x" || ext == ".w3m")
                mapFiles.push_back(entry.path());
        }
    }

    if (!g.mapPath.empty()) {
        // Resolve relative path against resources directory
        fs::path map_file_path = g.mapPath;
        if (map_file_path.is_relative()) {
            fs::path resolved = resources_dir / map_file_path;
            if (fs::exists(resolved)) {
                map_file_path = resolved;
            }
        }
        g.mapPath = map_file_path.string();
    } else if (!mapFiles.empty()) {
        // Check if default map exists
        fs::path default_path = resources_dir / DEFAULT_MAP;
        if (fs::exists(default_path)) {
            g.mapPath = default_path.string();
        } else {
            std::cout << "\nAvailable maps:" << std::endl;
            for (size_t i = 0; i < mapFiles.size(); i++)
                std::cout << "  [" << (i+1) << "] " << fs::relative(mapFiles[i], resources_dir).string() << std::endl;
            g.mapPath = mapFiles[0].string();
        }
    }

    if (!g.mapPath.empty()) {
        std::cout << "\nLoading: " << fs::path(g.mapPath).filename() << std::endl;
        g.scene.ResetLoadStats();

        W3ILoadFromMpq(g.mapPath, g.w3i);
        W3IPrint(g.w3i);

        WpmLoadFromMpq(g.mapPath, g.wpm);
        g.simulation.SetPathingMap(&g.wpm);
        W3rData w3r; W3rLoadFromMpq(g.mapPath, w3r);

        // Load map units and doodads (single pass)
        UnitsDooData units;
        if (UnitsDooLoadFromMpq(g.mapPath, units, &g.w3i)) {
            std::cout << "UnitsDoo: version=" << units.version
                      << " units=" << units.units.size()
                      << " skinId=" << ((((g.w3i.gameVersionMajor * 100) + g.w3i.gameVersionMinor) >= 132) ? "on" : "off")
                      << std::endl;
            // Initialize game simulation
            if (g.mpq.ArchiveCount() > 0) {
                CGameplayConstants gameplayConstants;
                g.unitData = CUnitData::LoadFromMpq(g.mpq, gameplayConstants);
                g.simulation.SetUnitData(&g.unitData);
                g.simulation.SetGameplayConstants(gameplayConstants);
                g.simulation.SetAudioSystem(&g.audio);

                for (int i = 0; i < 12; i++) {
                    auto player = std::make_unique<CPlayer>(i, "Player " + std::to_string(i + 1),
                                                              CMapControl::USER);
                    player->SetGold(500);
                    player->SetLumber(200);
                    player->SetFoodCap(10);
                    g.simulation.AddPlayer(std::move(player));
                }

                // Create simulation units from units.doo and let the simulation own
                // their visuals so animation/state starts from runtime data instead of
                // a separate editor-scene copy.
                int simUnitCreated = 0;
                int visualUnitCreated = 0;
                int visualMissForSimUnits = 0;
                for (auto& u : units.units) {
                    War3ID id(u.id);
                    CUnit* simUnit = nullptr;
                    if (g.unitData.HasUnitType(id)) {
                        simUnit = g.simulation.CreateUnit(
                            id, u.player, u.pos[0], u.pos[1], u.angle);
                        if (simUnit) {
                            simUnitCreated++;
                            g.simUnitVisualSeeds[simUnit->GetHandleId()] = {
                                {u.scale[0], u.scale[1], u.scale[2]},
                                u.skinId,
                                u.hitpoints,
                                u.flags
                            };
                            if (u.hitpoints > 0) simUnit->SetLife(g.simulation, (float)u.hitpoints);
                            if (u.mana > 0) simUnit->SetMana((float)u.mana);
                        }
                    }
                    // Keep direct scene visuals only for units we couldn't instantiate
                    // in the simulation.
                    if (!simUnit) {
                        if (g.scene.AddUnit(u.id, u.pos, u.angle, u.scale, u.player, u.hitpoints, u.flags, u.skinId)) {
                            visualUnitCreated++;
                        }
                    }
                }
                // Force process new units
                g.simulation.Update();
                g.simulationInitialized = true;
                g.ui.SetSimulation(&g.simulation);
                for (auto* unit : g.simulation.GetUnits()) {
                    if (EnsureSimulationUnitVisual(unit)) {
                        visualUnitCreated++;
                    } else {
                        visualMissForSimUnits++;
                    }
                }
                std::cout << "Simulation: " << g.unitData.GetAllTypes().size()
                          << " types, " << g.simulation.GetUnits().size() << " units" << std::endl;
                std::cout << "SceneUnitSync: simCreated=" << simUnitCreated
                          << " visualCreated=" << visualUnitCreated
                          << " visualMissForSimUnits=" << visualMissForSimUnits
                          << std::endl;

                // Give UI access to unit data and populate command card for first unit
                g.ui.SetUnitData(&g.unitData);
                if (!g.simulation.GetUnits().empty()) {
                    War3ID firstType = g.simulation.GetUnits()[0]->GetTypeId();
                    g.ui.PopulateCommandCardForUnitType(firstType);
                }
            } else {
                // No MPQ: just add visual models
                for (auto& u : units.units) {
                    g.scene.AddUnit(u.id, u.pos, u.angle, u.scale, u.player, u.hitpoints, u.flags, u.skinId);
                }
            }
        } else {
            std::cout << "UnitsDoo: failed to load war3mapUnits.doo" << std::endl;
        }

        // Fallback: load unit data and populate command card for footman
        if (!g.simulationInitialized) {
            if (g.mpq.ArchiveCount() > 0) {
                CGameplayConstants gameplayConstants;
                g.unitData = CUnitData::LoadFromMpq(g.mpq, gameplayConstants);
                g.ui.SetUnitData(&g.unitData);
                std::cout << "CUnitData: loaded " << g.unitData.GetAllTypes().size() << " types (fallback)" << std::endl;
            }
            if (g.ui.GetGameUI().rootFrame) {
                g.ui.PopulateCommandCardForUnitType(War3ID("hfoo"));
            }
        }

        W3dData w3d;
        if (W3dLoadFromMpq(g.mapPath, w3d)) {
            int doodAdded = 0;
            for (auto& d : w3d.doodads) {
                if (g.scene.AddDoodad(d.id, d.pos, d.angle, d.scale, d.variation, d.flags, d.life, d.skinId))
                    doodAdded++;
            }
            if (doodAdded > 0) std::cout << "  Rendered " << doodAdded << " doodads" << std::endl;
        }
        g.scene.PrintLoadStats();

        if (W3XLoadTerrain(g.mapPath, g.map)) {
            bool hasTextures = false;
            if (g.mpq.ArchiveCount() > 0 && g.tileset.LoadFromMpq(g.mpq, g.map))
                hasTextures = true;
            else if (g.mpq.ArchiveCount() > 0 && g.tileset.LoadFromWorldEditData(g.mpq, g.map.tileset, g.map.groundTilePalette.size()))
                hasTextures = true;

            if (hasTextures) {
                g.tileset.LoadCliffTextures(g.mpq, g.map);
                g.tileset.LoadCliffModelInfo(g.mpq);
                g.tileset.LoadWaterInfo(g.mpq, g.map);
                g.tileset.LoadWaterTextures(g.mpq);
                g.texturedTerrain.SetMpq(&g.mpq);
                g.terrainLoaded = g.texturedTerrain.Build(g.map, &g.tileset);
            } else {
                std::cout << "Terrain: falling back to simple (no textures)" << std::endl;
                g.terrainLoaded = g.simpleTerrain.Build(g.map);
            }

            g.texturedTerrain.SetPathing(&g.wpm);
            g.texturedTerrain.SetRegions(&w3r);
            if (g.w3i.playableWidth > 0) {
                g.texturedTerrain.SetMapBounds(g.w3i.cameraBounds[0], g.w3i.cameraBounds[1],
                                                 g.w3i.cameraBounds[2], g.w3i.cameraBounds[3]);
            } else {
                Vec3 c = g.texturedTerrain.GetCenter();
                float r = g.texturedTerrain.GetRadius();
                g.texturedTerrain.SetMapBounds(c.x - r, c.z - r, c.x + r, c.z + r);
            }
        }
    }

    // Initialize JASS VM and load map scripts
    auto ReadMapFile = [](const std::string& mapPath, const std::string& fileName, std::vector<uint8_t>& out) -> bool {
        #ifndef WITHOUT_STORMLIB
        HANDLE hMpq = nullptr;
        if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return false;
        HANDLE hFile = nullptr;
        if (!SFileOpenFileEx(hMpq, fileName.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) {
            SFileCloseArchive(hMpq); return false;
        }
        DWORD sz = SFileGetFileSize(hFile, nullptr);
        out.resize(sz);
        DWORD read = 0;
        SFileReadFile(hFile, out.data(), sz, &read, nullptr);
        SFileCloseFile(hFile);
        SFileCloseArchive(hMpq);
        return true;
        #else
        (void)mapPath; (void)fileName; (void)out;
        return false;
        #endif
    };

    if (g.mpq.ArchiveCount() > 0) {
        std::vector<uint8_t> jassData;
        if (ReadMapFile(g.mapPath, "war3map.j", jassData)) {
            std::string jassSource((const char*)jassData.data(), jassData.size());
            std::cout << "JASS: loaded war3map.j (" << jassSource.size() << " bytes)" << std::endl;
            if (g.jass.LoadScript(jassSource)) {
                // Register native functions with simulation bindings (heap-allocated for lifetime safety)
                g.jassNatives = std::make_unique<JassNatives>(g.jass, &g.simulation);
                g.jassNatives->RegisterAll();

                // Wire camera callbacks
                auto* jassData = g.jassNatives->GetData().get();
                jassData->camSetPosition = [](float x, float y, float z) {
                    g.camera.SetTarget(x, y, z);
                    g.camera.ResetToHome();
                };
                jassData->camSetTarget = [](float x, float y, float z) {
                    g.camera.SetTarget(x, y, z);
                    g.camera.ResetToHome();
                };
                jassData->camReset = []() {
                    g.camera.ResetToHome();
                };
                jassData->camGetX = []() -> float {
                    return g.camera.GetTarget().x;
                };
                jassData->camGetY = []() -> float {
                    return g.camera.GetTarget().y;
                };
                jassData->camGetZ = []() -> float {
                    return g.camera.GetTarget().z;
                };
                jassData->camGetEyeX = []() -> float {
                    return g.camera.GetPosition().x;
                };
                jassData->camGetEyeY = []() -> float {
                    return g.camera.GetPosition().y;
                };
                jassData->camGetEyeZ = []() -> float {
                    return g.camera.GetPosition().z;
                };

                // Wire animation callbacks
                jassData->unitSetAnimation = [](int simHandle, const std::string& animName) {
                    auto it = g.simUnitToVisualIdx.find(simHandle);
                    if (it != g.simUnitToVisualIdx.end()) {
                        auto* viewer = g.scene.GetViewer(it->second);
                        if (viewer && viewer->GetAnimator()) {
                            viewer->GetAnimator()->PlaySequence(animName);
                            g.simUnitPendingAnimations.erase(simHandle);
                            return;
                        }
                    }
                    g.simUnitPendingAnimations[simHandle] = {animName, -1, false};
                };
                jassData->unitSetAnimationByIndex = [](int simHandle, int index) {
                    auto it = g.simUnitToVisualIdx.find(simHandle);
                    if (it != g.simUnitToVisualIdx.end()) {
                        auto* viewer = g.scene.GetViewer(it->second);
                        if (viewer && viewer->GetAnimator()) {
                            viewer->GetAnimator()->SetSequence(index);
                            g.simUnitPendingAnimations.erase(simHandle);
                            return;
                        }
                    }
                    g.simUnitPendingAnimations[simHandle] = {"", index, true};
                };

                // Wire unit death events to trigger system
                g.simulation.SetUnitDeathCallback(
                    [](int unitSimHandle, int killerSimHandle, int /*playerIdx*/) {
                        if (g.jassNatives)
                            g.jassNatives->FireDeathEvent(unitSimHandle, killerSimHandle, JASS_EVENT_UNIT_DEATH);
                    }
                );
                // Wire unit damage events to trigger system
                g.simulation.SetUnitDamageCallback(
                    [](int unitHandle, int sourceHandle, float damage) {
                        if (g.jassNatives)
                            g.jassNatives->FireDamageEvent(unitHandle, sourceHandle, damage, JASS_EVENT_UNIT_DAMAGED);
                    }
                );
                // Wire unit order events to trigger system
                g.simulation.SetUnitOrderCallback(
                    [](int unitHandle, int orderId, int targetHandle, float x, float y) {
                        if (g.jassNatives)
                            g.jassNatives->FireOrderEvent(unitHandle, orderId, targetHandle, x, y);
                    }
                );
                // Warcraft III maps expect config() before main().
                if (g.jass.IsValid()) {
                    g.jass.Execute("config");
                    g.jass.Execute("main");
                    // Flush any CreateUnit() calls issued by JASS before we inspect the map state.
                    g.simulation.Update();
                    EnsureHumanMeleeStartsFromMapConfig();
                    FlushStartupSimulationState();
                }
            } else {
                for (auto& e : g.jass.GetErrors())
                    std::cerr << "JASS: " << e << std::endl;
            }
        } else {
            std::cout << "JASS: no war3map.j found in map" << std::endl;
        }
    }

    // Generate minimap
    if (g.terrainLoaded && g.texturedTerrain.IsValid()) {
        g.minimap.Generate(g.texturedTerrain, g.tileset, g.map, g.w3i, &g.mpq);
    }

    if (!g.terrainLoaded) {
        std::cout << "Generating synthetic terrain..." << std::endl;
        g.map = GenerateTestTerrain();
        bool hasTextures = g.mpq.ArchiveCount() > 0 && g.tileset.LoadFromMpq(g.mpq, g.map);
        if (!hasTextures)
            hasTextures = g.mpq.ArchiveCount() > 0 && g.tileset.LoadFromWorldEditData(g.mpq, g.map.tileset, g.map.groundTilePalette.size());
        if (hasTextures)
            g.terrainLoaded = g.texturedTerrain.Build(g.map, &g.tileset);
        else
            g.terrainLoaded = g.simpleTerrain.Build(g.map);
    }

    if (!g.terrainLoaded) {
        std::cerr << "Failed to build terrain!" << std::endl;
    }

    Vec3 center;
    float radius;
    if (g.texturedTerrain.IsValid()) {
        center = g.texturedTerrain.GetCenter();
        radius = g.texturedTerrain.GetRadius();
    } else {
        center = g.simpleTerrain.GetCenter();
        radius = g.simpleTerrain.GetRadius();
    }

    {
        float sampleH = g.texturedTerrain.IsValid() ? g.texturedTerrain.GetGroundHeight(center.x, center.z) : 0;
        float sampleW = g.texturedTerrain.IsValid() ? g.texturedTerrain.GetWaterHeight(center.x, center.z) : 0;
        bool playable = g.texturedTerrain.IsValid() ? g.texturedTerrain.InPlayableArea(center.x, center.z) : true;
        Vec3 campos = g.camera.GetPosition();
        std::cout << "Terrain center: (" << center.x << ", " << center.y << ", " << center.z
                  << ") radius=" << radius << std::endl;
        std::cout << "Camera pos: (" << campos.x << ", " << campos.y << ", " << campos.z
                  << ") target: (" << g.camera.GetPosition().x << "...)" << std::endl;
        std::cout << "Terrain center height: " << sampleH
                  << " water: " << sampleW
                  << " playable: " << playable << std::endl;
    }

    // Default camera fallback at terrain center. We may overwrite this below
    // with the initial spawn focus once map/JASS state is ready.
    g.camera.SetTarget(center.x, center.y, center.z);
    g.camera.SetDistance(2500.0f);
    g.camera.SetMaxDistance(5000.0f);
    g.camera.SaveHomeState();
    FocusCameraAndMinimapOnInitialSpawn();

    g.initialized = true;
    std::cout << "Initialization complete. Starting render loop..." << std::endl;
}

// --- Cleanup callback ---
static void cleanup_cb() {
    std::cout << "Shutting down after " << g.frameCount << " frames..." << std::endl;
    g.ui.Shutdown();
    Renderer::Shutdown();
    g.audio.Shutdown();
    // All sokol resource destructors are now guarded by Renderer::IsActive()
}

// --- sokol_main ---
sapp_desc sokol_main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--screenshot") == 0 && i + 1 < argc) {
            g.screenshotPath = argv[++i];
            g.screenshotFrame = 1;
        } else if (std::strncmp(arg, "--run=", 6) == 0) {
            g.mapPath = arg + 6; // --run=path
        } else if (std::strcmp(arg, "--run") == 0 && i + 1 < argc) {
            g.mapPath = argv[++i]; // --run path
        } else if (arg[0] != '-') {
            g.mapPath = arg; // bare path
        }
    }

    sapp_desc desc = {};
    desc.init_cb = init_cb;
    desc.frame_cb = frame_cb;
    desc.cleanup_cb = cleanup_cb;
    desc.event_cb = event_cb;
    desc.width = 1280;
    desc.height = 720;
	desc.sample_count = 4;
    desc.window_title = "Warsmash C++ Recreation (sokol)";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}

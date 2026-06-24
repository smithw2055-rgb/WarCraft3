#include <iostream>
#include <filesystem>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
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
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/behaviors/behavior_attack.h"
#include "ecs/behaviors/behavior_build.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/behaviors/behavior_move.h"
#include "ecs/behaviors/behavior_repair.h"
#include "ecs/behaviors/behavior_return_resources.h"
#include "ecs/behaviors/behavior_spell.h"
#include "ecs/orders/order.h"
#include "data/unitdata.h"
#include "data/destructabledata.h"
#include "data/gameplay_constants.h"
#include "ui/MainUI.h"
#include "audio/AudioSystem.h"
#include "debug/move_debug.h"
#include "scripting/JassVM.h"
#include "scripting/JassNatives.h"
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>
#include "render/daynight.h"
#include "core/warsmash_config.h"
#include "scripting/jass_config_env.h"
#include "scripting/jass_common_env.h"
#include "map/map_loader.h"

namespace fs = std::filesystem;

static fs::path ResolveConfiguredPath(const fs::path& projectRoot,
                                      const fs::path& resourcesDir,
                                      const std::string& configuredPath) {
    fs::path path(configuredPath);
    if (path.is_absolute()) {
        return path;
    }

    const fs::path fromProject = projectRoot / path;
    if (fs::exists(fromProject)) {
        return fromProject;
    }

    const fs::path fromResources = resourcesDir / path;
    if (fs::exists(fromResources)) {
        return fromResources;
    }

    const fs::path fromResourceFileName = resourcesDir / path.filename();
    if (fs::exists(fromResourceFileName)) {
        return fromResourceFileName;
    }

    return fromProject;
}

// --- Global state (for callbacks) ---
static struct {
    bool initialized = false;
    bool running = true;
    int frameCount = 0;
    float lastTime = 0;
    float simulationAccumulator = 0.0f;

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

    enum class UnitVisualAnimationMode : uint8_t {
        NONE,
        READY,
        READY_GOLD,
        READY_LUMBER,
        WALK,
        WALK_GOLD,
        WALK_LUMBER,
        WALK_WORK,
        WORK,
        SPELL,
        ATTACK,
        CONSTRUCT,
        DEAD,
        CORPSE,
        BONE_CORPSE,
        DISSIPATE
    };

    struct ScriptedUnitAnimationState {
        PendingUnitAnimation request;
        bool loop = false;
        bool queueStandAfterCurrent = false;
    };

    struct SelectionCircleVisual {
        float size = 0.0f;
        std::string texturePath;
        std::string dottedTexturePath;
    };

    struct UnitRenderSplatMeta {
        float selectionScale = 0.0f;
        float selectionHeight = 0.0f;
        float animationWalkSpeed = 0.0f;
        float animationRunSpeed = 0.0f;
        std::string uberSplatPath;
        float uberSplatScale = 0.0f;
        std::string buildingShadow;
    };

    CSimulation simulation;
    CUnitData unitData;
    CDestructableData destructableData;
    WpmData wpm;
    bool simulationInitialized = false;
    std::unordered_map<int, UnitVisualSeed> simUnitVisualSeeds;
    std::unordered_map<int, PendingUnitAnimation> simUnitPendingAnimations;
    std::unordered_map<int, int> simUnitToVisualIdx; // handleId → visual instance index
    std::unordered_map<int, std::pair<float, float>> simUnitVisualLastPositions;
    std::unordered_map<int, bool> simUnitVisualMovingStates;
    std::unordered_map<int, UnitVisualAnimationMode> simUnitVisualAnimationModes;
    std::unordered_map<int, int> simUnitVisualLastAttackAnimationTicks;
    std::unordered_map<int, CResourceType> simUnitVisualResourceTags;
    std::unordered_map<int, bool> simUnitVisualConstructionStates;
    std::unordered_map<int, std::vector<std::string>> simUnitVisualPropertyTags;
    std::unordered_map<int, ScriptedUnitAnimationState> simUnitScriptedAnimations;
    std::unordered_map<int, PendingUnitAnimation> simUnitQueuedAnimations;
    std::unordered_map<int, std::vector<std::string>> simUnitAnimationPropertyTags;
    std::unordered_set<int> simUnitVisualRegistrationFailed;
    std::vector<int> selectedUnitHandles;
    std::unordered_set<int> selectedUnitHandleSet;
    int selectedUnitHandle = -1;
    War3ID selectedUnitTypeId;
    std::unordered_map<uint32_t, UnitRenderSplatMeta> unitRenderSplatMeta;
    std::vector<SelectionCircleVisual> selectionCircleVisuals;
    float selectionCircleScaleFactor = 1.0f;
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
    const Vec3 rayOrigin(nearP.X, nearP.Y, nearP.Z);
    const Vec3 rayDirection = Normalize(Vec3(farP.X - nearP.X, farP.Y - nearP.Y, farP.Z - nearP.Z));

    if (g.texturedTerrain.IsValid() && rayDirection.y < -0.0001f) {
        const float maxDistance = std::max(8192.0f, g.texturedTerrain.GetRadius() * 4.0f);
        float previousT = 0.0f;

        for (int stepIndex = 1; stepIndex <= 256; ++stepIndex) {
            const float t = maxDistance * (static_cast<float>(stepIndex) / 256.0f);
            Vec3 point = rayOrigin + rayDirection * t;
            const float groundHeight = g.texturedTerrain.GetGroundHeight(point.x, point.z);
            const float delta = point.y - groundHeight;

            if (delta <= 0.0f) {
                float lowT = previousT;
                float highT = t;
                Vec3 hitPoint = point;
                for (int iter = 0; iter < 12; ++iter) {
                    const float midT = (lowT + highT) * 0.5f;
                    Vec3 midPoint = rayOrigin + rayDirection * midT;
                    const float midGroundHeight = g.texturedTerrain.GetGroundHeight(midPoint.x, midPoint.z);
                    const float midDelta = midPoint.y - midGroundHeight;
                    if (midDelta <= 0.0f) {
                        highT = midT;
                        hitPoint = Vec3(midPoint.x, midGroundHeight, midPoint.z);
                    } else {
                        lowT = midT;
                    }
                }
                return hitPoint;
            }

            previousT = t;
        }
    }

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

static std::string TrimMetadataValue(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static bool IsUnitSelected(int handleId);

static std::string NormalizeMetadataPath(std::string path) {
    for (char& c : path) {
        if (c == '/') {
            c = '\\';
        }
    }
    if (!path.empty() && path.find('.') == std::string::npos) {
        path += ".blp";
    }
    return path;
}

static bool ReadMpqFile(const std::string& path, std::vector<uint8_t>& out) {
    return g.mpq.ReadFile(path, out);
}

static bool ReadMpqFileAny(const std::initializer_list<const char*>& paths, std::vector<uint8_t>& out) {
    for (const char* path : paths) {
        if (g.mpq.ReadFile(path, out)) {
            return true;
        }
    }
    return false;
}

static void LoadSelectionAndBuildingSplatMetadata() {
    g.unitRenderSplatMeta.clear();
    g.selectionCircleVisuals.clear();
    g.selectionCircleScaleFactor = 1.0f;

    if (g.mpq.ArchiveCount() <= 0) {
        return;
    }

    SlkTable unitUiSlk;
    SlkTable uberSplatSlk;
    TxtTable miscData;
    std::vector<uint8_t> buf;

    if (ReadMpqFileAny({"Units\\UnitUI.slk", "Units\\unitUI.slk"}, buf)) {
        unitUiSlk.Load(buf.data(), buf.size());
    }
    buf.clear();
    if (ReadMpqFile("Splats\\UberSplatData.slk", buf)) {
        uberSplatSlk.Load(buf.data(), buf.size());
    }

    for (const char* miscPath : {
             "UI\\MiscData.txt",
             "Units\\MiscData.txt",
             "Units\\MiscGame.txt",
             "UI\\MiscUI.txt",
             "war3mapMisc.txt"
         }) {
        buf.clear();
        if (ReadMpqFile(miscPath, buf)) {
            miscData.Load(buf.data(), buf.size());
        }
    }

    std::unordered_map<std::string, std::pair<std::string, float>> uberSplatLookup;
    if (uberSplatSlk.Rows() > 0) {
        int idCol = uberSplatSlk.FindColumn("UberSplatID");
        if (idCol < 0) idCol = uberSplatSlk.FindColumn("splatID");
        if (idCol < 0) idCol = 0;
        const int dirCol = uberSplatSlk.FindColumn("Dir");
        const int fileCol = uberSplatSlk.FindColumn("file");
        const int scaleCol = uberSplatSlk.FindColumn("Scale");
        for (int row = 0; row < uberSplatSlk.Rows(); ++row) {
            const std::string id = TrimMetadataValue(uberSplatSlk.GetString(row, idCol));
            if (id.empty()) {
                continue;
            }
            std::string path = NormalizeMetadataPath(uberSplatSlk.GetString(row, dirCol) + "\\" +
                                                     uberSplatSlk.GetString(row, fileCol));
            if (dirCol < 0 || fileCol < 0) {
                path.clear();
            }
            const float scale = scaleCol >= 0 ? std::strtof(uberSplatSlk.GetString(row, scaleCol).c_str(), nullptr) : 0.0f;
            uberSplatLookup[id] = {path, scale};
        }
    }

    if (unitUiSlk.Rows() > 0) {
        int idCol = unitUiSlk.FindColumn("unitUIID");
        if (idCol < 0) idCol = unitUiSlk.FindColumn("unitID");
        if (idCol < 0) idCol = 0;
        const int scaleCol = unitUiSlk.FindColumn("scale");
        const int selZCol = unitUiSlk.FindColumn("selZ");
        const int walkSpeedCol = unitUiSlk.FindColumn("walk");
        const int runSpeedCol = unitUiSlk.FindColumn("run");
        const int uberCol = unitUiSlk.FindColumn("uberSplat");
        const int buildingShadowCol = unitUiSlk.FindColumn("buildingShadow");
        for (int row = 0; row < unitUiSlk.Rows(); ++row) {
            const std::string id = TrimMetadataValue(unitUiSlk.GetString(row, idCol));
            if (id.size() != 4) {
                continue;
            }

            auto& meta = g.unitRenderSplatMeta[War3ID::FromString(id).value];
            meta.selectionScale = scaleCol >= 0 ? std::strtof(unitUiSlk.GetString(row, scaleCol).c_str(), nullptr) : 0.0f;
            meta.selectionHeight = selZCol >= 0 ? std::strtof(unitUiSlk.GetString(row, selZCol).c_str(), nullptr) : 0.0f;
            meta.animationWalkSpeed = walkSpeedCol >= 0 ? std::strtof(unitUiSlk.GetString(row, walkSpeedCol).c_str(), nullptr) : 0.0f;
            meta.animationRunSpeed = runSpeedCol >= 0 ? std::strtof(unitUiSlk.GetString(row, runSpeedCol).c_str(), nullptr) : 0.0f;
            meta.buildingShadow = buildingShadowCol >= 0 ? unitUiSlk.GetString(row, buildingShadowCol) : std::string();

            if (id == "hpea" || id == "pea1" || id == "aeph") {
                const int animPropsCol = unitUiSlk.FindColumn("animProps");
                std::cout << "WalkDiag[UnitUI]: id=" << id
                          << " walk=" << meta.animationWalkSpeed
                          << " run=" << meta.animationRunSpeed
                          << " animProps=" << (animPropsCol >= 0 ? unitUiSlk.GetString(row, animPropsCol) : "")
                          << std::endl;
            }

            const std::string uberId = uberCol >= 0 ? TrimMetadataValue(unitUiSlk.GetString(row, uberCol)) : std::string();
            if (!uberId.empty()) {
                auto uberIt = uberSplatLookup.find(uberId);
                if (uberIt != uberSplatLookup.end()) {
                    meta.uberSplatPath = uberIt->second.first;
                    meta.uberSplatScale = uberIt->second.second;
                }
            }
        }
    }

    const TxtTable& misc = miscData;
    const int numSizes = misc.GetInt("SelectionCircle", "NumSizes", 0);
    g.selectionCircleScaleFactor = misc.GetFloat("SelectionCircle", "ScaleFactor", 1.0f);
    for (int i = 0; i < numSizes; ++i) {
        char index[3] = {};
        std::snprintf(index, sizeof(index), "%02d", i);
        decltype(g.selectionCircleVisuals)::value_type visual;
        visual.size = misc.GetFloat("SelectionCircle", std::string("Size") + index, 0.0f);
        visual.texturePath = NormalizeMetadataPath(misc.GetString("SelectionCircle", std::string("Texture") + index));
        visual.dottedTexturePath = NormalizeMetadataPath(
            misc.GetString("SelectionCircle", std::string("TextureDotted") + index));
        if (!visual.texturePath.empty()) {
            g.selectionCircleVisuals.push_back(std::move(visual));
        }
    }
    std::sort(g.selectionCircleVisuals.begin(), g.selectionCircleVisuals.end(),
              [](const auto& a, const auto& b) { return a.size < b.size; });

    std::cout << "UnitRenderMetadata: units=" << g.unitRenderSplatMeta.size()
              << " selectionCircles=" << g.selectionCircleVisuals.size()
              << " scaleFactor=" << g.selectionCircleScaleFactor
              << std::endl;
}

static const decltype(g.unitRenderSplatMeta)::mapped_type* GetUnitRenderSplatMeta(const CUnit& unit) {
    const auto it = g.unitRenderSplatMeta.find(unit.GetTypeId().value);
    return (it != g.unitRenderSplatMeta.end()) ? &it->second : nullptr;
}

static float ComputeSelectionCircleSizeWorld(const CUnit& unit) {
    if (const auto* meta = GetUnitRenderSplatMeta(unit)) {
        if (meta->selectionScale > 0.0f) {
            const float scaleFactor = g.selectionCircleScaleFactor > 0.0f ? g.selectionCircleScaleFactor : 1.0f;
            return meta->selectionScale * scaleFactor;
        }
    }
    return ComputeUnitSelectionWorldRadius(unit) * 2.0f;
}

static std::string PickSelectionCircleTexturePath(float selectionSizeWorld) {
    if (g.selectionCircleVisuals.empty()) {
        return {};
    }
    for (const auto& visual : g.selectionCircleVisuals) {
        if (selectionSizeWorld < visual.size) {
            return visual.texturePath;
        }
    }
    return g.selectionCircleVisuals.back().texturePath;
}

static void RefreshRuntimeUnitSplats() {
    if (!g.texturedTerrain.IsValid()) {
        return;
    }

    g.texturedTerrain.ClearRuntimeSplats();
    if (!g.simulationInitialized) {
        return;
    }

    for (auto* unit : g.simulation.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) {
            continue;
        }

        const auto* meta = GetUnitRenderSplatMeta(*unit);
        if (meta && unit->IsBuilding() && !meta->uberSplatPath.empty() && meta->uberSplatScale > 0.0f) {
            const float halfSize = meta->uberSplatScale * 0.5f;
            g.texturedTerrain.AddRuntimeSplatRect(
                "building:" + std::to_string(unit->GetHandleId()),
                meta->uberSplatPath,
                unit->GetX() - halfSize,
                unit->GetY() - halfSize,
                unit->GetX() + halfSize,
                unit->GetY() + halfSize,
                2.0f,
                false,
                false);
        }

        if (!IsUnitSelected(unit->GetHandleId())) {
            continue;
        }

        const float selectionSize = ComputeSelectionCircleSizeWorld(*unit);
        const std::string texturePath = PickSelectionCircleTexturePath(selectionSize);
        if (texturePath.empty() || selectionSize <= 0.0f) {
            continue;
        }

        const float halfSize = selectionSize * 0.5f;
        g.texturedTerrain.AddRuntimeSplatRect(
            "selection:" + std::to_string(unit->GetHandleId()),
            texturePath,
            unit->GetX() - halfSize,
            unit->GetY() - halfSize,
            unit->GetX() + halfSize,
            unit->GetY() + halfSize,
            6.0f,
            true,
            false);
    }
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
                                              int ignoreHandle = -1,
                                              bool preferBuildings = true,
                                              bool strictScreenTargeting = false) {
    const PickRay ray = BuildScreenPickRay(screenX, screenY, view, proj, screenW, screenH);
    CUnit* bestScreenUnit = nullptr;
    CUnit* bestScreenNonBuildingUnit = nullptr;
    CUnit* bestRayUnit = nullptr;
    CUnit* bestRayNonBuildingUnit = nullptr;
    float bestScreenScore = std::numeric_limits<float>::infinity();
    float bestScreenNonBuildingScore = std::numeric_limits<float>::infinity();
    float bestScreenDistSq = std::numeric_limits<float>::infinity();
    float bestScreenNonBuildingDistSq = std::numeric_limits<float>::infinity();
    float bestRayT = std::numeric_limits<float>::infinity();
    float bestRayNonBuildingT = std::numeric_limits<float>::infinity();

    for (auto* unit : g.simulation.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
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

        float footX = 0.0f;
        float footY = 0.0f;
        float headX = 0.0f;
        float headY = 0.0f;
        float radiusPixels = 0.0f;
        bool hasScreenShape = ComputeUnitSelectionScreenShape(*unit, view, proj, screenW, screenH,
                                                              footX, footY, headX, headY, radiusPixels);

        float hitT = 0.0f;
        const bool rayHit = IntersectSphere(ray, sphereCenter, sphereRadius, hitT);
        if (rayHit && hitT < bestRayT) {
            bestRayT = hitT;
            bestRayUnit = unit;
        }
        if (rayHit && !unit->IsBuilding() && hitT < bestRayNonBuildingT) {
            bestRayNonBuildingT = hitT;
            bestRayNonBuildingUnit = unit;
        }

        if (!hasScreenShape) {
            continue;
        }

        const float distSq = DistanceToSegmentSquared(screenX, screenY, footX, footY, headX, headY);
        float hitRadiusPixels = radiusPixels;
        if (strictScreenTargeting) {
            hitRadiusPixels *= unit->IsBuilding() ? 0.6f : 0.75f;
        }
        if (distSq > hitRadiusPixels * hitRadiusPixels) {
            continue;
        }

        float score = distSq / std::max(1.0f, hitRadiusPixels * hitRadiusPixels);
        if (preferBuildings && unit->IsBuilding()) {
            score = std::max(0.0f, score - 0.05f);
        }

        const bool betterScore = score < bestScreenScore;
        const bool closeScore = std::abs(score - bestScreenScore) <= 0.03f;
        const bool preferBuilding = preferBuildings && closeScore && unit->IsBuilding()
            && bestScreenUnit && !bestScreenUnit->IsBuilding();
        const bool betterDist = closeScore && (distSq < bestScreenDistSq);
        if (betterScore || preferBuilding || betterDist) {
            bestScreenScore = score;
            bestScreenDistSq = distSq;
            bestScreenUnit = unit;
        }

        if (!unit->IsBuilding()) {
            const bool betterNonBuildingScore = score < bestScreenNonBuildingScore;
            const bool closeNonBuildingScore =
                std::abs(score - bestScreenNonBuildingScore) <= 0.03f;
            const bool betterNonBuildingDist =
                closeNonBuildingScore && (distSq < bestScreenNonBuildingDistSq);
            if (betterNonBuildingScore || betterNonBuildingDist) {
                bestScreenNonBuildingScore = score;
                bestScreenNonBuildingDistSq = distSq;
                bestScreenNonBuildingUnit = unit;
            }
        }
    }

    if (!preferBuildings && bestScreenNonBuildingUnit) {
        return bestScreenNonBuildingUnit;
    }
    if (bestScreenUnit) {
        return bestScreenUnit;
    }
    if (strictScreenTargeting) {
        return nullptr;
    }
    if (!preferBuildings && bestRayNonBuildingUnit) {
        return bestRayNonBuildingUnit;
    }
    return bestRayUnit;
}

static CDestructable* PickSimulationDestructableAtScreenPoint(float screenX,
                                                              float screenY,
                                                              const Mat4& view,
                                                              const Mat4& proj,
                                                              int screenW,
                                                              int screenH) {
    if (!g.simulationInitialized) {
        return nullptr;
    }

    const PickRay ray = BuildScreenPickRay(screenX, screenY, view, proj, screenW, screenH);
    CDestructable* bestScreenTarget = nullptr;
    CDestructable* bestRayTarget = nullptr;
    float bestScreenScore = std::numeric_limits<float>::infinity();
    float bestRayT = std::numeric_limits<float>::infinity();

    for (auto* destructable : g.simulation.GetDestructables()) {
        if (!destructable || destructable->IsDead()) {
            continue;
        }

        float worldRadius = 56.0f;
        float worldHeight = 140.0f;
        if (const auto* destructableType = g.destructableData.GetDestructable(destructable->GetTypeId())) {
            if (destructableType->isTree) {
                worldRadius = 72.0f;
                worldHeight = 180.0f;
            }
            else if (destructableType->isWall) {
                worldRadius = 64.0f;
                worldHeight = 96.0f;
            }
        }

        const Vec3 sphereCenter(destructable->GetX(), worldHeight * 0.5f, destructable->GetY());
        float hitT = 0.0f;
        if (IntersectSphere(ray, sphereCenter, worldRadius, hitT) && hitT < bestRayT) {
            bestRayT = hitT;
            bestRayTarget = destructable;
        }

        float centerX = 0.0f;
        float centerY = 0.0f;
        if (!WorldToScreen(sphereCenter, view, proj, screenW, screenH, centerX, centerY)) {
            continue;
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
        updateRadiusFromPoint(Vec3(destructable->GetX() + worldRadius, worldHeight * 0.5f, destructable->GetY()));
        updateRadiusFromPoint(Vec3(destructable->GetX(), worldHeight * 0.5f, destructable->GetY() + worldRadius));
        radiusPixels = std::max(radiusPixels, 20.0f);

        const float dx = screenX - centerX;
        const float dy = screenY - centerY;
        const float distSq = dx * dx + dy * dy;
        if (distSq > radiusPixels * radiusPixels) {
            continue;
        }

        const float score = distSq / std::max(1.0f, radiusPixels * radiusPixels);
        if (score < bestScreenScore) {
            bestScreenScore = score;
            bestScreenTarget = destructable;
        }
    }

    return bestScreenTarget ? bestScreenTarget : bestRayTarget;
}

static std::string DescribeUnitType(War3ID typeId) {
    auto* unitType = g.unitData.GetUnitType(typeId);
    if (!unitType) {
        return typeId.ToString();
    }
    return unitType->GetName() + " [" + typeId.ToString() + "]";
}

static const char* BehaviorCategoryName(CBehaviorCategory category) {
    switch (category) {
        case CBehaviorCategory::NONE: return "NONE";
        case CBehaviorCategory::STOP: return "STOP";
        case CBehaviorCategory::MOVE: return "MOVE";
        case CBehaviorCategory::ATTACK: return "ATTACK";
        case CBehaviorCategory::SPELL: return "SPELL";
        case CBehaviorCategory::ATTACK_MOVE: return "ATTACK_MOVE";
        case CBehaviorCategory::PATROL: return "PATROL";
        case CBehaviorCategory::FOLLOW: return "FOLLOW";
        case CBehaviorCategory::HOLD_POSITION: return "HOLD_POSITION";
        case CBehaviorCategory::BUILD: return "BUILD";
        case CBehaviorCategory::HARVEST: return "HARVEST";
        case CBehaviorCategory::RETURN_RESOURCES: return "RETURN_RESOURCES";
        case CBehaviorCategory::BOARD_TRANSPORT: return "BOARD_TRANSPORT";
        case CBehaviorCategory::STUN: return "STUN";
        case CBehaviorCategory::INVENTORY: return "INVENTORY";
        case CBehaviorCategory::ROOT: return "ROOT";
        default: return "UNKNOWN";
    }
}

static std::string JoinStrings(const std::vector<std::string>& values, const char* delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            oss << delimiter;
        }
        oss << values[i];
    }
    return oss.str();
}

static std::string FormatSelectedUnitsBrief(const std::vector<CUnit*>& units) {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto* unit : units) {
        if (!unit) {
            continue;
        }
        if (!first) {
            oss << ";";
        }
        first = false;
        oss << unit->GetHandleId() << ":" << unit->GetTypeId().ToString()
            << "@(" << unit->GetX() << "," << unit->GetY() << ")";
    }
    oss << "]";
    return oss.str();
}

static CResourceType GetUnitCarriedResourceType(const CUnit* unit) {
    if (!unit) return CResourceType::NONE;
    auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit->GetAbility(War3ID("Ahar")));
    if (!harvestAbility || !harvestAbility->IsCarryingResources()) {
        return CResourceType::NONE;
    }
    return harvestAbility->GetCarriedResourceType();
}

static CBehaviorCategory GetUnitCurrentBehaviorCategory(const CUnit* unit) {
    if (!unit) {
        return CBehaviorCategory::NONE;
    }
    auto* behavior = unit->GetCurrentBehavior();
    return behavior ? behavior->GetCategory() : CBehaviorCategory::NONE;
}

static bool IsMovementBehaviorCategory(CBehaviorCategory category) {
    switch (category) {
        case CBehaviorCategory::MOVE:
        case CBehaviorCategory::FOLLOW:
        case CBehaviorCategory::PATROL:
        case CBehaviorCategory::ATTACK_MOVE:
            return true;
        default:
            return false;
    }
}

static void ClassifyScriptedAnimationName(const std::string& lowerName,
                                          bool& outLoop,
                                          bool& outQueueStandAfterCurrent) {
    if (lowerName.find("death") != std::string::npos
        || lowerName.find("decay") != std::string::npos
        || lowerName.find("corpse") != std::string::npos
        || lowerName.find("dissipate") != std::string::npos) {
        outLoop = false;
        outQueueStandAfterCurrent = false;
        return;
    }
    if (lowerName.find("stand") != std::string::npos
        || lowerName.find("walk") != std::string::npos
        || lowerName.find("swim") != std::string::npos
        || lowerName.find("channel") != std::string::npos
        || lowerName.find("work") != std::string::npos) {
        outLoop = true;
        outQueueStandAfterCurrent = false;
        return;
    }
    outLoop = false;
    outQueueStandAfterCurrent = true;
}

static bool GetRequestedAnimationLowerName(const MdxViewer* viewer,
                                           const decltype(g.simUnitPendingAnimations)::mapped_type& request,
                                           std::string& outLowerName) {
    outLowerName.clear();
    if (request.useSequenceIndex) {
        std::string sequenceName;
        if (!viewer || !viewer->GetSequenceName(request.sequenceIndex, sequenceName)) {
            return false;
        }
        outLowerName = sequenceName;
    }
    else if (!request.sequenceName.empty()) {
        outLowerName = request.sequenceName;
    }
    else {
        return false;
    }
    std::transform(outLowerName.begin(), outLowerName.end(), outLowerName.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return true;
}

static std::vector<std::string> ExtractAnimationSecondaryTags(const std::string& properties) {
    std::vector<std::string> tags;
    std::string lower = properties;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    auto maybeAdd = [&](const char* tag) {
        if (lower.find(tag) != std::string::npos
            && std::find(tags.begin(), tags.end(), tag) == tags.end()) {
            tags.emplace_back(tag);
        }
    };
    maybeAdd("work");
    maybeAdd("gold");
    maybeAdd("lumber");
    maybeAdd("alternate");
    maybeAdd("swim");
    maybeAdd("fast");
    maybeAdd("defend");
    maybeAdd("spell");
    maybeAdd("channel");
    maybeAdd("throw");
    return tags;
}

static void AppendUniqueAnimationTag(std::vector<std::string>& tags, std::string tag) {
    if (tag.empty()) {
        return;
    }
    std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
        tags.push_back(std::move(tag));
    }
}

static bool IsUnitWorkingFromPassiveState(const CUnit* unit) {
    if (!unit) {
        return false;
    }
    if (const auto* queueAbility = dynamic_cast<const CAbilityQueue*>(unit->GetAbility(War3ID("Aque")))) {
        if (queueAbility->GetQueueCount() > 0) {
            return true;
        }
    }
    if (const auto* goldMineAbility = dynamic_cast<const CAbilityGoldMine*>(unit->GetAbility(War3ID("Agld")))) {
        if (goldMineAbility->GetActiveMinerCount() > 0) {
            return true;
        }
    }
    return false;
}

static std::vector<std::string> CollectUnitAnimationPropertyTags(const CUnit* unit) {
    std::vector<std::string> tags;
    if (!unit) {
        return tags;
    }

    if (const auto* unitType = unit->GetUnitType()) {
        for (const std::string& tag : unitType->GetRequiredAnimationTags()) {
            AppendUniqueAnimationTag(tags, tag);
        }
    }

    const auto scriptIt = g.simUnitAnimationPropertyTags.find(unit->GetHandleId());
    if (scriptIt != g.simUnitAnimationPropertyTags.end()) {
        for (const std::string& tag : scriptIt->second) {
            AppendUniqueAnimationTag(tags, tag);
        }
    }

    if (IsUnitWorkingFromPassiveState(unit)) {
        AppendUniqueAnimationTag(tags, "work");
    }

    return tags;
}

static std::vector<std::string> BuildContextualAnimationTags(const CUnit* unit,
                                                             CResourceType carriedType,
                                                             std::initializer_list<const char*> extraTags = {}) {
    std::vector<std::string> tags = CollectUnitAnimationPropertyTags(unit);
    for (const char* extraTag : extraTags) {
        if (extraTag && extraTag[0]) {
            AppendUniqueAnimationTag(tags, extraTag);
        }
    }
    switch (carriedType) {
        case CResourceType::GOLD:
            AppendUniqueAnimationTag(tags, "gold");
            break;
        case CResourceType::LUMBER:
            AppendUniqueAnimationTag(tags, "lumber");
            break;
        default:
            break;
    }
    return tags;
}

static void PlayReadyVisualForUnit(CUnit* unit,
                                   MdxViewer* viewer,
                                   CBehaviorCategory behaviorCategory,
                                   CResourceType carriedType) {
    if (!viewer) {
        return;
    }

    auto tags = BuildContextualAnimationTags(unit, carriedType);
    const bool preferReady = behaviorCategory == CBehaviorCategory::ATTACK
        || behaviorCategory == CBehaviorCategory::ATTACK_MOVE
        || behaviorCategory == CBehaviorCategory::HOLD_POSITION;
    if (preferReady) {
        AppendUniqueAnimationTag(tags, "ready");
    }
    if (viewer->PlayStandSequenceWithTags(tags)) {
        return;
    }
    if (preferReady) {
        auto readyOnlyTags = BuildContextualAnimationTags(unit, CResourceType::NONE, {"ready"});
        if (viewer->PlayStandSequenceWithTags(readyOnlyTags)) {
            return;
        }
    }
    viewer->PlayStandSequence();
}

static bool PlayWalkVisualForUnit(CUnit* unit,
                                  MdxViewer* viewer,
                                  CBehaviorCategory behaviorCategory,
                                  CResourceType carriedType) {
    (void)behaviorCategory;
    if (!unit || !viewer) {
        return false;
    }
    const float movementSpeed = unit->GetSpeed();
    auto tags = BuildContextualAnimationTags(unit, carriedType);

    float animationMoveSpeed = 0.0f;
    const float animationScale = std::max(0.001f, viewer->GetAnimationSpeedScale());
    if (const auto* meta = GetUnitRenderSplatMeta(*unit)) {
        const float walkSpeed = meta->animationWalkSpeed;
        const float runSpeed = meta->animationRunSpeed;
        if (walkSpeed > 0.0f && runSpeed > walkSpeed) {
            const float midpoint = (walkSpeed + runSpeed) * 0.5f;
            if (movementSpeed >= midpoint) {
                AppendUniqueAnimationTag(tags, "fast");
                animationMoveSpeed = runSpeed;
            }
            else {
                animationMoveSpeed = walkSpeed;
            }
        }
        else if (walkSpeed > 0.0f) {
            animationMoveSpeed = walkSpeed;
        }
    }
    if (animationMoveSpeed > 0.0f) {
        animationMoveSpeed *= animationScale;
    }

    if (animationMoveSpeed > 0.0f && movementSpeed > 0.0f) {
        const float speedRatio = std::clamp(movementSpeed / animationMoveSpeed, 0.1f, 4.0f);
        if (viewer->PlayWalkSequenceWithTagsAtSpeedRatio(tags, speedRatio)) {
            return true;
        }
        return viewer->PlayWalkSequenceAtSpeedRatio(speedRatio);
    }

    if (viewer->PlayWalkSequenceWithTagsAtSpeed(tags, movementSpeed)) {
        return true;
    }
    return viewer->PlayWalkSequenceAtSpeed(movementSpeed);
}

static bool PlayWalkWorkVisualForUnit(CUnit* unit,
                                      MdxViewer* viewer,
                                      CBehaviorCategory behaviorCategory,
                                      CResourceType carriedType) {
    if (!unit || !viewer) {
        return false;
    }

    const float movementSpeed = unit->GetSpeed();
    auto workWalkTags = BuildContextualAnimationTags(unit, carriedType, {"work"});

    float animationMoveSpeed = 0.0f;
    const float animationScale = std::max(0.001f, viewer->GetAnimationSpeedScale());
    if (const auto* meta = GetUnitRenderSplatMeta(*unit)) {
        const float walkSpeed = meta->animationWalkSpeed;
        const float runSpeed = meta->animationRunSpeed;
        if (walkSpeed > 0.0f && runSpeed > walkSpeed) {
            const float midpoint = (walkSpeed + runSpeed) * 0.5f;
            if (movementSpeed >= midpoint) {
                AppendUniqueAnimationTag(workWalkTags, "fast");
                animationMoveSpeed = runSpeed;
            }
            else {
                animationMoveSpeed = walkSpeed;
            }
        }
        else if (walkSpeed > 0.0f) {
            animationMoveSpeed = walkSpeed;
        }
    }
    if (animationMoveSpeed > 0.0f) {
        animationMoveSpeed *= animationScale;
    }

    if (animationMoveSpeed > 0.0f && movementSpeed > 0.0f) {
        const float speedRatio = std::clamp(movementSpeed / animationMoveSpeed, 0.1f, 4.0f);
        if (viewer->PlayWalkSequenceWithTagsAtSpeedRatio(workWalkTags, speedRatio)) {
            return true;
        }
    }
    else if (viewer->PlayWalkSequenceWithTagsAtSpeed(workWalkTags, movementSpeed)) {
        return true;
    }

    return PlayWalkVisualForUnit(unit, viewer, behaviorCategory, carriedType);
}

static bool PlayWorkVisualForUnit(CUnit* unit,
                                  MdxViewer* viewer,
                                  CBehaviorCategory behaviorCategory,
                                  CResourceType carriedType) {
    if (!viewer) {
        return false;
    }

    auto workTags = BuildContextualAnimationTags(unit, carriedType, {"work"});
    if (viewer->PlayStandSequenceWithTags(workTags)) {
        return true;
    }

    auto workOnlyTags = BuildContextualAnimationTags(unit, CResourceType::NONE, {"work"});
    if (viewer->PlayStandSequenceWithTags(workOnlyTags)) {
        return true;
    }

    PlayReadyVisualForUnit(unit, viewer, behaviorCategory, carriedType);
    return true;
}

static const CBehaviorSpell* GetSpellBehavior(const CUnit* unit);

static bool PlayDeathVisualForUnit(CUnit* unit,
                                   MdxViewer* viewer,
                                   decltype(g.simUnitVisualAnimationModes)::mapped_type mode) {
    if (!unit || !viewer) {
        return false;
    }
    switch (mode) {
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::DEAD:
            return viewer->PlayDeathSequence();
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::CORPSE:
            return viewer->PlayDecaySequenceWithTag("flesh", g.simulation.GetGameplayConstants().decayTime);
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::BONE_CORPSE:
            return viewer->PlayDecaySequenceWithTag("bone", unit->GetEndingDecayTime(g.simulation));
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::DISSIPATE:
            return viewer->PlayDissipateSequence(unit->GetEndingDecayTime(g.simulation));
        default:
            return false;
    }
}

static bool PlaySpellVisualForUnit(CUnit* unit, MdxViewer* viewer) {
    if (!unit || !viewer) {
        return false;
    }
    const auto* spellBehavior = GetSpellBehavior(unit);
    if (!spellBehavior) {
        return false;
    }
    const float duration = spellBehavior->GetSpellAnimationDuration();
    if (spellBehavior->IsCasting()) {
        auto spellTags = BuildContextualAnimationTags(unit, CResourceType::NONE, {"spell"});
        if (viewer->PlaySpellSequenceWithTags(spellTags, duration)) {
            return true;
        }
        return viewer->PlaySpellSequence(duration);
    }
    if (spellBehavior->IsBackswing()) {
        auto channelTags = BuildContextualAnimationTags(unit, CResourceType::NONE, {"channel"});
        if (viewer->PlayChannelSequenceWithTags(channelTags, duration)) {
            return true;
        }
        if (viewer->PlayChannelSequence(duration)) {
            return true;
        }
        auto spellTags = BuildContextualAnimationTags(unit, CResourceType::NONE, {"spell"});
        if (viewer->PlaySpellSequenceWithTags(spellTags, duration)) {
            return true;
        }
        return viewer->PlaySpellSequence(duration);
    }
    return false;
}

static const CBehaviorHarvest* GetHarvestBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorHarvest*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorAttack* GetAttackBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorAttack*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorMove* GetMoveBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorMove*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorFollow* GetFollowBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorFollow*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorPatrol* GetPatrolBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorPatrol*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorAttackMove* GetAttackMoveBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorAttackMove*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorReturnResources* GetReturnResourcesBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorReturnResources*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorBuild* GetBuildBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorBuild*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorRepair* GetRepairBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorRepair*>(unit->GetCurrentBehavior()) : nullptr;
}

static const CBehaviorSpell* GetSpellBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorSpell*>(unit->GetCurrentBehavior()) : nullptr;
}

static bool ApplyAnimationRequestToViewer(int handleId,
                                          MdxViewer* viewer,
                                          const decltype(g.simUnitPendingAnimations)::mapped_type& request) {
    if (!viewer || !viewer->GetAnimator()) {
        return false;
    }

    bool loop = false;
    bool queueStandAfterCurrent = true;
    std::string lowerName;
    if (GetRequestedAnimationLowerName(viewer, request, lowerName)) {
        ClassifyScriptedAnimationName(lowerName, loop, queueStandAfterCurrent);
    }

    bool played = false;
    if (request.useSequenceIndex) {
        played = viewer->PlaySequenceByIndex(request.sequenceIndex, loop, 1.0f, queueStandAfterCurrent);
    }
    else if (!request.sequenceName.empty()) {
        played = viewer->PlaySequenceByName(request.sequenceName.c_str(), loop, 1.0f, queueStandAfterCurrent);
    }

    if (played) {
        g.simUnitScriptedAnimations[handleId] = {request, loop, queueStandAfterCurrent};
    }
    return played;
}

static void ApplyPendingUnitAnimation(int handleId, MdxViewer* viewer) {
    if (!viewer || !viewer->GetAnimator()) return;

    const auto pendingIt = g.simUnitPendingAnimations.find(handleId);
    if (pendingIt == g.simUnitPendingAnimations.end()) return;

    ApplyAnimationRequestToViewer(handleId, viewer, pendingIt->second);
    g.simUnitPendingAnimations.erase(pendingIt);
}

static bool ComputeUnitVisualIsMoving(CUnit* unit) {
    if (!unit) {
        return false;
    }
    if (const auto* moveBehavior = GetMoveBehavior(unit)) {
        return moveBehavior->IsPropelling();
    }
    if (const auto* followBehavior = GetFollowBehavior(unit)) {
        return followBehavior->IsPropelling();
    }
    if (const auto* patrolBehavior = GetPatrolBehavior(unit)) {
        return patrolBehavior->IsPropelling();
    }
    if (const auto* attackMoveBehavior = GetAttackMoveBehavior(unit)) {
        return attackMoveBehavior->IsPropelling();
    }
    if (const auto* harvestBehavior = GetHarvestBehavior(unit)) {
        return harvestBehavior->IsPropelling();
    }
    if (const auto* returnBehavior = GetReturnResourcesBehavior(unit)) {
        return returnBehavior->IsPropelling();
    }
    if (const auto* buildBehavior = GetBuildBehavior(unit)) {
        return buildBehavior->IsPropelling();
    }
    if (const auto* repairBehavior = GetRepairBehavior(unit)) {
        return repairBehavior->IsPropelling();
    }
    if (const auto* spellBehavior = GetSpellBehavior(unit)) {
        return spellBehavior->IsPropelling();
    }
    if (const auto* attackBehavior = GetAttackBehavior(unit)) {
        return attackBehavior->IsPropelling();
    }

    const int handleId = unit->GetHandleId();
    const std::pair<float, float> currentPos{unit->GetX(), unit->GetY()};
    const auto lastPosIt = g.simUnitVisualLastPositions.find(handleId);
    const bool hadLastPos = lastPosIt != g.simUnitVisualLastPositions.end();
    bool isMoving = false;

    if (hadLastPos) {
        const float dx = currentPos.first - lastPosIt->second.first;
        const float dy = currentPos.second - lastPosIt->second.second;
        isMoving = (dx * dx + dy * dy) > 0.01f;
    }
    if (!isMoving) {
        isMoving = IsMovementBehaviorCategory(GetUnitCurrentBehaviorCategory(unit));
    }
    return isMoving;
}

static const char* UnitVisualAnimationModeName(decltype(g.simUnitVisualAnimationModes)::mapped_type mode) {
    switch (mode) {
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::NONE: return "NONE";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::READY: return "READY";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_GOLD: return "READY_GOLD";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_LUMBER: return "READY_LUMBER";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK: return "WALK";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_GOLD: return "WALK_GOLD";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_LUMBER: return "WALK_LUMBER";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_WORK: return "WALK_WORK";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WORK: return "WORK";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::SPELL: return "SPELL";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::ATTACK: return "ATTACK";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::CONSTRUCT: return "CONSTRUCT";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::DEAD: return "DEAD";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::CORPSE: return "CORPSE";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::BONE_CORPSE: return "BONE_CORPSE";
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::DISSIPATE: return "DISSIPATE";
        default: return "UNKNOWN";
    }
}

static bool ShouldHoldScriptedAnimation(CUnit* unit,
                                        MdxViewer* viewer,
                                        bool isMoving,
                                        CBehaviorCategory behaviorCategory) {
    if (!unit || !viewer) {
        return false;
    }
    const int handleId = unit->GetHandleId();
    const auto it = g.simUnitScriptedAnimations.find(handleId);
    if (it == g.simUnitScriptedAnimations.end()) {
        return false;
    }

    if (unit->IsDead() || unit->IsConstructing() || unit->IsHidden()) {
        g.simUnitScriptedAnimations.erase(it);
        g.simUnitQueuedAnimations.erase(handleId);
        return false;
    }
    if (it->second.loop && (isMoving || behaviorCategory == CBehaviorCategory::ATTACK
        || behaviorCategory == CBehaviorCategory::BUILD
        || behaviorCategory == CBehaviorCategory::HARVEST
        || behaviorCategory == CBehaviorCategory::RETURN_RESOURCES)) {
        g.simUnitScriptedAnimations.erase(it);
        return false;
    }
    if (it->second.loop) {
        return true;
    }
    if (!viewer->IsCurrentSequenceFinished()) {
        return true;
    }
    g.simUnitScriptedAnimations.erase(it);
    const auto queuedIt = g.simUnitQueuedAnimations.find(handleId);
    if (queuedIt != g.simUnitQueuedAnimations.end()) {
        const auto queuedRequest = queuedIt->second;
        g.simUnitQueuedAnimations.erase(queuedIt);
        if (ApplyAnimationRequestToViewer(handleId, viewer, queuedRequest)) {
            return true;
        }
    }
    return false;
}

static decltype(g.simUnitVisualAnimationModes)::mapped_type ResolveUnitVisualAnimationMode(
    CUnit* unit,
    bool isMoving,
    CBehaviorCategory behaviorCategory,
    CResourceType carriedType) {
    if (!unit) {
        return decltype(g.simUnitVisualAnimationModes)::mapped_type::NONE;
    }
    if (unit->IsDead()) {
        if (!unit->IsCorpse()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::DEAD;
        }
        if (unit->IsHero() && unit->IsBoneCorpse()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::DISSIPATE;
        }
        if (unit->IsBoneCorpse()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::BONE_CORPSE;
        }
        return decltype(g.simUnitVisualAnimationModes)::mapped_type::CORPSE;
    }
    if (unit->IsConstructing()) {
        return decltype(g.simUnitVisualAnimationModes)::mapped_type::CONSTRUCT;
    }
    if (unit->IsBuilding()) {
        return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
    }

    if (const auto* moveBehavior = GetMoveBehavior(unit)) {
        if (!moveBehavior->IsPropelling()) {
            switch (carriedType) {
                case CResourceType::GOLD:
                    return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_GOLD;
                case CResourceType::LUMBER:
                    return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_LUMBER;
                default:
                    return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
            }
        }
    }

    if (const auto* buildBehavior = GetBuildBehavior(unit)) {
        if (buildBehavior->IsMovingToSite()) {
            return buildBehavior->IsPropelling()
                ? decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_WORK
                : decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
        }
        if (buildBehavior->IsActivelyBuilding()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::WORK;
        }
    }

    if (const auto* repairBehavior = GetRepairBehavior(unit)) {
        if (repairBehavior->IsMovingToTarget()) {
            return repairBehavior->IsPropelling()
                ? decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_WORK
                : decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
        }
        if (repairBehavior->IsRepairing()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::WORK;
        }
    }

    if (const auto* harvestBehavior = GetHarvestBehavior(unit)) {
        if (harvestBehavior->IsMovingToResource()) {
            return harvestBehavior->IsPropelling()
                ? decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_WORK
                : decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
        }
        if (harvestBehavior->IsGathering()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::WORK;
        }
    }

    if (const auto* returnBehavior = GetReturnResourcesBehavior(unit)) {
        if (returnBehavior->IsMovingToDropoff()) {
            if (returnBehavior->IsPropelling()) {
                if (carriedType == CResourceType::GOLD) {
                    return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_GOLD;
                }
                if (carriedType == CResourceType::LUMBER) {
                    return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_LUMBER;
                }
                return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK;
            }
            if (carriedType == CResourceType::GOLD) {
                return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_GOLD;
            }
            if (carriedType == CResourceType::LUMBER) {
                return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_LUMBER;
            }
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
        }
        if (returnBehavior->IsDepositing()) {
            if (carriedType == CResourceType::GOLD) {
                return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_GOLD;
            }
            if (carriedType == CResourceType::LUMBER) {
                return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_LUMBER;
            }
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
        }
    }

    if (const auto* spellBehavior = GetSpellBehavior(unit)) {
        if (spellBehavior->IsMovingToRange()) {
            return spellBehavior->IsPropelling()
                ? decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK
                : decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
        }
        if (spellBehavior->IsFacingTarget()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
        }
        if (spellBehavior->IsCasting() || spellBehavior->IsBackswing()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::SPELL;
        }
    }

    if (const auto* attackBehavior = GetAttackBehavior(unit)) {
        if (attackBehavior->IsPropelling()) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK;
        }
        if (attackBehavior->IsAttackAnimationActive(g.simulation.GetGameTurnTick())) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::ATTACK;
        }
        return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
    }

    switch (behaviorCategory) {
        case CBehaviorCategory::RETURN_RESOURCES:
            if (isMoving) {
                if (carriedType == CResourceType::GOLD) {
                    return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_GOLD;
                }
                if (carriedType == CResourceType::LUMBER) {
                    return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_LUMBER;
                }
                return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK;
            }
            break;
        default:
            break;
    }

    if (isMoving) {
        if (carriedType == CResourceType::GOLD) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_GOLD;
        }
        if (carriedType == CResourceType::LUMBER) {
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_LUMBER;
        }
        return decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK;
    }

    switch (carriedType) {
        case CResourceType::GOLD:
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_GOLD;
        case CResourceType::LUMBER:
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_LUMBER;
        default:
            return decltype(g.simUnitVisualAnimationModes)::mapped_type::READY;
    }
}

static void PlayResolvedUnitAnimation(CUnit* unit,
                                      MdxViewer* viewer,
                                      decltype(g.simUnitVisualAnimationModes)::mapped_type mode,
                                      CBehaviorCategory behaviorCategory,
                                      CResourceType carriedType) {
    if (!unit || !viewer) {
        return;
    }
    const int handleId = unit->GetHandleId();
    switch (mode) {
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::READY:
            PlayReadyVisualForUnit(unit, viewer, behaviorCategory, CResourceType::NONE);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_GOLD:
            PlayReadyVisualForUnit(unit, viewer, behaviorCategory, CResourceType::GOLD);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::READY_LUMBER:
            PlayReadyVisualForUnit(unit, viewer, behaviorCategory, CResourceType::LUMBER);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK:
            PlayWalkVisualForUnit(unit, viewer, behaviorCategory, CResourceType::NONE);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_GOLD:
            PlayWalkVisualForUnit(unit, viewer, behaviorCategory, CResourceType::GOLD);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_LUMBER:
            PlayWalkVisualForUnit(unit, viewer, behaviorCategory, CResourceType::LUMBER);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WALK_WORK:
            PlayWalkWorkVisualForUnit(unit, viewer, behaviorCategory, CResourceType::NONE);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::WORK:
            PlayWorkVisualForUnit(unit, viewer, behaviorCategory, carriedType);
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::SPELL:
            if (!PlaySpellVisualForUnit(unit, viewer)) {
                PlayReadyVisualForUnit(unit, viewer, behaviorCategory, carriedType);
            }
            break;
        case decltype(g.simUnitVisualAnimationModes)::mapped_type::ATTACK: {
            const int currentTick = g.simulation.GetGameTurnTick();
            auto* attackBehavior = dynamic_cast<CBehaviorAttack*>(unit->GetCurrentBehavior());
            const int behaviorAttackStartTick = attackBehavior ? attackBehavior->GetAttackAnimationStartTick() : -1;
            const float behaviorAttackDuration = attackBehavior ? attackBehavior->GetAttackAnimationDuration() : 0.0f;
            const auto lastAttackIt = g.simUnitVisualLastAttackAnimationTicks.find(handleId);
            const int lastAttackTick = (lastAttackIt != g.simUnitVisualLastAttackAnimationTicks.end())
                ? lastAttackIt->second
                : std::numeric_limits<int>::min();
            const bool shouldStartAttackAnimation = attackBehavior
                && attackBehavior->IsAttackAnimationActive(currentTick)
                && behaviorAttackStartTick >= 0
                && behaviorAttackStartTick != lastAttackTick;
            if (shouldStartAttackAnimation) {
                const auto attackTags = BuildContextualAnimationTags(unit, CResourceType::NONE);
                if (viewer->PlayAttackSequenceWithTagsAndDuration(attackTags, behaviorAttackDuration)) {
                    g.simUnitVisualLastAttackAnimationTicks[handleId] = behaviorAttackStartTick;
                }
                else {
                    PlayReadyVisualForUnit(unit, viewer, behaviorCategory, carriedType);
                }
            }
            else if (!attackBehavior || !attackBehavior->IsAttackAnimationActive(currentTick)) {
                if (!viewer->IsPlayingSequenceWithToken("stand") || viewer->IsCurrentSequenceFinished()) {
                    PlayReadyVisualForUnit(unit, viewer, CBehaviorCategory::ATTACK, carriedType);
                }
            }
            break;
        }
        default:
            break;
    }
}

static void SyncSimulationUnitAnimation(CUnit* unit, MdxViewer* viewer) {
    if (!unit || !viewer) return;

    const int handleId = unit->GetHandleId();
    const bool isMoving = ComputeUnitVisualIsMoving(unit);
    const CBehaviorCategory behaviorCategory = GetUnitCurrentBehaviorCategory(unit);
    const CResourceType carriedType = GetUnitCarriedResourceType(unit);
    const std::vector<std::string> currentPropertyTags = CollectUnitAnimationPropertyTags(unit);
    const auto lastModeIt = g.simUnitVisualAnimationModes.find(handleId);
    const auto lastMode = (lastModeIt != g.simUnitVisualAnimationModes.end())
        ? lastModeIt->second
        : decltype(g.simUnitVisualAnimationModes)::mapped_type::NONE;
    const auto lastPropertyTagsIt = g.simUnitVisualPropertyTags.find(handleId);
    const bool propertyTagsChanged = (lastPropertyTagsIt == g.simUnitVisualPropertyTags.end())
        || (lastPropertyTagsIt->second != currentPropertyTags);

    if (ShouldHoldScriptedAnimation(unit, viewer, isMoving, behaviorCategory)) {
        g.simUnitVisualLastPositions[handleId] = {unit->GetX(), unit->GetY()};
        g.simUnitVisualMovingStates[handleId] = isMoving;
        g.simUnitVisualPropertyTags[handleId] = currentPropertyTags;
        return;
    }

    if (unit->IsConstructing()) {
        if (lastMode != decltype(g.simUnitVisualAnimationModes)::mapped_type::CONSTRUCT) {
            viewer->PlayBirthThenStandSequence();
        }

        float buildRatio = 0.0f;
        if (auto* unitType = unit->GetUnitType()) {
            const float buildTime = (float)unitType->GetBuildTime();
            if (buildTime > 0.0f) {
                buildRatio = unit->GetConstructionProgress() / buildTime;
            }
        }
        viewer->SetCurrentSequenceFrameByRatio(std::clamp(buildRatio, 0.0f, 1.0f));
        g.simUnitVisualAnimationModes[handleId] = decltype(g.simUnitVisualAnimationModes)::mapped_type::CONSTRUCT;
    }
    else if (!unit->IsDead()) {
        const auto resolvedMode = ResolveUnitVisualAnimationMode(unit, isMoving, behaviorCategory, carriedType);
        const bool shouldRefreshAttack = (resolvedMode == decltype(g.simUnitVisualAnimationModes)::mapped_type::ATTACK)
            && viewer->IsCurrentSequenceFinished();
        if (resolvedMode != lastMode || propertyTagsChanged) {
            std::ostringstream oss;
            oss << "[MoveDebug] anim"
                << " unit=" << handleId
                << " type=" << unit->GetTypeId().ToString()
                << " behavior=" << BehaviorCategoryName(behaviorCategory)
                << " isMoving=" << (isMoving ? 1 : 0)
                << " from=" << UnitVisualAnimationModeName(lastMode)
                << " to=" << UnitVisualAnimationModeName(resolvedMode)
                << " tagCount=" << currentPropertyTags.size();
            if (!currentPropertyTags.empty()) {
                oss << " tags=[" << JoinStrings(currentPropertyTags, ",") << "]";
            }
            MoveDebugLogLine(oss.str());
        }
        if (resolvedMode != lastMode || shouldRefreshAttack || propertyTagsChanged) {
            PlayResolvedUnitAnimation(unit, viewer, resolvedMode, behaviorCategory, carriedType);
        }
        g.simUnitVisualAnimationModes[handleId] = resolvedMode;
    }
    else {
        const auto resolvedMode = ResolveUnitVisualAnimationMode(unit, isMoving, behaviorCategory, carriedType);
        if (resolvedMode != lastMode) {
            PlayDeathVisualForUnit(unit, viewer, resolvedMode);
        }
        g.simUnitVisualAnimationModes[handleId] = resolvedMode;
    }

    g.simUnitVisualConstructionStates[handleId] = unit->IsConstructing();
    g.simUnitVisualResourceTags[handleId] = carriedType;
    g.simUnitVisualPropertyTags[handleId] = currentPropertyTags;
    g.simUnitVisualLastPositions[handleId] = {unit->GetX(), unit->GetY()};
    g.simUnitVisualMovingStates[handleId] = isMoving;
    if (g.simUnitVisualAnimationModes[handleId] != decltype(g.simUnitVisualAnimationModes)::mapped_type::ATTACK) {
        g.simUnitVisualLastAttackAnimationTicks.erase(handleId);
    }
}

static void SetSimulationUnitVisualVisible(int handleId, bool visible) {
    const auto visualIt = g.simUnitToVisualIdx.find(handleId);
    if (visualIt == g.simUnitToVisualIdx.end()) return;
    g.scene.SetInstanceVisible(visualIt->second, visible);
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
    if (MdxViewer* viewer = g.scene.GetViewer(visIdx)) {
        ApplyPendingUnitAnimation(handleId, viewer);
        SyncSimulationUnitAnimation(unit, viewer);
    }
    std::cout << "SceneUnitSync: registered on-demand visual for sim unit "
              << DescribeUnitType(unit->GetTypeId())
              << " handle=" << handleId << std::endl;
    return true;
}

static void SyncSimulationUnitVisual(CUnit* unit) {
    if (!unit) return;

    const int handleId = unit->GetHandleId();
    const auto existingIt = g.simUnitToVisualIdx.find(handleId);
    if (unit->IsHidden()) {
        if (existingIt != g.simUnitToVisualIdx.end()) {
            SetSimulationUnitVisualVisible(handleId, false);
        }
        return;
    }

    if (!EnsureSimulationUnitVisual(unit)) return;

    auto it = g.simUnitToVisualIdx.find(handleId);
    if (it == g.simUnitToVisualIdx.end()) return;

    float pos[3] = {
        unit->GetX(),
        unit->GetY(),
        ComputeUnitVisualHeight(*unit)
    };
    SetSimulationUnitVisualVisible(handleId, true);
    g.scene.UpdateInstancePosition(it->second, pos, unit->GetFacing(),
                                   unit->GetSpeed(), unit->GetTurnRate(), true);
    if (MdxViewer* viewer = g.scene.GetViewer(it->second)) {
        SyncSimulationUnitAnimation(unit, viewer);
    }
}

static void SyncAllSimulationUnitVisuals() {
    for (auto* unit : g.simulation.GetUnits()) {
        SyncSimulationUnitVisual(unit);
    }
}

static void ReconcileSimulationUnitVisuals() {
    std::vector<int> removedHandles;
    removedHandles.reserve(g.simUnitToVisualIdx.size());
    for (const auto& [handleId, visualIndex] : g.simUnitToVisualIdx) {
        CUnit* unit = g.simulation.GetUnit(handleId);
        if (!unit) {
            g.scene.SetInstanceVisible(visualIndex, false);
            removedHandles.push_back(handleId);
            continue;
        }
        SetSimulationUnitVisualVisible(handleId, !unit->IsHidden());
    }

    for (int handleId : removedHandles) {
        g.simUnitToVisualIdx.erase(handleId);
        g.simUnitVisualSeeds.erase(handleId);
        g.simUnitPendingAnimations.erase(handleId);
        g.simUnitVisualLastPositions.erase(handleId);
        g.simUnitVisualMovingStates.erase(handleId);
        g.simUnitVisualAnimationModes.erase(handleId);
        g.simUnitVisualLastAttackAnimationTicks.erase(handleId);
        g.simUnitVisualResourceTags.erase(handleId);
        g.simUnitVisualConstructionStates.erase(handleId);
        g.simUnitVisualPropertyTags.erase(handleId);
        g.simUnitScriptedAnimations.erase(handleId);
        g.simUnitQueuedAnimations.erase(handleId);
        g.simUnitAnimationPropertyTags.erase(handleId);
        g.simUnitVisualRegistrationFailed.erase(handleId);
        g.selectedUnitHandleSet.erase(handleId);
        g.selectedUnitHandles.erase(
            std::remove(g.selectedUnitHandles.begin(), g.selectedUnitHandles.end(), handleId),
            g.selectedUnitHandles.end());
        if (g.selectedUnitHandle == handleId) {
            g.selectedUnitHandle = -1;
        }
    }
}

static void FlushStartupSimulationState() {
    if (!g.simulationInitialized) return;

    // Make editor and JASS-created units visible before the first render frame.
    SyncAllSimulationUnitVisuals();
    ReconcileSimulationUnitVisuals();

    if (!g.jassNatives) return;

    constexpr int maxStartupWarmupTicks = 4;
    for (int i = 0; i < maxStartupWarmupTicks; ++i) {
        const size_t unitCountBefore = g.simulation.GetUnits().size();
        const int firedTimers = g.jassNatives->UpdateTimers(CSimulation::SIM_STEP_TIME);
        const int pumpedThreads = g.jassNatives->PumpQueuedThreads(1024);
        g.simulation.Update();
        SyncAllSimulationUnitVisuals();
        ReconcileSimulationUnitVisuals();
        if (firedTimers == 0 && pumpedThreads == 0 && g.simulation.GetUnits().size() == unitCountBefore) {
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
    std::vector<CUnit*> buildingFallback;
    if (!rect.IsValid()) return result;

    for (auto* unit : g.simulation.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;

        ScreenRect unitRect;
        if (!ComputeUnitSelectionScreenBounds(*unit, view, proj, screenW, screenH, unitRect)) {
            continue;
        }
        if (RectanglesOverlap(rect, unitRect)) {
            if (unit->IsBuilding()) {
                buildingFallback.push_back(unit);
            } else {
                result.push_back(unit);
            }
        }
    }
    if (result.empty()) {
        return buildingFallback;
    }
    return result;
}

static std::vector<CUnit*> GetSelectedSimulationUnits() {
    std::vector<CUnit*> units;
    units.reserve(g.selectedUnitHandles.size());
    for (int handleId : g.selectedUnitHandles) {
        CUnit* unit = g.simulation.GetUnit(handleId);
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
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

    (void)rebuildCommandCard;
    g.ui.SetUnitData(&g.unitData);
    g.ui.PopulateCommandCardForUnit(primaryUnit);

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
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
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

    {
        std::vector<CUnit*> selectedUnits = GetSelectedSimulationUnits();
        std::ostringstream oss;
        oss << "[MoveDebug] selection"
            << " primary=" << g.selectedUnitHandle
            << " count=" << selectedUnits.size()
            << " units=" << FormatSelectedUnitsBrief(selectedUnits);
        MoveDebugLogLine(oss.str());
    }
}

static void SelectSingleUnit(CUnit* unit, bool additive) {
    if (!unit || unit->IsDead() || unit->IsHidden()) {
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
                                                                        g.selectedUnitHandle,
                                                                        false,
                                                                        true);
                    CDestructable* targetDestructable = targetUnit ? nullptr
                        : PickSimulationDestructableAtScreenPoint(ev->mouse_x, ev->mouse_y, view, proj, winW, winH);
                    CWidget* targetWidget = targetUnit ? static_cast<CWidget*>(targetUnit)
                        : static_cast<CWidget*>(targetDestructable);

                    int pendingCmd = g.ui.GetPendingCommand();
                    if (pendingCmd >= 0) {
                        const auto pendingMode = g.ui.GetPendingCommandMode();
                        g.ui.ClearPendingCommand();
                        Vec3 wPos = ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH);
                        {
                            std::ostringstream oss;
                            oss << "[MoveDebug] issue_pending"
                                << " pendingCmd=" << pendingCmd
                                << " pendingMode=" << static_cast<int>(pendingMode)
                                << " primary=" << g.selectedUnitHandle
                                << " selected=" << FormatSelectedUnitsBrief(selectedUnits);
                            if (targetWidget) {
                                oss << " targetWidget=" << targetWidget->GetHandleId()
                                    << " targetPos=(" << targetWidget->GetX() << ", " << targetWidget->GetY() << ")";
                            } else {
                                oss << " targetPoint=(" << wPos.x << ", " << wPos.z << ")";
                            }
                            MoveDebugLogLine(oss.str());
                        }
                        for (auto* selectedUnit : selectedUnits) {
                            if (!selectedUnit) continue;
                            if (pendingMode == MainUI::PendingCommandMode::POINT_ONLY) {
                                selectedUnit->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
                            } else if (targetWidget) {
                                selectedUnit->IssueOrder(std::make_unique<COrderTargetWidget>(pendingCmd, targetWidget));
                            } else {
                                selectedUnit->IssueOrder(std::make_unique<COrderTargetPoint>(pendingCmd, wPos.x, wPos.z));
                            }
                        }
                        issuedOrder = true;
                    } else if (targetWidget) {
                        for (auto* selectedUnit : selectedUnits) {
                            if (!selectedUnit) continue;
                            int orderId = OrderIds::SMART;
                            if (targetDestructable) {
                                if (auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(selectedUnit->GetAbility(War3ID("Ahar")))) {
                                    if (harvestAbility->CheckCanTarget(g.simulation, selectedUnit, OrderIds::HARVEST, targetDestructable)) {
                                        orderId = OrderIds::HARVEST;
                                    }
                                }
                            }
                            std::ostringstream oss;
                            oss << "[MoveDebug] issue_widget"
                                << " unit=" << selectedUnit->GetHandleId()
                                << " type=" << selectedUnit->GetTypeId().ToString()
                                << " order=" << orderId
                                << " primary=" << g.selectedUnitHandle
                                << " targetWidget=" << targetWidget->GetHandleId()
                                << " targetPos=(" << targetWidget->GetX() << ", " << targetWidget->GetY() << ")"
                                << " targetKind=" << (targetUnit ? "unit" : "destructable")
                                << " selected=" << FormatSelectedUnitsBrief(selectedUnits);
                            MoveDebugLogLine(oss.str());
                            selectedUnit->IssueOrder(std::make_unique<COrderTargetWidget>(orderId, targetWidget));
                        }
                        issuedOrder = true;
                    } else {
                        Vec3 wPos = ScreenToWorld(ev->mouse_x, ev->mouse_y, view, proj, winW, winH);
                        const float groundHeight = g.texturedTerrain.IsValid()
                            ? g.texturedTerrain.GetGroundHeight(wPos.x, wPos.z)
                            : 0.0f;
                        {
                            std::ostringstream oss;
                            oss << "[MoveDebug] click_point"
                                << " screen=(" << ev->mouse_x << ", " << ev->mouse_y << ")"
                                << " world=(" << wPos.x << ", " << wPos.z << ")"
                                << " terrainH=" << groundHeight
                                << " primary=" << g.selectedUnitHandle
                                << " selectedCount=" << selectedUnits.size()
                                << " selected=" << FormatSelectedUnitsBrief(selectedUnits);
                            MoveDebugLogLine(oss.str());
                        }
                        for (auto* selectedUnit : selectedUnits) {
                            if (!selectedUnit) continue;
                            std::ostringstream oss;
                            oss << "[MoveDebug] issue_order"
                                << " unit=" << selectedUnit->GetHandleId()
                                << " type=" << selectedUnit->GetTypeId().ToString()
                                << " order=" << OrderIds::SMART
                                << " from=(" << selectedUnit->GetX() << ", " << selectedUnit->GetY() << ")"
                                << " target=(" << wPos.x << ", " << wPos.z << ")";
                            MoveDebugLogLine(oss.str());
                            selectedUnit->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, wPos.x, wPos.z));
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
                                                                      view, proj, winW, winH,
                                                                      -1,
                                                                      false,
                                                                      true);
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
        const float simStep = CSimulation::SIM_STEP_TIME;
        g.simulationAccumulator = std::min(g.simulationAccumulator + dt, simStep * 3.0f);
        bool ranSimulationStep = false;
        while (g.simulationAccumulator >= simStep) {
            if (g.jassNatives) {
                g.jassNatives->UpdateTimers(simStep);
                g.jassNatives->PumpQueuedThreads(1024);
            }
            g.simulation.Update();
            g.simulationAccumulator -= simStep;
            ranSimulationStep = true;
        }

        if (ranSimulationStep) {
            // Sync simulation positions to visual models after timer-driven spawns and orders.
            SyncAllSimulationUnitVisuals();
            ReconcileSimulationUnitVisuals();
            RefreshSelectionUi(false);
        }
    }

    int winW = sapp_width(), winH = sapp_height();
    float aspect = (winH > 0) ? (float)winW / winH : 1.0f;

    Mat4 view = g.camera.GetViewMatrix();
    Mat4 proj = g.camera.GetProjectionMatrix(aspect);
    RefreshRuntimeUnitSplats();

    Renderer::BeginFrame();
    Renderer::SetWireframe(g.wireframe);

    if (g.texturedTerrain.IsValid())
        g.texturedTerrain.Render(view, proj, g.frameCount * 0.016f);
    else if (g.simpleTerrain.IsValid())
        g.simpleTerrain.Render(view, proj);

    sg_reset_state_cache();
    sg_apply_viewport(0, 0, winW, winH, true);
    sg_apply_scissor_rect(0, 0, winW, winH, true);
    g.scene.RenderAll(view, proj, dt);

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
    {
        std::ofstream(MoveDebugLogPath(), std::ios::trunc).close();
        MoveDebugLogLine("[MoveDebug] session_begin");
    }

    if (!Renderer::Initialize()) return;

    fs::path project_root = PROJECT_SOURCE_DIR;
    fs::path resources_dir = project_root / "resources";

    // Load warsmash.ini config
    WarsmashConfig warsmashCfg;
    fs::path iniPath = project_root / "resources" / "warsmash.ini";
    warsmashCfg.Load(iniPath.string());

    // Open MPQs (config-driven or default)
    if (!warsmashCfg.dataSources.empty()) {
        for (const auto& ds : warsmashCfg.dataSources) {
            if (ds.first == "MPQ" || ds.first == "mpq") {
                fs::path p = ResolveConfiguredPath(project_root, resources_dir, ds.second);
                if (fs::exists(p)) g.mpq.Open(p.string());
                else std::cerr << "MpqStorm: configured MPQ not found " << p.string() << std::endl;
            }
        }
    }
    if (g.mpq.ArchiveCount() == 0) {
        for (const auto& name : {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"}) {
            fs::path p = resources_dir / name;
            if (fs::exists(p)) g.mpq.Open(p.string());
        }
    }

    if (g.mpq.ArchiveCount() > 0) {
        LoadSelectionAndBuildingSplatMetadata();
        g.scene.Init(g.mpq);
    }

    g.audio.Init();
    if (g.audio.IsValid() && g.mpq.ArchiveCount() > 0) {
        g.audio.SetMpqManager(&g.mpq);
        g.audio.LoadSound("ui_click", "UI\\Mouse\\HumanClick1.wav", &g.mpq);
        g.audio.LoadSound("ui_error", "UI\\Mouse\\HumanClickError.wav", &g.mpq);
    }

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
        fs::path mapFilePath = g.mapPath;
        if (mapFilePath.is_relative()) {
            fs::path resolved = resources_dir / mapFilePath;
            if (fs::exists(resolved)) mapFilePath = resolved;
        }
        g.mapPath = mapFilePath.string();
    } else if (!mapFiles.empty()) {
        fs::path defaultPath = resources_dir / DEFAULT_MAP;
        if (fs::exists(defaultPath))
            g.mapPath = defaultPath.string();
        else
            g.mapPath = mapFiles[0].string();
    }

    if (g.mapPath.empty()) {
        std::cerr << "No map found!" << std::endl;
        return;
    }

    std::cout << "\nLoading: " << fs::path(g.mapPath).filename() << std::endl;
    g.scene.ResetLoadStats();

    // --- Phase 1: JASS config() ---
    W3ILoadFromMpq(g.mapPath, g.w3i);
    W3IPrint(g.w3i);

    JassConfigEnv configEnv;
    std::shared_ptr<JassNativesData> configData;
    if (g.mpq.ArchiveCount() > 0 && configEnv.Init(g.mpq, g.mapPath, warsmashCfg.jassFileList)) {
        configEnv.Config();
        configData = configEnv.GetData();
        int playerCount = configEnv.GetConfiguredPlayerCount();
        if (playerCount > 0)
            std::cout << "JASS Config: " << playerCount << " players configured" << std::endl;
    } else {
        std::cerr << "JASS Config: unavailable; continuing with map data" << std::endl;
    }

    // --- Phase 2: Staged map loading [align with Java MapLoader] ---
    MapLoader mapLoader(g.mpq, g.simulation, g.unitData, g.destructableData,
                        g.scene, g.texturedTerrain, g.tileset, g.map, g.w3i, g.wpm);
    mapLoader.SetConfigData(configData);
    mapLoader.CreateTasks(g.mapPath, 0);

    int stage = 0;
    while (!mapLoader.Process()) {
        stage++;
        std::cout << "  Loading stage " << stage << " ("
                  << (int)(mapLoader.GetCompletionRatio() * 100.0f) << "%)" << std::endl;
    }

    // Post-loading: sync terrain/pathing state
    g.simulation.SetPathingMap(g.wpm.IsValid() ? &g.wpm : nullptr, g.map.offsetX, g.map.offsetY);
    g.terrainLoaded = g.texturedTerrain.IsValid();
    g.scene.PrintLoadStats();

    if (g.map.tileWidth == 0 || g.map.tileHeight == 0) {
        std::cerr << "Initialization failed: terrain W3E did not load" << std::endl;
        return;
    }

    const auto& unitLoadResult = mapLoader.GetUnitLoadResult();
    for (const auto& seed : mapLoader.GetUnitVisualSeeds()) {
        g.simUnitVisualSeeds[seed.handleId] = {
            {seed.scale[0], seed.scale[1], seed.scale[2]},
            seed.skinId,
            seed.hitpoints,
            seed.flags
        };
    }

    int visualUnitCreated = unitLoadResult.visualCreated;
    int visualMissForSimUnits = unitLoadResult.visualMissForSimUnits;
    g.simulation.Update();
    g.simulationInitialized = true;
    g.ui.SetSimulation(&g.simulation);
    for (auto* unit : g.simulation.GetUnits()) {
        if (EnsureSimulationUnitVisual(unit)) visualUnitCreated++;
        else visualMissForSimUnits++;
    }
    g.ui.SetUnitData(&g.unitData);
    if (!g.simulation.GetUnits().empty()) {
        g.ui.PopulateCommandCardForUnitType(g.simulation.GetUnits()[0]->GetTypeId());
    }
    std::cout << "Simulation: " << g.simulation.GetUnits().size() << " units" << std::endl;
    std::cout << "SceneUnitSync: simCreated=" << unitLoadResult.simCreated
              << " visualCreated=" << visualUnitCreated
              << " visualMissForSimUnits=" << visualMissForSimUnits << std::endl;

    // --- Phase 3: JASS common/main() [align with Java loadCommon] ---
    if (g.mpq.ArchiveCount() > 0) {
        JassCommonEnv commonEnv;
        if (commonEnv.Init(g.mpq, g.mapPath, &g.simulation, warsmashCfg.jassFileList,
                           configData)) {
                auto* jassDataPtr = commonEnv.GetData().get();
                jassDataPtr->camSetPosition = [](float x, float y, float z) {
                    g.camera.SetTarget(x, y, z); g.camera.ResetToHome(); };
                jassDataPtr->camSetTarget = [](float x, float y, float z) {
                    g.camera.SetTarget(x, y, z); g.camera.ResetToHome(); };
                jassDataPtr->camReset = []() { g.camera.ResetToHome(); };
                jassDataPtr->camGetX = []() -> float { return g.camera.GetTarget().x; };
                jassDataPtr->camGetY = []() -> float { return g.camera.GetTarget().y; };
                jassDataPtr->camGetZ = []() -> float { return g.camera.GetTarget().z; };
                jassDataPtr->camGetEyeX = []() -> float { return g.camera.GetPosition().x; };
                jassDataPtr->camGetEyeY = []() -> float { return g.camera.GetPosition().y; };
                jassDataPtr->camGetEyeZ = []() -> float { return g.camera.GetPosition().z; };

                jassDataPtr->unitSetAnimation = [](int h, const std::string& a) {
                    const decltype(g.simUnitPendingAnimations)::mapped_type r{a, -1, false};
                    g.simUnitQueuedAnimations.erase(h);
                    auto it = g.simUnitToVisualIdx.find(h);
                    if (it != g.simUnitToVisualIdx.end()) {
                        auto* v = g.scene.GetViewer(it->second);
                        if (v && ApplyAnimationRequestToViewer(h, v, r)) {
                            g.simUnitPendingAnimations.erase(h); return;
                        }
                    }
                    g.simUnitPendingAnimations[h] = r;
                };
                jassDataPtr->unitQueueAnimation = [](int h, const std::string& a) {
                    const decltype(g.simUnitPendingAnimations)::mapped_type r{a, -1, false};
                    auto it = g.simUnitToVisualIdx.find(h);
                    if (it != g.simUnitToVisualIdx.end()) {
                        auto* v = g.scene.GetViewer(it->second);
                        if (v && v->GetAnimator()) {
                            bool q = !v->IsCurrentSequenceLooping() && !v->IsCurrentSequenceFinished();
                            if (q) { g.simUnitQueuedAnimations[h] = r; }
                            else if (ApplyAnimationRequestToViewer(h, v, r)) { g.simUnitPendingAnimations.erase(h); return; }
                        }
                    } else { g.simUnitPendingAnimations[h] = r; }
                };
                jassDataPtr->unitSetAnimationByIndex = [](int h, int idx) {
                    const decltype(g.simUnitPendingAnimations)::mapped_type r{"", idx, true};
                    g.simUnitQueuedAnimations.erase(h);
                    auto it = g.simUnitToVisualIdx.find(h);
                    if (it != g.simUnitToVisualIdx.end()) {
                        auto* v = g.scene.GetViewer(it->second);
                        if (v && ApplyAnimationRequestToViewer(h, v, r)) { g.simUnitPendingAnimations.erase(h); return; }
                    }
                    g.simUnitPendingAnimations[h] = r;
                };
                jassDataPtr->unitAddAnimationProperties = [](int h, const std::string& p, bool add) {
                    auto tags = ExtractAnimationSecondaryTags(p);
                    if (tags.empty()) return;
                    auto& at = g.simUnitAnimationPropertyTags[h];
                    if (add) {
                        for (auto& t : tags) if (std::find(at.begin(), at.end(), t) == at.end()) at.push_back(t);
                    } else {
                        at.erase(std::remove_if(at.begin(), at.end(), [&](const std::string& t) {
                            return std::find(tags.begin(), tags.end(), t) != tags.end(); }), at.end());
                        if (at.empty()) g.simUnitAnimationPropertyTags.erase(h);
                    }
                    g.simUnitVisualAnimationModes.erase(h);
                };
                jassDataPtr->unitClearAnimations = [](int h) {
                    g.simUnitPendingAnimations.erase(h); g.simUnitQueuedAnimations.erase(h);
                    g.simUnitScriptedAnimations.erase(h); g.simUnitVisualAnimationModes.erase(h);
                };

                // Death/damage/order events
                g.simulation.SetUnitDeathCallback([](int uh, int kh, int) {
                    g.simUnitPendingAnimations.erase(uh); g.simUnitScriptedAnimations.erase(uh);
                    g.simUnitQueuedAnimations.erase(uh);
                    g.simUnitVisualAnimationModes[uh] = decltype(g.simUnitVisualAnimationModes)::mapped_type::DEAD;
                    auto it = g.simUnitToVisualIdx.find(uh);
                    if (it != g.simUnitToVisualIdx.end()) {
                        SetSimulationUnitVisualVisible(uh, true);
                        if (auto* v = g.scene.GetViewer(it->second)) v->PlayDeathSequence();
                    }
                    if (g.jassNatives) g.jassNatives->FireDeathEvent(uh, kh, JASS_EVENT_UNIT_DEATH);
                });
                g.simulation.SetUnitDamageCallback([](int uh, int sh, float d) {
                    if (g.jassNatives) g.jassNatives->FireDamageEvent(uh, sh, d, JASS_EVENT_UNIT_DAMAGED);
                });
                g.simulation.SetUnitOrderCallback([](int uh, int oi, int th, float x, float y) {
                    if (g.jassNatives) g.jassNatives->FireOrderEvent(uh, oi, th, x, y);
                });

                commonEnv.Main();
                g.jassNatives = commonEnv.ReleaseNatives();
                if (g.jassNatives) g.jassNatives->PumpQueuedThreads(4096);
                g.simulation.Update();
                EnsureHumanMeleeStartsFromMapConfig();
                FlushStartupSimulationState();
        } else {
            std::cerr << "JASS Common: unavailable; terrain/static scene remains ready" << std::endl;
        }
    } else {
        std::cout << "JASS Common: skipped because no resource MPQs are open" << std::endl;
    }

    // Generate minimap
    if (g.terrainLoaded && g.texturedTerrain.IsValid()) {
        g.minimap.Generate(g.texturedTerrain, g.tileset, g.map, g.w3i, &g.mpq);
    }

    // Camera setup
    Vec3 center = g.texturedTerrain.IsValid() ? g.texturedTerrain.GetCenter()
                                              : g.simpleTerrain.GetCenter();
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
}

// --- sokol_main ---
sapp_desc sokol_main(int argc, char* argv[]) {
    // Store args for GameController (it handles its own parsing)
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "--screenshot") == 0 && i + 1 < argc) {
            g.screenshotPath = argv[++i];
            g.screenshotFrame = 1;
        } else if (std::strncmp(arg, "--run=", 6) == 0) {
            g.mapPath = arg + 6;
        } else if (std::strcmp(arg, "--run") == 0 && i + 1 < argc) {
            g.mapPath = argv[++i];
        } else if (std::strncmp(arg, "--map=", 6) == 0) {
            g.mapPath = arg + 6;
        } else if (std::strcmp(arg, "--map") == 0 && i + 1 < argc) {
            g.mapPath = argv[++i];
        } else if (arg[0] != '-') {
            g.mapPath = arg;
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

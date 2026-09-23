#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "resource/mpq_storm.h"
#include "model/mdx.h"
#include "render/lightning.h"
#include "render/mdx_viewer.h"
#include "render/picking.h"

struct PlacedModel {
    float pos[3] = {};
    float renderPos[3] = {};
    float angle = 0;
    float renderAngle = 0;
    float scale[3] = {1,1,1};
    float movementSpeed = 0.0f;
    float turnRate = 0.0f;
    int player = 0;
    int variation = 0;
    uint32_t debugWar3ID = 0;
    uint32_t debugSkinID = 0;
    std::string debugKind;
    bool visible = true;
    bool smoothMotion = false;
    bool transient = false;
    float transientAge = 0.0f;
    float transientMaxAge = 1.0f;
    bool transientHasPhysics = false;
    Vec3 transientVelocity = {0, 0, 0};
    float transientGravity = 0.0f;
    bool useCustomModelMatrix = false;
    Mat4 customModelMatrix = Mat4::Identity();
    std::unique_ptr<MdxViewer> ownedViewer;
    MdxViewer* viewer = nullptr; // shared, not owned
};

struct SceneEventEffect {
    MdxEventEffect effect;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool hasWorldTransform = false;
    Mat4 worldTransform = Mat4::Identity();
};

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() { Clear(); }

    bool Init(const MpqManager& mpq);
    void ApplyMapObjectModifications(const std::string& mapPath);
    void Clear();
    void ResetLoadStats();
    void PrintLoadStats() const;

    // Add a doodad/unit by War3ID and position
    bool AddDoodad(uint32_t war3ID, const float pos[3], float angle, const float scale[3],
                   int variation, int flags, int life, uint32_t skinId = 0);
    bool HasItemPath(uint32_t war3ID, uint32_t skinId = 0) const;
    int AddItem(uint32_t war3ID, const float pos[3], float angle, const float scale[3],
                 int flags, uint32_t skinId = 0);
    int AddUnit(uint32_t war3ID, const float pos[3], float angle, const float scale[3],
                 int player, int hitpoints, int flags, uint32_t skinId = 0,
                 bool playBirthAnimation = false);
    int AddTransientModelAtScenePosition(const std::string& path, const float pos[3],
                                         float angle, float scale, int player = 0,
                                         float fallbackLifeSeconds = 1.0f);
    int AddTransientModelWithSceneMatrix(const std::string& path, const Mat4& modelMatrix,
                                         int player = 0, float fallbackLifeSeconds = 1.0f,
                                         const Vec3& initialVelocity = Vec3{0, 0, 0},
                                         float gravity = 0.0f);

    // Render all placed models
    void RenderAll(const Mat4& view, const Mat4& proj, float dt);
    LightningSystem* GetLightning() { return &lightning_; }
    const std::vector<SceneEventEffect>& GetFrameEventEffects() const { return frameEventEffects_; }
    void SetDebugRenderRange(int start, int count);
    void SetDebugRenderKind(const std::string& kind);

    // Update position of a placed instance
    bool UpdateInstancePosition(int index, const float pos[3], float angle,
                                float movementSpeed = 0.0f, float turnRate = 0.0f,
                                bool smoothMotion = false);
    bool SetInstanceVisible(int index, bool visible);

    int InstanceCount() const { return (int)instances_.size(); }

    // Get viewer for a placed instance (for animation control)
    MdxViewer* GetViewer(int index) {
        if (index < 0 || index >= (int)instances_.size()) return nullptr;
        return instances_[index]->viewer;
    }
    bool IntersectInstanceRayWithCollision(int index, const picking::PickRay& ray, float& outT) const;

private:
    struct LoadStats {
        int unitAttempts = 0;
        int unitSuccess = 0;
        int itemAttempts = 0;
        int itemSuccess = 0;
        int doodadAttempts = 0;
        int doodadSuccess = 0;
        std::unordered_map<std::string, int> missReasons;
    };

    struct MdxEntry {
        std::string path;
        MdxModel model;
        bool modelLoaded = false;
        std::unique_ptr<MdxViewer> sharedViewer;
    };

    bool EnsureModelCached(const std::string& path, MdxEntry& entry);
    MdxViewer* GetOrLoadModel(const std::string& path);
    std::unique_ptr<MdxViewer> CreateUnitViewer(const std::string& path);
    MdxViewer* ResolveAndLoad(uint32_t war3ID, uint32_t skinId, int variation, bool doodad, const char** missReason = nullptr);

    std::unordered_map<std::string, MdxEntry> cache_;
    std::vector<PlacedModel*> instances_;
    MpqManager* mpq_ = nullptr;

    std::unordered_map<uint32_t, std::string> unitPaths_;
    std::unordered_map<uint32_t, std::string> unitFiles_;
    std::unordered_map<uint32_t, std::string> itemPaths_;
    std::unordered_map<uint32_t, std::string> itemFiles_;
    std::unordered_map<uint32_t, std::string> doodadPaths_;
    std::unordered_map<uint32_t, std::string> doodadDirs_;
    std::unordered_map<uint32_t, std::string> doodadFiles_;
    LoadStats loadStats_;
    std::vector<SceneEventEffect> frameEventEffects_;
    LightningSystem lightning_;
    int debugRenderStart_ = 0;
    int debugRenderCount_ = -1;
    std::string debugRenderKind_;
};

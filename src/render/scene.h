#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "resource/mpq_storm.h"
#include "model/mdx.h"
#include "render/mdx_viewer.h"

struct PlacedModel {
    float pos[3] = {};
    float angle = 0;
    float scale[3] = {1,1,1};
    int player = 0;
    int variation = 0;
    std::unique_ptr<MdxViewer> ownedViewer;
    MdxViewer* viewer = nullptr; // shared, not owned
};

class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() { Clear(); }

    bool Init(const MpqManager& mpq);
    void Clear();
    void ResetLoadStats();
    void PrintLoadStats() const;

    // Add a doodad/unit by War3ID and position
    bool AddDoodad(uint32_t war3ID, const float pos[3], float angle, const float scale[3],
                   int variation, int flags, int life, uint32_t skinId = 0);
    bool AddUnit(uint32_t war3ID, const float pos[3], float angle, const float scale[3],
                 int player, int hitpoints, int flags, uint32_t skinId = 0,
                 bool playBirthAnimation = false);

    // Render all placed models
    void RenderAll(const Mat4& view, const Mat4& proj);

    // Update position of a placed instance
    bool UpdateInstancePosition(int index, const float pos[3], float angle);

    int InstanceCount() const { return (int)instances_.size(); }

    // Get viewer for a placed instance (for animation control)
    MdxViewer* GetViewer(int index) {
        if (index < 0 || index >= (int)instances_.size()) return nullptr;
        return instances_[index]->viewer;
    }

private:
    struct LoadStats {
        int unitAttempts = 0;
        int unitSuccess = 0;
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
    std::unordered_map<uint32_t, std::string> doodadPaths_;
    LoadStats loadStats_;
};

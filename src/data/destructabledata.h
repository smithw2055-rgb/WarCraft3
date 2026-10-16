#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "data/combat_types.h"
#include "resource/slk.h"
#include "map/path_tex.h"

struct CDestructableType {
    War3ID id;
    std::string name;
    std::string modelPath;
    std::string iconPath;
    int maxLife = 100;
    int armor = 0;
    std::string armorType;
    // Targeting gate (ref targType/btar -> CTargetType set): an ability or
    // attack whose targetsAllowed contains all of these may hit this
    // destructable (tree/debris/wall/bridge fine branches).
    std::unordered_set<CTargetType> targetedAs;
    bool canDropItems = false;
    bool isTree = false;
    bool isWall = false;
    bool isBlight = false;
    bool isVisible = true;
    int goldContent = 0;
    std::string deathEffect;
    std::string deathSound;
    std::string deathModel;
    float occlusionHeight = 0.0f;
    // Pathing footprint textures (ref pathTex/pathTexDeath): decoded on
    // demand for blight checks and (next pass) static WPM stamps.
    std::string pathTex;
    std::string pathTexDeath;
    std::shared_ptr<const PathTexImage> pathTexImage;
    std::shared_ptr<const PathTexImage> pathTexDeathImage;
};

class CDestructableData {
public:
    CDestructableData() = default;

    bool HasDestructable(War3ID id) const;
    const CDestructableType* GetDestructable(War3ID id) const;
    // Test/script injection (mirrors CUnitData::RegisterUnitType).
    void RegisterDestructable(War3ID id, std::unique_ptr<CDestructableType> type);

    static CDestructableData LoadFromMpq(const class MpqManager& mpq,
                                         const std::string& mapPath = "");

private:
    std::unordered_map<War3ID, std::unique_ptr<CDestructableType>> destructables_;
};

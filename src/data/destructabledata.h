#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include "resource/slk.h"

struct CDestructableType {
    War3ID id;
    std::string name;
    std::string modelPath;
    std::string iconPath;
    int maxLife = 100;
    int armor = 0;
    std::string armorType;
    bool canDropItems = false;
    bool isTree = false;
    bool isWall = false;
    bool isBlight = false;
    bool isVisible = true;
    int goldContent = 0;
    std::string deathEffect;
    std::string deathSound;
    std::string deathModel;
    // Pathing footprint textures (ref pathTex/pathTexDeath): decoded on
    // demand for blight checks and (next pass) static WPM stamps.
    std::string pathTex;
    std::string pathTexDeath;
};

class CDestructableData {
public:
    CDestructableData() = default;

    bool HasDestructable(War3ID id) const;
    const CDestructableType* GetDestructable(War3ID id) const;

    static CDestructableData LoadFromMpq(const class MpqManager& mpq,
                                         const std::string& mapPath = "");

private:
    std::unordered_map<War3ID, std::unique_ptr<CDestructableType>> destructables_;
};

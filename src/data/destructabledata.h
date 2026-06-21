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
};

class CDestructableData {
public:
    CDestructableData() = default;

    bool HasDestructable(War3ID id) const;
    const CDestructableType* GetDestructable(War3ID id) const;

    static CDestructableData LoadFromMpq(const class MpqManager& mpq);

private:
    std::unordered_map<War3ID, std::unique_ptr<CDestructableType>> destructables_;
};

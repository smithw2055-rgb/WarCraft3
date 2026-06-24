#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include "resource/slk.h"

struct CItemType {
    War3ID id;
    std::string name;
    std::string legacyName;
    std::string modelPath;
    std::string iconPath;
    std::vector<War3ID> abilityList;
    int goldCost = 0;
    int lumberCost = 0;
    int maxLife = 50;
    int level = 0;
    int charges = 0;
    bool perishable = false;
    bool powerup = false;
    bool useOnly = false;
    bool ignoreCooldown = false;
    bool droppable = false;
    bool pawnable = false;
    bool dropOnDeath = false;
    bool sellable = false;
    bool stockable = false;
    int stockRegen = 0;
    int stockMax = 0;
    int cooldownID = 0;
};

class CItemData {
public:
    CItemData() = default;

    bool HasItem(War3ID id) const;
    const CItemType* GetItem(War3ID id) const;

    static CItemData LoadFromMpq(const class MpqManager& mpq);
    static CItemData LoadFromMpq(const class MpqManager& mpq, const std::string& mapPath);

private:
    std::unordered_map<War3ID, std::unique_ptr<CItemType>> items_;
};

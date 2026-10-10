#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include "ecs/item_class.h"
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
    CItemClass itemClass = CItemClass::UNKNOWN;
    bool includeAsRandomChoice = false;
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
    // Test and script registration. Replaces an existing row for the same id.
    void RegisterItemType(War3ID id, std::unique_ptr<CItemType> type) { items_[id] = std::move(type); }
    War3ID ChooseRandomItem(int level, std::function<float()> rand01) const;
    War3ID ChooseRandomItem(CItemClass itemClass, int level, std::function<float()> rand01) const;

    static CItemData LoadFromMpq(const class MpqManager& mpq);
    static CItemData LoadFromMpq(const class MpqManager& mpq, const std::string& mapPath);

private:
    struct RandomItemSet {
        std::vector<War3ID> unclassifiedItems;
        std::unordered_map<int, std::vector<War3ID>> classificationToItems;
    };

    std::unordered_map<War3ID, std::unique_ptr<CItemType>> items_;
    std::unordered_map<int, RandomItemSet> levelToRandomChoices_;
};

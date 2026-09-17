#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "w3i.h"

struct DroppedItemEntry {
    uint32_t id = 0; // War3ID item type (resolved by map script drop triggers)
    int chance = 0;  // percent
};

struct DroppedItemSet {
    std::vector<DroppedItemEntry> items;
};

struct DooInventoryItem {
    int slot = 0;
    uint32_t id = 0; // War3ID item type
};

struct DooModifiedAbility {
    uint32_t id = 0;      // War3ID ability
    int autocastActive = 0;
    int level = 1;
};

struct DooRandomUnit {
    uint32_t id = 0; // War3ID unit type
    int chance = 0;  // percent
};

struct UnitData {
    uint32_t id = 0;       // War3ID unit type
    uint32_t skinId = 0;
    int variation = 0;
    float pos[3] = {};
    float angle = 0;
    float scale[3] = {1,1,1};
    int flags = 0;
    int player = 0;
    int hitpoints = -1;
    int mana = -1;
    int droppedItemTable = 0;
    std::vector<DroppedItemSet> dropSets;
    int goldAmount = 0;
    float targetAcquisition = -1.0f;
    int heroLevel = 0;
    int heroStrength = 0;
    int heroAgility = 0;
    int heroIntelligence = 0;
    std::vector<DooInventoryItem> inventory;
    std::vector<DooModifiedAbility> modifiedAbilities;
    int randomFlag = 0;
    int randomLevel[3] = {};
    int randomItemClass = 0;
    uint32_t randomGroup = 0;
    uint32_t randomPosition = 0;
    std::vector<DooRandomUnit> randomTable;
    int customTeamColor = 0;
    int waygate = -1;
    int creationNumber = 0;
};

struct UnitsDooData {
    int version = 0;
    std::vector<UnitData> units;
};

bool UnitsDooLoad(const uint8_t* data, size_t size, UnitsDooData& out, const W3iInfo* mapInfo = nullptr);
bool UnitsDooLoadFromMpq(const std::string& mapPath, UnitsDooData& out, const W3iInfo* mapInfo = nullptr);

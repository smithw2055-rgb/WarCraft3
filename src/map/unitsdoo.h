#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "w3i.h"

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
    int goldAmount = 0;
    int heroLevel = 0;
    int customTeamColor = 0;
    int creationNumber = 0;
};

struct UnitsDooData {
    int version = 0;
    std::vector<UnitData> units;
};

bool UnitsDooLoad(const uint8_t* data, size_t size, UnitsDooData& out, const W3iInfo* mapInfo = nullptr);
bool UnitsDooLoadFromMpq(const std::string& mapPath, UnitsDooData& out, const W3iInfo* mapInfo = nullptr);

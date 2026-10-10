#pragma once
#include <cstdint>
#include <string>
#include <vector>

// war3map.doo parser (NOTE: filename w3d.* is historic; this parses the
// DOO doodad placement file, NOT war3map.w3d object mods which live in
// object_mods.cpp:LoadWar3MapDoodadMods). Mirrors ref
// parsers/w3x/doo/War3MapDoo+Doodad+RandomItem(Set).
struct DoodadRandomItemEntry {
    uint32_t id = 0; // War3ID item type
    int chance = 0;  // percent
};

struct DoodadRandomItemSet {
    std::vector<DoodadRandomItemEntry> items;
};

struct DoodadData {
    uint32_t id = 0;       // War3ID
    uint32_t skinId = 0;
    int variation = 0;
    float pos[3] = {};
    float angle = 0;
    float scale[3] = {1,1,1};
    int flags = 0;
    int life = 255;
    // ref Doodad.itemTable + itemSets (v>7 only, else empty / table=-1).
    int itemTable = -1;
    std::vector<DoodadRandomItemSet> itemSets;
    int editorId = 0;
};

struct TerrainDoodadData {
    uint32_t id = 0;       // War3ID
    uint32_t u1 = 0;
    uint32_t location[2] = {};
};

struct W3dData {
    int version = 0;
    std::vector<DoodadData> doodads;
    std::vector<TerrainDoodadData> terrainDoodads;
    // Legacy count mirror (kept in sync with terrainDoodads.size()).
    uint32_t terrainDoodadCount = 0;
};

bool W3dLoad(const uint8_t* data, size_t size, W3dData& out);
bool W3dLoadFromMpq(const std::string& mapPath, W3dData& out);

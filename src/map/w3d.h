#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct DoodadData {
    uint32_t id = 0;       // War3ID
    uint32_t skinId = 0;
    int variation = 0;
    float pos[3] = {};
    float angle = 0;
    float scale[3] = {1,1,1};
    int flags = 0;
    int life = 255;
    int editorId = 0;
};

struct W3dData {
    int version = 0;
    std::vector<DoodadData> doodads;
    // Terrain doodads (simplified - just count)
    uint32_t terrainDoodadCount = 0;
};

bool W3dLoad(const uint8_t* data, size_t size, W3dData& out);
bool W3dLoadFromMpq(const std::string& mapPath, W3dData& out);

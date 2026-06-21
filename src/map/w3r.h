#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct W3rRegion {
    float left = 0, bottom = 0, right = 0, top = 0;
    std::string name;
    int creationNumber = 0;
    uint32_t weatherId = 0;
    std::string ambientId;
    uint8_t color[4] = {}; // BGRA
};

struct W3rData {
    int version = 0;
    std::vector<W3rRegion> regions;
};

bool W3rLoad(const uint8_t* data, size_t size, W3rData& out);
bool W3rLoadFromMpq(const std::string& mapPath, W3rData& out);

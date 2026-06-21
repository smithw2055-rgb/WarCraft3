#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct WpmData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pathing; // width * height bytes

    bool IsWalkable(int x, int y) const;
    bool IsBuildable(int x, int y) const;
    bool IsBlight(int x, int y) const;
    bool IsWater(int x, int y) const;
};

bool WpmLoad(const uint8_t* data, size_t size, WpmData& out);
bool WpmLoadFromMpq(const std::string& mapPath, WpmData& out);

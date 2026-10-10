#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct WpmData {
    enum Flags : uint8_t {
        UNWALKABLE = 0x02,
        UNFLYABLE = 0x04,
        UNBUILDABLE = 0x08,
        BLOCKVISION = 0x10,
        BLIGHTED = 0x20,
        UNSWIMABLE = 0x40,
    };

    int width = 0;
    int height = 0;
    std::vector<uint8_t> pathing; // width * height bytes
    bool IsValid() const { return width > 0 && height > 0 && pathing.size() == static_cast<size_t>(width) * static_cast<size_t>(height); }

    bool IsWalkable(int x, int y) const;
    bool IsFlyable(int x, int y) const;
    bool IsBuildable(int x, int y) const;
    bool IsBlight(int x, int y) const;
    // Naval sense (ref PathingGrid FLOAT-pathable): clear UNSWIMABLE means
    // ships can navigate the cell (deep water). This is NOT the w3e tile
    // water flag (shallow/shore rendering); see W3ETileCorner::IsWater.
    bool IsWater(int x, int y) const;
};

bool WpmLoad(const uint8_t* data, size_t size, WpmData& out);
bool WpmLoadFromMpq(const std::string& mapPath, WpmData& out);

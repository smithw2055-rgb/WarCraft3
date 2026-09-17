#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct W3ETileCorner {
    uint16_t groundHeight;
    uint16_t waterAndBoundary;
    uint8_t  textureBits;
    uint8_t  variationBits;
    uint8_t  cliffAndLayer;

    // Decoded bitfields (computed after load)
    int  GroundTexture() const { return textureBits & 0x0F; }
    bool IsRamp()       const { return (textureBits & 0x10) != 0; }
    bool IsBlight()     const { return (textureBits & 0x20) != 0; }
    bool IsWater()      const { return (textureBits & 0x40) != 0; }
    bool IsBoundary()   const { return (textureBits & 0x80) != 0; }

    int  GroundVariation() const { return variationBits & 0x1F; }
    int  CliffVariation()  const { return (variationBits >> 5) & 0x07; }

    int  CliffTexture() const { return (cliffAndLayer >> 4) & 0x0F; }
    int  LayerHeight()  const { return cliffAndLayer & 0x0F; }

    float DecodeGroundHeight() const;
    float DecodeWaterHeight() const;
    float FinalGroundHeight() const;
    float FinalWaterHeight(float waterOffset = 0.0f) const;
};

struct W3EMap {
    int32_t  version;
    char     tileset;
    int32_t  customTilesets;
    int32_t  tileWidth;
    int32_t  tileHeight;
    float    offsetX;
    float    offsetY;

    std::vector<uint32_t> groundTilePalette;  // War3ID ground tile codes
    std::vector<uint32_t> cliffTilePalette;   // War3ID cliff tile codes
    std::vector<W3ETileCorner> tiles;

    int CornerCount() const { return tileWidth * tileHeight; }
};

bool W3ELoad(const uint8_t* data, size_t size, W3EMap& out);
bool W3ELoadFromFile(const std::string& path, W3EMap& out);

// World-space tile lookup for JASS terrain queries (ref terrain natives).
// Tiles are 128x128 world units starting at (offsetX, offsetY); returns
// nullptr when outside the map or when the map has no tiles.
inline const W3ETileCorner* W3ECornerAt(const W3EMap& map, float x, float y) {
    if (map.tileWidth <= 0 || map.tileHeight <= 0 || map.tiles.empty()) return nullptr;
    constexpr float kTileSize = 128.0f;
    const int cx = (int)((x - map.offsetX) / kTileSize);
    const int cy = (int)((y - map.offsetY) / kTileSize);
    if (cx < 0 || cy < 0 || cx >= map.tileWidth || cy >= map.tileHeight) return nullptr;
    const size_t idx = (size_t)cy * (size_t)map.tileWidth + (size_t)cx;
    if (idx >= map.tiles.size()) return nullptr;
    return &map.tiles[idx];
}

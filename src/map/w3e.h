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

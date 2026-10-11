#pragma once
#include <cstdint>
#include <vector>

struct BLPHeader {
    char     magic[4];         // "BLP1" or "BLP2"
    uint32_t compression;      // 0=JPEG, 1=Paletted, 2=DXT
    uint32_t flags;            // usually 8
    uint32_t width;
    uint32_t height;
    uint32_t imageType;        // 3=JPEG, etc.
    uint32_t hasMips;
    uint32_t mipmapOffset[16]; // starts at byte 28 (0x1C)
    // uint32_t mipmapSize[16]; // starts at byte 92 (0x5C)
};

struct BLPImage {
    int width;
    int height;
    std::vector<uint8_t> pixels; // RGBA8
};

bool DecodeBLP(const uint8_t* data, size_t size, BLPImage& out, int mipLevel = 0);

// Decodes every available level (0 = base) until the first failure, 1x1,
// or 16 levels. Never throws; empty when even the base fails. Levels that
// decode with unexpected dims are KEPT (callers validate per level).
bool DecodeBLPLevels(const uint8_t* data, size_t size, std::vector<BLPImage>& out);

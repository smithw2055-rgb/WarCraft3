#include "w3e.h"
#include <cstring>
#include <cstdio>
#include <iostream>

static uint16_t ReadLE16(const uint8_t* p) {
    return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

static uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

static float ReadFloat(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

float W3ETileCorner::DecodeGroundHeight() const {
    return (static_cast<float>(groundHeight) - 8192.0f) / 512.0f;
}

float W3ETileCorner::DecodeWaterHeight() const {
    return (static_cast<float>(waterAndBoundary & 0x3FFF) - 8192.0f) / 512.0f;
}

float W3ETileCorner::FinalGroundHeight() const {
    return DecodeGroundHeight() + static_cast<float>(LayerHeight()) - 2.0f;
}

float W3ETileCorner::FinalWaterHeight(float waterOffset) const {
    return DecodeWaterHeight() + waterOffset;
}

static uint32_t ReadWar3ID(const uint8_t* p) {
    return ReadLE32(p);
}

bool W3ELoad(const uint8_t* data, size_t size, W3EMap& out) {
    if (size < 32) {
        std::cerr << "W3E: file too small (" << size << " bytes)" << std::endl;
        return false;
    }
    if (std::memcmp(data, "W3E!", 4) != 0) {
        std::cerr << "W3E: invalid magic" << std::endl;
        return false;
    }

    size_t off = 4;
    out.version = (int32_t)ReadLE32(data + off); off += 4;
    out.tileset = (char)data[off++];
    out.customTilesets = (int32_t)ReadLE32(data + off); off += 4;

    int32_t numTilePalettes = (int32_t)ReadLE32(data + off); off += 4;
    out.groundTilePalette.resize(numTilePalettes);
    for (int i = 0; i < numTilePalettes; ++i) {
        out.groundTilePalette[i] = ReadWar3ID(data + off); off += 4;
    }

    int32_t numCliffPalettes = (int32_t)ReadLE32(data + off); off += 4;
    out.cliffTilePalette.resize(numCliffPalettes);
    for (int i = 0; i < numCliffPalettes; ++i) {
        out.cliffTilePalette[i] = ReadWar3ID(data + off); off += 4;
    }

    out.tileWidth = (int32_t)ReadLE32(data + off); off += 4;
    out.tileHeight = (int32_t)ReadLE32(data + off); off += 4;
    out.offsetX = ReadFloat(data + off); off += 4;
    out.offsetY = ReadFloat(data + off); off += 4;

    if (out.tileWidth <= 0 || out.tileHeight <= 0 ||
        out.tileWidth > 512 || out.tileHeight > 512) {
        std::cerr << "W3E: invalid dimensions " << out.tileWidth << "x" << out.tileHeight << std::endl;
        return false;
    }

    int tileCount = out.tileWidth * out.tileHeight;
    size_t tileSize = 7;
    size_t expectedDataSize = tileCount * tileSize;

    if (size - off < expectedDataSize) {
        std::cerr << "W3E: not enough data! Have " << (size - off) << ", need " << expectedDataSize << std::endl;
    }

    out.tiles.resize(tileCount);

    for (int i = 0; i < tileCount; ++i) {
        if (off + tileSize > size) {
            std::cerr << "W3E: unexpected end of file at tile " << i << std::endl;
            return false;
        }
        W3ETileCorner& c = out.tiles[i];
        c.groundHeight = ReadLE16(data + off); off += 2;
        c.waterAndBoundary = ReadLE16(data + off); off += 2;
        c.textureBits = data[off++];
        c.variationBits = data[off++];
        c.cliffAndLayer = data[off++];
    }

    std::cout << "W3E: v" << out.version << " tileset=" << out.tileset
              << " " << out.tileWidth << "x" << out.tileHeight
              << " palettes=" << out.groundTilePalette.size() << "g+" << out.cliffTilePalette.size() << "c"
              << " tiles=" << tileCount << std::endl;
    return true;
}

bool W3ELoadFromFile(const std::string& path, W3EMap& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) { return false; }
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(fp); return false; }
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (std::fread(buf.data(), 1, buf.size(), fp) != buf.size()) {
        std::fclose(fp); return false;
    }
    std::fclose(fp);
    return W3ELoad(buf.data(), buf.size(), out);
}

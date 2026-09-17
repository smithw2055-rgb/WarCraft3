#include "map/w3e.h"

#include <cstdint>
#include <cstring>
#include <vector>

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

static void PushLE32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

static void PushLE16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

static void PushFloatLE(std::vector<uint8_t>& out, float v) {
    uint32_t u = 0;
    static_assert(sizeof(float) == sizeof(uint32_t), "Unexpected float size");
    std::memcpy(&u, &v, sizeof(uint32_t));
    PushLE32(out, u);
}

bool TestW3ELoadValidMinimal() {
    std::vector<uint8_t> data;
    data.insert(data.end(), {'W','3','E','!'});
    PushLE32(data, 12);      // version
    data.push_back('A');     // tileset
    PushLE32(data, 0);       // customTilesets
    PushLE32(data, 0);       // numTilePalettes
    PushLE32(data, 0);       // numCliffPalettes
    PushLE32(data, 1);       // width
    PushLE32(data, 1);       // height
    PushFloatLE(data, 0.0f); // offsetX
    PushFloatLE(data, 0.0f); // offsetY

    // One tile corner (7 bytes, width*height = 1)
    PushLE16(data, 8192); // groundHeight
    PushLE16(data, 0);    // waterAndBoundary
    data.push_back(1);    // texture
    data.push_back(0);    // variation
    data.push_back(0);    // cliffAndLayer

    W3EMap out{};
    TEST_ASSERT(W3ELoad(data.data(), data.size(), out));
    TEST_ASSERT(out.version == 12);
    TEST_ASSERT(out.tileset == 'A');
    TEST_ASSERT(out.tileWidth == 1);
    TEST_ASSERT(out.tileHeight == 1);
    TEST_ASSERT(out.tiles.size() == 1);
    TEST_ASSERT(out.tiles[0].groundHeight == 8192);
    TEST_ASSERT(out.tiles[0].GroundTexture() == 1);
    return true;
}

bool TestW3ELoadRejectsBadMagic() {
    std::vector<uint8_t> data(40, 0);
    data[0] = 'B'; data[1] = 'A'; data[2] = 'D'; data[3] = '!';
    W3EMap out{};
    TEST_ASSERT(!W3ELoad(data.data(), data.size(), out));
    return true;
}

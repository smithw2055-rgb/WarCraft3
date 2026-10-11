#include "asset/BLP.h"
#include "asset/DDS.h"
#include "asset/MipChain.h"
#include "asset/stb_image.h"

#include <cstdint>
#include <vector>

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

static void PushLE32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

static void MakeBLPHeader(std::vector<uint8_t>& out, uint32_t compression,
                          uint32_t alphaBits,
                          uint32_t w, uint32_t h, uint32_t hasMips) {
    out.insert(out.end(), {'B','L','P','1'});
    PushLE32(out, compression); // 0=JPEG, 1=Paletted, 2=DXT
    PushLE32(out, alphaBits);   // alpha bits: 0, 1, 4, 8
    PushLE32(out, w);           // width at offset 12
    PushLE32(out, h);           // height at offset 16
    PushLE32(out, 3);           // imageType
    PushLE32(out, hasMips);     // hasMips at offset 24
    // mipOffset[16] at offset 28
    for (int i = 0; i < 16; ++i) PushLE32(out, 0);
    // mipSize[16] at offset 92
    for (int i = 0; i < 16; ++i) PushLE32(out, 0);
}

bool TestDecodeBLPRejectsBadMagic() {
    std::vector<uint8_t> data;
    data.insert(data.end(), {'N','O','P','E'});
    BLPImage out{};
    TEST_ASSERT(!DecodeBLP(data.data(), data.size(), out, 0));
    return true;
}

bool TestDecodeBLPDecodesMinimalPaletted() {
    std::vector<uint8_t> data;
    MakeBLPHeader(data, 1, 8, 1, 1, 1); // compression=1 (paletted), 8-bit alpha, 1x1, hasMips

    // Set mipOffset[0] = right after header + palette
    uint32_t headerSize = 0x9C; // 156 bytes
    uint32_t paletteSize = 256 * 4; // 1024 bytes
    uint32_t mipOffset = headerSize + paletteSize;
    // Write mipOffset at byte 28
    data[28] = mipOffset & 0xFF;
    data[29] = (mipOffset >> 8) & 0xFF;
    data[30] = (mipOffset >> 16) & 0xFF;
    data[31] = (mipOffset >> 24) & 0xFF;
    // mipSize[0] at byte 92 = 2 (index + alpha byte)
    data[92] = 2;

    // Palette RGBA[256] at offset 156
    std::vector<uint8_t> palette(paletteSize, 0);
    palette[1 * 4 + 0] = 10; // R
    palette[1 * 4 + 1] = 20; // G
    palette[1 * 4 + 2] = 30; // B
    palette[1 * 4 + 3] = 255;
    data.insert(data.end(), palette.begin(), palette.end());

    // Pixel data: index + alpha
    data.push_back(1); // index
    data.push_back(40); // 8-bit alpha

    BLPImage out{};
    TEST_ASSERT(DecodeBLP(data.data(), data.size(), out, 0));
    TEST_ASSERT(out.width == 1);
    TEST_ASSERT(out.height == 1);
    TEST_ASSERT(out.pixels.size() == 4);
    TEST_ASSERT(out.pixels[0] == 10);
    TEST_ASSERT(out.pixels[1] == 20);
    TEST_ASSERT(out.pixels[2] == 30);
    TEST_ASSERT(out.pixels[3] == 40);
    return true;
}

bool TestTgaDecodesViaStb() {
    // TGA chain (ref TgaFile/TgaHandler): our Texture::LoadFromMemory
    // decodes TGA through stb_image, so a minimal 2x2 BGR TGA must load.
    // Bottom-left origin (descriptor 0), uncompressed true-color, 24bpp.
    std::vector<uint8_t> tga = {
        0,    // id length
        0,    // colormap type
        2,    // image type: uncompressed true-color
        0, 0, 0, 0, 0, // colormap spec
        0, 0, // x origin
        0, 0, // y origin
        2, 0, // width = 2
        2, 0, // height = 2
        24,   // bits per pixel
        0,    // descriptor
        // 4 pixels, BGR order: red, green, blue, white
        0, 0, 255,  0, 255, 0,  255, 0, 0,  255, 255, 255,
    };
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(tga.data(), (int)tga.size(), &w, &h, &comp, STBI_rgb_alpha);
    TEST_ASSERT(px != nullptr);
    TEST_ASSERT(w == 2 && h == 2);
    // stb flips bottom-left origin to top-left row order, so scan all four
    // pixels for the red one instead of assuming index 0.
    bool foundRed = false;
    for (int i = 0; i < 4; i++) {
        if (px[i * 4 + 0] == 255 && px[i * 4 + 1] == 0 && px[i * 4 + 2] == 0 &&
            px[i * 4 + 3] == 255) {
            foundRed = true;
        }
    }
    TEST_ASSERT(foundRed);
    stbi_image_free(px);
    return true;
}

bool TestMipChainCountsAndDims() {
    TEST_ASSERT(FullMipCount(0, 4) == 0);
    TEST_ASSERT(FullMipCount(1, 1) == 1);
    TEST_ASSERT(FullMipCount(2, 2) == 2);
    TEST_ASSERT(FullMipCount(256, 256) == 9);
    TEST_ASSERT(FullMipCount(256, 128) == 9);
    TEST_ASSERT(FullMipCount(3, 5) == 3); // 3x5 -> 1x2 -> 1x1
    auto [w2, h2] = MipLevelDims(256, 128, 2);
    TEST_ASSERT(w2 == 64 && h2 == 32);
    auto [w3, h3] = MipLevelDims(3, 5, 2);
    TEST_ASSERT(w3 == 1 && h3 == 1);
    // 2x2 box average -> 1x1 (all channels).
    const uint8_t src[16] = {0, 0, 0, 0,  100, 0, 0, 255,
                             0, 100, 0, 255,  0, 0, 100, 255};
    uint8_t dst[4] = {};
    BoxDownsampleRGBA(src, 2, 2, dst);
    TEST_ASSERT(dst[0] == 25 && dst[1] == 25 && dst[2] == 25);
    TEST_ASSERT(dst[3] == (uint8_t)((0 + 255 + 255 + 255) / 4));
    // Odd dims clamp the edge texel instead of over-reading (row 0 used
    // twice when sh == 1: (10+50+10+50)/4).
    const uint8_t odd[3 * 4] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
    uint8_t oddDst[4] = {};
    BoxDownsampleRGBA(odd, 3, 1, oddDst); // -> 1x1
    TEST_ASSERT(oddDst[0] == 30);
    return true;
}

bool TestMipChainEmbeddedFallback() {
    // 4x4 base: embedded level with WRONG dims is skipped, generator fills.
    std::vector<uint8_t> base(4 * 4 * 4, 128);
    MipLevel wrong;
    wrong.width = 3;
    wrong.height = 3;
    wrong.pixels.assign(3 * 3 * 4, 200);
    std::vector<MipLevel> embedded;
    embedded.push_back(std::move(wrong));
    MipChain chain = BuildMipChain(4, 4, base.data(), std::move(embedded));
    TEST_ASSERT(chain.levels.size() == 3); // 4x4 -> 2x2 -> 1x1
    TEST_ASSERT(chain.levels[1].width == 2 && chain.levels[1].height == 2);
    TEST_ASSERT(chain.levels[1].pixels[0] == 128); // generated, not 200
    TEST_ASSERT(chain.levels[2].width == 1 && chain.levels[2].height == 1);
    // Exact embedded level is taken as-is.
    MipLevel exact;
    exact.width = 2;
    exact.height = 2;
    exact.pixels.assign(2 * 2 * 4, 77);
    std::vector<MipLevel> embedded2;
    embedded2.push_back(std::move(exact));
    MipChain chain2 = BuildMipChain(4, 4, base.data(), std::move(embedded2));
    TEST_ASSERT(chain2.levels[1].pixels[0] == 77);
    // Invalid base -> empty chain (uploaders must check).
    MipChain empty = BuildMipChain(0, 4, base.data());
    TEST_ASSERT(empty.levels.empty());
    MipChain nullBase = BuildMipChain(4, 4, nullptr);
    TEST_ASSERT(nullBase.levels.empty());
    return true;
}

bool TestBLPLevelsWalkPalettedMips() {
    // Two-level paletted BLP: 2x2 base + 1x1 mip, 8-bit alpha.
    std::vector<uint8_t> data;
    MakeBLPHeader(data, 1, 8, 2, 2, 1);
    const uint32_t headerSize = 0x9C;
    const uint32_t paletteSize = 256 * 4;
    const uint32_t l0off = headerSize + paletteSize; // 1180
    const uint32_t l0size = 4 + 4;                   // 2x2 indices + alpha
    const uint32_t l1off = l0off + l0size;
    const uint32_t l1size = 1 + 1;
    auto poke32 = [&](size_t at, uint32_t v) {
        data[at] = v & 0xFF;
        data[at + 1] = (v >> 8) & 0xFF;
        data[at + 2] = (v >> 16) & 0xFF;
        data[at + 3] = (v >> 24) & 0xFF;
    };
    poke32(28, l0off);
    poke32(32, l1off);
    poke32(92, l0size);
    poke32(96, l1size);
    std::vector<uint8_t> palette(paletteSize, 0);
    palette[2 * 4 + 0] = 11;
    palette[2 * 4 + 1] = 22;
    palette[2 * 4 + 2] = 33;
    data.insert(data.end(), palette.begin(), palette.end());
    for (int i = 0; i < 4; i++) data.push_back(2);   // base indices
    for (int i = 0; i < 4; i++) data.push_back(44);  // base alpha
    data.push_back(2);                                // mip index
    data.push_back(55);                               // mip alpha

    std::vector<BLPImage> levels;
    TEST_ASSERT(DecodeBLPLevels(data.data(), data.size(), levels));
    TEST_ASSERT(levels.size() == 2);
    TEST_ASSERT(levels[0].width == 2 && levels[0].height == 2);
    TEST_ASSERT(levels[0].pixels[0] == 11 && levels[0].pixels[3] == 44);
    TEST_ASSERT(levels[1].width == 1 && levels[1].height == 1);
    TEST_ASSERT(levels[1].pixels[0] == 11 && levels[1].pixels[3] == 55);
    // Truncated tail keeps the good prefix instead of failing all.
    std::vector<BLPImage> partial;
    TEST_ASSERT(DecodeBLPLevels(data.data(), l1off, partial));
    TEST_ASSERT(partial.size() == 1);
    // Garbage decodes to nothing.
    std::vector<BLPImage> none;
    TEST_ASSERT(!DecodeBLPLevels((const uint8_t*)"NOPE", 4, none));
    TEST_ASSERT(none.empty());
    return true;
}

static void PushDdsHeader(std::vector<uint8_t>& out, int w, int h, uint32_t mipCount) {
    out.insert(out.end(), {'D', 'D', 'S', ' '});
    auto le32 = [&](uint32_t v) {
        out.push_back(v & 0xFF);
        out.push_back((v >> 8) & 0xFF);
        out.push_back((v >> 16) & 0xFF);
        out.push_back((v >> 24) & 0xFF);
    };
    le32(124);          // size
    le32(0x20007);      // flags: caps+height+width+mipmapcount
    le32((uint32_t)h);  // height
    le32((uint32_t)w);  // width
    le32(0);            // pitch
    le32(0);            // depth
    le32(mipCount);     // mipMapCount
    for (int i = 0; i < 11; i++) le32(0); // reserved1
    le32(32);           // pf size
    le32(0x04);         // pf flags: FOURCC
    out.insert(out.end(), {'D', 'X', 'T', '1'});
    le32(0);            // rgbBitCount
    le32(0); le32(0); le32(0); le32(0); // masks
    le32(0x1000);       // caps: texture
    le32(0x400000);     // caps2: mipmap
    le32(0); le32(0);   // caps3/4
    le32(0);            // reserved2
}

bool TestDDSLevelsWalkAndTruncate() {
    // 8x8 DXT1 base (4 blocks = 32B) + 4x4 mip (1 block = 8B).
    std::vector<uint8_t> data;
    PushDdsHeader(data, 8, 8, 2);
    // One white DXT1 block (c0=white, c1=black, all indices 0).
    const uint8_t white[8] = {0xFF, 0xFF, 0x00, 0x00, 0, 0, 0, 0};
    for (int i = 0; i < 4; i++) data.insert(data.end(), white, white + 8);
    data.insert(data.end(), white, white + 8); // mip block
    TEST_ASSERT(data.size() == 128 + 32 + 8);

    std::vector<DdsImage> levels;
    TEST_ASSERT(DecodeDDSLevels(data.data(), data.size(), levels));
    TEST_ASSERT(levels.size() == 2);
    TEST_ASSERT(levels[0].width == 8 && levels[0].height == 8);
    TEST_ASSERT(levels[1].width == 4 && levels[1].height == 4);
    TEST_ASSERT(levels[0].pixels[0] == 255 && levels[0].pixels[3] == 255);
    // Truncated mip keeps the base (old code would over-read here).
    std::vector<DdsImage> partial;
    TEST_ASSERT(DecodeDDSLevels(data.data(), 128 + 32, partial));
    TEST_ASSERT(partial.size() == 1);
    // Garbage and uncompressed layouts fail clean.
    std::vector<DdsImage> none;
    TEST_ASSERT(!DecodeDDSLevels((const uint8_t*)"NOPE....", 8, none));
    std::vector<uint8_t> bad = data;
    bad[84] = 'X'; // break FOURCC
    std::vector<DdsImage> badOut;
    TEST_ASSERT(!DecodeDDSLevels(bad.data(), bad.size(), badOut));
    return true;
}

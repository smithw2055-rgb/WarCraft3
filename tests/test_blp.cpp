#include "asset/BLP.h"
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

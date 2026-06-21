#include "DDS.h"
#include <cstring>
#include <iostream>

// DDS header
struct DDSPixelFormat {
    uint32_t size = 32;
    uint32_t flags = 0;
    char fourCC[4] = {};
    uint32_t rgbBitCount = 0;
    uint32_t rBitMask = 0;
    uint32_t gBitMask = 0;
    uint32_t bBitMask = 0;
    uint32_t aBitMask = 0;
};

struct DDSHeader {
    uint32_t size = 124;
    uint32_t flags = 0;
    uint32_t height = 0;
    uint32_t width = 0;
    uint32_t pitchOrLinearSize = 0;
    uint32_t depth = 0;
    uint32_t mipMapCount = 0;
    uint32_t reserved1[11] = {};
    DDSPixelFormat pixelFormat;
    uint32_t caps[4] = {};
    uint32_t reserved2 = 0;
};

static const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

// DXT block decoder (from BLP.cpp)
static void DecodeDXT1Block(const uint8_t* block, uint8_t out[4][4][4]) {
    uint16_t c0 = block[0] | (uint16_t(block[1]) << 8);
    uint16_t c1 = block[2] | (uint16_t(block[3]) << 8);
    auto unpack = [](uint16_t c, uint8_t* rgb) {
        rgb[0] = uint8_t((c >> 11) * 255 / 31);
        rgb[1] = uint8_t(((c >> 5) & 0x3F) * 255 / 63);
        rgb[2] = uint8_t((c & 0x1F) * 255 / 31);
    };
    uint8_t col0[3], col1[3];
    unpack(c0, col0); unpack(c1, col1);
    uint8_t colors[4][4];
    for (int i = 0; i < 3; i++) {
        colors[0][i] = col0[i]; colors[1][i] = col1[i];
    }
    colors[0][3] = colors[1][3] = 255;
    if (c0 > c1) {
        for (int i = 0; i < 3; i++) {
            colors[2][i] = (2 * col0[i] + col1[i]) / 3;
            colors[3][i] = (col0[i] + 2 * col1[i]) / 3;
        }
        colors[2][3] = colors[3][3] = 255;
    } else {
        for (int i = 0; i < 3; i++) colors[2][i] = (col0[i] + col1[i]) / 2;
        colors[2][3] = 255;
        colors[3][0] = colors[3][1] = colors[3][2] = colors[3][3] = 0;
    }
    uint32_t bits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            int idx = (bits >> (2 * (y * 4 + x))) & 3;
            for (int c = 0; c < 4; c++) out[y][x][c] = colors[idx][c];
        }
}

static void DecodeDXT5Block(const uint8_t* block, uint8_t out[4][4][4]) {
    uint8_t a0 = block[0], a1 = block[1];
    uint8_t a[8];
    a[0] = a0; a[1] = a1;
    if (a0 > a1)
        for (int i = 2; i < 8; i++) a[i] = ((8 - i) * a0 + (i - 1) * a1) / 7;
    else {
        for (int i = 2; i < 6; i++) a[i] = ((6 - i) * a0 + (i - 1) * a1) / 5;
        a[6] = 0; a[7] = 255;
    }
    DecodeDXT1Block(block + 8, out);
    uint64_t alphaBits = *(uint64_t*)block;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            out[y][x][3] = a[(alphaBits >> (3 * (y * 4 + x))) & 7];
}

static void DecodeDXT3Block(const uint8_t* block, uint8_t out[4][4][4]) {
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            out[y][x][3] = ((block[y * 2 + x / 2] >> (4 * (x % 2))) & 0xF) * 17;
    DecodeDXT1Block(block + 8, out);
}

static void DecodeDXT(const uint8_t* data, int w, int h, uint8_t* out, bool dxt5, bool dxt3) {
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    int blockSize = dxt5 || dxt3 ? 16 : 8;
    for (int by = 0; by < bh; by++)
        for (int bx = 0; bx < bw; bx++) {
            uint8_t block[4][4][4];
            const uint8_t* src = data + (by * bw + bx) * blockSize;
            if (dxt5) DecodeDXT5Block(src, block);
            else if (dxt3) DecodeDXT3Block(src, block);
            else DecodeDXT1Block(src, block);
            for (int py = 0; py < 4; py++)
                for (int px = 0; px < 4; px++) {
                    int dx = bx * 4 + px, dy = by * 4 + py;
                    if (dx < w && dy < h) {
                        int o = (dy * w + dx) * 4;
                        out[o] = block[py][px][0];
                        out[o+1] = block[py][px][1];
                        out[o+2] = block[py][px][2];
                        out[o+3] = block[py][px][3];
                    }
                }
        }
}

bool DecodeDDS(const uint8_t* data, size_t size, DdsImage& out) {
    if (size < 128 || *(uint32_t*)data != DDS_MAGIC) return false;

    const DDSHeader* hdr = (const DDSHeader*)(data + 4);
    out.width = (int)hdr->width;
    out.height = (int)hdr->height;
    out.pixels.resize((size_t)out.width * out.height * 4);

    const uint8_t* pixelData = data + sizeof(uint32_t) + sizeof(DDSHeader);
    size_t pixelSize = size - sizeof(uint32_t) - sizeof(DDSHeader);

    // Check FourCC for DXT format
    bool dxt1 = false, dxt3 = false, dxt5 = false;
    if (hdr->pixelFormat.flags & 0x04) { // DDPF_FOURCC
        char cc[5] = {hdr->pixelFormat.fourCC[0], hdr->pixelFormat.fourCC[1],
                      hdr->pixelFormat.fourCC[2], hdr->pixelFormat.fourCC[3], 0};
        dxt1 = (strcmp(cc, "DXT1") == 0);
        dxt3 = (strcmp(cc, "DXT3") == 0);
        dxt5 = (strcmp(cc, "DXT5") == 0);
    }

    if (dxt1 || dxt3 || dxt5)
        DecodeDXT(pixelData, out.width, out.height, out.pixels.data(), dxt5, dxt3);
    else
        return false;

    return true;
}

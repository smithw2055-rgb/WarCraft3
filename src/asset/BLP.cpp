#include "BLP.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <cmath>

#include "stb_image.h"

// -----------------------------------------------------------------------
// DXT (S3TC) decompression helpers
// -----------------------------------------------------------------------
static void DecodeColorBlock(const uint8_t* block, uint8_t out[4][4][4]) {
    uint16_t c0 = block[0] | (uint16_t(block[1]) << 8);
    uint16_t c1 = block[2] | (uint16_t(block[3]) << 8);

    auto unpack565 = [](uint16_t c, uint8_t* rgb) {
        rgb[0] = uint8_t((c >> 11) * 255 / 31);
        rgb[1] = uint8_t(((c >> 5) & 0x3F) * 255 / 63);
        rgb[2] = uint8_t((c & 0x1F) * 255 / 31);
    };

    uint8_t col0[4] = {0,0,0,255}, col1[4] = {0,0,0,255};
    unpack565(c0, col0);
    unpack565(c1, col1);

    uint8_t colors[4][4];
    colors[0][0] = col0[0]; colors[0][1] = col0[1]; colors[0][2] = col0[2]; colors[0][3] = 255;
    colors[1][0] = col1[0]; colors[1][1] = col1[1]; colors[1][2] = col1[2]; colors[1][3] = 255;

    if (c0 > c1) {
        for (int i = 0; i < 3; ++i) {
            colors[2][i] = uint8_t((2 * col0[i] + col1[i]) / 3);
            colors[3][i] = uint8_t((col0[i] + 2 * col1[i]) / 3);
        }
        colors[2][3] = 255;
        colors[3][3] = 255;
    } else {
        for (int i = 0; i < 3; ++i) {
            colors[2][i] = uint8_t((col0[i] + col1[i]) / 2);
            colors[3][i] = 0;
        }
        colors[2][3] = 255;
        colors[3][3] = 0;
    }

    uint32_t bits = block[4] | (uint32_t(block[5]) << 8) |
                    (uint32_t(block[6]) << 16) | (uint32_t(block[7]) << 24);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            int idx = (bits >> (2 * (y * 4 + x))) & 3;
            out[y][x][0] = colors[idx][0];
            out[y][x][1] = colors[idx][1];
            out[y][x][2] = colors[idx][2];
            out[y][x][3] = colors[idx][3];
        }
    }
}

static void DecodeDXT1(const uint8_t* data, int w, int h, uint8_t* out) {
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            uint8_t block[4][4][4];
            DecodeColorBlock(data + (by * bw + bx) * 8, block);
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    int dx = bx * 4 + px, dy = by * 4 + py;
                    if (dx < w && dy < h) {
                        int o = (dy * w + dx) * 4;
                        out[o+0] = block[py][px][0];
                        out[o+1] = block[py][px][1];
                        out[o+2] = block[py][px][2];
                        out[o+3] = block[py][px][3];
                    }
                }
            }
        }
    }
}

static void DecodeDXT5Block(const uint8_t* block, uint8_t out[4][4][4]) {
    uint64_t alpha = block[0] | (uint64_t(block[1]) << 8) | (uint64_t(block[2]) << 16) |
                     (uint64_t(block[3]) << 24) | (uint64_t(block[4]) << 32) |
                     (uint64_t(block[5]) << 40) | (uint64_t(block[6]) << 48) |
                     (uint64_t(block[7]) << 56);
    uint8_t a0 = block[0], a1 = block[1];
    uint8_t a[8];
    a[0] = a0; a[1] = a1;
    if (a0 > a1) {
        for (int i = 2; i < 8; ++i)
            a[i] = uint8_t(((8 - i) * a0 + (i - 1) * a1) / 7);
    } else {
        for (int i = 2; i < 6; ++i)
            a[i] = uint8_t(((6 - i) * a0 + (i - 1) * a1) / 5);
        a[6] = 0; a[7] = 255;
    }

    DecodeColorBlock(block + 8, out);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            int idx = (alpha >> (3 * (y * 4 + x))) & 7;
            out[y][x][3] = a[idx];
        }
    }
}

static void DecodeDXT3Block(const uint8_t* block, uint8_t out[4][4][4]) {
    uint64_t alpha = *(const uint64_t*)block;
    for (int i = 0; i < 16; i++) {
        int y = i / 4, x = i % 4;
        out[y][x][3] = uint8_t(((alpha >> (4 * i)) & 0xF) * 17);
    }
    DecodeColorBlock(block + 8, out);
}

static uint8_t LinearToSrgbByte(uint8_t value) {
    const float linear = value / 255.0f;
    float srgb = 0.0f;
    if (linear <= 0.0031308f) {
        srgb = linear * 12.92f;
    } else {
        srgb = 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    }
    srgb = std::fmax(0.0f, std::fmin(1.0f, srgb));
    return (uint8_t)std::lround(srgb * 255.0f);
}

static void ApplyApproximateSrgbFix(std::vector<uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        pixels[i + 0] = LinearToSrgbByte(pixels[i + 0]);
        pixels[i + 1] = LinearToSrgbByte(pixels[i + 1]);
        pixels[i + 2] = LinearToSrgbByte(pixels[i + 2]);
    }
}

// -----------------------------------------------------------------------
// Main BLP decoder
// -----------------------------------------------------------------------
bool DecodeBLP(const uint8_t* data, size_t size, BLPImage& out, int mipLevel) {
    if (size < 32) return false;
    if (std::memcmp(data, "BLP", 3) != 0) return false;

    uint32_t compression = *(const uint32_t*)(data + 4);
    uint32_t alphaBits   = *(const uint32_t*)(data + 8);     // Alpha 通道位数：0, 1, 4, 8
    uint32_t width       = *(const uint32_t*)(data + 12);
    uint32_t height      = *(const uint32_t*)(data + 16);
  //  uint32_t extra       = *(const uint32_t*)(data + 20);     // 额外标记/Mipmap子类型
    uint32_t hasMips     = *(const uint32_t*)(data + 24);
     
    if (width == 0 || height == 0 || width > 4096 || height > 4096)
        return false;

    int mipW = (int)(width >> mipLevel);
    int mipH = (int)(height >> mipLevel);
    if (mipW < 1) mipW = 1;
    if (mipH < 1) mipH = 1;

    out.width = mipW;
    out.height = mipH;
    out.pixels.resize((size_t)mipW * mipH * 4);

    if (compression == 0) {
        // JPEG compressed BLP
        uint32_t mipOff = *(const uint32_t*)(data + 28 + mipLevel * 4);
        uint32_t mipSize  = *(const uint32_t*)(data + 0x5C + mipLevel * 4);

        if (mipOff == 0 || mipSize == 0 || mipOff + mipSize > size)
            return false;

        // Java: shared header is length-prefixed after mip offsets/sizes table.
        // Offset 0x9C = header end, then 4-byte LE int = shared header length.
        uint32_t sharedLen = *(const uint32_t*)(data + 0x9C);
        const uint8_t* sharedHeader = data + 0x9C + 4;

        if (sharedLen > 0xFFFF || 0x9C + 4 + sharedLen > mipOff)
            sharedLen = 0;  // fallback: treat entire mip data as full JPEG

        // Java: JPEG = sharedHeader + mipScanData
        size_t combinedSize = sharedLen + mipSize;
        if (combinedSize > 1024 * 1024 * 16) return false;

        std::vector<uint8_t> jpegData(combinedSize);
        if (sharedLen > 0)
            std::memcpy(jpegData.data(), sharedHeader, sharedLen);
        std::memcpy(jpegData.data() + sharedLen, data + mipOff, mipSize);

        // Check SOF0 marker for CMYK (4 components). stb_image needs the
        // APP14 Adobe marker to correctly identify CMYK vs YCbCr + alpha.
        bool isCMYK = false;
        int jpegComponents = 0;
        for (size_t j = 0; j + 11 < jpegData.size(); j++) {
            if (jpegData[j] == 0xFF && jpegData[j + 1] == 0xC0) {
                jpegComponents = (j + 9 < jpegData.size()) ? jpegData[j + 9] : -1;
                if (jpegComponents == 4)
                    isCMYK = true;
                break;
            }
        }

        if (isCMYK) {
            bool hasApp14 = false;
            for (size_t j = 2; j + 1 < jpegData.size(); j++) {
                if (jpegData[j] == 0xFF && jpegData[j + 1] == 0xEE) {
                    hasApp14 = true;
                    break;
                }
                if (jpegData[j] == 0xFF && jpegData[j + 1] == 0xDB) break;
            }
            if (!hasApp14) {
                // APP14 Adobe marker: transform 0 = CMYK
                uint8_t app14[] = {0xFF, 0xEE, 0x00, 0x0E, 'A','d','o','b','e',0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                std::vector<uint8_t> tmp(combinedSize + sizeof(app14));
                tmp[0] = jpegData[0]; tmp[1] = jpegData[1];
                std::memcpy(tmp.data() + 2, app14, sizeof(app14));
                std::memcpy(tmp.data() + 2 + sizeof(app14), jpegData.data() + 2, combinedSize - 2);
                jpegData.swap(tmp);
            }
        }

        // Primary decode: with APP14 -> correct CMYK->RGB, alpha=255
        int imgW = 0, imgH = 0, channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(jpegData.data(), (int)jpegData.size(),
                                                 &imgW, &imgH, &channels, STBI_rgb_alpha);
        if (!pixels) return false;

        out.width = imgW;
        out.height = imgH;
        out.pixels.assign(pixels, pixels + (size_t)imgW * imgH * 4);
        stbi_image_free(pixels);

        // For CMYK JPEGs, stb_image outputs inverted data: R=255-C, G=255-M, B=255-Y, A=K
        if (isCMYK) {
            if (alphaBits > 0) {
                // Ground texture: swap R and B to match Java's BLP plugin
                for (size_t i = 0; i < out.pixels.size(); i += 4)
                    std::swap(out.pixels[i], out.pixels[i + 2]);
            } else {
                // Water/cliff texture: swap R/B, then K is shade map
                for (size_t i = 0; i < out.pixels.size(); i += 4)
                    std::swap(out.pixels[i], out.pixels[i + 2]);
                for (size_t i = 0; i < out.pixels.size(); i += 4) {
                    int kf = out.pixels[i + 3];
                    out.pixels[i + 0] = uint8_t((int(out.pixels[i + 0]) * kf + 127) / 255);
                    out.pixels[i + 1] = uint8_t((int(out.pixels[i + 1]) * kf + 127) / 255);
                    out.pixels[i + 2] = uint8_t((int(out.pixels[i + 2]) * kf + 127) / 255);
                    out.pixels[i + 3] = 255;
                }
            }
        }

        // NOTE: ApplyApproximateSrgbFix was removed because stb_image decodes JPEG
        // to sRGB, so applying linear->sRGB would double-correct. Java's blp-iio-plugin
        // also outputs sRGB directly.

        // Decode alpha channel for JPEG BLP (stored after the JPEG scan data in the mip level)
        if (alphaBits > 0 && (size_t)imgW * imgH <= out.pixels.size() / 4) {
            // Search for JPEG EOI marker (0xFFD9) in mip data to find alpha section start
            size_t eoiInMip = 0;
            for (size_t j = 0; j + 1 < mipSize; j++) {
                if (data[mipOff + j] == 0xFF && data[mipOff + j + 1] == 0xD9) {
                    eoiInMip = j + 2; // first EOI = end of JPEG scan data
                    break;
                }
            }
            if (eoiInMip > 0 && eoiInMip < mipSize) {
                const uint8_t* alphaSec = data + mipOff + eoiInMip;
                size_t alphaSecSize = mipSize - eoiInMip;
                if (alphaSecSize >= 1) {
                    uint8_t alphaFlag = alphaSec[0];
                    if (alphaFlag == 1 && alphaSecSize >= 1 + (size_t)imgW * imgH) {
                        // Raw 8-bit alpha
                        for (int i = 0; i < imgW * imgH; i++)
                            out.pixels[i * 4 + 3] = alphaSec[1 + i];
                    } else if (alphaFlag == 0 && alphaSecSize >= 1 + 1024 + (size_t)imgW * imgH) {
                        // Palette-based alpha: 256-color BGRA palette + index bytes
                        const uint8_t* pal = alphaSec + 1; // 1024 bytes, BGRA entries
                        const uint8_t* idx = alphaSec + 1 + 1024;
                        for (int i = 0; i < imgW * imgH; i++)
                            out.pixels[i * 4 + 3] = pal[idx[i] * 4 + 3];
                    }
                }
            }
        }
        return true;
    }
    else if (compression == 1) {
        // Paletted BLP
        // Header is 156 bytes, then 256-color RGBA palette (1024 bytes)
        if (size < 0x9C + 1024) return false;

        const uint8_t* palette = data + 0x9C;

        // Pixel indices start after the palette, followed by optional packed alpha data
        const size_t pixelCount = (size_t)mipW * mipH;
        const uint8_t* pixelData = data + 0x9C + 1024;
        const uint8_t* alphaData = pixelData + pixelCount;
        const uint8_t* fileEnd = data + size;
        if (pixelData + pixelCount > fileEnd) return false;

        out.pixels.resize(pixelCount * 4);
        for (size_t i = 0; i < pixelCount; ++i) {
            int idx = pixelData[i];
            // Warcraft III paletted terrain/model textures decode correctly when the
            // palette table is treated as already ordered RGBA.
            out.pixels[i*4+0] = palette[idx*4+0]; // R
            out.pixels[i*4+1] = palette[idx*4+1]; // G
            out.pixels[i*4+2] = palette[idx*4+2]; // B

            uint8_t alpha = 255;
            if (alphaBits == 8) {
                if (alphaData + pixelCount <= fileEnd) {
                    alpha = alphaData[i];
                }
            } else if (alphaBits == 4) {
                if (alphaData + ((pixelCount + 1) / 2) <= fileEnd) {
                    uint8_t packed = alphaData[i >> 1];
                    uint8_t nibble = (i & 1) ? (packed >> 4) : (packed & 0x0F);
                    alpha = uint8_t(nibble * 17);
                }
            } else if (alphaBits == 1) {
                if (alphaData + ((pixelCount + 7) / 8) <= fileEnd) {
                    uint8_t packed = alphaData[i >> 3];
                    alpha = (packed & (1u << (i & 7))) ? 255 : 0;
                }
            } else if (alphaBits != 0) {
                alpha = palette[idx*4+3];
            }
            out.pixels[i*4+3] = alpha;
        }
        return true;
    }
    else if (compression == 2 || compression == 4) {
        // DXT/S3TC compressed BLP
        uint32_t mipOff = *(const uint32_t*)(data + 28 + mipLevel * 4);
        uint32_t mipSz  = *(const uint32_t*)(data + 0x5C + mipLevel * 4);
        if (mipOff == 0 || mipSz == 0 || mipOff + mipSz > size)
            return false;

        const uint8_t* pixData = data + mipOff;

        // Select DXT decoder based on alphaBits (offset 8, already read above)
        if (alphaBits == 0 || alphaBits == 1) {
            // DXT1 (no alpha or 1-bit alpha)
            DecodeDXT1(pixData, mipW, mipH, out.pixels.data());
        } else if (alphaBits == 4) {
            // DXT3 (explicit 4-bit alpha)
            int bw = (mipW + 3) / 4, bh = (mipH + 3) / 4;
            for (int by = 0; by < bh; ++by) {
                for (int bx = 0; bx < bw; ++bx) {
                    uint8_t block[4][4][4];
                    DecodeDXT3Block(pixData + (by * bw + bx) * 16, block);
                    for (int py = 0; py < 4; ++py)
                        for (int px = 0; px < 4; ++px) {
                            int dx = bx * 4 + px, dy = by * 4 + py;
                            if (dx < mipW && dy < mipH) {
                                int o = (dy * mipW + dx) * 4;
                                out.pixels[o+0] = block[py][px][0];
                                out.pixels[o+1] = block[py][px][1];
                                out.pixels[o+2] = block[py][px][2];
                                out.pixels[o+3] = block[py][px][3];
                            }
                        }
                }
            }
        } else if (alphaBits == 8) {
            // DXT5 (interpolated 8-bit alpha)
            int bw = (mipW + 3) / 4, bh = (mipH + 3) / 4;
            for (int by = 0; by < bh; ++by) {
                for (int bx = 0; bx < bw; ++bx) {
                    uint8_t block[4][4][4];
                    DecodeDXT5Block(pixData + (by * bw + bx) * 16, block);
                    for (int py = 0; py < 4; ++py)
                        for (int px = 0; px < 4; ++px) {
                            int dx = bx * 4 + px, dy = by * 4 + py;
                            if (dx < mipW && dy < mipH) {
                                int o = (dy * mipW + dx) * 4;
                                out.pixels[o+0] = block[py][px][0];
                                out.pixels[o+1] = block[py][px][1];
                                out.pixels[o+2] = block[py][px][2];
                                out.pixels[o+3] = block[py][px][3];
                            }
                        }
                }
            }
        } else {
            // Default: try DXT1
            DecodeDXT1(pixData, mipW, mipH, out.pixels.data());
        }
        return true;
    }

    return false;
}
/* rebuild */
/* rebuild2 */
/* rebuild3 */
// force

/* f */

#include "TextRenderer.h"
#include "../resource/mpq_storm.h"
#include "../render/renderer.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../asset/stb_truetype.h"

#include <cstring>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>

TextRenderer::TextRenderer() {}
TextRenderer::~TextRenderer() {
    Shutdown();
}

void TextRenderer::Shutdown() {
    if (!atlasValid_) return;
    if (Renderer::IsActive() && sg_query_image_state(atlasTex_) == SG_RESOURCESTATE_VALID) {
        sg_destroy_image(atlasTex_);
    }
    atlasTex_ = {};
    atlasValid_ = false;
    glyphs_.clear();
    atlasW_ = 0;
    atlasH_ = 0;
    ascender_ = 0.0f;
    lineHeight_ = 0.0f;
    fontSize_ = 0.0f;
}

bool TextRenderer::LoadFromMpq(MpqManager* mpq, const char* fontPath, float fontSize) {
    if (!mpq) return false;

    // Retail UI fonts live in the MPQ (ref war3skins.txt MasterFont).
    // FRIZQT__.TTF is the Western UI face; the CJK faces follow.
    const char* paths[] = {
        fontPath,
        "Fonts\\FRIZQT__.TTF",
        "Fonts/FRIZQT__.TTF",
        "Fonts\\NIM_____.ttf",
        "Fonts\\DFHeiMd.ttf",
        "Fonts\\dfst-m3u.ttf",
        "Fonts\\tt5500m_.ttf",
        "Fonts\\dffn_b31.ttf",
        "Fonts/Nimrod_Normal_12.TTF",
        "Fonts/Nimrod_Normal_14.TTF",
        "Fonts/Nimrod_Normal_16.TTF",
        "Fonts/DFKai-SB_10_12.TTF",
    };

    for (auto p : paths) {
        if (!p) continue;
        std::vector<uint8_t> buf;
        if (mpq->ReadFile(p, buf) && !buf.empty()) {
            if (LoadFromMemory(buf.data(), (int)buf.size(), fontSize)) {
                std::cout << "TextRenderer: loaded font '" << p << "' at size " << fontSize << std::endl;
                return true;
            }
        }
    }

    const char* fallbackPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "C:/Windows/Fonts/framd.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/mnt/c/Windows/Fonts/framd.ttf",
        "/mnt/c/Windows/Fonts/arial.ttf",
        "/mnt/c/Windows/Fonts/segoeui.ttf",
    };
    for (auto p : fallbackPaths) {
        std::ifstream in(p, std::ios::binary | std::ios::ate);
        if (!in) continue;
        const std::streamsize size = in.tellg();
        if (size <= 0) continue;
        std::vector<uint8_t> buf((size_t)size);
        in.seekg(0, std::ios::beg);
        if (!in.read(reinterpret_cast<char*>(buf.data()), size)) continue;
        if (LoadFromMemory(buf.data(), (int)buf.size(), fontSize)) {
            std::cout << "TextRenderer: loaded fallback font '" << p << "' at size " << fontSize << std::endl;
            return true;
        }
    }

    std::cerr << "TextRenderer: no font found in MPQ or Windows fallback fonts" << std::endl;
    return false;
}

bool TextRenderer::LoadFromMemory(const uint8_t* ttfData, int ttfSize, float fontSize) {
    if (!ttfData || ttfSize <= 0) return false;

    fontSize_ = fontSize;
    BakeAtlas(ttfData, fontSize);
    return atlasValid_;
}

void TextRenderer::BakeAtlas(const uint8_t* ttfData, float fontSize) {
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttfData, stbtt_GetFontOffsetForIndex(ttfData, 0))) {
        std::cerr << "TextRenderer: stbtt_InitFont failed" << std::endl;
        return;
    }

    float scale = stbtt_ScaleForPixelHeight(&info, fontSize);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    ascender_ = ascent * scale;
    lineHeight_ = (ascent - descent + lineGap) * scale;

    int firstChar = 32, lastChar = 126;
    int numChars = lastChar - firstChar + 1;
    glyphs_.resize(numChars);

    // First pass: measure all glyphs and determine atlas size
    struct TempGlyph {
        int w, h;
        int xoff, yoff;
        float ax, ay;
    };
    std::vector<TempGlyph> temp(numChars);

    int totalArea = 0;
    int maxH = 0;
    for (int i = 0; i < numChars; i++) {
        int ch = firstChar + i;
        int gi = stbtt_FindGlyphIndex(&info, ch);
        if (gi == 0) continue;

        stbtt_GetGlyphHMetrics(&info, gi, nullptr, nullptr);
        int advance, lsb;
        stbtt_GetGlyphHMetrics(&info, gi, &advance, &lsb);
        temp[i].ax = advance * scale;
        temp[i].ay = 0;

        int w, h, xoff, yoff;
        unsigned char* bitmap = stbtt_GetGlyphBitmap(&info, scale, scale, gi, &w, &h, &xoff, &yoff);
        if (bitmap) {
            temp[i].w = w;
            temp[i].h = h;
            temp[i].xoff = xoff;
            temp[i].yoff = yoff;
            totalArea += (w + 1) * (h + 1);
            if (h > maxH) maxH = h;
            stbtt_FreeBitmap(bitmap, nullptr);
        } else {
            temp[i].w = 0;
            temp[i].h = 0;
            temp[i].xoff = 0;
            temp[i].yoff = 0;
        }
    }

    // Determine atlas size (try to fit in a reasonable texture)
    int atlasW = 256, atlasH = 256;
    while (atlasW < 2048 && atlasH < 2048 && atlasW * atlasH < totalArea * 2) {
        if (atlasW <= atlasH) atlasW *= 2;
        else atlasH *= 2;
    }

    atlasW_ = atlasW;
    atlasH_ = atlasH;

    // Pack glyphs row by row
    std::vector<uint8_t> atlasPixels(atlasW_ * atlasH_, 0);
    int curX = 1, curY = 1, rowH = 0;

    for (int i = 0; i < numChars; i++) {
        int ch = firstChar + i;
        int gi = stbtt_FindGlyphIndex(&info, ch);
        if (gi == 0) continue;

        int w = temp[i].w, h = temp[i].h;
        if (w <= 0 || h <= 0) {
            glyphs_[i].ax = temp[i].ax;
            glyphs_[i].ay = 0;
            glyphs_[i].bw = 0; glyphs_[i].bh = 0;
            glyphs_[i].bl = 0; glyphs_[i].bt = 0;
            glyphs_[i].tx0 = 0; glyphs_[i].ty0 = 0;
            glyphs_[i].tx1 = 0; glyphs_[i].ty1 = 0;
            continue;
        }

        if (curX + w + 1 > atlasW_) {
            curX = 1;
            curY += rowH + 1;
            rowH = 0;
        }
        if (curY + h + 1 > atlasH_) {
            std::cerr << "TextRenderer: atlas too small!" << std::endl;
            break;
        }

        unsigned char* bitmap = stbtt_GetGlyphBitmap(&info, scale, scale, gi, &w, &h, nullptr, nullptr);
        if (!bitmap) continue;

        // Copy bitmap to atlas
        for (int py = 0; py < h; py++) {
            for (int px = 0; px < w; px++) {
                int atlasIdx = (curY + py) * atlasW_ + (curX + px);
                atlasPixels[atlasIdx] = bitmap[py * w + px];
            }
        }
        stbtt_FreeBitmap(bitmap, nullptr);

        glyphs_[i].ax = temp[i].ax;
        glyphs_[i].ay = 0;
        glyphs_[i].bw = (float)w;
        glyphs_[i].bh = (float)h;
        glyphs_[i].bl = (float)temp[i].xoff;
        glyphs_[i].bt = (float)temp[i].yoff;
        glyphs_[i].tx0 = (float)curX / atlasW_;
        glyphs_[i].ty0 = (float)curY / atlasH_;
        glyphs_[i].tx1 = (float)(curX + w) / atlasW_;
        glyphs_[i].ty1 = (float)(curY + h) / atlasH_;

        if (h > rowH) rowH = h;
        curX += w + 1;
    }

    // Convert alpha-only atlas to RGBA
    std::vector<uint8_t> rgba(atlasW_ * atlasH_ * 4);
    for (int i = 0; i < atlasW_ * atlasH_; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = atlasPixels[i];
    }

    // Create sokol texture
    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_2D;
    desc.width = atlasW_;
    desc.height = atlasH_;
    desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    desc.num_mipmaps = 1;
    desc.data.mip_levels[0].ptr = rgba.data();
    desc.data.mip_levels[0].size = (size_t)(atlasW_ * atlasH_ * 4);
    atlasTex_ = sg_make_image(&desc);

    atlasValid_ = sg_query_image_state(atlasTex_) == SG_RESOURCESTATE_VALID;
    if (!atlasValid_) {
        std::cerr << "TextRenderer: failed to create atlas texture" << std::endl;
    }
}

float TextRenderer::MeasureText(const char* text, float size) const {
    if (!text || !atlasValid_) return 0;
    float scale = size / fontSize_;
    float x = 0;
    while (*text) {
        unsigned char ch = (unsigned char)*text;
        if (ch >= 32 && ch <= 126) {
            const auto& g = glyphs_[ch - 32];
            x += g.ax * scale;
        } else if (ch == '\t') {
            x += MeasureText("    ", size);
        }
        text++;
    }
    return x;
}

void TextRenderer::RenderText(UiRenderer& r, float x, float y, const char* text, uint32_t color, float size) {
    if (!text || !atlasValid_) return;

    // y is the TOP of the line box (all callers pass box tops); the stb
    // glyph top offset (yoff, negative above the baseline) applies from the
    // baseline below it.
    float scale = size / fontSize_;
    float baselineY = y + ascender_ * scale;
    float posX = x;

    while (*text) {
        unsigned char ch = (unsigned char)*text;
        if (ch == '\n') {
            posX = x;
            baselineY += lineHeight_ * scale;
            text++;
            continue;
        }
        if (ch < 32 || ch > 126) {
            text++;
            continue;
        }

        const auto& g = glyphs_[ch - 32];
        if (g.bw > 0 && g.bh > 0) {
            float qx = posX + g.bl * scale;
            float qy = baselineY + g.bt * scale;
            float qw = g.bw * scale;
            float qh = g.bh * scale;
            r.DrawTexturedQuad(qx, qy, qw, qh, atlasTex_,
                                g.tx0, g.ty0, g.tx1, g.ty1, color);
        }
        posX += g.ax * scale;
        text++;
    }
}

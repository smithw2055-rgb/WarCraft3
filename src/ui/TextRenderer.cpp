#include "TextRenderer.h"
#include "../resource/mpq_storm.h"
#include "render/rhi_device.h"

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
    if (atlasValid_ && Renderer::IsActive() && sg_query_image_state(atlasTex_) == SG_RESOURCESTATE_VALID) {
        sg_destroy_image(atlasTex_);
    }
    atlasTex_ = {};
    atlasValid_ = false;
    metricsValid_ = false;
    glyphs_.clear();
    unicodeGlyphs_.clear();
    layoutCache_.clear();
    atlasW_ = 0;
    atlasH_ = 0;
    ascender_ = 0.0f;
    lineHeight_ = 0.0f;
    fontSize_ = 24.0f;
}

void TextRenderer::InitHeadlessMetrics(float fontSize, float asciiAdvanceRatio) {
    Shutdown();
    fontSize_ = std::max(1.0f, fontSize);
    ascender_ = fontSize_ * 0.8f;
    lineHeight_ = fontSize_ * 1.2f;
    const int firstChar = 32, lastChar = 126;
    const int numChars = lastChar - firstChar + 1;
    glyphs_.assign((size_t)numChars, GlyphInfo{});
    for (int i = 0; i < numChars; ++i) {
        glyphs_[(size_t)i].ax = fontSize_ * asciiAdvanceRatio;
    }
    metricsValid_ = true;
}

bool TextRenderer::LoadFromMpq(MpqManager* mpq, const char* fontPath, float fontSize) {
    if (!mpq) return false;

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

    fontSize_ = std::max(1.0f, fontSize);
    layoutCache_.clear();
    BakeAtlas(ttfData, fontSize_);
    return atlasValid_ || metricsValid_;
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

        int advance = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&info, gi, &advance, &lsb);
        temp[i].ax = advance * scale;
        temp[i].ay = 0;

        int w = 0, h = 0, xoff = 0, yoff = 0;
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

    int atlasW = 256, atlasH = 256;
    while (atlasW < 2048 && atlasH < 2048 && atlasW * atlasH < totalArea * 2) {
        if (atlasW <= atlasH) atlasW *= 2;
        else atlasH *= 2;
    }

    atlasW_ = atlasW;
    atlasH_ = atlasH;

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

    metricsValid_ = true;
    if (!Renderer::IsActive()) {
        return;
    }

    std::vector<uint8_t> rgba(atlasW_ * atlasH_ * 4);
    for (int i = 0; i < atlasW_ * atlasH_; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = atlasPixels[i];
    }

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

uint32_t TextRenderer::DecodeNextUtf8Codepoint(const char*& cursor) {
    if (!cursor || !*cursor) return 0;
    const unsigned char b0 = static_cast<unsigned char>(*cursor++);
    if (b0 < 0x80) {
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0) {
        const unsigned char b1 = static_cast<unsigned char>(*cursor);
        if ((b1 & 0xC0) != 0x80) return b0;
        ++cursor;
        return ((uint32_t)(b0 & 0x1F) << 6) | (uint32_t)(b1 & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0) {
        const unsigned char b1 = static_cast<unsigned char>(cursor[0]);
        if ((b1 & 0xC0) != 0x80) return b0;
        const unsigned char b2 = static_cast<unsigned char>(cursor[1]);
        if ((b2 & 0xC0) != 0x80) return b0;
        cursor += 2;
        return ((uint32_t)(b0 & 0x0F) << 12) |
               ((uint32_t)(b1 & 0x3F) << 6) |
               (uint32_t)(b2 & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0) {
        const unsigned char b1 = static_cast<unsigned char>(cursor[0]);
        if ((b1 & 0xC0) != 0x80) return b0;
        const unsigned char b2 = static_cast<unsigned char>(cursor[1]);
        if ((b2 & 0xC0) != 0x80) return b0;
        const unsigned char b3 = static_cast<unsigned char>(cursor[2]);
        if ((b3 & 0xC0) != 0x80) return b0;
        cursor += 3;
        return ((uint32_t)(b0 & 0x07) << 18) |
               ((uint32_t)(b1 & 0x3F) << 12) |
               ((uint32_t)(b2 & 0x3F) << 6) |
               (uint32_t)(b3 & 0x3F);
    }
    return b0;
}

bool TextRenderer::TryParseColorCodeO1(const char* p, uint32_t& outColor) {
    // Expects p pointing at "|c" or "|C"; checks exactly 8 hex digits in O(1) without strlen()
    if (!p || p[0] != '|' || (p[1] != 'c' && p[1] != 'C')) return false;
    auto hexNibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    uint8_t bytes[4] = {};
    for (int i = 0; i < 4; ++i) {
        const char chH = p[2 + i * 2];
        if (chH == '\0') return false;
        const char chL = p[2 + i * 2 + 1];
        if (chL == '\0') return false;
        const int h = hexNibble(chH);
        const int l = hexNibble(chL);
        if (h < 0 || l < 0) return false;
        bytes[i] = static_cast<uint8_t>((h << 4) | l);
    }
    // WC3 |cAARRGGBB -> packed ABGR uint32_t used by UiRenderer
    const uint32_t a = bytes[0];
    const uint32_t r = bytes[1];
    const uint32_t g = bytes[2];
    const uint32_t b = bytes[3];
    outColor = (a << 24) | (b << 16) | (g << 8) | r;
    return true;
}

void TextRenderer::RegisterUnicodeGlyphAdvance(uint32_t codepoint, float advanceAtBaseSize) {
    GlyphInfo info{};
    info.ax = advanceAtBaseSize;
    unicodeGlyphs_[codepoint] = info;
    layoutCache_.clear();
}

float TextRenderer::GetCodepointAdvanceBase(uint32_t cp) const {
    if (cp >= 32 && cp <= 126 && (cp - 32) < glyphs_.size()) {
        return glyphs_[cp - 32].ax;
    }
    if (cp == '\t') {
        const float spaceAdv = (glyphs_.size() > 0) ? glyphs_[0].ax : (fontSize_ * 0.5f);
        return spaceAdv * 4.0f;
    }
    auto it = unicodeGlyphs_.find(cp);
    if (it != unicodeGlyphs_.end()) {
        return it->second.ax;
    }
    // CJK Unified Ideographs, CJK Symbols/Punctuation, Halfwidth/Fullwidth Forms
    if ((cp >= 0x4E00 && cp <= 0x9FFF) ||
        (cp >= 0x3000 && cp <= 0x303F) ||
        (cp >= 0xFF00 && cp <= 0xFFEF)) {
        return fontSize_;
    }
    if (cp > 126) {
        return fontSize_ * 0.6f;
    }
    return 0.0f;
}

const CachedTextLayout& TextRenderer::GetOrBuildLayout(const char* text) const {
    static const CachedTextLayout kEmptyLayout{};
    if (!text) return kEmptyLayout;

    std::string key(text);
    auto it = layoutCache_.find(key);
    if (it != layoutCache_.end()) {
        ++layoutCacheHits_;
        return it->second;
    }
    ++layoutCacheMisses_;

    if (layoutCache_.size() >= 1024) {
        layoutCache_.clear();
    }

    CachedTextLayout built;
    RichTextSpan curSpan;
    bool hasActiveColor = false;
    uint32_t activeColor = 0;
    float curLineWidth = 0.0f;
    float maxLineWidth = 0.0f;
    int lines = 1;

    const char* p = text;
    while (*p) {
        if (p[0] == '|' && p[1] != '\0') {
            const char code = p[1];
            uint32_t parsedColor = 0;
            if ((code == 'c' || code == 'C') && TryParseColorCodeO1(p, parsedColor)) {
                if (!curSpan.text.empty()) {
                    built.spans.push_back(std::move(curSpan));
                    curSpan = RichTextSpan{};
                }
                hasActiveColor = true;
                activeColor = parsedColor;
                curSpan.colorOverride = activeColor;
                curSpan.hasColorOverride = true;
                p += 10;
                continue;
            }
            if (code == 'r' || code == 'R') {
                if (!curSpan.text.empty()) {
                    built.spans.push_back(std::move(curSpan));
                    curSpan = RichTextSpan{};
                }
                hasActiveColor = false;
                activeColor = 0;
                curSpan.hasColorOverride = false;
                p += 2;
                continue;
            }
            if (code == 'n' || code == 'N') {
                if (!curSpan.text.empty() || curSpan.newlineBefore) {
                    built.spans.push_back(std::move(curSpan));
                    curSpan = RichTextSpan{};
                }
                curSpan.newlineBefore = true;
                curSpan.hasColorOverride = hasActiveColor;
                curSpan.colorOverride = activeColor;
                maxLineWidth = std::max(maxLineWidth, curLineWidth);
                curLineWidth = 0.0f;
                ++lines;
                p += 2;
                continue;
            }
        }

        if (*p == '\n') {
            if (!curSpan.text.empty() || curSpan.newlineBefore) {
                built.spans.push_back(std::move(curSpan));
                curSpan = RichTextSpan{};
            }
            curSpan.newlineBefore = true;
            curSpan.hasColorOverride = hasActiveColor;
            curSpan.colorOverride = activeColor;
            maxLineWidth = std::max(maxLineWidth, curLineWidth);
            curLineWidth = 0.0f;
            ++lines;
            ++p;
            continue;
        }

        const char* prev = p;
        const uint32_t cp = DecodeNextUtf8Codepoint(p);
        curSpan.text.append(prev, (size_t)(p - prev));
        curLineWidth += GetCodepointAdvanceBase(cp);
    }

    if (!curSpan.text.empty() || curSpan.newlineBefore) {
        built.spans.push_back(std::move(curSpan));
    }
    maxLineWidth = std::max(maxLineWidth, curLineWidth);
    built.unscaledWidth = maxLineWidth;
    built.lineCount = lines;

    auto [insIt, _] = layoutCache_.emplace(std::move(key), std::move(built));
    return insIt->second;
}

float TextRenderer::MeasureText(const char* text, float size) const {
    if (!text || (!atlasValid_ && !metricsValid_)) return 0.0f;
    const CachedTextLayout& layout = GetOrBuildLayout(text);
    const float scale = (fontSize_ > 0.0f) ? (size / fontSize_) : 1.0f;
    return layout.unscaledWidth * scale;
}

void TextRenderer::RenderText(UiRenderer& r, float x, float y, const char* text, uint32_t color, float size) {
    if (!text || (!atlasValid_ && !metricsValid_)) return;

    const CachedTextLayout& layout = GetOrBuildLayout(text);
    const float scale = (fontSize_ > 0.0f) ? (size / fontSize_) : 1.0f;
    float baselineY = y + ascender_ * scale;
    float posX = x;
    uint32_t curColor = color;

    for (const RichTextSpan& span : layout.spans) {
        if (span.newlineBefore) {
            posX = x;
            baselineY += lineHeight_ * scale;
        }
        curColor = span.hasColorOverride ? span.colorOverride : color;

        const char* p = span.text.c_str();
        while (*p) {
            const uint32_t cp = DecodeNextUtf8Codepoint(p);
            if (cp >= 32 && cp <= 126 && (cp - 32) < glyphs_.size()) {
                const auto& g = glyphs_[cp - 32];
                if (atlasValid_ && g.bw > 0 && g.bh > 0) {
                    const float qx = posX + g.bl * scale;
                    const float qy = baselineY + g.bt * scale;
                    const float qw = g.bw * scale;
                    const float qh = g.bh * scale;
                    r.DrawTexturedQuad(qx, qy, qw, qh, atlasTex_,
                                       g.tx0, g.ty0, g.tx1, g.ty1, curColor);
                }
                posX += g.ax * scale;
            } else {
                posX += GetCodepointAdvanceBase(cp) * scale;
            }
        }
    }
}

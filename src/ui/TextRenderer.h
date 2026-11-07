#pragma once
#include "UiRenderer.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class MpqManager;

struct RichTextSpan {
    std::string text;
    uint32_t colorOverride = 0;
    bool hasColorOverride = false;
    bool newlineBefore = false;
};

struct CachedTextLayout {
    std::vector<RichTextSpan> spans;
    float unscaledWidth = 0.0f; // Max line width at base fontSize_
    int lineCount = 1;
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    bool LoadFromMpq(MpqManager* mpq, const char* fontPath = nullptr, float fontSize = 24.0f);
    bool LoadFromMemory(const uint8_t* ttfData, int ttfSize, float fontSize);
    // Stage CC1: Initialize deterministic font metrics in headless mode (without GPU context)
    void InitHeadlessMetrics(float fontSize = 24.0f, float asciiAdvanceRatio = 0.5f);
    void Shutdown();

    float MeasureText(const char* text, float size) const;
    float GetAscender() const { return ascender_; }
    float GetLineHeight() const { return lineHeight_; }

    void RenderText(UiRenderer& r, float x, float y, const char* text, uint32_t color, float size);
    bool IsValid() const { return atlasValid_ || metricsValid_; }
    bool HasGpuAtlas() const { return atlasValid_; }

    // Stage CC1 & CC2: O(1) |cAARRGGBB RichTextLayoutCache & UTF-8 / CJK glyph metrics
    static uint32_t DecodeNextUtf8Codepoint(const char*& cursor);
    static bool TryParseColorCodeO1(const char* p, uint32_t& outColor);
    const CachedTextLayout& GetOrBuildLayout(const char* text) const;
    void RegisterUnicodeGlyphAdvance(uint32_t codepoint, float advanceAtBaseSize);
    void ClearLayoutCache() const { layoutCache_.clear(); }
    uint64_t GetLayoutCacheHitCount() const { return layoutCacheHits_; }
    uint64_t GetLayoutCacheMissCount() const { return layoutCacheMisses_; }

private:
    struct GlyphInfo {
        float ax = 0.0f, ay = 0.0f;
        float bw = 0.0f, bh = 0.0f;
        float bl = 0.0f, bt = 0.0f;
        float tx0 = 0.0f, ty0 = 0.0f, tx1 = 0.0f, ty1 = 0.0f;
    };

    void BakeAtlas(const uint8_t* ttfData, float fontSize);
    float GetCodepointAdvanceBase(uint32_t cp) const;

    sg_image atlasTex_ = {};
    bool atlasValid_ = false;
    bool metricsValid_ = false;
    std::vector<GlyphInfo> glyphs_;
    std::unordered_map<uint32_t, GlyphInfo> unicodeGlyphs_;
    int atlasW_ = 0, atlasH_ = 0;
    float ascender_ = 0.0f;
    float lineHeight_ = 0.0f;
    float fontSize_ = 24.0f;

    mutable std::unordered_map<std::string, CachedTextLayout> layoutCache_;
    mutable uint64_t layoutCacheHits_ = 0;
    mutable uint64_t layoutCacheMisses_ = 0;
};

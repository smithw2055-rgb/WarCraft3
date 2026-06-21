#pragma once
#include "UiRenderer.h"
#include <string>
#include <vector>

class MpqManager;

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    bool LoadFromMpq(MpqManager* mpq, const char* fontPath = nullptr, float fontSize = 24.0f);
    bool LoadFromMemory(const uint8_t* ttfData, int ttfSize, float fontSize);
    void Shutdown();

    float MeasureText(const char* text, float size) const;
    float GetAscender() const { return ascender_; }
    float GetLineHeight() const { return lineHeight_; }

    void RenderText(UiRenderer& r, float x, float y, const char* text, uint32_t color, float size);
    bool IsValid() const { return atlasValid_; }

private:
    struct GlyphInfo {
        float ax, ay;
        float bw, bh;
        float bl, bt;
        float tx0, ty0, tx1, ty1;
    };

    void BakeAtlas(const uint8_t* ttfData, float fontSize);

    sg_image atlasTex_ = {};
    bool atlasValid_ = false;
    std::vector<GlyphInfo> glyphs_;
    int atlasW_ = 0, atlasH_ = 0;
    float ascender_ = 0;
    float lineHeight_ = 0;
    float fontSize_ = 0;
};

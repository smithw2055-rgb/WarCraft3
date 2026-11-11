#pragma once
#include "SimpleFrame.h"
#include "../asset/Texture.h"
#include <memory>

class BackdropFrame : public SimpleFrame {
public:
    // Corner/edge mask bits (ref BackdropCornerFlags in warsmash_ref).
    enum Corner : uint8_t {
        CornerBL = 1 << 0,
        CornerBR = 1 << 1,
        CornerUL = 1 << 2,
        CornerUR = 1 << 3,
        EdgeT = 1 << 4,
        EdgeL = 1 << 5,
        EdgeB = 1 << 6,
        EdgeR = 1 << 7,
    };

    static constexpr engine::ui::FrameKind kStaticFrameKind = engine::ui::FrameKind::Backdrop;

    BackdropFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetBackground(const std::shared_ptr<Texture>& tex) { bgTex_ = tex; }
    void SetEdgeFile(const std::shared_ptr<Texture>& tex) { edgeTex_ = tex; }
    void SetCornerSize(float s) { cornerSize_ = s; }
    void SetBackgroundSize(float s) { bgSize_ = s; }
    void SetTileBackground(bool t) { tileBg_ = t; }
    void SetBackgroundInsets(float l, float b, float r, float t);
    void SetCornerFlags(uint8_t mask) { cornerMask_ = mask; }
    static uint8_t ParseCornerFlags(const std::string& text);

    void Render(UiRenderer& ren) override;

private:
    void RenderEdges(UiRenderer& ren) const;

    std::shared_ptr<Texture> bgTex_;
    std::shared_ptr<Texture> edgeTex_;
    float cornerSize_ = 8;
    float bgSize_ = 32;
    bool tileBg_ = false;
    float insetL_ = 0, insetB_ = 0, insetR_ = 0, insetT_ = 0;
    uint8_t cornerMask_ = 0;
};

#pragma once
#include "UIFrame.h"
#include "../asset/Texture.h"
#include <memory>

class BackdropFrame : public UIFrame {
public:
    BackdropFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetBackground(const std::shared_ptr<Texture>& tex) { bgTex_ = tex; }
    void SetEdgeFile(const std::shared_ptr<Texture>& tex) { edgeTex_ = tex; }
    void SetCornerSize(float s) { cornerSize_ = s; }
    void SetBackgroundSize(float s) { bgSize_ = s; }
    void SetTileBackground(bool t) { tileBg_ = t; }
    void SetBackgroundInsets(float l, float b, float r, float t);

    void Render(UiRenderer& ren) override;

private:
    std::shared_ptr<Texture> bgTex_;
    std::shared_ptr<Texture> edgeTex_;
    float cornerSize_ = 8;
    float bgSize_ = 32;
    bool tileBg_ = false;
    float insetL_ = 0, insetB_ = 0, insetR_ = 0, insetT_ = 0;
};

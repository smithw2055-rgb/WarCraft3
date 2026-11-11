#pragma once
#include "UIFrame.h"
#include "../asset/Texture.h"
#include <memory>

class TextureFrame : public UIFrame {
public:
    static constexpr engine::ui::FrameKind kStaticFrameKind = engine::ui::FrameKind::Texture;

    TextureFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTexture(const std::shared_ptr<Texture>& tex) { texture_ = tex; }
    bool HasTexture() const { return texture_ && texture_->IsValid(); }
    void SetTexture(sg_image img) { image_ = img; hasImage_ = true; }
    void SetColor(uint32_t c) { color_ = c; }
    void SetTexCoords(float u0, float v0, float u1, float v1) {
        u0_ = u0; v0_ = v0; u1_ = u1; v1_ = v1;
    }

    void Render(UiRenderer& r) override;

private:
    std::shared_ptr<Texture> texture_;
    sg_image image_ = {};
    bool hasImage_ = false;
    uint32_t color_ = 0xFFFFFFFF;
    float u0_ = 0, v0_ = 0, u1_ = 1, v1_ = 1;
};

#include "TextureFrame.h"

TextureFrame::TextureFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void TextureFrame::Render(UiRenderer& r) {
    if (!visible_) return;
    sg_image img = {};
    if (hasImage_) {
        img = image_;
    } else if (texture_ && texture_->IsValid()) {
        img = texture_->GetImage();
    } else {
        return;
    }
    r.DrawTexturedQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                        img, u0_, v0_, u1_, v1_, color_);
}

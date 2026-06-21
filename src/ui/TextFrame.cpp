#include "TextFrame.h"

TextFrame::TextFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void TextFrame::Render(UiRenderer& r) {
    if (!visible_ || !textRenderer_) return;

    if (hasBg_) {
        r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, bgColor_);
    }

    float tx = bounds_.x;
    float ty = bounds_.y + (bounds_.h - fontSize_) * 0.5f;

    if (align_ != TextAlign::LEFT) {
        float tw = textRenderer_->MeasureText(text_.c_str(), fontSize_);
        if (align_ == TextAlign::CENTER) tx += (bounds_.w - tw) * 0.5f;
        else tx += bounds_.w - tw;
    }

    textRenderer_->RenderText(r, tx, ty, text_.c_str(), color_, fontSize_);
}

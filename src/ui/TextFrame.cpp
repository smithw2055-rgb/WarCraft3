#include "TextFrame.h"

TextFrame::TextFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void TextFrame::SetText(const std::string& t) {
    text_ = t;
    if (assignedW_ == 0 && textRenderer_ && !text_.empty()) {
        bounds_.w = textRenderer_->MeasureText(text_.c_str(), fontSize_);
    }
}

void TextFrame::PositionBounds(GameUI& gui) {
    if (assignedW_ == 0 && textRenderer_ && !text_.empty()) {
        bounds_.w = textRenderer_->MeasureText(text_.c_str(), fontSize_);
    }
    UIFrame::PositionBounds(gui);
}

void TextFrame::Render(UiRenderer& r) {
    if (!visible_ || !textRenderer_) return;

    if (hasBg_) {
        r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, bgColor_);
    }

    float tw = textRenderer_->MeasureText(text_.c_str(), fontSize_);
    if (assignedW_ == 0 && bounds_.w == 0) {
        bounds_.w = tw;
    }

    float tx = bounds_.x;
    float ty = bounds_.y + (bounds_.h - fontSize_) * 0.5f;

    if (align_ != TextAlign::LEFT) {
        if (bounds_.w > 0) {
            if (align_ == TextAlign::CENTER) tx += (bounds_.w - tw) * 0.5f;
            else tx += bounds_.w - tw;
        }
    }

    textRenderer_->RenderText(r, tx, ty, text_.c_str(), color_, fontSize_);
}

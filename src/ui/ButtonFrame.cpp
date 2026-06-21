#include "ButtonFrame.h"

ButtonFrame::ButtonFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void ButtonFrame::Render(UiRenderer& r) {
    if (!visible_) return;

    uint32_t bgColor;
    if (!enabled_) bgColor = colorDisabled_;
    else if (pressed_) bgColor = colorPressed_;
    else if (hovered_) bgColor = colorHover_;
    else bgColor = colorNormal_;

    r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, bgColor);

    if (textRenderer_ && !text_.empty()) {
        uint32_t tc = enabled_ ? textColor_ : disabledTextColor_;
        float tw = textRenderer_->MeasureText(text_.c_str(), fontSize_);
        float tx = bounds_.x + (bounds_.w - tw) * 0.5f;
        float ty = bounds_.y + (bounds_.h - fontSize_) * 0.5f;
        textRenderer_->RenderText(r, tx, ty, text_.c_str(), tc, fontSize_);
    }

    // Bottom highlight when hovered
    if (hovered_ && enabled_) {
        r.DrawQuad(bounds_.x, bounds_.y + bounds_.h - 2, bounds_.w, 2, 0x88FFFFFF);
    }
}

UIFrame* ButtonFrame::OnMouseDown(float /*sx*/, float /*sy*/, int btn) {
    if (!enabled_ || btn != 0) return this;
    pressed_ = true;
    return this;
}

UIFrame* ButtonFrame::OnMouseUp(float /*sx*/, float /*sy*/, int btn) {
    if (btn != 0) return this;
    if (pressed_ && enabled_ && onClick_) {
        onClick_();
    }
    pressed_ = false;
    return this;
}

UIFrame* ButtonFrame::HitTest(float sx, float sy) {
    if (!visible_ || !enabled_ || !bounds_.Contains(sx, sy)) return nullptr;
    hovered_ = true;
    return this;
}

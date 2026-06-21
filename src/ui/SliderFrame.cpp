#include "SliderFrame.h"
#include <algorithm>
#include <cstdio>

SliderFrame::SliderFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

float SliderFrame::GetThumbX() const {
    float ratio = (value_ - min_) / (max_ - min_);
    return bounds_.x + 4.0f + ratio * (bounds_.w - thumbW_ - 8.0f);
}

void SliderFrame::SetValue(float v) {
    value_ = std::clamp(v, min_, max_);
    if (step_ > 0) {
        value_ = std::round(value_ / step_) * step_;
    }
}

void SliderFrame::Render(UiRenderer& r) {
    if (!visible_) return;

    float x = bounds_.x;
    float y = bounds_.y;
    float w = bounds_.w;
    float h = bounds_.h;

    // Label
    if (textRenderer_ && !label_.empty()) {
        textRenderer_->RenderText(r, x, y + 2, label_.c_str(), 0xFFCCCCCC, labelSize_);
    }

    // Track
    float trackY = y + 22.0f;
    float trackH = 6.0f;
    float trackX = x + 4.0f;
    float trackW = w - thumbW_ - 8.0f;
    trackX_ = trackX; trackW_ = trackW; trackY_ = trackY; trackH_ = trackH;

    // Track background
    r.DrawQuad(trackX, trackY, trackW, trackH, 0xFF333333);
    // Track fill
    float fillW = trackW * (value_ - min_) / (max_ - min_);
    if (fillW > 2.0f) {
        r.DrawQuad(trackX + 1, trackY + 1, fillW - 1, trackH - 2, 0xFF4488FF);
    }

    // Thumb
    float thumbX = GetThumbX();
    float thumbY = trackY - (thumbW_ * 0.5f) + trackH * 0.5f;
    r.DrawQuad(thumbX, thumbY, thumbW_, thumbW_, dragging_ ? 0xFFFFFFFF : 0xFFCCCCCC);
    r.DrawQuad(thumbX + 2, thumbY + 2, thumbW_ - 4, thumbW_ - 4, 0xFF4488FF);

    // Value text
    if (textRenderer_) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d%%", (int)(value_ * 100.0f));
        float tw = textRenderer_->MeasureText(buf, 12.0f);
        textRenderer_->RenderText(r, x + w - tw - 4, y + 2, buf, 0xFFAAAAAA, 12.0f);
    }
}

UIFrame* SliderFrame::OnMouseDown(float sx, float /*sy*/, int btn) {
    if (btn != 0) return this;
    dragging_ = true;
    // Compute value from x position
    float ratio = (sx - trackX_) / trackW_;
    SetValue(min_ + ratio * (max_ - min_));
    if (callback_) callback_(value_);
    return this;
}

UIFrame* SliderFrame::OnMouseUp(float /*sx*/, float /*sy*/, int btn) {
    if (btn != 0) return this;
    if (dragging_) {
        dragging_ = false;
        if (callback_) callback_(value_);
    }
    return this;
}

UIFrame* SliderFrame::HitTest(float sx, float sy) {
    if (!visible_ || !bounds_.Contains(sx, sy)) return nullptr;
    return this;
}

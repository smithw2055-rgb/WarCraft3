#include "ButtonFrame.h"

ButtonFrame::ButtonFrame(const std::string& name, UIFrame* parent)
    : SimpleFrame(name, parent) {}

void ButtonFrame::Render(UiRenderer& r) {
    if (!visible_) return;

    // Toggle control backdrops according to button state (ref Java GlueButtonFrame)
    if (controlBackdrop_) {
        bool showNormal = enabled_ && !pressed_;
        bool showPushed = enabled_ && pressed_ && controlPushedBackdrop_;
        bool showDisabled = !enabled_ && controlDisabledBackdrop_;
        controlBackdrop_->SetVisible(showNormal || (!showPushed && !showDisabled));
        if (controlPushedBackdrop_) controlPushedBackdrop_->SetVisible(showPushed);
        if (controlDisabledBackdrop_) controlDisabledBackdrop_->SetVisible(showDisabled);
        if (controlHighlight_) controlHighlight_->SetVisible(hovered_ && enabled_);
    }

    // Textured states (retail SIMPLEBUTTON art) win over flat colors.
    const StateArt* art = nullptr;
    if (!enabled_ && disabled_.valid) art = &disabled_;
    else if (pressed_ && pushed_.valid) art = &pushed_;
    else if (normal_.valid) art = &normal_;
    if (art) {
        if (art->tex && art->tex->IsValid()) {
            r.DrawTexturedQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                                art->tex->GetImage(), art->u0, art->v0, art->u1, art->v1);
        }
        if (hovered_ && enabled_ && highlight_.valid && highlight_.tex &&
            highlight_.tex->IsValid()) {
            r.DrawTexturedQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h,
                                highlight_.tex->GetImage(), highlight_.u0, highlight_.v0,
                                highlight_.u1, highlight_.v1);
        }
    } else if (!controlBackdrop_) {
        uint32_t bgColor;
        if (!enabled_) bgColor = colorDisabled_;
        else if (pressed_) bgColor = colorPressed_;
        else if (hovered_) bgColor = colorHover_;
        else bgColor = colorNormal_;

        r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, bgColor);
    }

    // FDF inner frames (ButtonText labels, backdrops) render.
    SimpleFrame::Render(r);

    if (textRenderer_ && !text_.empty()) {
        uint32_t tc = enabled_ ? textColor_ : disabledTextColor_;
        float tw = textRenderer_->MeasureText(text_.c_str(), fontSize_);
        float tx = bounds_.x + (bounds_.w - tw) * 0.5f;
        float ty = bounds_.y + (bounds_.h - fontSize_) * 0.5f;
        textRenderer_->RenderText(r, tx, ty, text_.c_str(), tc, fontSize_);
    }

    // Bottom highlight when hovered (only if no textured highlight)
    if (hovered_ && enabled_ && !controlHighlight_ && !highlight_.valid) {
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

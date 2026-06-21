#include "PortraitFrame.h"

PortraitFrame::PortraitFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void PortraitFrame::Clear() {
    hasUnit_ = false;
    unitId_.clear();
}

void PortraitFrame::Render(UiRenderer& r) {
    if (!visible_) return;

    // Background (dark gradient-like)
    uint32_t bgColor = hasUnit_ ? 0xFF1A1A2E : 0xFF111111;
    r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, bgColor);

    // Border
    uint32_t borderColor = 0xFF444444;
    r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, 1, borderColor);
    r.DrawQuad(bounds_.x, bounds_.y + bounds_.h - 1, bounds_.w, 1, borderColor);
    r.DrawQuad(bounds_.x, bounds_.y, 1, bounds_.h, borderColor);
    r.DrawQuad(bounds_.x + bounds_.w - 1, bounds_.y, 1, bounds_.h, borderColor);

    // Placeholder: colored circle/diamond for unit type indicator
    if (hasUnit_) {
        float cx = bounds_.x + bounds_.w * 0.5f;
        float cy = bounds_.y + bounds_.h * 0.5f;
        float size = std::min(bounds_.w, bounds_.h) * 0.15f;

        // Simple diamond shape (4 quads approximating a diamond)
        uint32_t glowColor = 0xFF4488FF;
        r.DrawQuad(cx - size, cy, size * 2, 2, glowColor);
        r.DrawQuad(cx, cy - size, 2, size * 2, glowColor);
    } else {
        // No unit - show dim indicator
        float cx = bounds_.x + bounds_.w * 0.5f;
        float cy = bounds_.y + bounds_.h * 0.5f;
        r.DrawQuad(cx - 8, cy, 16, 1, 0xFF444444);
        r.DrawQuad(cx, cy - 8, 1, 16, 0xFF444444);
    }
}

#include "CommandCard.h"
#include <cstdio>
#include <cmath>

// --- CommandCardIcon ---
CommandCardIcon::CommandCardIcon(int row, int col, UIFrame* parent)
    : UIFrame("CmdIcon_" + std::to_string(row) + "_" + std::to_string(col), parent)
    , row_(row), col_(col) {}

void CommandCardIcon::SetIcon(const std::shared_ptr<Texture>& tex) {
    iconTex_ = tex;
}

void CommandCardIcon::Render(UiRenderer& r) {
    if (!visible_) return;

    // Background
    const float* bg = pressed_ ? bgColorPressed_ :
                      highlight_ ? bgColorHover_ : bgColorNormal_;
    uint32_t bgColor =
        ((uint8_t)(bg[3]*255) << 24) |
        ((uint8_t)(bg[2]*255) << 16) |
        ((uint8_t)(bg[1]*255) << 8) |
        (uint8_t)(bg[0]*255);
    r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, bgColor);

    // Icon
    if (iconTex_ && iconTex_->IsValid()) {
        uint32_t iconColor = enabled_ ? 0xFFFFFFFF : 0x80808080;
        float pad = 2;
        r.DrawTexturedQuad(bounds_.x + pad, bounds_.y + pad,
                            bounds_.w - pad * 2, bounds_.h - pad * 2,
                            iconTex_->GetImage(), 0, 0, 1, 1, iconColor);
    }

    // Highlight border (when selected/active)
    if (highlight_) {
        // Top
        r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, 2, 0xFFFFFF00);
        // Bottom
        r.DrawQuad(bounds_.x, bounds_.y + bounds_.h - 2, bounds_.w, 2, 0xFFFFFF00);
        // Left
        r.DrawQuad(bounds_.x, bounds_.y, 2, bounds_.h, 0xFFFFFF00);
        // Right
        r.DrawQuad(bounds_.x + bounds_.w - 2, bounds_.y, 2, bounds_.h, 0xFFFFFF00);
    }
}

UIFrame* CommandCardIcon::OnMouseDown(float /*sx*/, float /*sy*/, int /*btn*/) {
    pressed_ = true;
    return this;
}

UIFrame* CommandCardIcon::OnMouseUp(float /*sx*/, float /*sy*/, int btn) {
    pressed_ = false;
    if (onClick_ && enabled_) {
        onClick_(orderId_, btn == 1);
    }
    return this;
}

// --- CommandCard ---
CommandCard::CommandCard(const std::string& name, UIFrame* parent)
    : SimpleFrame(name, parent) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            auto* icon = new CommandCardIcon(r, c, this);
            icons_[r][c] = icon;
            AddChild(icon);
        }
    }
}

CommandCardIcon* CommandCard::GetIcon(int row, int col) {
    if (row >= 0 && row < ROWS && col >= 0 && col < COLS)
        return icons_[row][col];
    return nullptr;
}

void CommandCard::ClearAll() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            icons_[r][c]->SetVisible(false);
}

void CommandCard::SetIconAt(int row, int col, const std::shared_ptr<Texture>& tex, int orderId) {
    auto* icon = GetIcon(row, col);
    if (!icon) return;
    icon->SetIcon(tex);
    icon->SetOrderId(orderId);
    icon->SetVisible(true);
    icon->SetEnabled(true);
}

void CommandCard::SetHighlightAt(int row, int col, bool h) {
    auto* icon = GetIcon(row, col);
    if (!icon) return;
    icon->SetHighlight(h);
}

void CommandCard::PositionGrid(float startX, float startY, float iconW, float iconH,
                                float gapX, float gapY) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            auto* btn = icons_[r][c];
            float x = startX + c * (iconW + gapX);
            float y = startY + (ROWS - 1 - r) * (iconH + gapY); // bottom-up
            btn->SetSize(iconW, iconH);
            btn->SetPosition(x, y);
        }
    }
}

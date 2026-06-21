#include "BuffBar.h"
#include <algorithm>

BuffBar::BuffBar() {}

BuffBar::~BuffBar() {}

void BuffBar::Init() {}

void BuffBar::SetBuffs(const std::vector<BuffIconInfo>& buffs) {
    buffs_ = buffs;
}

void BuffBar::ClearBuffs() {
    buffs_.clear();
}

void BuffBar::Render(UiRenderer& renderer, int screenW, int screenH) {
    if (buffs_.empty()) return;

    int count = (int)buffs_.size();
    int cols = std::min(count, maxColumns_);
    int rows = (count + maxColumns_ - 1) / maxColumns_;

    float barWidth = cols * (iconSize_ + padding_) + padding_;
    float barHeight = rows * (iconSize_ + padding_) + padding_;

    // Position above the command card (bottom-left area)
    float barX = 20.0f;
    float barY = screenH - 300.0f - barHeight;

    // Background
    renderer.DrawQuad(barX, barY, barWidth, barHeight, 0xCC000000);

    // Draw buff icons
    for (int i = 0; i < count; i++) {
        int col = i % maxColumns_;
        int row = i / maxColumns_;
        float x = barX + padding_ + col * (iconSize_ + padding_);
        float y = barY + padding_ + row * (iconSize_ + padding_);

        auto& buff = buffs_[i];

        // Icon background - green for positive, red for negative
        uint32_t bgColor = buff.isPositive ? 0xFF00AA00 : 0xFFAA0000;
        renderer.DrawQuad(x, y, iconSize_, iconSize_, bgColor);

        // Duration bar (shrinks as buff expires)
        if (buff.duration > 0) {
            float ratio = buff.remaining / buff.duration;
            float durBarWidth = iconSize_ * ratio;
            renderer.DrawQuad(x, y + iconSize_ - 3.0f, durBarWidth, 3.0f, 0xFFFFFFFF);
        }

        // Stack count
        if (buff.stackCount > 1) {
            renderer.DrawQuad(x, y, 14.0f, 12.0f, 0xFF000000);
            // TODO: draw stack count number
        }
    }
}

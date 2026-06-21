#include "MultiSelectionPanel.h"
#include <algorithm>

MultiSelectionPanel::MultiSelectionPanel() {}

MultiSelectionPanel::~MultiSelectionPanel() {}

void MultiSelectionPanel::Init(int screenW, int screenH) {
    (void)screenW; (void)screenH;
}

void MultiSelectionPanel::SetSelection(const std::vector<MultiSelectUnitInfo>& units) {
    units_ = units;
}

void MultiSelectionPanel::ClearSelection() {
    units_.clear();
}

void MultiSelectionPanel::Render(UiRenderer& renderer, int screenW, int screenH) {
    if (units_.empty()) return;

    int count = (int)units_.size();
    int cols = std::min(count, maxColumns_);
    int rows = (count + maxColumns_ - 1) / maxColumns_;

    float panelWidth = cols * (iconSize_ + padding_) + padding_;
    float panelHeight = rows * (iconSize_ + padding_) + padding_;

    // Position at bottom-right of screen
    float panelX = screenW - panelWidth - 10.0f;
    float panelY = screenH - panelHeight - 10.0f;

    // Background
    renderer.DrawQuad(panelX, panelY, panelWidth, panelHeight, 0xCC000000);

    // Draw unit icons
    for (int i = 0; i < count; i++) {
        int col = i % maxColumns_;
        int row = i / maxColumns_;
        float x = panelX + padding_ + col * (iconSize_ + padding_);
        float y = panelY + padding_ + row * (iconSize_ + padding_);

        auto& unit = units_[i];

        // Icon background with player color
        renderer.DrawQuad(x, y, iconSize_, iconSize_, unit.playerColor);

        // HP bar
        float hpBarWidth = iconSize_ - 4.0f;
        float hpBarHeight = 4.0f;
        float hpBarX = x + 2.0f;
        float hpBarY = y + iconSize_ - hpBarHeight - 2.0f;
        renderer.DrawQuad(hpBarX, hpBarY, hpBarWidth, hpBarHeight, 0xFF333333);
        renderer.DrawQuad(hpBarX, hpBarY, hpBarWidth * unit.hpRatio, hpBarHeight, 0xFF00FF00);

        // Hero level indicator
        if (unit.isHero) {
            renderer.DrawQuad(x, y, 16.0f, 12.0f, 0xFF000000);
            // TODO: draw level number
        }
    }
}

int MultiSelectionPanel::HitTest(float x, float y, int screenW, int screenH) const {
    if (units_.empty()) return -1;

    int count = (int)units_.size();
    int cols = std::min(count, maxColumns_);
    int rows = (count + maxColumns_ - 1) / maxColumns_;

    float panelWidth = cols * (iconSize_ + padding_) + padding_;
    float panelHeight = rows * (iconSize_ + padding_) + padding_;
    float panelX = screenW - panelWidth - 10.0f;
    float panelY = screenH - panelHeight - 10.0f;

    // Check if click is inside panel
    if (x < panelX || x > panelX + panelWidth || y < panelY || y > panelY + panelHeight)
        return -1;

    // Find which unit was clicked
    float relX = x - panelX - padding_;
    float relY = y - panelY - padding_;
    int col = (int)(relX / (iconSize_ + padding_));
    int row = (int)(relY / (iconSize_ + padding_));
    int index = row * maxColumns_ + col;

    if (index >= 0 && index < count) {
        // Check if click is actually on the icon (not padding)
        float iconX = panelX + padding_ + col * (iconSize_ + padding_);
        float iconY = panelY + padding_ + row * (iconSize_ + padding_);
        if (x >= iconX && x <= iconX + iconSize_ && y >= iconY && y <= iconY + iconSize_) {
            return index;
        }
    }
    return -1;
}

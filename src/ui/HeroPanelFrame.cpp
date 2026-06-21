#include "HeroPanelFrame.h"
#include <cstdio>
#include <algorithm>

HeroPanelFrame::HeroPanelFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void HeroPanelFrame::Clear() {
    heroName_.clear();
    heroLevel_ = 1;
    xp_ = 0; maxXp_ = 100;
    xpRatio_ = 0.0f;
    strBase_ = strBonus_ = 0;
    agiBase_ = agiBonus_ = 0;
    intBase_ = intBonus_ = 0;
    heroIcon_.reset();
}

void HeroPanelFrame::Render(UiRenderer& r) {
    if (!visible_ || !textRenderer_) return;

    float pad = 6.0f;
    float x = bounds_.x;
    float y = bounds_.y;
    float w = bounds_.w;
    float h = bounds_.h;

    // Background
    r.DrawQuad(x, y, w, h, 0xCC111122);
    r.DrawQuad(x, y, w, 1, 0xFF4488FF);
    r.DrawQuad(x, y + h - 1, w, 1, 0xFF4488FF);
    r.DrawQuad(x, y, 1, h, 0xFF4488FF);
    r.DrawQuad(x + w - 1, y, 1, h, 0xFF4488FF);

    float curY = y + h - pad;
    float leftX = x + pad;
    float labelW = 30.0f;
    float valueW = 30.0f;

    // Hero name + level
    if (!heroName_.empty()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s (Lv.%d)", heroName_.c_str(), heroLevel_);
        float nameSize = 16.0f;
        curY -= nameSize + 2;
        textRenderer_->RenderText(r, leftX, curY, buf, 0xFFFFCC00, nameSize);
    }

    // XP bar
    {
        float barW = w - pad * 2;
        float barH = 8.0f;
        curY -= barH + 4;
        r.DrawQuad(leftX, curY, barW, barH, 0xCC111111);
        float ratio = std::clamp(xpRatio_, 0.0f, 1.0f);
        if (ratio > 0.01f) {
            r.DrawQuad(leftX + 1, curY + 1, (barW - 2) * ratio, barH - 2, 0xFF00CC33);
        }
    }

    // Attributes
    struct Attr { const char* label; int base; int bonus; uint32_t color; };
    Attr attrs[] = {
        {"STR", strBase_, strBonus_, 0xFFFF8800},
        {"AGI", agiBase_, agiBonus_, 0xFF00CC33},
        {"INT", intBase_, intBonus_, 0xFF4488FF},
    };

    float attrY = bounds_.y + pad;
    for (auto& a : attrs) {
        char buf[32];
        float val = (float)(a.base + a.bonus);
        if (a.bonus > 0) {
            std::snprintf(buf, sizeof(buf), "%d+%d", a.base, a.bonus);
        } else {
            std::snprintf(buf, sizeof(buf), "%d", a.base);
        }

        textRenderer_->RenderText(r, leftX, attrY, a.label, a.color, 12.0f);
        textRenderer_->RenderText(r, leftX + 30.0f, attrY, buf, 0xFFDDDDDD, 12.0f);
        attrY += 14.0f;
    }
}

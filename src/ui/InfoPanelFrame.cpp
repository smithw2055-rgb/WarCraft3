#include "InfoPanelFrame.h"
#include <algorithm>
#include <cstdio>

InfoPanelFrame::InfoPanelFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent)
    , nameLabel_("InfoName", this)
    , levelLabel_("InfoLevel", this) {}

void InfoPanelFrame::SetTextRenderer(TextRenderer* tr) {
    textRenderer_ = tr;
    nameLabel_.SetTextRenderer(tr);
    levelLabel_.SetTextRenderer(tr);
}

void InfoPanelFrame::SetUnitName(const std::string& name) {
    unitName_ = name;
    hasUnit_ = true;
}

void InfoPanelFrame::SetUnitLevel(int level) {
    unitLevel_ = level;
    hasUnit_ = true;
}

void InfoPanelFrame::SetHp(float current, float max) {
    hp_ = current;
    maxHp_ = max;
    hasUnit_ = true;
}

void InfoPanelFrame::SetMp(float current, float max) {
    mp_ = current;
    maxMp_ = max;
    hasUnit_ = true;
}

void InfoPanelFrame::Clear() {
    hasUnit_ = false;
    unitName_.clear();
    unitLevel_ = 0;
    hp_ = -1;
    maxHp_ = -1;
    mp_ = -1;
    maxMp_ = -1;
}

uint32_t InfoPanelFrame::LerpColor(uint32_t from, uint32_t to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto lerp = [t](uint8_t a, uint8_t b) -> uint8_t {
        return (uint8_t)((float)a + ((float)b - (float)a) * t);
    };
    return 0xFF000000 |
           ((uint32_t)lerp((from >> 16) & 0xFF, (to >> 16) & 0xFF) << 16) |
           ((uint32_t)lerp((from >> 8) & 0xFF, (to >> 8) & 0xFF) << 8) |
           ((uint32_t)lerp(from & 0xFF, to & 0xFF));
}

void InfoPanelFrame::Render(UiRenderer& r) {
    if (!visible_) return;

    float pad = 6.0f;
    float barH = 14.0f;
    float barGap = 4.0f;
    float textY = bounds_.y + bounds_.h - pad;
    float barY = bounds_.y + pad;

    if (hasUnit_) {
        // Unit name (top area)
        if (textRenderer_ && !unitName_.empty()) {
            char nameBuf[128];
            if (unitLevel_ > 0) {
                std::snprintf(nameBuf, sizeof(nameBuf), "%s (Lv.%d)", unitName_.c_str(), unitLevel_);
            } else {
                std::snprintf(nameBuf, sizeof(nameBuf), "%s", unitName_.c_str());
            }
            float nameSize = 16.0f;
            float nameH = nameSize + 2;
            float nameY = textY - nameH;
            textRenderer_->RenderText(r, bounds_.x + pad, nameY, nameBuf, 0xFFFFFFFF, nameSize);
        }

        // HP bar
        if (hp_ >= 0 && maxHp_ > 0) {
            float ratio = std::clamp(hp_ / maxHp_, 0.0f, 1.0f);
            float barW = bounds_.w - pad * 2;
            float curBarY = barY;

            // Background
            r.DrawQuad(bounds_.x + pad, curBarY, barW, barH, 0xFF333333);
            // Bar color: green -> yellow -> red
            uint32_t barColor;
            if (ratio > 0.6f) barColor = LerpColor(0xFF00FF00, 0xFFFFFF00, (1.0f - ratio) / 0.4f);
            else barColor = LerpColor(0xFFFFFF00, 0xFFFF0000, (1.0f - ratio / 0.6f));
            if (ratio > 0.01f) {
                r.DrawQuad(bounds_.x + pad + 1, curBarY + 1, (barW - 2) * ratio, barH - 2, barColor);
            }
            // HP text
            char hpText[32];
            std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", hp_, maxHp_);
            if (textRenderer_) {
                float tsize = 11.0f;
                float tw = textRenderer_->MeasureText(hpText, tsize);
                textRenderer_->RenderText(r, bounds_.x + pad + (barW - tw) * 0.5f, curBarY + 1, hpText, 0xFFFFFFFF, tsize);
            }
            barY += barH + barGap;
        }

        // MP bar
        if (mp_ >= 0 && maxMp_ > 0) {
            float ratio = std::clamp(mp_ / maxMp_, 0.0f, 1.0f);
            float barW = bounds_.w - pad * 2;
            float curBarY = barY;

            r.DrawQuad(bounds_.x + pad, curBarY, barW, barH, 0xFF333333);
            if (ratio > 0.01f) {
                r.DrawQuad(bounds_.x + pad + 1, curBarY + 1, (barW - 2) * ratio, barH - 2, 0xFF4488FF);
            }
            char mpText[32];
            std::snprintf(mpText, sizeof(mpText), "%.0f / %.0f", mp_, maxMp_);
            if (textRenderer_) {
                float tsize = 11.0f;
                float tw = textRenderer_->MeasureText(mpText, tsize);
                textRenderer_->RenderText(r, bounds_.x + pad + (barW - tw) * 0.5f, curBarY + 1, mpText, 0xFFFFFFFF, tsize);
            }
            barY += barH + barGap;
        }
    } else {
        // No unit selected
        if (textRenderer_) {
            const char* msg = "No Selection";
            float tsize = 14.0f;
            float tw = textRenderer_->MeasureText(msg, tsize);
            textRenderer_->RenderText(r, bounds_.x + (bounds_.w - tw) * 0.5f,
                                     bounds_.y + (bounds_.h - tsize) * 0.5f,
                                     msg, 0xFF888888, tsize);
        }
    }
}

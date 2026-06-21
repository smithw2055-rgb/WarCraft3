#include "ResourceBarFrame.h"
#include <cstdio>

ResourceBarFrame::ResourceBarFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void ResourceBarFrame::Render(UiRenderer& r) {
    if (!visible_ || !textRenderer_) return;

    // Background bar
    r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, 0xCC000000);

    float pad = 8.0f;
    float fontSize = 14.0f;
    float itemGap = 20.0f;
    float textY = bounds_.y + (bounds_.h - fontSize) * 0.5f;
    float curX = bounds_.x + pad;

    auto drawItem = [&](const char* icon, const char* value, uint32_t iconColor) {
        // Draw colored quad as "icon"
        r.DrawQuad(curX, textY + 2, 10, fontSize - 4, iconColor);
        curX += 14;
        textRenderer_->RenderText(r, curX, textY, value, 0xFFFFFFFF, fontSize);
        curX += textRenderer_->MeasureText(value, fontSize) + itemGap;
    };

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d", gold_);
    drawItem("G", buf, 0xFFFFD700);

    std::snprintf(buf, sizeof(buf), "%d", lumber_);
    drawItem("L", buf, 0xFF8B4513);

    std::snprintf(buf, sizeof(buf), "%d/%d", foodUsed_, foodMax_);
    drawItem("F", buf, 0xFF44FF44);

    // Time of day on the right
    int totalMinutes = (int)(timeOfDay_ * 1440.0f);
    int hours = (totalMinutes / 60) % 24;
    int mins = totalMinutes % 60;
    std::snprintf(buf, sizeof(buf), "%02d:%02d", hours, mins);
    float timeW = textRenderer_->MeasureText(buf, fontSize);
    textRenderer_->RenderText(r, bounds_.x + bounds_.w - pad - timeW, textY, buf, 0xFFAAAAAA, fontSize);
}

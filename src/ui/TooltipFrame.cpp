#include "TooltipFrame.h"
#include <algorithm>

TooltipFrame::TooltipFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void TooltipFrame::ShowAt(float screenX, float screenY, int screenW, int screenH) {
    visible_ = true;

    if (!textRenderer_) {
        SetSize(0, 0);
        return;
    }

    float tw = 0.0f;
    float th = pad_ * 2;

    // Title
    float titleW = 0.0f;
    if (!title_.empty()) {
        titleW = textRenderer_->MeasureText(title_.c_str(), fontSize_ + 2);
        th += fontSize_ + 4;
    }

    // Body text (multi-line by \n)
    float bodyW = 0.0f;
    int bodyLines = 1;
    if (!text_.empty()) {
        size_t pos = 0;
        while (pos < text_.size()) {
            size_t nl = text_.find('\n', pos);
            std::string line = text_.substr(pos, nl - pos);
            float lw = textRenderer_->MeasureText(line.c_str(), fontSize_);
            if (lw > bodyW) bodyW = lw;
            if (nl == std::string::npos) break;
            pos = nl + 1;
            bodyLines++;
        }
        th += (float)bodyLines * (fontSize_ + 2);
    }

    tw = std::max(titleW, bodyW) + pad_ * 2;
    tw = std::min(tw, maxWidth_);
    SetSize(tw, th);

    // Position near cursor, avoiding screen edges
    float px = screenX + 12.0f;
    float py = screenY - th - 8.0f;
    if (px + tw > (float)screenW) px = (float)screenW - tw - 4.0f;
    if (py < 0) py = screenY + 12.0f;
    SetPosition(px, py);
}

void TooltipFrame::Render(UiRenderer& r) {
    if (!visible_ || !textRenderer_) return;

    // Background
    r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, bounds_.h, 0xEE101018);
    // Border
    r.DrawQuad(bounds_.x, bounds_.y, bounds_.w, 1, 0xCC6A5A2A);
    r.DrawQuad(bounds_.x, bounds_.y + bounds_.h - 1, bounds_.w, 1, 0xCC6A5A2A);
    r.DrawQuad(bounds_.x, bounds_.y, 1, bounds_.h, 0xCC6A5A2A);
    r.DrawQuad(bounds_.x + bounds_.w - 1, bounds_.y, 1, bounds_.h, 0xCC6A5A2A);

    float curY = bounds_.y + pad_;

    if (!title_.empty()) {
        textRenderer_->RenderText(r, bounds_.x + pad_, curY, title_.c_str(), 0xFFFFCC00, fontSize_ + 2);
        curY += fontSize_ + 4;
    }

    if (!text_.empty()) {
        size_t pos = 0;
        while (pos < text_.size()) {
            size_t nl = text_.find('\n', pos);
            std::string line = text_.substr(pos, nl - pos);
            textRenderer_->RenderText(r, bounds_.x + pad_, curY, line.c_str(), 0xFFDDDDDD, fontSize_);
            curY += fontSize_ + 2;
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
    }
}

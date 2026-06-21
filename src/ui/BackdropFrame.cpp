#include "BackdropFrame.h"

BackdropFrame::BackdropFrame(const std::string& name, UIFrame* parent)
    : UIFrame(name, parent) {}

void BackdropFrame::SetBackgroundInsets(float l, float b, float r, float t) {
    insetL_ = l; insetB_ = b; insetR_ = r; insetT_ = t;
}

void BackdropFrame::Render(UiRenderer& ren) {
    if (!visible_) return;

    if (bgTex_ && bgTex_->IsValid()) {
        float bx = bounds_.x + insetL_;
        float by = bounds_.y + insetB_;
        float bw = bounds_.w - insetL_ - insetR_;
        float bh = bounds_.h - insetB_ - insetT_;

        if (tileBg_ && bgSize_ > 0) {
            float curX = bx;
            while (curX < bx + bw) {
                float curY = by;
                float tileW = std::min(bgSize_, bx + bw - curX);
                while (curY < by + bh) {
                    float tileH = std::min(bgSize_, by + bh - curY);
                    ren.DrawTexturedQuad(curX, curY, tileW, tileH,
                                          bgTex_->GetImage(), 0, 1.0f - tileH / bgSize_,
                                          tileW / bgSize_, 1.0f);
                    curY += bgSize_;
                }
                curX += bgSize_;
            }
        } else {
            ren.DrawTexturedQuad(bx, by, bw, bh, bgTex_->GetImage());
        }
    }
}

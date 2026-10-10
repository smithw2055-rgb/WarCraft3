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
        float by = bounds_.y + insetT_;
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
    RenderEdges(ren);
}

uint8_t BackdropFrame::ParseCornerFlags(const std::string& text) {
    uint8_t mask = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('|', pos);
        std::string tok = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t')) tok.erase(tok.begin());
        while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t')) tok.pop_back();
        if (tok == "BL") mask |= CornerBL;
        else if (tok == "BR") mask |= CornerBR;
        else if (tok == "UL") mask |= CornerUL;
        else if (tok == "UR") mask |= CornerUR;
        else if (tok == "T") mask |= EdgeT;
        else if (tok == "L") mask |= EdgeL;
        else if (tok == "B") mask |= EdgeB;
        else if (tok == "R") mask |= EdgeR;
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return mask;
}

void BackdropFrame::RenderEdges(UiRenderer& ren) const {
    // Retail edge strip: 8 vertical slices (ref BackdropFrame in
    // warsmash_ref). Corners BL/BR/UL/UR use slices 6/7/4/5; L/R edges tile
    // slices 0/1 vertically; B/T edges tile slices 3/2 transposed (the Java
    // 270-degree tile rotation equals a UV transpose).
    if (!edgeTex_ || !edgeTex_->IsValid() || cornerSize_ <= 0) return;
    const sg_image img = edgeTex_->GetImage();
    const float x = bounds_.x, y = bounds_.y, w = bounds_.w, h = bounds_.h;
    if (w <= 0 || h <= 0) return;
    // Clamp cs so corners don't overlap or underflow on small frames/buttons
    const float cs = std::min(cornerSize_, std::min(w * 0.5f, h * 0.5f));
    if (cs <= 0) return;

    auto slice = [&](int k, float qx, float qy, float qw, float qh) {
        ren.DrawTexturedQuad(qx, qy, qw, qh, img, k / 8.0f, 0.0f, (k + 1) / 8.0f, 1.0f);
    };
    auto sliceV = [&](int k, float qx, float qy, float qw, float qh, float vFrac) {
        // Partial-height tile shows the bottom vFrac of the slice.
        ren.DrawTexturedQuad(qx, qy, qw, qh, img, k / 8.0f, 1.0f - vFrac, (k + 1) / 8.0f, 1.0f);
    };
    if (cornerMask_ & CornerBL) slice(6, x, y + h - cs, cs, cs);
    if (cornerMask_ & CornerBR) slice(7, x + w - cs, y + h - cs, cs, cs);
    if (cornerMask_ & CornerUL) slice(4, x, y, cs, cs);
    if (cornerMask_ & CornerUR) slice(5, x + w - cs, y, cs, cs);

    float xNeed = x, wNeed = w, yNeed = y, hNeed = h;
    if (cornerMask_ & (CornerUL | CornerUR)) {
        yNeed = y + cs;
        hNeed -= cs;
    }
    if (cornerMask_ & (CornerBL | CornerBR)) {
        hNeed -= cs;
    }
    if (cornerMask_ & (CornerUL | CornerBL)) {
        xNeed = x + cs;
        wNeed -= cs;
    }
    if (cornerMask_ & (CornerUR | CornerBR)) {
        wNeed -= cs;
    }
    if (wNeed < 0) wNeed = 0;
    if (hNeed < 0) hNeed = 0;
    // Vertical edges tile bottom-up from edgeBottom - cs downward... in
    // y-down coords tile i occupies [edgeBottom - cs*(i+1), edgeBottom - cs*i].
    const float edgeBottom = yNeed + hNeed;
    if (cornerMask_ & EdgeL) {
        int full = (int)(hNeed / cs);
        float rem = hNeed - full * cs;
        for (int i = 0; i < full; ++i) {
            slice(0, x, edgeBottom - cs * (i + 1), cs, cs);
        }
        if (rem > 0.5f) {
            sliceV(0, x, edgeBottom - full * cs - rem, cs, rem, rem / cs);
        }
    }
    if (cornerMask_ & EdgeR) {
        int full = (int)(hNeed / cs);
        float rem = hNeed - full * cs;
        for (int i = 0; i < full; ++i) {
            slice(1, x + w - cs, edgeBottom - cs * (i + 1), cs, cs);
        }
        if (rem > 0.5f) {
            sliceV(1, x + w - cs, edgeBottom - full * cs - rem, cs, rem, rem / cs);
        }
    }
    // Horizontal edges tile left-to-right with transposed slices (the strip
    // art is authored vertically; ref Java's 270-degree tile rotation, whose
    // quads match cornerSize thickness and tile along the border).
    auto hEdge = [&](int slice, float qy) {
        const float th = cs;
        int full = (int)(wNeed / cs);
        float rem = wNeed - full * cs;
        for (int i = 0; i < full; ++i) {
            ren.DrawTransposedQuad(xNeed + cs * i, qy, cs, th, img, slice / 8.0f, 0.0f,
                                   (slice + 1) / 8.0f, 1.0f);
        }
        if (rem > 0.5f) {
            const float frac = rem / cs;
            ren.DrawTransposedQuad(xNeed + full * cs, qy, rem, th, img, slice / 8.0f, 0.0f,
                                   (slice + 1) / 8.0f, frac);
        }
    };
    if (cornerMask_ & EdgeB) hEdge(3, y + h - cs);
    if (cornerMask_ & EdgeT) hEdge(2, y);
}

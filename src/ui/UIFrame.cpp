#include "UIFrame.h"
#include <algorithm>
#include <cstdio>

// --- GameUI ---
bool GameUI::Init(int w, int h) {
    screenW = w;
    screenH = h;
    return renderer.Init(w, h);
}

void GameUI::Update(int w, int h) {
    screenW = w;
    screenH = h;
}

void GameUI::Render() {
    renderer.BeginFrame(screenW, screenH);
    if (rootFrame && rootFrame->IsVisible()) {
        rootFrame->PositionBounds(*this);
        rootFrame->Render(renderer);
    }
    renderer.EndFrame();
}

// --- UIFrame helper priorities ---
static const FramePoint kLeftPrio[] = { FramePoint::LEFT, FramePoint::TOPLEFT, FramePoint::BOTTOMLEFT };
static const FramePoint kRightPrio[] = { FramePoint::RIGHT, FramePoint::TOPRIGHT, FramePoint::BOTTOMRIGHT };
static const FramePoint kTopPrio[] = { FramePoint::TOP, FramePoint::TOPLEFT, FramePoint::TOPRIGHT };
static const FramePoint kBottomPrio[] = { FramePoint::BOTTOM, FramePoint::BOTTOMLEFT, FramePoint::BOTTOMRIGHT };
static const FramePoint kCenterHPrio[] = { FramePoint::CENTER, FramePoint::TOP, FramePoint::BOTTOM };
static const FramePoint kCenterVPrio[] = { FramePoint::CENTER, FramePoint::LEFT, FramePoint::RIGHT };

static const FramePointAssign* FindAssign(const std::unordered_map<FramePoint, FramePointAssign>& map,
                                           const FramePoint* prios, int n) {
    for (int i = 0; i < n; i++) {
        auto it = map.find(prios[i]);
        if (it != map.end()) return &it->second;
    }
    return nullptr;
}

UIFrame::UIFrame(const std::string& name, UIFrame* parent)
    : name_(name), parent_(parent) {}

void UIFrame::Render(UiRenderer& /*r*/) {}

void UIFrame::SetAllPoints() {
    auto add = [&](FramePoint p) {
        assignments_[p] = { p, "", p, 0, 0 };
    };
    add(FramePoint::TOPLEFT); add(FramePoint::TOP); add(FramePoint::TOPRIGHT);
    add(FramePoint::LEFT); add(FramePoint::CENTER); add(FramePoint::RIGHT);
    add(FramePoint::BOTTOMLEFT); add(FramePoint::BOTTOM); add(FramePoint::BOTTOMRIGHT);
}

void UIFrame::SetAllPoints(float inset) {
    assignments_[FramePoint::TOPLEFT]    = { FramePoint::TOPLEFT, "", FramePoint::TOPLEFT, inset, -inset };
    assignments_[FramePoint::LEFT]       = { FramePoint::LEFT, "", FramePoint::LEFT, inset, 0 };
    assignments_[FramePoint::BOTTOMLEFT] = { FramePoint::BOTTOMLEFT, "", FramePoint::BOTTOMLEFT, inset, inset };
    assignments_[FramePoint::BOTTOM]     = { FramePoint::BOTTOM, "", FramePoint::BOTTOM, 0, inset };
    assignments_[FramePoint::BOTTOMRIGHT]= { FramePoint::BOTTOMRIGHT, "", FramePoint::BOTTOMRIGHT, -inset, inset };
    assignments_[FramePoint::RIGHT]      = { FramePoint::RIGHT, "", FramePoint::RIGHT, -inset, 0 };
    assignments_[FramePoint::TOPRIGHT]   = { FramePoint::TOPRIGHT, "", FramePoint::TOPRIGHT, -inset, -inset };
    assignments_[FramePoint::TOP]        = { FramePoint::TOP, "", FramePoint::TOP, 0, -inset };
    assignments_[FramePoint::CENTER]     = { FramePoint::CENTER, "", FramePoint::CENTER, 0, 0 };
}

void UIFrame::AddSetPoint(FramePoint myPt, const std::string& other, FramePoint otherPt, float ox, float oy) {
    assignments_[myPt] = { myPt, other, otherPt, ox, oy };
}
void UIFrame::AddAnchor(FramePoint pt, float ox, float oy) {
    assignments_[pt] = { pt, "", pt, ox, oy };
}

float UIFrame::GetFrameX(FramePoint pt) const {
    switch (pt) {
        case FramePoint::CENTER: case FramePoint::BOTTOM: case FramePoint::TOP:
            return bounds_.x + bounds_.w / 2;
        case FramePoint::BOTTOMLEFT: case FramePoint::LEFT: case FramePoint::TOPLEFT:
            return bounds_.x;
        case FramePoint::BOTTOMRIGHT: case FramePoint::RIGHT: case FramePoint::TOPRIGHT:
            return bounds_.x + bounds_.w;
    }
    return bounds_.x;
}

float UIFrame::GetFrameY(FramePoint pt) const {
    switch (pt) {
        case FramePoint::LEFT: case FramePoint::CENTER: case FramePoint::RIGHT:
            return bounds_.y + bounds_.h / 2;
        case FramePoint::BOTTOMLEFT: case FramePoint::BOTTOM: case FramePoint::BOTTOMRIGHT:
            return bounds_.y;
        case FramePoint::TOPLEFT: case FramePoint::TOP: case FramePoint::TOPRIGHT:
            return bounds_.y + bounds_.h;
    }
    return bounds_.y;
}

static float resolveTargetX(const FramePointAssign& asgn, UIFrame* target) {
    if (!target) return 0;
    float tx = target->GetFrameX(asgn.targetPoint) + asgn.x;
    return tx;
}
static float resolveTargetY(const FramePointAssign& asgn, UIFrame* target) {
    if (!target) return 0;
    return target->GetFrameY(asgn.targetPoint) + asgn.y;
}

void UIFrame::SetFrameX(FramePoint pt, float x) {
    auto lp = FindAssign(assignments_, kLeftPrio, 3);
    auto rp = FindAssign(assignments_, kRightPrio, 3);
    switch (pt) {
        case FramePoint::CENTER: case FramePoint::BOTTOM: case FramePoint::TOP:
            bounds_.x = x - bounds_.w / 2;
            break;
        case FramePoint::BOTTOMLEFT: case FramePoint::LEFT: case FramePoint::TOPLEFT:
            if (rp) { float oldR = bounds_.x + bounds_.w; bounds_.x = x; bounds_.w = oldR - x; }
            else bounds_.x = x;
            break;
        case FramePoint::BOTTOMRIGHT: case FramePoint::RIGHT: case FramePoint::TOPRIGHT:
            if (lp) bounds_.w = x - bounds_.x;
            else { bounds_.x = x - bounds_.w; }
            break;
        default: break;
    }
}

void UIFrame::SetFrameY(FramePoint pt, float y) {
    auto tp = FindAssign(assignments_, kTopPrio, 3);
    auto bp = FindAssign(assignments_, kBottomPrio, 3);
    switch (pt) {
        case FramePoint::LEFT: case FramePoint::CENTER: case FramePoint::RIGHT:
            bounds_.y = y - bounds_.h / 2;
            break;
        case FramePoint::TOPLEFT: case FramePoint::TOP: case FramePoint::TOPRIGHT:
            bounds_.y = y - bounds_.h;
            break;
        case FramePoint::BOTTOMLEFT: case FramePoint::BOTTOM: case FramePoint::BOTTOMRIGHT:
            if (tp) { float oldB = bounds_.y + bounds_.h; bounds_.y = y; bounds_.h = oldB - y; }
            else bounds_.y = y;
            break;
        default: break;
    }
}

void UIFrame::PositionBounds(GameUI& gui) {
    if (!parent_) return;

    auto lp = FindAssign(assignments_, kLeftPrio, 3);
    auto rp = FindAssign(assignments_, kRightPrio, 3);
    auto tp = FindAssign(assignments_, kTopPrio, 3);
    auto bp = FindAssign(assignments_, kBottomPrio, 3);
    auto chp = FindAssign(assignments_, kCenterHPrio, 3);
    auto cvp = FindAssign(assignments_, kCenterVPrio, 3);

    // Resolve target frames
    UIFrame* parent = parent_;

    auto resolveTarget = [&](const FramePointAssign* a) -> UIFrame* {
        if (!a) return nullptr;
        if (a->targetName.empty()) return parent;
        auto it = gui.frameRegistry.find(a->targetName);
        return it != gui.frameRegistry.end() ? it->second : parent;
    };

    // X position
    UIFrame* lTarget = resolveTarget(lp);
    UIFrame* rTarget = resolveTarget(rp);
    UIFrame* chTarget = resolveTarget(chp);

    if (lp) {
        bounds_.x = resolveTargetX(*lp, lTarget);
        if (assignedW_ == 0) {
            if (rp) bounds_.w = resolveTargetX(*rp, rTarget) - bounds_.x;
            else if (chp) bounds_.w = (resolveTargetX(*chp, chTarget) - bounds_.x) * 2;
        }
    } else if (rp) {
        bounds_.x = resolveTargetX(*rp, rTarget) - bounds_.w;
        if (chp) bounds_.w = (bounds_.x - resolveTargetX(*chp, chTarget)) * 2;
    } else if (chp) {
        bounds_.x = resolveTargetX(*chp, chTarget) - bounds_.w / 2;
    }

    // Y position
    UIFrame* bTarget = resolveTarget(bp);
    UIFrame* tTarget = resolveTarget(tp);
    UIFrame* cvTarget = resolveTarget(cvp);

    if (bp) {
        bounds_.y = resolveTargetY(*bp, bTarget);
        if (assignedH_ == 0) {
            if (tp) bounds_.h = resolveTargetY(*tp, tTarget) - bounds_.y;
            else if (cvp) bounds_.h = (resolveTargetY(*cvp, cvTarget) - bounds_.y) * 2;
        }
    } else if (tp) {
        bounds_.y = resolveTargetY(*tp, tTarget) - bounds_.h;
        if (cvp) bounds_.h = (bounds_.y - resolveTargetY(*cvp, cvTarget)) * 2;
    } else if (cvp) {
        bounds_.y = resolveTargetY(*cvp, cvTarget) - bounds_.h / 2;
    }
}

UIFrame* UIFrame::HitTest(float sx, float sy) {
    if (!visible_) return nullptr;
    return bounds_.Contains(sx, sy) ? this : nullptr;
}

UIFrame* UIFrame::OnMouseDown(float /*sx*/, float /*sy*/, int /*btn*/) { return this; }
UIFrame* UIFrame::OnMouseUp(float /*sx*/, float /*sy*/, int /*btn*/) { return this; }

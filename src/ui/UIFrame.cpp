#include "UIFrame.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// --- GameUI ---
bool GameUI::Init(int w, int h) {
    screenW = w;
    screenH = h;
    lastLayoutW_ = w;
    lastLayoutH_ = h;
    return renderer.Init(w, h);
}

void GameUI::Update(int w, int h) {
    if (screenW != w || screenH != h) {
        screenW = w;
        screenH = h;
        if (rootFrame) {
            rootFrame->MarkLayoutDirty();
        }
    }
}

void GameUI::Render() {
    renderer.BeginFrame(screenW, screenH);
    if (rootFrame && rootFrame->IsVisible()) {
        if (lastLayoutW_ != screenW || lastLayoutH_ != screenH) {
            rootFrame->MarkLayoutDirty();
            lastLayoutW_ = screenW;
            lastLayoutH_ = screenH;
        }
        if (rootFrame->IsLayoutDirty()) {
            rootFrame->PositionBounds(*this);
            layoutPassCount_++;
        }
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

void UIFrame::AddChild(UIFrame* child) {
    if (!child || child == this) return;
    if (child->parent_ && child->parent_ != this) {
        child->parent_->RemoveChild(child);
    }
    if (std::find(children_.begin(), children_.end(), child) == children_.end()) {
        children_.push_back(child);
        childrenZOrderDirty_ = true;
    }
    child->parent_ = this;
    child->MarkLayoutDirty();
}

void UIFrame::RemoveChild(UIFrame* child) {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        children_.erase(it);
        MarkLayoutDirty();
    }
}

void UIFrame::SetZOrder(int z) {
    if (zOrder_ == z) return;
    zOrder_ = z;
    if (parent_) {
        parent_->childrenZOrderDirty_ = true;
    }
}

void UIFrame::SortChildrenByZOrderIfNeeded() {
    if (!childrenZOrderDirty_) return;
    std::stable_sort(children_.begin(), children_.end(),
        [](const UIFrame* a, const UIFrame* b) {
            int za = a ? a->GetZOrder() : 0;
            int zb = b ? b->GetZOrder() : 0;
            return za < zb;
        });
    childrenZOrderDirty_ = false;
}

void UIFrame::MarkLayoutDirty() {
    layoutDirty_ = true;
    for (auto* c : children_) {
        if (c && !c->layoutDirty_) {
            c->MarkLayoutDirty();
        }
    }
    // Also mark ancestor chain dirty so rootFrame->IsLayoutDirty() detects deep changes
    UIFrame* p = parent_;
    while (p) {
        p->layoutDirty_ = true;
        p = p->parent_;
    }
}

void UIFrame::Render(UiRenderer& r) {
    if (!visible_) return;
    SortChildrenByZOrderIfNeeded();
    for (auto* c : children_) {
        if (c && c->IsVisible()) {
            c->Render(r);
        }
    }
}

void UIFrame::SetAllPoints() {
    auto add = [&](FramePoint p) {
        assignments_[p] = { p, "", p, 0, 0 };
    };
    add(FramePoint::TOPLEFT); add(FramePoint::TOP); add(FramePoint::TOPRIGHT);
    add(FramePoint::LEFT); add(FramePoint::CENTER); add(FramePoint::RIGHT);
    add(FramePoint::BOTTOMLEFT); add(FramePoint::BOTTOM); add(FramePoint::BOTTOMRIGHT);
    MarkLayoutDirty();
}

void UIFrame::SetAllPoints(float inset) {
    // Y-down screen space: TOP edge is bounds_.y, BOTTOM edge is y+h.
    assignments_[FramePoint::TOPLEFT]    = { FramePoint::TOPLEFT, "", FramePoint::TOPLEFT, inset, inset };
    assignments_[FramePoint::LEFT]       = { FramePoint::LEFT, "", FramePoint::LEFT, inset, 0 };
    assignments_[FramePoint::BOTTOMLEFT] = { FramePoint::BOTTOMLEFT, "", FramePoint::BOTTOMLEFT, inset, -inset };
    assignments_[FramePoint::BOTTOM]     = { FramePoint::BOTTOM, "", FramePoint::BOTTOM, 0, -inset };
    assignments_[FramePoint::BOTTOMRIGHT]= { FramePoint::BOTTOMRIGHT, "", FramePoint::BOTTOMRIGHT, -inset, -inset };
    assignments_[FramePoint::RIGHT]      = { FramePoint::RIGHT, "", FramePoint::RIGHT, -inset, 0 };
    assignments_[FramePoint::TOPRIGHT]   = { FramePoint::TOPRIGHT, "", FramePoint::TOPRIGHT, -inset, inset };
    assignments_[FramePoint::TOP]        = { FramePoint::TOP, "", FramePoint::TOP, 0, inset };
    assignments_[FramePoint::CENTER]     = { FramePoint::CENTER, "", FramePoint::CENTER, 0, 0 };
    MarkLayoutDirty();
}

void UIFrame::AddSetPoint(FramePoint myPt, const std::string& other, FramePoint otherPt, float ox, float oy) {
    assignments_[myPt] = { myPt, other, otherPt, ox, oy };
    MarkLayoutDirty();
}
void UIFrame::AddAnchor(FramePoint pt, float ox, float oy) {
    assignments_[pt] = { pt, "", pt, ox, oy };
    MarkLayoutDirty();
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
    // Y-down screen space (ref FDF bottom-left origin mapped at inflate):
    // TOP edge is bounds_.y, BOTTOM edge is bounds_.y + bounds_.h.
    switch (pt) {
        case FramePoint::LEFT: case FramePoint::CENTER: case FramePoint::RIGHT:
            return bounds_.y + bounds_.h / 2;
        case FramePoint::TOPLEFT: case FramePoint::TOP: case FramePoint::TOPRIGHT:
            return bounds_.y;
        case FramePoint::BOTTOMLEFT: case FramePoint::BOTTOM: case FramePoint::BOTTOMRIGHT:
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
    MarkLayoutDirty();
}

void UIFrame::SetFrameY(FramePoint pt, float y) {
    auto tp = FindAssign(assignments_, kTopPrio, 3);
    auto bp = FindAssign(assignments_, kBottomPrio, 3);
    switch (pt) {
        case FramePoint::LEFT: case FramePoint::CENTER: case FramePoint::RIGHT:
            bounds_.y = y - bounds_.h / 2;
            break;
        case FramePoint::TOPLEFT: case FramePoint::TOP: case FramePoint::TOPRIGHT:
            if (bp) { float oldB = bounds_.y + bounds_.h; bounds_.y = y; bounds_.h = oldB - y; }
            else bounds_.y = y;
            break;
        case FramePoint::BOTTOMLEFT: case FramePoint::BOTTOM: case FramePoint::BOTTOMRIGHT:
            if (tp) bounds_.h = y - bounds_.y;
            else bounds_.y = y - bounds_.h;
            break;
        default: break;
    }
    MarkLayoutDirty();
}

void UIFrame::ComputeSelfPositionBounds(GameUI& gui) {
    layoutComputeCount_++;
    if (!parent_) return;

    auto lp = FindAssign(assignments_, kLeftPrio, 3);
    auto rp = FindAssign(assignments_, kRightPrio, 3);
    auto tp = FindAssign(assignments_, kTopPrio, 3);
    auto bp = FindAssign(assignments_, kBottomPrio, 3);
    auto chp = FindAssign(assignments_, kCenterHPrio, 3);
    auto cvp = FindAssign(assignments_, kCenterVPrio, 3);

    if (!lp && !rp && !tp && !bp && !chp && !cvp) {
        // No positioning info: FDF-built frames snap to the parent origin
        // (Java parity); explicitly placed code frames never take this path.
        if (defaultToParent_) {
            bounds_.x = parent_->GetFrameX(FramePoint::TOPLEFT);
            bounds_.y = parent_->GetFrameY(FramePoint::TOPLEFT);
        }
        return;
    }

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

    // Y position (y-down: TOP edge is bounds_.y, BOTTOM edge is y+h)
    UIFrame* bTarget = resolveTarget(bp);
    UIFrame* tTarget = resolveTarget(tp);
    UIFrame* cvTarget = resolveTarget(cvp);

    if (bp && tp) {
        float yTop = resolveTargetY(*tp, tTarget);
        float yBot = resolveTargetY(*bp, bTarget);
        bounds_.y = std::min(yTop, yBot);
        if (assignedH_ == 0) bounds_.h = std::fabs(yBot - yTop);
    } else if (bp) {
        bounds_.y = resolveTargetY(*bp, bTarget) - bounds_.h;
    } else if (tp) {
        bounds_.y = resolveTargetY(*tp, tTarget);
    } else if (cvp) {
        bounds_.y = resolveTargetY(*cvp, cvTarget) - bounds_.h / 2;
    }
}

void UIFrame::PositionBounds(GameUI& gui) {
    if (!visible_) return;
    ComputeSelfPositionBounds(gui);
    SortChildrenByZOrderIfNeeded();
    for (auto* c : children_) {
        if (c) c->PositionBounds(gui);
    }
    layoutDirty_ = false;
}

UIFrame* UIFrame::HitTest(float sx, float sy) {
    if (!visible_ || !bounds_.Contains(sx, sy)) return nullptr;
    SortChildrenByZOrderIfNeeded();
    for (int i = (int)children_.size() - 1; i >= 0; --i) {
        if (children_[i]) {
            if (auto* hit = children_[i]->HitTest(sx, sy)) {
                return hit;
            }
        }
    }
    return this;
}

UIFrame* UIFrame::OnMouseDown(float sx, float sy, int btn) {
    SortChildrenByZOrderIfNeeded();
    for (int i = (int)children_.size() - 1; i >= 0; --i) {
        if (children_[i]) {
            if (auto* hit = children_[i]->HitTest(sx, sy)) {
                return hit->OnMouseDown(sx, sy, btn);
            }
        }
    }
    return this;
}

UIFrame* UIFrame::OnMouseUp(float sx, float sy, int btn) {
    SortChildrenByZOrderIfNeeded();
    for (int i = (int)children_.size() - 1; i >= 0; --i) {
        if (children_[i]) {
            if (auto* hit = children_[i]->HitTest(sx, sy)) {
                return hit->OnMouseUp(sx, sy, btn);
            }
        }
    }
    return this;
}

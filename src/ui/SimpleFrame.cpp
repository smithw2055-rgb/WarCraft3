#include "SimpleFrame.h"
#include <algorithm>

SimpleFrame::~SimpleFrame() {
    for (auto* c : children_) delete c;
}

void SimpleFrame::AddChild(UIFrame* child) {
    if (!child) return;
    children_.push_back(child);
    child->SetParent(this);
}

void SimpleFrame::RemoveChild(UIFrame* child) {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) children_.erase(it);
}

void SimpleFrame::Render(UiRenderer& r) {
    if (!visible_) return;
    for (auto* c : children_) {
        if (c->IsVisible()) c->Render(r);
    }
}

void SimpleFrame::PositionBounds(GameUI& gui) {
    if (!visible_) return;
    UIFrame::PositionBounds(gui);
    for (auto* c : children_) {
        c->PositionBounds(gui);
    }
}

UIFrame* SimpleFrame::HitTest(float sx, float sy) {
    if (!visible_ || !bounds_.Contains(sx, sy)) return nullptr;
    for (int i = (int)children_.size() - 1; i >= 0; i--) {
        auto* hit = children_[i]->HitTest(sx, sy);
        if (hit) return hit;
    }
    return this;
}

UIFrame* SimpleFrame::OnMouseDown(float sx, float sy, int btn) {
    for (int i = (int)children_.size() - 1; i >= 0; i--) {
        auto* hit = children_[i]->HitTest(sx, sy);
        if (hit) return hit->OnMouseDown(sx, sy, btn);
    }
    return this;
}

UIFrame* SimpleFrame::OnMouseUp(float sx, float sy, int btn) {
    for (int i = (int)children_.size() - 1; i >= 0; i--) {
        auto* hit = children_[i]->HitTest(sx, sy);
        if (hit) return hit->OnMouseUp(sx, sy, btn);
    }
    return this;
}

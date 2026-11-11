#pragma once
#include "UIFrame.h"
#include <vector>

class SimpleFrame : public UIFrame {
public:
    static constexpr engine::ui::FrameKind kStaticFrameKind = engine::ui::FrameKind::Simple;

    SimpleFrame(const std::string& name,
                UIFrame* parent = nullptr,
                engine::ui::FrameKind leafKind = engine::ui::FrameKind::Simple)
        : UIFrame(name, parent, leafKind) {}

    ~SimpleFrame() override {
        for (auto* c : children_) {
            if (c) {
                c->SetParent(nullptr);
                delete c;
            }
        }
        children_.clear();
    }
};

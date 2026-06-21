#pragma once
#include "UIFrame.h"
#include <vector>

class SimpleFrame : public UIFrame {
public:
    SimpleFrame(const std::string& name, UIFrame* parent = nullptr)
        : UIFrame(name, parent) {}
    ~SimpleFrame() override;

    void AddChild(UIFrame* child);
    void RemoveChild(UIFrame* child);

    void Render(UiRenderer& r) override;
    void PositionBounds(GameUI& gui) override;

    UIFrame* HitTest(float sx, float sy) override;
    UIFrame* OnMouseDown(float sx, float sy, int btn) override;
    UIFrame* OnMouseUp(float sx, float sy, int btn) override;

    const auto& GetChildren() const { return children_; }

private:
    std::vector<UIFrame*> children_;
};

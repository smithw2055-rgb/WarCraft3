#pragma once
#include "UIFrame.h"
#include "TextRenderer.h"
#include <functional>
#include <string>

class SliderFrame : public UIFrame {
public:
    SliderFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void SetLabel(const std::string& l) { label_ = l; }
    void SetRange(float min, float max) { min_ = min; max_ = max; }
    void SetValue(float v);
    float GetValue() const { return value_; }
    void SetStep(float s) { step_ = s; }

    using ValueCallback = std::function<void(float)>;
    void SetCallback(ValueCallback cb) { callback_ = std::move(cb); }

    void Render(UiRenderer& r) override;
    UIFrame* OnMouseDown(float sx, float sy, int btn) override;
    UIFrame* OnMouseUp(float sx, float sy, int btn) override;
    UIFrame* HitTest(float sx, float sy) override;

private:
    TextRenderer* textRenderer_ = nullptr;
    std::string label_;
    float value_ = 0.5f;
    float min_ = 0.0f, max_ = 1.0f;
    float step_ = 0.05f;
    bool dragging_ = false;

    ValueCallback callback_;

    float GetThumbX() const;
    float trackX_, trackW_, trackY_, trackH_;
    float thumbW_ = 16.0f;
    float labelSize_ = 14.0f;
};

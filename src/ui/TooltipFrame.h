#pragma once
#include "UIFrame.h"
#include "TextRenderer.h"
#include <string>

class TooltipFrame : public UIFrame {
public:
    TooltipFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void SetText(const std::string& t) { text_ = t; }
    void SetTitle(const std::string& t) { title_ = t; }
    void SetFontSize(float s) { fontSize_ = s; }

    void ShowAt(float screenX, float screenY, int screenW, int screenH);
    void Hide() { visible_ = false; }
    bool IsShown() const { return visible_; }

    void Render(UiRenderer& r) override;

private:
    TextRenderer* textRenderer_ = nullptr;
    std::string text_;
    std::string title_;
    float fontSize_ = 12.0f;
    float pad_ = 6.0f;
    float maxWidth_ = 250.0f;
};

#pragma once
#include "UIFrame.h"
#include "TextRenderer.h"
#include <string>

enum class TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

class TextFrame : public UIFrame {
public:
    static constexpr engine::ui::FrameKind kStaticFrameKind = engine::ui::FrameKind::Text;

    TextFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetText(const std::string& t);
    const std::string& GetText() const { return text_; }
    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void SetColor(uint32_t c) { color_ = c; }
    void SetFontSize(float s) { fontSize_ = s; }
    void SetAlign(TextAlign a) { align_ = a; }
    void SetBackground(uint32_t c) { bgColor_ = c; hasBg_ = true; }
    void ClearBackground() { hasBg_ = false; }

    void PositionBounds(GameUI& gui) override;
    void Render(UiRenderer& r) override;

private:
    std::string text_;
    TextRenderer* textRenderer_ = nullptr;
    uint32_t color_ = 0xFFFFFFFF;
    float fontSize_ = 14.0f;
    TextAlign align_ = TextAlign::LEFT;
    uint32_t bgColor_ = 0;
    bool hasBg_ = false;
};

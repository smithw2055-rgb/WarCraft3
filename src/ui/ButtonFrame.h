#pragma once
#include "UIFrame.h"
#include "TextRenderer.h"
#include <string>
#include <functional>

class ButtonFrame : public UIFrame {
public:
    ButtonFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetText(const std::string& t) { text_ = t; }
    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void SetFontSize(float s) { fontSize_ = s; }
    void SetTextColor(uint32_t c) { textColor_ = c; }
    void SetHotkey(char c) { hotkey_ = c; }

    void SetNormalColor(uint32_t c) { colorNormal_ = c; }
    void SetHoverColor(uint32_t c) { colorHover_ = c; }
    void SetPressedColor(uint32_t c) { colorPressed_ = c; }
    void SetDisabledColor(uint32_t c) { colorDisabled_ = c; }
    void SetDisabledTextColor(uint32_t c) { disabledTextColor_ = c; }

    using ClickCallback = std::function<void()>;
    void SetClickCallback(ClickCallback cb) { onClick_ = std::move(cb); }
    void SetEnabled(bool en) { enabled_ = en; }
    bool IsEnabled() const { return enabled_; }

    void SetTooltip(const std::string& tip) { tooltip_ = tip; }
    const std::string& GetTooltip() const { return tooltip_; }

    void Render(UiRenderer& r) override;
    UIFrame* OnMouseDown(float sx, float sy, int btn) override;
    UIFrame* OnMouseUp(float sx, float sy, int btn) override;
    UIFrame* HitTest(float sx, float sy) override;

    bool IsHovered() const { return hovered_; }

private:
    std::string text_;
    TextRenderer* textRenderer_ = nullptr;
    float fontSize_ = 14.0f;
    uint32_t textColor_ = 0xFFFFFFFF;
    char hotkey_ = 0;
    std::string tooltip_;

    uint32_t colorNormal_ = 0xFF333344;
    uint32_t colorHover_ = 0xFF555577;
    uint32_t colorPressed_ = 0xFF222233;
    uint32_t colorDisabled_ = 0xFF222222;
    uint32_t disabledTextColor_ = 0xFF666666;

    bool enabled_ = true;
    bool pressed_ = false;
    bool hovered_ = false;

    ClickCallback onClick_;
};

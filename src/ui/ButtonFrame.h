#pragma once
#include "SimpleFrame.h"
#include "TextRenderer.h"
#include "../asset/Texture.h"
#include <memory>
#include <string>
#include <functional>

// ButtonFrame is a SimpleFrame so FDF ButtonText/inner frames (ref retail
// GLUETEXTBUTTON ButtonText "X" children) parent and render like any other
// container; chrome + label draw first, children on top.
class ButtonFrame : public SimpleFrame {
public:
    // Textured button state (ref SIMPLEBUTTON NormalTexture/PushedTexture/
    // DisabledTexture + UseHighlight, each a Texture template with its own
    // TexCoord subregion of a states sheet).
    struct StateArt {
        std::shared_ptr<Texture> tex;
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        bool valid = false;
    };

    ButtonFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetText(const std::string& t) { text_ = t; }
    const std::string& GetText() const { return text_; }
    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void SetFontSize(float s) { fontSize_ = s; }
    void SetTextColor(uint32_t c) { textColor_ = c; }
    void SetHotkey(char c) { hotkey_ = c; }

    void SetNormalColor(uint32_t c) { colorNormal_ = c; }
    void SetHoverColor(uint32_t c) { colorHover_ = c; }
    void SetPressedColor(uint32_t c) { colorPressed_ = c; }
    void SetDisabledColor(uint32_t c) { colorDisabled_ = c; }
    void SetDisabledTextColor(uint32_t c) { disabledTextColor_ = c; }
    void SetNormalTexture(const StateArt& art) { normal_ = art; }
    void SetPushedTexture(const StateArt& art) { pushed_ = art; }
    void SetDisabledTexture(const StateArt& art) { disabled_ = art; }
    void SetHighlightTexture(const StateArt& art) { highlight_ = art; }

    void SetControlBackdrop(UIFrame* f) { controlBackdrop_ = f; }
    void SetControlPushedBackdrop(UIFrame* f) { controlPushedBackdrop_ = f; }
    void SetControlDisabledBackdrop(UIFrame* f) { controlDisabledBackdrop_ = f; }
    void SetControlHighlight(UIFrame* f) { controlHighlight_ = f; }

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

protected:
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

    StateArt normal_;
    StateArt pushed_;
    StateArt disabled_;
    StateArt highlight_;

    UIFrame* controlBackdrop_ = nullptr;
    UIFrame* controlPushedBackdrop_ = nullptr;
    UIFrame* controlDisabledBackdrop_ = nullptr;
    UIFrame* controlHighlight_ = nullptr;

    ClickCallback onClick_;
};

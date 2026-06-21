#pragma once
#include "UIFrame.h"
#include "SimpleFrame.h"
#include "TextureFrame.h"
#include "UiTypes.h"
#include "../asset/Texture.h"
#include <memory>
#include <functional>
#include <string>

// A single command button in the 3x4 grid
class CommandCardIcon : public UIFrame {
public:
    CommandCardIcon(int row, int col, UIFrame* parent);

    void SetIcon(const std::shared_ptr<Texture>& tex);
    void SetEnabled(bool en) { enabled_ = en; }
    bool IsEnabled() const { return enabled_; }
    void SetHighlight(bool h) { highlight_ = h; }
    void SetTooltip(const std::string& tip) { tooltip_ = tip; }
    void SetHotkey(char c) { hotkey_ = c; }

    // Callback: (orderId, isRightClick)
    using ClickCallback = std::function<void(int, bool)>;
    void SetClickCallback(ClickCallback cb) { onClick_ = std::move(cb); }
    void SetOrderId(int id) { orderId_ = id; }
    int GetOrderId() const { return orderId_; }

    void Render(UiRenderer& r) override;
    UIFrame* OnMouseDown(float sx, float sy, int btn) override;
    UIFrame* OnMouseUp(float sx, float sy, int btn) override;

    int GetRow() const { return row_; }
    int GetCol() const { return col_; }

private:
    int row_, col_;
    std::shared_ptr<Texture> iconTex_;
    bool enabled_ = true;
    bool highlight_ = false;
    bool pressed_ = false;
    char hotkey_ = 0;
    int orderId_ = 0;
    std::string tooltip_;
    ClickCallback onClick_;

    // Visual state
    float bgColorNormal_[4] = { 0.15f, 0.15f, 0.2f, 0.9f };
    float bgColorHover_[4] = { 0.3f, 0.3f, 0.4f, 0.9f };
    float bgColorPressed_[4] = { 0.1f, 0.1f, 0.15f, 0.9f };
};

// The 3x4 command card grid container
class CommandCard : public SimpleFrame {
public:
    static const int ROWS = 3;
    static const int COLS = 4;

    CommandCard(const std::string& name, UIFrame* parent);

    CommandCardIcon* GetIcon(int row, int col);
    void ClearAll();
    void SetIconAt(int row, int col, const std::shared_ptr<Texture>& tex, int orderId);
    void SetHighlightAt(int row, int col, bool h);

    // Position the grid at bottom-right of parent using anchors
    void PositionGrid(float startX, float startY, float iconW, float iconH, float gapX, float gapY);

private:
    CommandCardIcon* icons_[ROWS][COLS] = {};
};

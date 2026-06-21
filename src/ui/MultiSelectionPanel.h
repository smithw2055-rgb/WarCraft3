#pragma once
#include "ui/UIFrame.h"
#include "ui/UiRenderer.h"
#include <vector>
#include <string>

// Multi-selection unit info
struct MultiSelectUnitInfo {
    int handleId = -1;
    std::string name;
    std::string icon;
    float hpRatio = 1.0f;
    float mpRatio = 0.0f;
    uint32_t playerColor = 0xFF888888;
    int level = 1;
    bool isHero = false;
};

// MultiSelectionPanel - shows multiple selected units
class MultiSelectionPanel {
public:
    MultiSelectionPanel();
    ~MultiSelectionPanel();

    void Init(int screenW, int screenH);
    void Render(UiRenderer& renderer, int screenW, int screenH);

    // Selection management
    void SetSelection(const std::vector<MultiSelectUnitInfo>& units);
    void ClearSelection();
    int GetSelectedCount() const { return (int)units_.size(); }

    // Click handling
    int HitTest(float x, float y, int screenW, int screenH) const;

private:
    std::vector<MultiSelectUnitInfo> units_;
    int maxColumns_ = 6;
    float iconSize_ = 48.0f;
    float padding_ = 4.0f;
};

#pragma once
#include "ui/UiRenderer.h"
#include <vector>
#include <string>

// Buff icon info for display
struct BuffIconInfo {
    std::string name;
    std::string iconPath;
    float remaining = 0;
    float duration = 0;
    int stackCount = 1;
    bool isPositive = true;
    bool isDebuff = false;
};

// BuffBar - displays active buffs/debuffs on selected unit
class BuffBar {
public:
    BuffBar();
    ~BuffBar();

    void Init();
    void Render(UiRenderer& renderer, int screenW, int screenH);

    // Buff management
    void SetBuffs(const std::vector<BuffIconInfo>& buffs);
    void ClearBuffs();
    int GetBuffCount() const { return (int)buffs_.size(); }

private:
    std::vector<BuffIconInfo> buffs_;
    float iconSize_ = 32.0f;
    float padding_ = 2.0f;
    int maxColumns_ = 6;
};

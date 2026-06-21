#pragma once
#include "UIFrame.h"
#include "TextFrame.h"
#include "TextRenderer.h"
#include <string>

class InfoPanelFrame : public UIFrame {
public:
    InfoPanelFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTextRenderer(TextRenderer* tr);

    void SetUnitName(const std::string& name);
    void SetUnitLevel(int level);
    void SetHp(float current, float max);
    void SetMp(float current, float max);
    void Clear();

    void Render(UiRenderer& r) override;

private:
    TextRenderer* textRenderer_ = nullptr;
    TextFrame nameLabel_;
    TextFrame levelLabel_;

    std::string unitName_;
    int unitLevel_ = 1;
    float hp_ = -1, maxHp_ = -1;
    float mp_ = -1, maxMp_ = -1;
    bool hasUnit_ = false;

    static uint32_t LerpColor(uint32_t from, uint32_t to, float t);
};

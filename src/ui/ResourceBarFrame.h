#pragma once
#include "UIFrame.h"
#include "TextRenderer.h"
#include <string>

class ResourceBarFrame : public UIFrame {
public:
    ResourceBarFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetTextRenderer(TextRenderer* tr) { textRenderer_ = tr; }
    void SetGold(int g) { gold_ = g; }
    void SetLumber(int l) { lumber_ = l; }
    void SetFood(int used, int max) { foodUsed_ = used; foodMax_ = max; }
    void SetTimeOfDay(float t) { timeOfDay_ = t; }

    void Render(UiRenderer& r) override;

private:
    TextRenderer* textRenderer_ = nullptr;
    int gold_ = 0;
    int lumber_ = 0;
    int foodUsed_ = 0;
    int foodMax_ = 0;
    float timeOfDay_ = 0.5f;
};

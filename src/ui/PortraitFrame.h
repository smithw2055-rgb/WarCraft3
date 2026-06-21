#pragma once
#include "UIFrame.h"
#include <string>

class PortraitFrame : public UIFrame {
public:
    PortraitFrame(const std::string& name, UIFrame* parent = nullptr);

    void SetUnitType(const std::string& unitId) { unitId_ = unitId; hasUnit_ = true; }
    void Clear();

    void Render(UiRenderer& r) override;

private:
    std::string unitId_;
    bool hasUnit_ = false;
};

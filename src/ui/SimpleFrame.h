#pragma once
#include "UIFrame.h"
#include <vector>

class SimpleFrame : public UIFrame {
public:
    SimpleFrame(const std::string& name, UIFrame* parent = nullptr)
        : UIFrame(name, parent) {}
    ~SimpleFrame() override;
};

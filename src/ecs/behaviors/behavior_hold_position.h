#pragma once
#include "ecs/behaviors/behavior.h"

class CBehaviorHoldPosition : public CBehavior {
public:
    CBehaviorHoldPosition();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::HOLD_POSITION; }
    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
};

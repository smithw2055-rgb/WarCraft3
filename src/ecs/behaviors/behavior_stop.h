#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/orders/order.h"

class CBehaviorStop : public CBehavior {
public:
    CBehaviorStop();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::STOP; }
    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
};

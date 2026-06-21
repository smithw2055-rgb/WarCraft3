#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/unit.h"

class CBehaviorFollow : public CBehavior {
public:
    CBehaviorFollow();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::FOLLOW; }

    void SetTarget(CWidget* target, float followRange = 128.0f);

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    CWidget* target_ = nullptr;
    float followRange_ = 128.0f;
};

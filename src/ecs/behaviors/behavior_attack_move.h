#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/unit.h"

// Attack-move: move to a point, attacking enemies along the way
class CBehaviorAttackMove : public CBehavior {
public:
    CBehaviorAttackMove();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::ATTACK_MOVE; }

    void SetDestination(float x, float y);

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    float destX_ = 0, destY_ = 0;
    float arrivalDistance_ = 64.0f;
    int lastAttackScanTick_ = 0;
    int targetHandle_ = -1;
};

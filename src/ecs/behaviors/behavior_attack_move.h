#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/unit.h"
#include <deque>

// Attack-move: move to a point, attacking enemies along the way
class CBehaviorAttackMove : public CBehavior {
public:
    CBehaviorAttackMove();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::ATTACK_MOVE; }

    void SetDestination(float x, float y);
    bool IsPropelling() const { return propelling_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool RecalculatePath(CSimulation& simulation, float targetX, float targetY);
    bool MoveTowardWaypoint(CSimulation& simulation);

    float destX_ = 0, destY_ = 0;
    float arrivalDistance_ = 64.0f;
    int lastAttackScanTick_ = 0;
    int targetHandle_ = -1;
    std::deque<std::pair<float, float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool pathFound_ = false;
    bool propelling_ = false;
};

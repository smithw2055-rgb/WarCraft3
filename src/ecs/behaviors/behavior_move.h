#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/orders/order.h"
#include <deque>

class CBehaviorMove : public CBehavior {
public:
    CBehaviorMove();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::MOVE; }

    void SetTarget(float x, float y);
    void SetTargetWidget(CWidget* target);

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool RecalculatePath(CSimulation& simulation);
    bool MoveTowardWaypoint(CSimulation& simulation);

    float targetX_ = 0, targetY_ = 0;
    CWidget* targetWidget_ = nullptr;
    float arrivalDistance_ = 48.0f;

    // Pathfinding state
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool pathFound_ = false;
};

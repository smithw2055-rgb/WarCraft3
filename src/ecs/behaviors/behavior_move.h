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
    bool IsPropelling() const { return propelling_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool RecalculatePath(CSimulation& simulation);
    bool MoveTowardWaypoint(CSimulation& simulation);
    bool UpdateFacingToward(float targetX, float targetY);
    float ComputeFinalArrivalDistance(const CSimulation& simulation) const;

    float targetX_ = 0, targetY_ = 0;
    CWidget* targetWidget_ = nullptr;

    // Pathfinding state
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    int lastProgressLogTick_ = -9999;
    bool pathFound_ = false;
    bool propelling_ = false;
    int blockedTicks_ = 0;
    int repathRetryCount_ = 0;
};

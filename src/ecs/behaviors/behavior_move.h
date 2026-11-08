#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/orders/order.h"
#include <cstdint>
#include <deque>

class CBehaviorMove : public CBehavior {
public:
    CBehaviorMove();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::MOVE; }

    void SetTarget(float x, float y) override;
    void SetTarget(CWidget* target) override { SetTargetWidget(target); }
    void SetTargetWidget(CWidget* target) override;
    bool TracksPropellingState() const override { return true; }
    bool IsPropelling() const override { return propelling_; }

    // Phase B2: Optional FlowField + RVO Crowd Steering mode for squad navigation
    void SetUseFlowFieldSteering(bool enabled) override { useFlowFieldSteering_ = enabled; }
    bool IsUsingFlowFieldSteering() const override { return useFlowFieldSteering_; }
    uint64_t GetFlowFieldSteeringSteps() const override { return flowFieldSteeringSteps_; }
    uint64_t GetRvoSidestepAdjustments() const override { return rvoSidestepAdjustments_; }

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
    bool useFlowFieldSteering_ = false;
    uint64_t flowFieldSteeringSteps_ = 0;
    uint64_t rvoSidestepAdjustments_ = 0;
};

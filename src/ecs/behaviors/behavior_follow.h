#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/unit.h"
#include <deque>

class CBehaviorFollow : public CBehavior {
public:
    CBehaviorFollow();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::FOLLOW; }

    void SetTarget(CWidget* target, float followRange = 128.0f);
    bool IsPropelling() const { return propelling_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool RecalculatePath(CSimulation& simulation, float targetX, float targetY);
    bool MoveTowardWaypoint(CSimulation& simulation);

    CWidget* target_ = nullptr;
    float followRange_ = 128.0f;
    std::deque<std::pair<float, float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool pathFound_ = false;
    bool propelling_ = false;
};

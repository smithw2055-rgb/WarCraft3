#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/unit.h"
#include <deque>
#include <vector>

class CBehaviorPatrol : public CBehavior {
public:
    CBehaviorPatrol();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::PATROL; }

    void SetPoints(float x1, float y1, float x2, float y2) override;
    bool TracksPropellingState() const override { return true; }
    bool IsPropelling() const override { return propelling_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool RecalculatePath(CSimulation& simulation, float targetX, float targetY);
    bool MoveTowardWaypoint(CSimulation& simulation);

    struct PatrolPoint { float x, y; };
    std::vector<PatrolPoint> points_;
    std::deque<std::pair<float, float>> waypoints_;
    int currentTarget_ = 0;
    int pathRecalcCooldown_ = 0;
    bool pathFound_ = false;
    float arrivalDistance_ = 64.0f;
    bool propelling_ = false;
};

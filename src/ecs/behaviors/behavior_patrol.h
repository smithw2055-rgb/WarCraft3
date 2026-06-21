#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/unit.h"
#include <vector>

class CBehaviorPatrol : public CBehavior {
public:
    CBehaviorPatrol();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::PATROL; }

    void SetPoints(float x1, float y1, float x2, float y2);

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    struct PatrolPoint { float x, y; };
    std::vector<PatrolPoint> points_;
    int currentTarget_ = 0;
    float arrivalDistance_ = 64.0f;
};

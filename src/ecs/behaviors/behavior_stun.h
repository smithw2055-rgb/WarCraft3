#pragma once
#include "ecs/behaviors/behavior.h"

// CBehaviorStun - unit is stunned and cannot act
class CBehaviorStun : public CBehavior {
public:
    CBehaviorStun();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::STUN; }

    void SetDuration(float duration) { duration_ = duration; }
    float GetRemaining() const { return remaining_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    float duration_ = 0;
    float remaining_ = 0;
};

#pragma once
#include "ecs/behaviors/behavior.h"

// CBehaviorStun - unit is stunned and cannot act
class CBehaviorStun : public CBehavior {
public:
    CBehaviorStun();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::STUN; }
    // Stun cannot be interrupted: new orders defer until it expires
    // (ref CBehaviorStun.interruptable false).
    bool IsInterruptable() const override { return false; }

    void SetDuration(float duration) { duration_ = duration; }
    float GetRemaining() const { return remaining_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    float duration_ = 0;
    float remaining_ = 0;
};

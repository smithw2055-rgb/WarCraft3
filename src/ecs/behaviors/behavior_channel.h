#pragma once
// Channeled casting behavior (ref ABActionBeginChanneling): holds the
// caster for a fixed tick count, running an optional per-tick chain,
// then runs the finish chain once and completes. Any behavior replacement
// (new orders, stun) or caster death ends the channel WITHOUT the finish
// effect. Targets resolve by handle each run so mid-channel death or
// removal fizzles safely instead of dangling.
#include "ecs/behaviors/behavior.h"
#include "ecs/abilities/ability_builder.h"

class CSimulation;
class CWidget;

class CBehaviorChannel : public CBehavior {
public:
    CBehaviorChannel(ABEffectChain tick, ABEffectChain finish, int ticks,
                     int targetHandle, bool hasPoint, float x, float y);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::SPELL; }

    bool Update(CSimulation& simulation) override;

    int TicksLeft() const { return ticksLeft_; }

private:
    ABEffectChain tick_;
    ABEffectChain finish_;
    int ticksLeft_;
    int targetHandle_ = -1;
    bool hasPoint_ = false;
    float x_ = 0.0f;
    float y_ = 0.0f;
};

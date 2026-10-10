#pragma once
#include "ecs/behaviors/behavior.h"

// Timed root/uproot morph (ref behaviors/root/CBehaviorRoot+Uproot).
//
// The ability itself flips IsRooted + swaps the command card immediately
// (instant gameplay state, keeps existing tests green); this behavior then
// holds the unit's order queue for the morph animation window so the ancient
// cannot move/attack mid-morph, mirroring ref's AcceptingOrders=false gap.
// Duration 0 finishes on the first Update (back-compat instant path).
class CBehaviorRoot : public CBehavior {
public:
    explicit CBehaviorRoot(float durationSec = 0.5f);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::ROOT; }

    void SetDuration(float v) { duration_ = v; remaining_ = v; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    float duration_ = 0.5f;
    float remaining_ = 0.5f;
};

class CBehaviorUproot : public CBehavior {
public:
    explicit CBehaviorUproot(float durationSec = 0.5f);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::ROOT; }

    void SetDuration(float v) { duration_ = v; remaining_ = v; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    float duration_ = 0.5f;
    float remaining_ = 0.5f;
};

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

    void SetDuration(float v) override { duration_ = v; remaining_ = v; }
    float GetRemaining() const override { return remaining_; }
    void SetRemaining(float v) override { remaining_ = v; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

protected:
    float duration_ = 0.5f;
    float remaining_ = 0.5f;
};

// Phase 23 de-redundancy: CBehaviorUproot shares CBehaviorRoot's timed morph logic directly.
class CBehaviorUproot : public CBehaviorRoot {
public:
    using CBehaviorRoot::CBehaviorRoot;
};

#include "buff.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

CBuff::CBuff(War3ID id, float duration, bool isPositive)
    : id_(id), duration_(duration), remaining_(duration),
      isPositive_(isPositive), periodic_(false), suspended_(false) {}

bool CBuff::AddStack() {
    if (stackCount_ < maxStacks_) {
        stackCount_++;
        RefreshDuration();
        return true;
    }
    RefreshDuration();
    return false;
}

void CBuff::RemoveStack() {
    if (stackCount_ > 0) {
        stackCount_--;
    }
}

void CBuff::OnApply(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
}

void CBuff::OnRemove(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
}

void CBuff::OnRefresh(CSimulation& game, CUnit* target, const CBuff& newBuff) {
    (void)game;
    (void)target;
    if (newBuff.GetDuration() > duration_) {
        duration_ = newBuff.GetDuration();
    }
    if (newBuff.GetRemaining() > remaining_) {
        remaining_ = newBuff.GetRemaining();
    } else {
        RefreshDuration();
    }
}

void CBuff::OnTick(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
    if (suspended_) return;
    // Update remaining duration
    float dt = game.GetSimStepTime();
    remaining_ -= dt;

    // Handle periodic effects
    if (periodic_) {
        periodicTimer_ += dt;
        if (periodicTimer_ >= periodicInterval_) {
            periodicTimer_ -= periodicInterval_;
            OnPeriodic(game, target);
        }
    }
}

void CBuff::OnPeriodic(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
}

void CBuff::OnDeath(CSimulation& game, CUnit* target) {
    (void)game; (void)target;
}

#include "behavior_root.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"

CBehaviorRoot::CBehaviorRoot(float durationSec)
    : duration_(durationSec), remaining_(durationSec) {}

void CBehaviorRoot::Begin(CSimulation& simulation) {
    (void)simulation;
    remaining_ = duration_;
    // Gameplay state (IsRooted + card swap) is already applied by
    // CAbilityRoot::BeginNoTarget; this behavior only models the morph hold.
}

void CBehaviorRoot::End(CSimulation& simulation) {
    (void)simulation;
    remaining_ = 0;
}

bool CBehaviorRoot::Update(CSimulation& simulation) {
    if (!unit_ || unit_->IsDead()) return true;
    if (remaining_ <= 0.0f) return true;
    remaining_ -= simulation.GetSimStepTime();
    return remaining_ <= 0.0f;
}

CBehaviorUproot::CBehaviorUproot(float durationSec)
    : duration_(durationSec), remaining_(durationSec) {}

void CBehaviorUproot::Begin(CSimulation& simulation) {
    (void)simulation;
    remaining_ = duration_;
}

void CBehaviorUproot::End(CSimulation& simulation) {
    (void)simulation;
    remaining_ = 0;
}

bool CBehaviorUproot::Update(CSimulation& simulation) {
    if (!unit_ || unit_->IsDead()) return true;
    if (remaining_ <= 0.0f) return true;
    remaining_ -= simulation.GetSimStepTime();
    return remaining_ <= 0.0f;
}

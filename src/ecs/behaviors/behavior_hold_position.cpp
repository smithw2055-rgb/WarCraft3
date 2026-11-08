#include "behavior_hold_position.h"
#include "ecs/csimulation.h"

CBehaviorHoldPosition::CBehaviorHoldPosition() {}

void CBehaviorHoldPosition::Begin(CSimulation& simulation) {
    (void)simulation;
}

bool CBehaviorHoldPosition::Update(CSimulation& simulation) {
    (void)simulation;
    return false; // never completes
}

std::unique_ptr<CBehavior> CBehavior::CreateHoldPosition() { return std::make_unique<CBehaviorHoldPosition>(); }

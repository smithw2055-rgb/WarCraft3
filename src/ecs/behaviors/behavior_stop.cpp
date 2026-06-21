#include "behavior_stop.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"

CBehaviorStop::CBehaviorStop() {}

void CBehaviorStop::Begin(CSimulation& simulation) {
    // Cancel current order
    unit_->ClearOrders();
    (void)simulation;
}

bool CBehaviorStop::Update(CSimulation& simulation) {
    (void)simulation;
    return true; // completes immediately
}

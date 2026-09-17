#include "behavior_stun.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"

CBehaviorStun::CBehaviorStun() {}

void CBehaviorStun::Begin(CSimulation& simulation) {
    (void)simulation;
    remaining_ = duration_;
    // No sim-side animation hook: sequence selection is render-side
    // (game_controller maps behavior category to MDX sequences there).
}

void CBehaviorStun::End(CSimulation& simulation) {
    (void)simulation;
    remaining_ = 0;
}

bool CBehaviorStun::Update(CSimulation& simulation) {
    float dt = simulation.GetSimStepTime();
    remaining_ -= dt;

    if (remaining_ <= 0) {
        return true; // stun expired
    }

    // Stunned unit does nothing
    return false;
}

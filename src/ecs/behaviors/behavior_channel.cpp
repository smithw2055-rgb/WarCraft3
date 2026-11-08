#include "ecs/behaviors/behavior_channel.h"
#include <utility>
#include "ecs/csimulation.h"
#include "ecs/unit.h"

CBehaviorChannel::CBehaviorChannel(ABEffectChain tick, ABEffectChain finish,
                                   int ticks, int targetHandle,
                                   bool hasPoint, float x, float y)
    : tick_(std::move(tick)),
      finish_(std::move(finish)),
      ticksLeft_(ticks),
      targetHandle_(targetHandle),
      hasPoint_(hasPoint),
      x_(x),
      y_(y) {}

bool CBehaviorChannel::Update(CSimulation& simulation) {
    CUnit* caster = GetUnit();
    if (!caster || caster->IsDead()) return true; // fizzle: no finish
    CWidget* target =
        targetHandle_ >= 0 ? simulation.GetWidget(targetHandle_) : nullptr;
    // Duration ticks run the per-tick chain; the tick that spends the last
    // one fires the finish instead (ref channelEndTick semantics).
    if (--ticksLeft_ <= 0) {
        finish_.Run(simulation, caster, target, hasPoint_, x_, y_);
        return true;
    }
    tick_.Run(simulation, caster, target, hasPoint_, x_, y_);
    return false;
}

std::unique_ptr<CBehavior> CreateChannelBehavior(ABEffectChain tick, ABEffectChain finish,
                                                 int ticks, int targetHandle,
                                                 bool hasPoint, float x, float y) {
    return std::make_unique<CBehaviorChannel>(std::move(tick), std::move(finish),
                                              ticks, targetHandle, hasPoint, x, y);
}

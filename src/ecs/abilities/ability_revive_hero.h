#pragma once
#include "ecs/ability.h"

// CAbilityReviveHero - hero revive ability
// Allows altars to revive dead heroes
class CAbilityReviveHero : public CAbility {
public:
    CAbilityReviveHero(int handleId);

    // Order handling
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
};

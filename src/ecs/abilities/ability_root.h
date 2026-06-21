#pragma once
#include "ecs/ability.h"

// CAbilityRoot - Night Elf building roots into the ground
class CAbilityRoot : public CAbility {
public:
    CAbilityRoot(int handleId);

    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
};

// CAbilityUproot - Night Elf building uproots and can move
class CAbilityUproot : public CAbility {
public:
    CAbilityUproot(int handleId);

    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
};

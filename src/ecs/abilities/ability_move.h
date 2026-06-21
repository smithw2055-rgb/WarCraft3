#pragma once
#include "ecs/ability.h"

// CAbilityMove - the "Amov" ability
// Validates move/follow/patrol/hold-position orders and creates behaviors
class CAbilityMove : public CAbility {
public:
    CAbilityMove(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;

    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;

    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
};

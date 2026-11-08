#include "ability_repair.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"

CAbilityRepair::CAbilityRepair(int handleId)
    : CAbility(handleId, War3ID("Arep"), CAbilityCategory::BUILD) {
    iconShowing_ = true;
}

void CAbilityRepair::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbilityRepair::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

bool CAbilityRepair::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    (void)game;
    (void)orderId;
    if (IsDisabled()) return false;
    if (!target || target->IsDead()) return false;

    // Can only repair buildings owned by the same player
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return false;
    if (!targetUnit->IsBuilding()) return false;
    if (targetUnit->GetPlayerIndex() != unit->GetPlayerIndex()) return false;
    if (targetUnit->IsConstructing()) return false;
    if (targetUnit->GetLife() >= targetUnit->GetMaxLife()) return false;

    return true;
}

CBehavior* CAbilityRepair::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)game;
    (void)orderId;
    if (!target) return nullptr;

    return CBehavior::CreateRepair(caster, this, target);
}

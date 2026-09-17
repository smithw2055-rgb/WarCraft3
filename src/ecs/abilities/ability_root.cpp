#include "ability_root.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

// ===== CAbilityRoot =====
CAbilityRoot::CAbilityRoot(int handleId)
    : CAbility(handleId, War3ID("Aroo"), CAbilityCategory::ROOT) {
    iconShowing_ = true;
}

bool CAbilityRoot::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit; (void)orderId;
    if (IsDisabled()) return false;
    // Can only root if not already rooted
    if (unit && unit->IsRooted()) return false;
    return true;
}

CBehavior* CAbilityRoot::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)orderId;
    if (!caster || caster->IsRooted()) return nullptr;
    caster->SetRooted(true);
    // Halt any in-progress movement; attacks continue while rooted.
    caster->Stop(game);
    // Swap the command card: Root out, Uproot in (ref transformation swap).
    if (CAbility* uproot = caster->GetAbility(War3ID("Aro1"))) {
        uproot->SetDisabled(false, CAbilityDisableType::TRANSFORMATION);
    }
    SetDisabled(true, CAbilityDisableType::TRANSFORMATION);
    return nullptr;
}

// ===== CAbilityUproot =====
CAbilityUproot::CAbilityUproot(int handleId)
    : CAbility(handleId, War3ID("Aro1"), CAbilityCategory::ROOT) {
    iconShowing_ = true;
    // Uproot starts hidden until the ancient roots (ref ability swap).
    SetDisabled(true, CAbilityDisableType::TRANSFORMATION);
}

bool CAbilityUproot::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit; (void)orderId;
    if (IsDisabled()) return false;
    // Can only uproot if rooted
    if (unit && !unit->IsRooted()) return false;
    return true;
}

CBehavior* CAbilityUproot::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)orderId;
    if (!caster || !caster->IsRooted()) return nullptr;
    caster->SetRooted(false);
    // Swap the command card back: Uproot out, Root in.
    if (CAbility* root = caster->GetAbility(War3ID("Aroo"))) {
        root->SetDisabled(false, CAbilityDisableType::TRANSFORMATION);
    }
    SetDisabled(true, CAbilityDisableType::TRANSFORMATION);
    return nullptr;
}

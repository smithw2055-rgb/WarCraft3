#include "ability_root.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include <iostream>

// ===== CAbilityRoot =====
CAbilityRoot::CAbilityRoot(int handleId)
    : CAbility(handleId, War3ID("Aroo"), CAbilityCategory::ROOT) {
    iconShowing_ = true;
}

bool CAbilityRoot::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit; (void)orderId;
    if (IsDisabled()) return false;
    // Can only root if not already rooted
    // TODO: check rooted state
    return true;
}

CBehavior* CAbilityRoot::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)orderId;
    // Root the building
    // TODO: set rooted state, disable movement, enable attack
    std::cout << "Building rooted" << std::endl;
    return nullptr;
}

// ===== CAbilityUproot =====
CAbilityUproot::CAbilityUproot(int handleId)
    : CAbility(handleId, War3ID("Aro1"), CAbilityCategory::ROOT) {
    iconShowing_ = true;
}

bool CAbilityUproot::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit; (void)orderId;
    if (IsDisabled()) return false;
    // Can only uproot if rooted
    // TODO: check rooted state
    return true;
}

CBehavior* CAbilityUproot::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)orderId;
    // Uproot the building
    // TODO: unset rooted state, enable movement, disable attack
    std::cout << "Building uprooted" << std::endl;
    return nullptr;
}

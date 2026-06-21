#include "ability_transport.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include <iostream>

// ===== CAbilityLoad =====
CAbilityLoad::CAbilityLoad(int handleId)
    : CAbility(handleId, War3ID("Aloa"), CAbilityCategory::CARGO_HOLD) {
    iconShowing_ = true;
}

bool CAbilityLoad::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    (void)orderId;
    if (IsDisabled()) return false;
    if (!target || target->IsDead()) return false;

    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return false;
    if (targetUnit->IsBuilding()) return false; // can't load buildings
    if (targetUnit->GetPlayerIndex() != unit->GetPlayerIndex()) return false; // same player only

    // Check cargo capacity
    // TODO: track loaded units
    return true;
}

CBehavior* CAbilityLoad::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)orderId;
    if (!target) return nullptr;

    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return nullptr;

    // Load the unit into the transport
    targetUnit->SetHidden(true);
    // TODO: track loaded units, implement cargo system
    std::cout << "Loaded unit into transport" << std::endl;

    return nullptr; // instant action, no behavior needed
}

// ===== CAbilityUnload =====
CAbilityUnload::CAbilityUnload(int handleId)
    : CAbility(handleId, War3ID("Adro"), CAbilityCategory::CARGO_HOLD) {
    iconShowing_ = true;
}

bool CAbilityUnload::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)unit; (void)orderId; (void)x; (void)y;
    if (IsDisabled()) return false;
    // TODO: check if transport has loaded units
    return true;
}

CBehavior* CAbilityUnload::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)orderId;
    // Unload all units at the specified point
    // TODO: iterate loaded units, unhide and reposition
    std::cout << "Unloaded units at (" << x << ", " << y << ")" << std::endl;
    return nullptr; // instant action
}

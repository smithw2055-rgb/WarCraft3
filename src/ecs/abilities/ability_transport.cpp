#include "ability_transport.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_board_transport.h"
#include <cmath>
#include <iostream>
#include <vector>

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
    if (targetUnit->IsLoadedAboard()) return false;

    // Check cargo capacity
    if ((int)unit->GetCargoUnits().size() >= cargoCapacity_) return false;
    return true;
}

CBehavior* CAbilityLoad::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)orderId;
    if (!target) return nullptr;

    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return nullptr;
    if ((int)caster->GetCargoUnits().size() >= cargoCapacity_) return nullptr; // full

    const float dx = target->GetX() - caster->GetX();
    const float dy = target->GetY() - caster->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= loadRange_ && (int)caster->GetCargoUnits().size() < cargoCapacity_) {
        // In range: load immediately.
        caster->LoadUnit(targetUnit);
        std::cout << "Loaded unit into transport" << std::endl;
        return nullptr;
    }

    // Out of range (or full): walk into load range first. The behavior
    // re-checks capacity on arrival.
    auto* board = new CBehaviorBoardTransport(loadRange_, cargoCapacity_);
    board->SetTarget(target);
    return board;
}

// ===== CAbilityUnload =====
CAbilityUnload::CAbilityUnload(int handleId)
    : CAbility(handleId, War3ID("Adro"), CAbilityCategory::CARGO_HOLD) {
    iconShowing_ = true;
}

bool CAbilityUnload::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)orderId; (void)x; (void)y;
    if (IsDisabled()) return false;
    if (!unit || unit->GetCargoUnits().empty()) return false;
    return true;
}

CBehavior* CAbilityUnload::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)game; (void)orderId;
    // Unload all units around the point on a small ring so they don't stack.
    std::vector<CUnit*> cargo = caster->GetCargoUnits();
    const float step = 2.0f * 3.14159265f / (float)std::max<size_t>(1, cargo.size());
    for (size_t i = 0; i < cargo.size(); i++) {
        const float a = step * (float)i;
        caster->UnloadUnitAt(cargo[i], x + 48.0f * std::cos(a), y + 48.0f * std::sin(a));
    }
    std::cout << "Unloaded " << cargo.size() << " units at (" << x << ", " << y << ")" << std::endl;
    return nullptr; // instant action
}

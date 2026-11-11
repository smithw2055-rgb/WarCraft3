#include "ability_transport.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "data/abilitydata.h"
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

    auto* targetUnit = ((target) ? (target)->AsUnit() : nullptr);
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

    auto* targetUnit = ((target) ? (target)->AsUnit() : nullptr);
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
    return CBehavior::CreateBoardTransport(loadRange_, cargoCapacity_, target);
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
    (void)orderId;
    if (caster->GetCargoUnits().empty()) return nullptr;
    // Timed unload, one passenger per duration interval (ref
    // CBehaviorDrop). Duration from ability data, 0.5s fallback.
    float duration = 0.5f;
    if (CAbilityData* data = game.GetAbilityData()) {
        if (const CAbilityDefFull* def = data->GetAbility(GetAlias())) {
            duration = def->DurationAt(1);
            if (duration <= 0) duration = 0.5f;
        }
    }
    return CBehavior::CreateDropCargo(x, y, duration);
}

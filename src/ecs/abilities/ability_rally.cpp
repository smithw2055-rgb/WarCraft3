#include "ability_rally.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include <iostream>
#include <memory>

CAbilityRally::CAbilityRally(int handleId)
    : CAbility(handleId, War3ID("ARal"), CAbilityCategory::RALLY) {
    iconShowing_ = false;
}

void CAbilityRally::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    // Default rally to self
    rallyTarget_ = unit;
    rallyX_ = unit->GetX();
    rallyY_ = unit->GetY();
    hasRallyPoint_ = true;
    explicitlySet_ = false;
}

bool CAbilityRally::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    (void)game; (void)unit;
    if (IsDisabled()) return false;
    if ((orderId != OrderIds::SMART) && (orderId != OrderIds::SET_RALLY) && (orderId != OrderIds::RALLY)) {
        return false;
    }
    return target != nullptr && !target->IsDead();
}

bool CAbilityRally::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)unit; (void)x; (void)y;
    if (IsDisabled()) return false;
    return (orderId == OrderIds::SMART) || (orderId == OrderIds::SET_RALLY) || (orderId == OrderIds::RALLY);
}

CBehavior* CAbilityRally::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)game; (void)orderId;
    rallyTarget_ = target;
    if (target) {
        rallyX_ = target->GetX();
        rallyY_ = target->GetY();
    } else if (caster) {
        rallyX_ = caster->GetX();
        rallyY_ = caster->GetY();
    }
    hasRallyPoint_ = true;
    explicitlySet_ = true;
    std::cout << "Rally point set to unit" << std::endl;
    return nullptr; // rally doesn't create a behavior
}

CBehavior* CAbilityRally::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)game; (void)orderId;
    rallyTarget_ = nullptr;
    rallyX_ = x;
    rallyY_ = y;
    hasRallyPoint_ = true;
    explicitlySet_ = true;
    std::cout << "Rally point set to (" << x << ", " << y << ")" << std::endl;
    return nullptr;
}

void CAbilityRally::ApplyToTrainedUnit(CSimulation& game, CUnit* building, CUnit* trainedUnit) const {
    (void)game;
    (void)building;
    if (!trainedUnit || !explicitlySet_ || !HasRallyPoint()) {
        return;
    }

    if (rallyTarget_ && !rallyTarget_->IsDead()) {
        trainedUnit->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::SMART, rallyTarget_));
    } else {
        trainedUnit->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, rallyX_, rallyY_));
    }
}

void CAbilityRally::ClearRallyPoint() {
    rallyTarget_ = nullptr;
    hasRallyPoint_ = false;
    explicitlySet_ = false;
}

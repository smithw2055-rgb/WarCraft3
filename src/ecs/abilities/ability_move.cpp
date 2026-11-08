#include "ability_move.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"

CAbilityMove::CAbilityMove(int handleId)
    : CAbility(handleId, War3ID("Amov"), CAbilityCategory::MOVEMENT) {
    iconShowing_ = false; // movement is always available, no icon
}

void CAbilityMove::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    // Create movement-related behaviors and store on unit
    auto moveBehavior = CBehavior::CreateMove();
    moveBehavior->SetUnit(unit);
    unit->SetMoveBehavior(std::move(moveBehavior));

    auto followBehavior = CBehavior::CreateFollow();
    followBehavior->SetUnit(unit);
    unit->SetFollowBehavior(std::move(followBehavior));

    auto patrolBehavior = CBehavior::CreatePatrol();
    patrolBehavior->SetUnit(unit);
    unit->SetPatrolBehavior(std::move(patrolBehavior));

    auto holdBehavior = CBehavior::CreateHoldPosition();
    holdBehavior->SetUnit(unit);
    unit->SetHoldPositionBehavior(std::move(holdBehavior));

    // Set stop as default behavior
    auto stopBehavior = CBehavior::CreateStop();
    stopBehavior->SetUnit(unit);
    unit->SetDefaultBehavior(std::move(stopBehavior));
}

void CAbilityMove::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    unit->SetMoveBehavior(nullptr);
    unit->SetFollowBehavior(nullptr);
    unit->SetPatrolBehavior(nullptr);
    unit->SetHoldPositionBehavior(nullptr);
    unit->SetDefaultBehavior(nullptr);
}

bool CAbilityMove::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    (void)game;
    if (IsDisabled()) return false;
    if ((orderId != OrderIds::SMART) && (orderId != OrderIds::MOVE) && (orderId != OrderIds::PATROL)) {
        return false;
    }
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    return (targetUnit != nullptr) && (targetUnit != unit) && !targetUnit->IsDead();
}

bool CAbilityMove::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)unit; (void)x; (void)y;
    if (IsDisabled()) return false;
    return (orderId == OrderIds::SMART) || (orderId == OrderIds::MOVE) || (orderId == OrderIds::PATROL);
}

bool CAbilityMove::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game;
    if (IsDisabled()) return false;
    // Hold position is a no-target order
    return orderId == OrderIds::HOLD_POSITION;
}

CBehavior* CAbilityMove::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)game;
    if (!target) return nullptr;

    // SMART right-click boarding: passenger walks to a friendly transport
    // with cargo space (ref CBehaviorLoad passenger perspective).
    if (orderId == OrderIds::SMART) {
        if (auto* transport = dynamic_cast<CUnit*>(target)) {
            if (transport != caster && !transport->IsDead() && !caster->IsBuilding() &&
                !caster->IsLoadedAboard() &&
                transport->GetPlayerIndex() == caster->GetPlayerIndex() &&
                (transport->GetAbility(War3ID("Aloa")) || transport->GetAbility(War3ID("Acar")) ||
                 transport->GetAbility(War3ID("Abun")) || transport->GetAbility(War3ID("Aenc")))) {
                return CBehavior::CreateLoadPassenger(256.0f, transport);
            }
        }
    }

    // Follow the target widget
    auto* followBehavior = caster->GetFollowBehavior();
    if (followBehavior) {
        followBehavior->SetTarget(target);
        return followBehavior;
    }
    return nullptr;
}

CBehavior* CAbilityMove::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)game;
    if (orderId == OrderIds::PATROL) {
        // Patrol from current position to target
        auto* patrolBehavior = caster->GetPatrolBehavior();
        if (patrolBehavior) {
            patrolBehavior->SetPoints(caster->GetX(), caster->GetY(), x, y);
            return patrolBehavior;
        }
    }
    // Regular move
    auto* moveBehavior = caster->GetMoveBehavior();
    if (moveBehavior) {
        moveBehavior->SetTarget(x, y);
        return moveBehavior;
    }
    return nullptr;
}

CBehavior* CAbilityMove::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game;
    if (orderId == OrderIds::HOLD_POSITION) {
        auto* holdBehavior = caster->GetHoldPositionBehavior();
        if (holdBehavior) {
            return holdBehavior;
        }
    }
    return nullptr;
}

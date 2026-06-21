#include "ability_move.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_move.h"
#include "ecs/behaviors/behavior_follow.h"
#include "ecs/behaviors/behavior_patrol.h"
#include "ecs/behaviors/behavior_hold_position.h"
#include "ecs/behaviors/behavior_stop.h"

CAbilityMove::CAbilityMove(int handleId)
    : CAbility(handleId, War3ID("Amov"), CAbilityCategory::MOVEMENT) {
    iconShowing_ = false; // movement is always available, no icon
}

void CAbilityMove::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    // Create movement-related behaviors and store on unit
    auto moveBehavior = std::make_unique<CBehaviorMove>();
    moveBehavior->SetUnit(unit);
    unit->SetMoveBehavior(std::move(moveBehavior));

    auto followBehavior = std::make_unique<CBehaviorFollow>();
    followBehavior->SetUnit(unit);
    unit->SetFollowBehavior(std::move(followBehavior));

    auto patrolBehavior = std::make_unique<CBehaviorPatrol>();
    patrolBehavior->SetUnit(unit);
    unit->SetPatrolBehavior(std::move(patrolBehavior));

    auto holdBehavior = std::make_unique<CBehaviorHoldPosition>();
    holdBehavior->SetUnit(unit);
    unit->SetHoldPositionBehavior(std::move(holdBehavior));

    // Set stop as default behavior
    auto stopBehavior = std::make_unique<CBehaviorStop>();
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
    (void)game; (void)unit; (void)orderId; (void)target;
    if (IsDisabled()) return false;
    // Move/follow can target any widget
    return target != nullptr && !target->IsDead();
}

bool CAbilityMove::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)unit; (void)orderId; (void)x; (void)y;
    if (IsDisabled()) return false;
    return true; // move can target any point
}

bool CAbilityMove::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game;
    if (IsDisabled()) return false;
    // Hold position is a no-target order
    return orderId == OrderIds::HOLD_POSITION;
}

CBehavior* CAbilityMove::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)game; (void)orderId;
    if (!target) return nullptr;

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

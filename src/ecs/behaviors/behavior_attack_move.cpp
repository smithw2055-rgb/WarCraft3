#include "behavior_attack_move.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/pathing/world_collision.h"

CBehaviorAttackMove::CBehaviorAttackMove() {}

void CBehaviorAttackMove::SetDestination(float x, float y) {
    destX_ = x;
    destY_ = y;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    pathFound_ = false;
}

void CBehaviorAttackMove::Begin(CSimulation& simulation) {
    propelling_ = false;
    RecalculatePath(simulation, destX_, destY_);
}

void CBehaviorAttackMove::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    pathFound_ = false;
}

bool CBehaviorAttackMove::RecalculatePath(CSimulation& simulation, float targetX, float targetY) {
    pathFound_ = MoveIntoRange::RecalculateWaypoints(simulation, unit_, targetX, targetY, waypoints_, true);
    return pathFound_;
}

bool CBehaviorAttackMove::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || waypoints_.empty()) return false;
    MoveIntoRange::StepFrontWaypoint(simulation, unit_, waypoints_, pathRecalcCooldown_,
                                     propelling_, true, true, destX_, destY_, true, &pathFound_);
    return false;
}

bool CBehaviorAttackMove::Update(CSimulation& simulation) {
    if (!unit_) return true;
    if (unit_->IsBuilding() || !unit_->GetMoveBehavior() || unit_->IsRooted()) {
        return true;
    }

    const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), destX_, destY_);
    if (dist < arrivalDistance_) {
        propelling_ = false;
        return true; // arrived
    }

    // Periodically scan for enemies
    const int tick = simulation.GetGameTurnTick();
    if (tick - lastAttackScanTick_ > 5) {
        lastAttackScanTick_ = tick;
        float acqRange = unit_->GetUnitType()->GetDefaultAcquisitionRange();
        if (acqRange <= 0) acqRange = 256.0f;

        std::vector<CUnit*> enemies;
        simulation.GetWorldCollision()->EnumUnitsInRange(
            unit_->GetX(), unit_->GetY(), acqRange, enemies);

        for (auto* e : enemies) {
            if (e == unit_ || e->IsDead()) continue;
            if (e->GetPlayerIndex() == unit_->GetPlayerIndex()) continue;
            targetHandle_ = e->GetHandleId();
            break;
        }
    }

    if (targetHandle_ >= 0) {
        CUnit* target = simulation.GetUnit(targetHandle_);
        if (!target || target->IsDead()) {
            targetHandle_ = -1;
            waypoints_.clear();
            pathFound_ = false;
        } else {
            const float targetX = target->GetX();
            const float targetY = target->GetY();
            const float edist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), targetX, targetY);
            constexpr float attackRange = 128.0f;

            if (edist > attackRange) {
                MoveIntoRange::RefreshWaypointsIfNeeded(simulation, unit_, targetX, targetY,
                                                        waypoints_, pathRecalcCooldown_, true, true, &pathFound_);
                if (!waypoints_.empty()) {
                    return MoveTowardWaypoint(simulation);
                }
                MoveIntoRange::StepDirectValidated(simulation, unit_, targetX, targetY, edist, propelling_);
            } else {
                unit_->TurnTowardPoint(targetX, targetY);
                propelling_ = false;
            }
            return false;
        }
    }

    MoveIntoRange::RefreshWaypointsIfNeeded(simulation, unit_, destX_, destY_,
                                            waypoints_, pathRecalcCooldown_, false, true, &pathFound_);
    if (!waypoints_.empty()) {
        return MoveTowardWaypoint(simulation);
    }

    MoveIntoRange::StepDirectValidated(simulation, unit_, destX_, destY_, dist, propelling_);
    return false;
}

std::unique_ptr<CBehavior> CBehavior::CreateAttackMove() { return std::make_unique<CBehaviorAttackMove>(); }

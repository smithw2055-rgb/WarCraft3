#include "behavior_follow.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"

CBehaviorFollow::CBehaviorFollow() {}

void CBehaviorFollow::SetTarget(CWidget* target, float followRange) {
    target_ = target;
    followRange_ = followRange;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    pathFound_ = false;
}

void CBehaviorFollow::Begin(CSimulation& simulation) {
    propelling_ = false;
    if (!target_) return;
    RecalculatePath(simulation, target_->GetX(), target_->GetY());
}

void CBehaviorFollow::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    pathFound_ = false;
}

bool CBehaviorFollow::RecalculatePath(CSimulation& simulation, float targetX, float targetY) {
    pathFound_ = MoveIntoRange::RecalculateWaypoints(simulation, unit_, targetX, targetY, waypoints_, true);
    return pathFound_;
}

bool CBehaviorFollow::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || waypoints_.empty()) return false;
    const float rx = target_ ? target_->GetX() : waypoints_.front().first;
    const float ry = target_ ? target_->GetY() : waypoints_.front().second;
    MoveIntoRange::StepFrontWaypoint(simulation, unit_, waypoints_, pathRecalcCooldown_,
                                     propelling_, true, true, rx, ry, true, &pathFound_);
    return false;
}

bool CBehaviorFollow::Update(CSimulation& simulation) {
    if (!unit_ || !target_ || unit_->IsRooted()) return true;

    const float targetX = target_->GetX();
    const float targetY = target_->GetY();
    const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), targetX, targetY);

    if (dist <= followRange_) {
        unit_->TurnTowardPoint(targetX, targetY);
        propelling_ = false;
        return false;
    }

    MoveIntoRange::RefreshWaypointsIfNeeded(simulation, unit_, targetX, targetY,
                                            waypoints_, pathRecalcCooldown_, true, true, &pathFound_);
    if (!waypoints_.empty()) {
        return MoveTowardWaypoint(simulation);
    }

    MoveIntoRange::StepDirectValidated(simulation, unit_, targetX, targetY,
                                       dist - followRange_ + 16.0f, propelling_, &pathFound_);
    return false;
}

std::unique_ptr<CBehavior> CBehavior::CreateFollow() { return std::make_unique<CBehaviorFollow>(); }

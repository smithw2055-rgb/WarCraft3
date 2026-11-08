#include "behavior_patrol.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"

CBehaviorPatrol::CBehaviorPatrol() {}

void CBehaviorPatrol::SetPoints(float x1, float y1, float x2, float y2) {
    points_.clear();
    points_.push_back({x1, y1});
    points_.push_back({x2, y2});
    currentTarget_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    pathFound_ = false;
}

void CBehaviorPatrol::Begin(CSimulation& simulation) {
    propelling_ = false;
    if (points_.empty()) return;
    RecalculatePath(simulation, points_[currentTarget_].x, points_[currentTarget_].y);
}

void CBehaviorPatrol::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    pathFound_ = false;
}

bool CBehaviorPatrol::RecalculatePath(CSimulation& simulation, float targetX, float targetY) {
    pathFound_ = MoveIntoRange::RecalculateWaypoints(simulation, unit_, targetX, targetY, waypoints_, true);
    return pathFound_;
}

bool CBehaviorPatrol::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || waypoints_.empty()) return false;
    MoveIntoRange::StepFrontWaypoint(simulation, unit_, waypoints_, pathRecalcCooldown_,
                                     propelling_, true, true,
                                     points_[currentTarget_].x, points_[currentTarget_].y,
                                     true, &pathFound_);
    return false;
}

bool CBehaviorPatrol::Update(CSimulation& simulation) {
    if (!unit_ || points_.empty() || unit_->IsRooted()) return true;

    auto& target = points_[currentTarget_];
    const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), target.x, target.y);

    if (dist < arrivalDistance_) {
        currentTarget_ = (currentTarget_ + 1) % points_.size();
        waypoints_.clear();
        pathFound_ = false;
        pathRecalcCooldown_ = 0;
        propelling_ = false;
        return false;
    }

    MoveIntoRange::RefreshWaypointsIfNeeded(simulation, unit_, target.x, target.y,
                                            waypoints_, pathRecalcCooldown_, false, true, &pathFound_);
    if (!waypoints_.empty()) {
        return MoveTowardWaypoint(simulation);
    }

    MoveIntoRange::StepDirectValidated(simulation, unit_, target.x, target.y, dist, propelling_);
    return false;
}

std::unique_ptr<CBehavior> CBehavior::CreatePatrol() { return std::make_unique<CBehaviorPatrol>(); }

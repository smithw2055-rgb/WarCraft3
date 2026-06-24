#include "behavior_patrol.h"
#include "ecs/csimulation.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include "ecs/unit.h"
#include <algorithm>
#include <cmath>

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
    if (points_.empty()) {
        return;
    }
    RecalculatePath(simulation, points_[currentTarget_].x, points_[currentTarget_].y);
}

void CBehaviorPatrol::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    pathFound_ = false;
}

bool CBehaviorPatrol::RecalculatePath(CSimulation& simulation, float targetX, float targetY) {
    waypoints_.clear();
    pathFound_ = false;

    if (!unit_) {
        return false;
    }

    auto* pathfinding = simulation.GetPathfinding();
    if (!pathfinding) {
        return false;
    }

    std::vector<std::pair<float, float>> worldPath;
    if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(), targetX, targetY,
                                   std::max(0.0f, unit_->GetCollisionSize()),
                                   unit_->GetMovementType(), unit_, worldPath)) {
        for (size_t i = 1; i < worldPath.size(); ++i) {
            waypoints_.push_back(worldPath[i]);
        }
        pathFound_ = !waypoints_.empty();
        return pathFound_;
    }

    if (!pathfinding->HasPathingMap()) {
        waypoints_.push_back({targetX, targetY});
        pathFound_ = true;
        return true;
    }

    return false;
}

bool CBehaviorPatrol::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || waypoints_.empty()) {
        return false;
    }

    auto& wp = waypoints_.front();
    const float dx = wp.first - unit_->GetX();
    const float dy = wp.second - unit_->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float stepDistance = unit_->GetSpeed() * simulation.GetSimStepTime();
    if (dist <= std::max(1.0f, stepDistance)) {
        waypoints_.pop_front();
        propelling_ = false;
        return false;
    }

    if (!unit_->TurnTowardPoint(wp.first, wp.second)) {
        propelling_ = false;
        return false;
    }

    const float step = std::min(stepDistance, dist);
    const float nx = unit_->GetX() + (dx / dist) * step;
    const float ny = unit_->GetY() + (dy / dist) * step;

    auto* pathfinding = simulation.GetPathfinding();
    const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
    if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                 unit_->GetMovementType(), unit_)) {
        propelling_ = false;
        pathFound_ = false;
        waypoints_.clear();
        pathRecalcCooldown_ = 0;
        RecalculatePath(simulation, points_[currentTarget_].x, points_[currentTarget_].y);
        return false;
    }

    auto* collision = simulation.GetWorldCollision();
    if (collision) {
        float dynamicCollisionRadius = unit_->GetCollisionSize();
        if (dynamicCollisionRadius < 0) {
            dynamicCollisionRadius = 16.0f;
        }
        if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit_)) {
            propelling_ = false;
            pathFound_ = false;
            waypoints_.clear();
            pathRecalcCooldown_ = 0;
            RecalculatePath(simulation, points_[currentTarget_].x, points_[currentTarget_].y);
            return false;
        }
    }

    unit_->SetPosition(nx, ny);
    propelling_ = true;
    return false;
}

bool CBehaviorPatrol::Update(CSimulation& simulation) {
    if (!unit_ || points_.empty()) return true;

    auto& target = points_[currentTarget_];
    float dx = target.x - unit_->GetX();
    float dy = target.y - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < arrivalDistance_) {
        // Reached waypoint, go to next
        currentTarget_ = (currentTarget_ + 1) % points_.size();
        waypoints_.clear();
        pathFound_ = false;
        pathRecalcCooldown_ = 0;
        propelling_ = false;
        return false;
    }

    pathRecalcCooldown_--;
    if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
        RecalculatePath(simulation, target.x, target.y);
        pathRecalcCooldown_ = 5;
    }

    if (!waypoints_.empty()) {
        return MoveTowardWaypoint(simulation);
    }

    auto* pathfinding = simulation.GetPathfinding();
    if (pathfinding && pathfinding->HasPathingMap()) {
        propelling_ = false;
        return false;
    }

    if (!unit_->TurnTowardPoint(target.x, target.y)) {
        propelling_ = false;
        return false;
    }

    float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
    float step = std::min(speed, dist);
    float nx = unit_->GetX() + (dx / dist) * step;
    float ny = unit_->GetY() + (dy / dist) * step;
    const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
    if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                 unit_->GetMovementType(), unit_)) {
        propelling_ = false;
        return false;
    }
    if (auto* collision = simulation.GetWorldCollision()) {
        const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
        if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit_)) {
            propelling_ = false;
            return false;
        }
    }
    unit_->SetPosition(nx, ny);
    propelling_ = true;
    return false;
}

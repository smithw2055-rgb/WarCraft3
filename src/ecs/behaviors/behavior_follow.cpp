#include "behavior_follow.h"
#include "ecs/csimulation.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include "ecs/unit.h"
#include <algorithm>
#include <cmath>

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
    if (!target_) {
        return;
    }
    RecalculatePath(simulation, target_->GetX(), target_->GetY());
}

void CBehaviorFollow::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    pathFound_ = false;
}

bool CBehaviorFollow::RecalculatePath(CSimulation& simulation, float targetX, float targetY) {
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

bool CBehaviorFollow::MoveTowardWaypoint(CSimulation& simulation) {
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
        RecalculatePath(simulation, target_ ? target_->GetX() : wp.first, target_ ? target_->GetY() : wp.second);
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
            RecalculatePath(simulation, target_ ? target_->GetX() : wp.first, target_ ? target_->GetY() : wp.second);
            return false;
        }
    }

    unit_->SetPosition(nx, ny);
    propelling_ = true;
    return false;
}

bool CBehaviorFollow::Update(CSimulation& simulation) {
    if (!unit_ || !target_ || unit_->IsRooted()) return true;

    const float targetX = target_->GetX();
    const float targetY = target_->GetY();
    const float dx = targetX - unit_->GetX();
    const float dy = targetY - unit_->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist <= followRange_) {
        unit_->TurnTowardPoint(targetX, targetY);
        propelling_ = false;
        return false;
    }

    pathRecalcCooldown_--;
    if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
        static constexpr float kRecalcDist = 64.0f;
        bool shouldRecalculate = waypoints_.empty();
        if (!shouldRecalculate && !waypoints_.empty()) {
            const auto& lastWp = waypoints_.back();
            const float ddx = lastWp.first - targetX;
            const float ddy = lastWp.second - targetY;
            shouldRecalculate = (ddx * ddx + ddy * ddy) > (kRecalcDist * kRecalcDist);
        }
        if (shouldRecalculate) {
            RecalculatePath(simulation, targetX, targetY);
        }
        pathRecalcCooldown_ = 5;
    }

    if (!waypoints_.empty()) {
        return MoveTowardWaypoint(simulation);
    }

    auto* pathfinding = simulation.GetPathfinding();
    if (pathfinding && pathfinding->HasPathingMap()) {
        propelling_ = false;
        pathFound_ = false;
        return false;
    }

    if (!unit_->TurnTowardPoint(targetX, targetY)) {
        propelling_ = false;
        return false;
    }

    const float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
    const float step = std::min(speed, dist - followRange_ + 16.0f);
    const float nx = unit_->GetX() + (dx / dist) * step;
    const float ny = unit_->GetY() + (dy / dist) * step;
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

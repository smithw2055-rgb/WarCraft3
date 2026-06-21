#include "behavior_move.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include <cmath>
#include <algorithm>

CBehaviorMove::CBehaviorMove() {}

void CBehaviorMove::SetTarget(float x, float y) {
    targetX_ = x;
    targetY_ = y;
    targetWidget_ = nullptr;
    waypoints_.clear();
    pathFound_ = false;
}

void CBehaviorMove::SetTargetWidget(CWidget* target) {
    targetWidget_ = target;
    if (target) {
        targetX_ = target->GetX();
        targetY_ = target->GetY();
    }
    waypoints_.clear();
    pathFound_ = false;
}

void CBehaviorMove::Begin(CSimulation& simulation) {
    RecalculatePath(simulation);
}

void CBehaviorMove::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
}

bool CBehaviorMove::RecalculatePath(CSimulation& simulation) {
    waypoints_.clear();
    pathFound_ = false;

    if (!unit_) return false;

    auto* pathfinding = simulation.GetPathfinding();
    if (!pathfinding) return false;

    std::vector<std::pair<float,float>> worldPath;
    if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                    targetX_, targetY_, worldPath)) {
        // Skip the first waypoint (current position)
        for (size_t i = 1; i < worldPath.size(); i++) {
            waypoints_.push_back(worldPath[i]);
        }
        pathFound_ = !waypoints_.empty();
        return pathFound_;
    }

    // Fallback: direct line if pathfinding fails
    waypoints_.push_back({targetX_, targetY_});
    pathFound_ = true;
    return true;
}

bool CBehaviorMove::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || waypoints_.empty()) return false;

    auto& wp = waypoints_.front();
    float dx = wp.first - unit_->GetX();
    float dy = wp.second - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    // Face movement direction
    if (dist > 0.1f) {
        float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
        unit_->SetFacing(facing);
    }

    // Check if arrived at waypoint
    if (dist < arrivalDistance_) {
        waypoints_.pop_front();

        // Check if all waypoints done
        if (waypoints_.empty()) return true; // arrived at final target
        return false; // more waypoints to go
    }

    float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
    float step = std::min(speed, dist);

    float nx = unit_->GetX() + (dx / dist) * step;
    float ny = unit_->GetY() + (dy / dist) * step;

    // Check collision with other units
    auto* collision = simulation.GetWorldCollision();
    if (collision) {
        float collisionRadius = unit_->GetCollisionSize();
        if (collisionRadius < 0) collisionRadius = 16.0f;

        if (collision->CheckCollision(nx, ny, collisionRadius, unit_)) {
            return false; // blocked this frame
        }
    }

    unit_->SetPosition(nx, ny);
    return false;
}

bool CBehaviorMove::Update(CSimulation& simulation) {
    if (!unit_) return true;

    // If following a moving target, update destination
    if (targetWidget_) {
        targetX_ = targetWidget_->GetX();
        targetY_ = targetWidget_->GetY();
    }

    // Recalculate path periodically or when target moves significantly
    pathRecalcCooldown_--;
    if (pathRecalcCooldown_ <= 0) {
        static float kRecalcDist = 64.0f;
        if (!waypoints_.empty()) {
            auto& lastWp = waypoints_.back();
            float ddx = lastWp.first - targetX_;
            float ddy = lastWp.second - targetY_;
            if (ddx * ddx + ddy * ddy > kRecalcDist * kRecalcDist) {
                RecalculatePath(simulation);
            }
        }
        pathRecalcCooldown_ = 5;
    }

    // If no path yet, try to find one
    if (waypoints_.empty() && !pathFound_) {
        RecalculatePath(simulation);
    }

    // Check if we're at the final target
    if (waypoints_.empty()) {
        return true; // arrived
    }

    return MoveTowardWaypoint(simulation);
}

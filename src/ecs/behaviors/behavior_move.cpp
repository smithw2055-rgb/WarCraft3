#include "behavior_move.h"
#include "debug/move_debug.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace {

void LogMoveDebug(const CUnit* unit, const char* phase, float a, float b, float c, float d,
                  const std::string& extra = std::string()) {
    if (!unit) {
        return;
    }
    std::ostringstream oss;
    oss << "[MoveDebug] unit=" << unit->GetHandleId()
        << " type=" << unit->GetTypeId().ToString()
        << " phase=" << phase
        << " from=(" << a << ", " << b << ")"
        << " to=(" << c << ", " << d << ")";
    if (!extra.empty()) {
        oss << " " << extra;
    }
    MoveDebugLogLine(oss.str());
}

}

CBehaviorMove::CBehaviorMove() {}

void CBehaviorMove::SetTarget(float x, float y) {
    targetX_ = x;
    targetY_ = y;
    targetWidget_ = nullptr;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    lastProgressLogTick_ = -9999;
    pathFound_ = false;
    blockedTicks_ = 0;
    repathRetryCount_ = 0;
}

void CBehaviorMove::SetTargetWidget(CWidget* target) {
    targetWidget_ = target;
    if (target) {
        targetX_ = target->GetX();
        targetY_ = target->GetY();
    }
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    lastProgressLogTick_ = -9999;
    pathFound_ = false;
    blockedTicks_ = 0;
    repathRetryCount_ = 0;
}

void CBehaviorMove::Begin(CSimulation& simulation) {
    propelling_ = false;
    lastProgressLogTick_ = -9999;
    blockedTicks_ = 0;
    repathRetryCount_ = 0;
    LogMoveDebug(unit_, "begin", unit_ ? unit_->GetX() : 0.0f, unit_ ? unit_->GetY() : 0.0f,
                 targetX_, targetY_);
    RecalculatePath(simulation);
}

void CBehaviorMove::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
}

bool CBehaviorMove::RecalculatePath(CSimulation& simulation) {
    waypoints_.clear();
    pathFound_ = false;

    if (!unit_) return false;

    auto* pathfinding = simulation.GetPathfinding();
    if (!pathfinding) return false;

    std::vector<std::pair<float,float>> worldPath;
    const int startCellX = pathfinding->WorldToCellX(unit_->GetX());
    const int startCellY = pathfinding->WorldToCellY(unit_->GetY());
    const int targetCellX = pathfinding->WorldToCellX(targetX_);
    const int targetCellY = pathfinding->WorldToCellY(targetY_);
    const bool targetCellWalkable = !pathfinding->HasPathingMap()
        || pathfinding->IsCellWalkable(targetCellX, targetCellY);
    const float collisionSize = std::max(0.0f, unit_->GetCollisionSize());
    const auto gridMapping = CPathfindingProcessor::SelectGridMapping(collisionSize);
    if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                    targetX_, targetY_, collisionSize,
                                    unit_->GetMovementType(), unit_, worldPath)) {
        // Skip the first waypoint (current position)
        for (size_t i = 1; i < worldPath.size(); i++) {
            waypoints_.push_back(worldPath[i]);
        }
        pathFound_ = !waypoints_.empty();
        std::string extra = "pathFound=" + std::to_string(pathFound_ ? 1 : 0)
            + " waypoints=" + std::to_string(waypoints_.size())
            + " startCell=(" + std::to_string(startCellX)
            + "," + std::to_string(startCellY) + ")"
            + " targetCell=(" + std::to_string(targetCellX)
            + "," + std::to_string(targetCellY) + ")"
            + " targetCellWalkable=" + std::to_string(targetCellWalkable ? 1 : 0)
            + " grid=" + std::string(gridMapping == CPathfindingProcessor::GridMapping::CORNERS ? "corners" : "cells");
        if (!waypoints_.empty()) {
            const float lastDx = waypoints_.back().first - targetX_;
            const float lastDy = waypoints_.back().second - targetY_;
            const float lastDrift = std::sqrt(lastDx * lastDx + lastDy * lastDy);
            extra += " firstWp=(" + std::to_string(waypoints_.front().first)
                + "," + std::to_string(waypoints_.front().second) + ")";
            extra += " lastWp=(" + std::to_string(waypoints_.back().first)
                + "," + std::to_string(waypoints_.back().second) + ")";
            extra += " lastWpDrift=" + std::to_string(lastDrift);
        }
        LogMoveDebug(unit_, "repath_ok", unit_->GetX(), unit_->GetY(), targetX_, targetY_, extra);
        if (pathFound_) {
            repathRetryCount_ = 0;
        }
        return pathFound_;
    }

    if (!pathfinding->HasPathingMap()) {
        waypoints_.push_back({targetX_, targetY_});
        pathFound_ = true;
        LogMoveDebug(unit_, "repath_direct", unit_->GetX(), unit_->GetY(), targetX_, targetY_,
                     "reason=no_pathing_map");
        return true;
    }

    LogMoveDebug(unit_, "repath_fail", unit_->GetX(), unit_->GetY(), targetX_, targetY_,
                 "reason=find_path_failed startCell=(" + std::to_string(startCellX)
                 + "," + std::to_string(startCellY) + ") targetCell=("
                 + std::to_string(targetCellX) + "," + std::to_string(targetCellY)
                 + ") targetCellWalkable=" + std::to_string(targetCellWalkable ? 1 : 0));
    return false;
}

bool CBehaviorMove::UpdateFacingToward(float targetX, float targetY) {
    return unit_ ? unit_->TurnTowardPoint(targetX, targetY) : false;
}

float CBehaviorMove::ComputeFinalArrivalDistance(const CSimulation& simulation) const {
    if (!unit_) {
        return 8.0f;
    }
    const float stepDistance = unit_->GetSpeed() * simulation.GetSimStepTime();
    const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
    return std::max(8.0f, std::min(20.0f, std::max(stepDistance * 1.25f, collisionRadius * 0.35f)));
}

bool CBehaviorMove::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || waypoints_.empty()) return false;

    auto& wp = waypoints_.front();
    float dx = wp.first - unit_->GetX();
    float dy = wp.second - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);
    const bool finalWaypoint = waypoints_.size() == 1;
    const float stepDistance = unit_->GetSpeed() * simulation.GetSimStepTime();
    const float arrivalDistance = finalWaypoint
        ? ComputeFinalArrivalDistance(simulation)
        : std::max(1.0f, stepDistance);

    // Check if arrived at waypoint
    if (dist <= arrivalDistance) {
        if (finalWaypoint) {
            unit_->SetPosition(wp.first, wp.second);
            const float finalErrorX = targetX_ - unit_->GetX();
            const float finalErrorY = targetY_ - unit_->GetY();
            const float finalError = std::sqrt(finalErrorX * finalErrorX + finalErrorY * finalErrorY);
            LogMoveDebug(unit_, "arrive_final", unit_->GetX(), unit_->GetY(), targetX_, targetY_,
                         "error=" + std::to_string(finalError));
        } else {
            LogMoveDebug(unit_, "arrive_wp", unit_->GetX(), unit_->GetY(), wp.first, wp.second,
                         "remaining=" + std::to_string(waypoints_.size() - 1));
        }
        waypoints_.pop_front();
        propelling_ = false;

        // Check if all waypoints done
        if (waypoints_.empty()) return true; // arrived at final target
        return false; // more waypoints to go
    }

    if (!UpdateFacingToward(wp.first, wp.second)) {
        propelling_ = false;
        return false;
    }

    float step = std::min(stepDistance, dist);
    if (finalWaypoint && step >= dist) {
        unit_->SetPosition(wp.first, wp.second);
        const float finalErrorX = targetX_ - unit_->GetX();
        const float finalErrorY = targetY_ - unit_->GetY();
        const float finalError = std::sqrt(finalErrorX * finalErrorX + finalErrorY * finalErrorY);
        LogMoveDebug(unit_, "snap_final", unit_->GetX(), unit_->GetY(), targetX_, targetY_,
                     "error=" + std::to_string(finalError));
        waypoints_.pop_front();
        propelling_ = false;
        return true;
    }

    float nx = unit_->GetX() + (dx / dist) * step;
    float ny = unit_->GetY() + (dy / dist) * step;

    auto* pathfinding = simulation.GetPathfinding();
    const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
    if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                 unit_->GetMovementType(), unit_)) {
        propelling_ = false;
        LogMoveDebug(unit_, "blocked_terrain", unit_->GetX(), unit_->GetY(), nx, ny,
                     "target=(" + std::to_string(targetX_) + "," + std::to_string(targetY_) + ")");
        pathFound_ = false;
        waypoints_.clear();
        pathRecalcCooldown_ = 0;
        RecalculatePath(simulation);
        return false;
    }

    // Check collision with other units
    auto* collision = simulation.GetWorldCollision();
    if (collision) {
        float dynamicCollisionRadius = unit_->GetCollisionSize();
        if (dynamicCollisionRadius < 0) dynamicCollisionRadius = 16.0f;

        if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit_)) {
            propelling_ = false;
            blockedTicks_++;
            LogMoveDebug(unit_, "blocked", unit_->GetX(), unit_->GetY(), nx, ny,
                         "radius=" + std::to_string(dynamicCollisionRadius) + " blockedTicks=" + std::to_string(blockedTicks_));
            // Only repath after being blocked for multiple consecutive ticks
            if (blockedTicks_ >= 5) {
                pathFound_ = false;
                waypoints_.clear();
                pathRecalcCooldown_ = 0;
                RecalculatePath(simulation);
                blockedTicks_ = 0;
            }
            return false; // blocked this frame
        }
    }

    unit_->SetPosition(nx, ny);
    propelling_ = true;
    blockedTicks_ = 0;
    if ((simulation.GetGameTurnTick() - lastProgressLogTick_) >= 5) {
        lastProgressLogTick_ = simulation.GetGameTurnTick();
        const float targetDx = targetX_ - unit_->GetX();
        const float targetDy = targetY_ - unit_->GetY();
        const float targetDist = std::sqrt(targetDx * targetDx + targetDy * targetDy);
        LogMoveDebug(unit_, finalWaypoint ? "move_final" : "move_wp",
                     unit_->GetX(), unit_->GetY(), wp.first, wp.second,
                     "dist=" + std::to_string(dist)
                     + " targetDist=" + std::to_string(targetDist)
                     + " remainingWp=" + std::to_string(waypoints_.size()));
    }
    return false;
}

bool CBehaviorMove::Update(CSimulation& simulation) {
    if (!unit_ || unit_->IsRooted()) return true;

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
                LogMoveDebug(unit_, "repath_target_shift", unit_->GetX(), unit_->GetY(), targetX_, targetY_,
                             "lastWp=(" + std::to_string(lastWp.first) + "," + std::to_string(lastWp.second)
                             + ") driftSq=" + std::to_string(ddx * ddx + ddy * ddy));
                RecalculatePath(simulation);
            }
        }
        pathRecalcCooldown_ = 5;
    }

    // If no path yet, try to find one with retry budget
    if (waypoints_.empty() && !pathFound_) {
        if (repathRetryCount_ < 10) {
            repathRetryCount_++;
            RecalculatePath(simulation);
        } else {
            propelling_ = false;
            return true;
        }
    }

    // Check if we're at the final target
    if (waypoints_.empty()) {
        if (!pathFound_ && repathRetryCount_ < 10) {
            return false;
        }
        propelling_ = false;
        return true; // arrived
    }

    return MoveTowardWaypoint(simulation);
}

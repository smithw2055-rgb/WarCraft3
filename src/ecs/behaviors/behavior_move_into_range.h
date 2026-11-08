#pragma once
// Shared "move into range" and waypoint navigation kernel (Phase 23–24).
//
// Centralizes 2D distance/stepping math, pathfinding/collision-validated
// waypoint stepping, pruned-waypoint worker navigation, and nearest-tree
// resource discovery shared across CBehaviorAttack, CBehaviorAttackMove,
// CBehaviorFollow, CBehaviorPatrol, CBehaviorSpell, CBehaviorHarvest,
// CBehaviorReturnResources, CBehaviorRepair, CBehaviorBoardTransport,
// and custom plugin behaviors.
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <utility>
#include <vector>
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include "data/destructabledata.h"

namespace MoveIntoRange {

inline float Distance2D(float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    return std::sqrt(dx * dx + dy * dy);
}

inline bool IsWithinRange(float ax, float ay, float bx, float by, float range) {
    return Distance2D(ax, ay, bx, by) <= range;
}

// Step (ax,ay) toward (tx,ty) by at most stepLen; writes nx/ny.
// Returns false when already there (dist <= 0).
inline bool StepToward(float ax, float ay, float tx, float ty, float stepLen,
                       float& nx, float& ny) {
    const float dx = tx - ax;
    const float dy = ty - ay;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= 0.0f || stepLen <= 0.0f) return false;
    const float step = std::min(stepLen, dist);
    nx = ax + (dx / dist) * step;
    ny = ay + (dy / dist) * step;
    return true;
}

// Compute world path waypoints from unit's current position to (targetX, targetY).
inline bool RecalculateWaypoints(CSimulation& sim, CUnit* unit,
                                 float targetX, float targetY,
                                 std::deque<std::pair<float, float>>& waypoints,
                                 bool fallbackDirectIfNoMap = false) {
    waypoints.clear();
    if (!unit) return false;
    auto* pathfinding = sim.GetPathfinding();
    if (!pathfinding) return false;

    std::vector<std::pair<float, float>> worldPath;
    if (pathfinding->FindPathWorld(unit->GetX(), unit->GetY(), targetX, targetY,
                                   std::max(0.0f, unit->GetCollisionSize()),
                                   unit->GetMovementType(), unit, worldPath)) {
        for (size_t i = 1; i < worldPath.size(); ++i) {
            waypoints.push_back(worldPath[i]);
        }
        return !waypoints.empty();
    }

    if (fallbackDirectIfNoMap && !pathfinding->HasPathingMap()) {
        waypoints.push_back({targetX, targetY});
        return true;
    }
    return false;
}

// Compute world path waypoints while dropping already-reached near waypoints
// (used by worker behaviors: Harvest, ReturnResources, Repair, Build).
inline bool RecalculatePrunedWaypoints(CSimulation& sim, CUnit* unit,
                                       float targetX, float targetY,
                                       std::deque<std::pair<float, float>>& waypoints) {
    waypoints.clear();
    if (!unit) return false;
    auto* pathfinding = sim.GetPathfinding();
    if (!pathfinding) return false;

    std::vector<std::pair<float, float>> worldPath;
    if (!pathfinding->FindPathWorld(unit->GetX(), unit->GetY(), targetX, targetY,
                                    std::max(0.0f, unit->GetCollisionSize()),
                                    unit->GetMovementType(), unit, worldPath)) {
        return false;
    }

    const float stepReach = std::max(1.0f, unit->GetSpeed() * sim.GetSimStepTime()) * 2.0f;
    const float reachSq = stepReach * stepReach;
    for (size_t i = 1; i < worldPath.size(); ++i) {
        const float rdx = worldPath[i].first - unit->GetX();
        const float rdy = worldPath[i].second - unit->GetY();
        if (rdx * rdx + rdy * rdy > reachSq) {
            waypoints.push_back(worldPath[i]);
        }
    }
    return !waypoints.empty();
}

// Periodic cooldown-gated waypoint refresh with optional target-drift threshold check.
inline void RefreshWaypointsIfNeeded(CSimulation& sim, CUnit* unit,
                                     float targetX, float targetY,
                                     std::deque<std::pair<float, float>>& waypoints,
                                     int& pathRecalcCooldown,
                                     bool checkDrift = true,
                                     bool fallbackDirectIfNoMap = false,
                                     bool* pathFoundOut = nullptr) {
    pathRecalcCooldown--;
    if (waypoints.empty() || pathRecalcCooldown <= 0) {
        static constexpr float kRecalcDist = 64.0f;
        bool shouldRecalculate = !checkDrift || waypoints.empty();
        if (!shouldRecalculate && !waypoints.empty()) {
            const auto& lastWp = waypoints.back();
            const float ddx = lastWp.first - targetX;
            const float ddy = lastWp.second - targetY;
            shouldRecalculate = (ddx * ddx + ddy * ddy) > (kRecalcDist * kRecalcDist);
        }
        if (shouldRecalculate) {
            const bool ok = RecalculateWaypoints(sim, unit, targetX, targetY, waypoints, fallbackDirectIfNoMap);
            if (pathFoundOut) *pathFoundOut = ok;
        }
        pathRecalcCooldown = 5;
    }
}

// Advance one step along the front waypoint with pathability + dynamic collision checks.
inline bool StepFrontWaypoint(CSimulation& sim, CUnit* unit,
                              std::deque<std::pair<float, float>>& waypoints,
                              int& pathRecalcCooldown,
                              bool& propelling,
                              bool returnOnWaypointPop = true,
                              bool recalcOnBlock = false,
                              float recalcTargetX = 0.0f,
                              float recalcTargetY = 0.0f,
                              bool fallbackDirectIfNoMap = false,
                              bool* pathFoundOut = nullptr) {
    if (!unit || waypoints.empty()) return false;

    auto& wp = waypoints.front();
    const float wdx = wp.first - unit->GetX();
    const float wdy = wp.second - unit->GetY();
    const float wdist = std::sqrt(wdx * wdx + wdy * wdy);
    const float speed = unit->GetSpeed() * sim.GetSimStepTime();
    if (wdist <= std::max(1.0f, speed)) {
        waypoints.pop_front();
        propelling = false;
        return returnOnWaypointPop;
    }

    if (!unit->TurnTowardPoint(wp.first, wp.second)) {
        propelling = false;
        return true;
    }

    const float step = std::min(speed, wdist);
    const float nx = unit->GetX() + (wdx / wdist) * step;
    const float ny = unit->GetY() + (wdy / wdist) * step;
    auto* pathfinding = sim.GetPathfinding();
    const float collisionRadius = std::max(0.0f, unit->GetCollisionSize());

    auto handleBlocked = [&]() {
        propelling = false;
        if (pathFoundOut) *pathFoundOut = false;
        waypoints.clear();
        pathRecalcCooldown = 0;
        if (recalcOnBlock) {
            const bool ok = RecalculateWaypoints(sim, unit, recalcTargetX, recalcTargetY, waypoints, fallbackDirectIfNoMap);
            if (pathFoundOut) *pathFoundOut = ok;
        }
    };

    if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                 unit->GetMovementType(), unit)) {
        handleBlocked();
        return true;
    }

    if (auto* collision = sim.GetWorldCollision()) {
        float dynamicCollisionRadius = unit->GetCollisionSize();
        if (dynamicCollisionRadius < 0.0f) {
            dynamicCollisionRadius = 16.0f;
        } else if (!recalcOnBlock) {
            dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
        }
        if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit)) {
            handleBlocked();
            return true;
        }
    }

    unit->SetPosition(nx, ny);
    propelling = true;
    return true;
}

// Worker navigation step (shared by Harvest, ReturnResources, Repair):
// recalculates pruned waypoints periodically and steps along front waypoint or directly to target.
inline void StepWorkerTowardTarget(CSimulation& sim, CUnit* unit,
                                   float targetX, float targetY,
                                   float distToTarget,
                                   std::deque<std::pair<float, float>>& waypoints,
                                   int& pathRecalcCooldown,
                                   bool& propelling,
                                   bool checkWaypointCollision = true,
                                   bool checkDirectCollision = false,
                                   bool propelOnWaypointPop = true) {
    if (!unit) return;
    pathRecalcCooldown--;
    if (waypoints.empty() || pathRecalcCooldown <= 0) {
        RecalculatePrunedWaypoints(sim, unit, targetX, targetY, waypoints);
        pathRecalcCooldown = 8;
    }

    auto* pathfinding = sim.GetPathfinding();
    const float speed = unit->GetSpeed() * sim.GetSimStepTime();
    const float collisionRadius = std::max(0.0f, unit->GetCollisionSize());

    if (!waypoints.empty()) {
        auto& wp = waypoints.front();
        const float wdx = wp.first - unit->GetX();
        const float wdy = wp.second - unit->GetY();
        const float wdist = std::sqrt(wdx * wdx + wdy * wdy);
        if (wdist <= std::max(1.0f, speed)) {
            waypoints.pop_front();
            propelling = propelOnWaypointPop;
            return;
        }
        if (!unit->TurnTowardPoint(wp.first, wp.second)) {
            propelling = false;
            return;
        }
        const float step = std::min(speed, wdist);
        const float nx = unit->GetX() + (wdx / wdist) * step;
        const float ny = unit->GetY() + (wdy / wdist) * step;
        if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                     unit->GetMovementType(), unit)) {
            propelling = false;
            waypoints.clear();
            pathRecalcCooldown = 0;
            return;
        }
        if (checkWaypointCollision) {
            if (auto* collision = sim.GetWorldCollision()) {
                const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
                if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit)) {
                    propelling = false;
                    waypoints.clear();
                    pathRecalcCooldown = 0;
                    return;
                }
            }
        }
        unit->SetPosition(nx, ny);
        propelling = true;
        return;
    }

    if (!unit->TurnTowardPoint(targetX, targetY)) {
        propelling = false;
        return;
    }
    if (distToTarget <= 0.001f) {
        propelling = false;
        return;
    }
    const float dx = targetX - unit->GetX();
    const float dy = targetY - unit->GetY();
    const float step = std::min(speed, distToTarget);
    const float nx = unit->GetX() + (dx / distToTarget) * step;
    const float ny = unit->GetY() + (dy / distToTarget) * step;
    if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                 unit->GetMovementType(), unit)) {
        propelling = false;
        return;
    }
    if (checkDirectCollision) {
        if (auto* collision = sim.GetWorldCollision()) {
            const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
            if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit)) {
                propelling = false;
                return;
            }
        }
    }
    unit->SetPosition(nx, ny);
    propelling = true;
}

// Validated direct step toward (targetX, targetY) when no pathing map blocks direct motion.
inline bool StepDirectValidated(CSimulation& sim, CUnit* unit,
                                float targetX, float targetY,
                                float maxStepDist,
                                bool& propelling,
                                bool* pathFoundOut = nullptr) {
    if (!unit) return false;
    auto* pathfinding = sim.GetPathfinding();
    if (pathfinding && pathfinding->HasPathingMap()) {
        propelling = false;
        if (pathFoundOut) *pathFoundOut = false;
        return false;
    }
    if (!unit->TurnTowardPoint(targetX, targetY)) {
        propelling = false;
        return false;
    }
    const float dx = targetX - unit->GetX();
    const float dy = targetY - unit->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= 0.0f) {
        propelling = false;
        return false;
    }
    const float speed = unit->GetSpeed() * sim.GetSimStepTime();
    const float step = std::min(speed, maxStepDist);
    const float nx = unit->GetX() + (dx / dist) * step;
    const float ny = unit->GetY() + (dy / dist) * step;
    const float collisionRadius = std::max(0.0f, unit->GetCollisionSize());
    if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                 unit->GetMovementType(), unit)) {
        propelling = false;
        return false;
    }
    if (auto* collision = sim.GetWorldCollision()) {
        const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
        if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit)) {
            propelling = false;
            return false;
        }
    }
    unit->SetPosition(nx, ny);
    propelling = true;
    return true;
}

// Nearest living tree destructable discovery around (refX, refY) (shared by Harvest & ReturnResources).
inline CDestructable* FindNearestLivingTree(CSimulation& sim, float refX, float refY) {
    const auto* destData = sim.GetDestructableData();
    if (!destData) return nullptr;

    CDestructable* nearest = nullptr;
    float bestDistSq = std::numeric_limits<float>::max();
    for (auto* d : sim.GetDestructables()) {
        if (!d || d->IsDead()) continue;
        const auto* dt = destData->GetDestructable(d->GetTypeId());
        if (!dt || !dt->isTree) continue;
        const float dx = d->GetX() - refX;
        const float dy = d->GetY() - refY;
        const float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            nearest = d;
        }
    }
    return nearest;
}

} // namespace MoveIntoRange

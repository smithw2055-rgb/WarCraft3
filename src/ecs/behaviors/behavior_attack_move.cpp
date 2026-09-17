#include "behavior_attack_move.h"
#include "ecs/csimulation.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/unit.h"
#include "ecs/pathing/world_collision.h"
#include <algorithm>
#include <cmath>

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

bool CBehaviorAttackMove::MoveTowardWaypoint(CSimulation& simulation) {
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
        RecalculatePath(simulation, destX_, destY_);
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
            RecalculatePath(simulation, destX_, destY_);
            return false;
        }
    }

    unit_->SetPosition(nx, ny);
    propelling_ = true;
    return false;
}

bool CBehaviorAttackMove::Update(CSimulation& simulation) {
    if (!unit_) return true;
    if (unit_->IsBuilding() || !unit_->GetMoveBehavior() || unit_->IsRooted()) {
        return true;
    }

    float dx = destX_ - unit_->GetX();
    float dy = destY_ - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < arrivalDistance_) {
        propelling_ = false;
        return true; // arrived
    }

    // Periodically scan for enemies
    int tick = simulation.GetGameTurnTick();
    if (tick - lastAttackScanTick_ > 5) { // scan every 5 ticks
        lastAttackScanTick_ = tick;
        float acqRange = unit_->GetUnitType()->GetDefaultAcquisitionRange();
        if (acqRange <= 0) acqRange = 256.0f;

        std::vector<CUnit*> enemies;
        simulation.GetWorldCollision()->EnumUnitsInRange(
            unit_->GetX(), unit_->GetY(), acqRange, enemies);

        for (auto* e : enemies) {
            if (e == unit_ || e->IsDead()) continue;
            if (e->GetPlayerIndex() == unit_->GetPlayerIndex()) continue; // skip allies
            // Found enemy, switch to attack
            targetHandle_ = e->GetHandleId();
            break;
        }
    }

    if (targetHandle_ >= 0) {
        // Found target - attack it. The main loop will handle behavior transitions.
        // For now, just move towards the enemy.
        CUnit* target = simulation.GetUnit(targetHandle_);
        if (!target || target->IsDead()) {
            targetHandle_ = -1; // target gone, resume move
            waypoints_.clear();
            pathFound_ = false;
        } else {
            const float targetX = target->GetX();
            const float targetY = target->GetY();
            float edx = targetX - unit_->GetX();
            float edy = targetY - unit_->GetY();
            float edist = std::sqrt(edx * edx + edy * edy);
            float attackRange = 128.0f;

            if (edist > attackRange) {
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
                    return false;
                }

                if (!unit_->TurnTowardPoint(targetX, targetY)) {
                    propelling_ = false;
                    return false;
                }
                float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
                float step = std::min(speed, edist);
                float nx = unit_->GetX() + (edx / edist) * step;
                float ny = unit_->GetY() + (edy / edist) * step;
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
            }
            else {
                unit_->TurnTowardPoint(targetX, targetY);
                propelling_ = false;
            }
            return false;
        }
    }

    pathRecalcCooldown_--;
    if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
        RecalculatePath(simulation, destX_, destY_);
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

    if (!unit_->TurnTowardPoint(destX_, destY_)) {
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

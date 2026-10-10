#include "behavior_repair.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_repair.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include <algorithm>
#include <cmath>

CBehaviorRepair::CBehaviorRepair() {}

void CBehaviorRepair::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

void CBehaviorRepair::Begin(CSimulation& simulation) {
    (void)simulation;
    movingToTarget_ = true;
    repairing_ = false;
    propelling_ = false;
    totalGoldCharged_ = 0.0f;
    totalLumberCharged_ = 0.0f;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    RefreshRepairRates();
}

void CBehaviorRepair::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    movingToTarget_ = false;
    repairing_ = false;
}

void CBehaviorRepair::RefreshRepairRates() {
    goldPerUpdate_ = 0.0f;
    lumberPerUpdate_ = 0.0f;
    hpPerUpdate_ = 0.0f;

    auto* targetUnit = dynamic_cast<CUnit*>(target_);
    if (!repairAbility_ || !targetUnit) {
        return;
    }

    const int repairTime = std::max(1, targetUnit->GetUnitType()->GetRepairTime());
    const float timeRatio = repairAbility_->GetRepairRate();
    const float normalizedStep = (repairTime > 0) ? (1.0f / static_cast<float>(repairTime)) : 0.0f;
    goldPerUpdate_ = targetUnit->GetUnitType()->GetGoldRepairCost() * normalizedStep * timeRatio;
    lumberPerUpdate_ = targetUnit->GetUnitType()->GetLumberRepairCost() * normalizedStep * timeRatio;
    hpPerUpdate_ = targetUnit->GetMaxLife() * normalizedStep * timeRatio;
}

bool CBehaviorRepair::RecalculatePath(CSimulation& simulation) {
    waypoints_.clear();
    auto* pathfinding = simulation.GetPathfinding();
    if (!pathfinding || !target_) {
        return false;
    }

    std::vector<std::pair<float, float>> worldPath;
    if (!pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(), target_->GetX(), target_->GetY(),
                                    std::max(0.0f, unit_->GetCollisionSize()),
                                    unit_->GetMovementType(), unit_, worldPath)) {
        return false;
    }

    for (size_t i = 1; i < worldPath.size(); ++i) {
        // Drop already-reached waypoints (ref CBehaviorHarvest: degenerate
        // refills treadmill forever otherwise).
        const float stepReach = std::max(1.0f,
            unit_->GetSpeed() * simulation.GetSimStepTime()) * 2.0f;
        const float rdx = worldPath[i].first - unit_->GetX();
        const float rdy = worldPath[i].second - unit_->GetY();
        if (rdx * rdx + rdy * rdy > stepReach * stepReach)
            waypoints_.push_back(worldPath[i]);
    }
    return !waypoints_.empty();
}

bool CBehaviorRepair::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || !target_) {
        return true;
    }
    auto* pathfinding = simulation.GetPathfinding();

    float targetX = target_->GetX();
    float targetY = target_->GetY();

    if (!waypoints_.empty()) {
        auto& wp = waypoints_.front();
        const float wdx = wp.first - unit_->GetX();
        const float wdy = wp.second - unit_->GetY();
        const float wdist = std::sqrt(wdx * wdx + wdy * wdy);
        const float stepDistance = unit_->GetSpeed() * simulation.GetSimStepTime();
        if (wdist <= std::max(1.0f, stepDistance)) {
            waypoints_.pop_front();
            propelling_ = false;
            return false;
        }
        targetX = wp.first;
        targetY = wp.second;
    }
    // Empty waypoints fall through to the direct approach below (same
    // rationale as CBehaviorHarvest): the step stays validated against the
    // pathing map + dynamic collision.

    if (!unit_->TurnTowardPoint(targetX, targetY)) {
        propelling_ = false;
        return false;
    }

    const float dx = targetX - unit_->GetX();
    const float dy = targetY - unit_->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= 0.001f) {
        propelling_ = false;
        return false;
    }

    const float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
    const float step = std::min(speed, dist);
    const float nx = unit_->GetX() + (dx / dist) * step;
    const float ny = unit_->GetY() + (dy / dist) * step;
    const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
    if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                 unit_->GetMovementType(), unit_)) {
        propelling_ = false;
        waypoints_.clear();
        pathRecalcCooldown_ = 0;
        return false;
    }
    if (auto* collision = simulation.GetWorldCollision()) {
        const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
        if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit_)) {
            propelling_ = false;
            waypoints_.clear();
            pathRecalcCooldown_ = 0;
            return false;
        }
    }
    unit_->SetPosition(nx, ny);
    propelling_ = true;
    return false;
}

bool CBehaviorRepair::ChargeRepairResources(CSimulation& simulation) {
    auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
    if (!player) {
        return false;
    }

    const int goldDelta = static_cast<int>(totalGoldCharged_ + goldPerUpdate_) - static_cast<int>(totalGoldCharged_);
    const int lumberDelta = static_cast<int>(totalLumberCharged_ + lumberPerUpdate_) - static_cast<int>(totalLumberCharged_);

    if (player->GetGold() < goldDelta || player->GetLumber() < lumberDelta) {
        return false;
    }

    totalGoldCharged_ += goldPerUpdate_;
    totalLumberCharged_ += lumberPerUpdate_;
    if (goldDelta != 0) {
        player->AddGold(-goldDelta);
    }
    if (lumberDelta != 0) {
        player->AddLumber(-lumberDelta);
    }
    return true;
}

bool CBehaviorRepair::Update(CSimulation& simulation) {
    if (!unit_ || !target_) {
        return true;
    }

    auto* targetUnit = dynamic_cast<CUnit*>(target_);
    if (!targetUnit || targetUnit->IsDead() || targetUnit->GetLife() >= targetUnit->GetMaxLife()) {
        propelling_ = false;
        movingToTarget_ = false;
        repairing_ = false;
        return true;
    }

    const float repairRange = repairAbility_ ? repairAbility_->GetRepairRange() : 128.0f;
    const float dx = target_->GetX() - unit_->GetX();
    const float dy = target_->GetY() - unit_->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    // Footprint-aware arrival (ref CBehaviorReturnResources): repair from
    // the target's edge instead of orbiting big footprints.
    const float arrival = std::max(repairRange, targetUnit->GetCollisionSize() + 64.0f);

    if (movingToTarget_) {
        if (dist <= arrival) {
            movingToTarget_ = false;
            repairing_ = true;
            propelling_ = false;
            unit_->TurnTowardPoint(target_->GetX(), target_->GetY());
            return false;
        }

        pathRecalcCooldown_--;
        if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
            RecalculatePath(simulation);
            pathRecalcCooldown_ = 8;
        }
        // Empty waypoints flow into MoveTowardWaypoint's direct approach
        // (validated step); no stall here (ref CBehaviorHarvest).
        return MoveTowardWaypoint(simulation);
    }

    propelling_ = false;
    repairing_ = true;
    unit_->TurnTowardPoint(target_->GetX(), target_->GetY());

    if (!ChargeRepairResources(simulation)) {
        return true;
    }

    const float repairedLife = std::min(targetUnit->GetMaxLife(), targetUnit->GetLife() + hpPerUpdate_);
    targetUnit->SetLife(simulation, repairedLife);
    return repairedLife >= targetUnit->GetMaxLife();
}

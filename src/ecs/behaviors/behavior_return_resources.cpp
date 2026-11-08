#include "behavior_return_resources.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/orders/order.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include "data/destructabledata.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

CAbility* ResolveHarvestAbility(CUnit* unit, const CAbility* preferred) {
    if (!unit) {
        return nullptr;
    }
    if (preferred) {
        for (CAbility* ab : unit->GetAbilities()) {
            if (ab == preferred) {
                return ab;
            }
        }
    }
    if (CAbility* ahar = unit->GetAbility(War3ID("Ahar"))) {
        return ahar;
    }
    for (CAbility* ab : unit->GetAbilities()) {
        if (ab && ab->IsCarryingResources()) {
            return ab;
        }
    }
    return nullptr;
}

bool HasActiveResourceMine(const CUnit* unit) {
    if (!unit) {
        return false;
    }
    if (const CAbility* overlay = unit->GetAbility(War3ID("Agl2"))) {
        if (overlay->IsResourceMine()) return true;
    }
    if (const CAbility* base = unit->GetAbility(War3ID("Agld"))) {
        if (base->IsResourceMine()) return true;
    }
    for (const auto& ab : unit->GetAbilities()) {
        if (ab && ab->IsResourceMine()) {
            return true;
        }
    }
    return false;
}

bool CanAcceptReturnedResource(const CUnit* targetUnit, CResourceType resourceType) {
    if (!targetUnit) {
        return false;
    }
    if (const CAbility* returnAbility = targetUnit->GetAbility(War3ID("Aret"))) {
        if (returnAbility->AcceptsResourceDropoff(resourceType)) {
            return true;
        }
    }
    for (const auto& ab : targetUnit->GetAbilities()) {
        if (ab && ab->AcceptsResourceDropoff(resourceType)) {
            return true;
        }
    }
    return false;
}

CUnit* FindNearestMine(CSimulation& simulation, const CUnit* worker) {
    if (!worker) {
        return nullptr;
    }

    CUnit* nearest = nullptr;
    float minDistSq = std::numeric_limits<float>::max();
    for (auto* otherUnit : simulation.GetUnits()) {
        if (!otherUnit || otherUnit->IsDead() || otherUnit == worker) {
            continue;
        }
        if (!HasActiveResourceMine(otherUnit)) {
            continue;
        }
        const float dx = otherUnit->GetX() - worker->GetX();
        const float dy = otherUnit->GetY() - worker->GetY();
        const float distSq = dx * dx + dy * dy;
        if (distSq < minDistSq) {
            minDistSq = distSq;
            nearest = otherUnit;
        }
    }
    return nearest;
}

CDestructable* FindNearestTree(CSimulation& simulation, const CUnit* worker, const CWidget* origin) {
    if (!worker) {
        return nullptr;
    }

    const float refX = origin ? origin->GetX() : worker->GetX();
    const float refY = origin ? origin->GetY() : worker->GetY();
    const auto* destructableData = simulation.GetDestructableData();

    CDestructable* nearest = nullptr;
    float minDistSq = std::numeric_limits<float>::max();
    for (auto* destructable : simulation.GetDestructables()) {
        if (!destructable || destructable->IsDead()) {
            continue;
        }
        if (!destructableData) {
            return nullptr;
        }
        const auto* dt = destructableData->GetDestructable(destructable->GetTypeId());
        if (!dt || !dt->isTree) {
            continue;
        }
        const float dx = destructable->GetX() - refX;
        const float dy = destructable->GetY() - refY;
        const float distSq = dx * dx + dy * dy;
        if (distSq < minDistSq) {
            minDistSq = distSq;
            nearest = destructable;
        }
    }
    return nearest;
}

} // namespace

CBehaviorReturnResources::CBehaviorReturnResources() {}

void CBehaviorReturnResources::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

void CBehaviorReturnResources::FindNearestDropoff(CSimulation& simulation) {
    if (!unit_) return;

    CAbility* harvestAbility = ResolveHarvestAbility(unit_, harvestAbility_);
    const CResourceType resourceType = harvestAbility ? harvestAbility->GetCarriedResourceType() : CResourceType::NONE;
    if (resourceType == CResourceType::NONE) {
        target_ = nullptr;
        return;
    }

    float minDist = std::numeric_limits<float>::max();
    CUnit* nearest = nullptr;

    for (auto* otherUnit : simulation.GetUnits()) {
        if (!otherUnit || otherUnit->IsDead() || otherUnit == unit_) continue;
        if (otherUnit->GetPlayerIndex() != unit_->GetPlayerIndex()) continue;
        if (otherUnit->IsConstructing()) continue;
        if (!CanAcceptReturnedResource(otherUnit, resourceType)) continue;

        float dx = otherUnit->GetX() - unit_->GetX();
        float dy = otherUnit->GetY() - unit_->GetY();
        float dist = dx * dx + dy * dy;
        if (dist < minDist) {
            minDist = dist;
            nearest = otherUnit;
        }
    }

    target_ = nearest;
}

void CBehaviorReturnResources::Begin(CSimulation& simulation) {
    movingToDropoff_ = true;
    depositing_ = false;
    depositTimer_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    propelling_ = false;

    if (!target_ || target_->IsDead()) {
        FindNearestDropoff(simulation);
    }
}

void CBehaviorReturnResources::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
}

void CBehaviorReturnResources::DepositCarriedResources(CSimulation& simulation) {
    if (!unit_) return;
    auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
    CAbility* harvestAbility = ResolveHarvestAbility(unit_, harvestAbility_);
    if (!player || !harvestAbility) return;

    const int amount = harvestAbility->GetCarriedAmount();
    const CResourceType type = harvestAbility->GetCarriedResourceType();

    if (type == CResourceType::GOLD) {
        player->AddGold(player->ApplyUpkeepGoldTax(amount, &simulation));
    } else if (type == CResourceType::LUMBER) {
        player->AddLumber(amount);
    }

    CWidget* nextTarget = simulation.GetWidget(harvestAbility->GetLastTargetHandle());
    if ((!nextTarget || nextTarget->IsDead()) && type == CResourceType::GOLD) {
        nextTarget = FindNearestMine(simulation, unit_);
    } else if ((!nextTarget || nextTarget->IsDead()) && type == CResourceType::LUMBER) {
        CWidget* origin = simulation.GetWidget(harvestAbility->GetLastTargetHandle());
        if (!origin) {
            origin = unit_;
        }
        nextTarget = FindNearestTree(simulation, unit_, origin);
    }

    harvestAbility->ClearCarried();
    if (nextTarget && !nextTarget->IsDead()) {
        harvestAbility->SetLastTargetHandle(nextTarget->GetHandleId());
        unit_->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, nextTarget));
    }
}

bool CBehaviorReturnResources::Update(CSimulation& simulation) {
    if (!unit_) return true;

    if (!target_ || target_->IsDead()) {
        FindNearestDropoff(simulation);
        if (!target_) {
            propelling_ = false;
            return true;
        }
    }

    if (movingToDropoff_) {
        float dx = target_->GetX() - unit_->GetX();
        float dy = target_->GetY() - unit_->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);
        float arrival = 128.0f;
        if (auto* targetUnit = dynamic_cast<CUnit*>(target_)) {
            arrival = std::max(220.0f, targetUnit->GetCollisionSize() + 96.0f);
        }
        auto* pathfinding = simulation.GetPathfinding();

        if (dist < arrival) {
            movingToDropoff_ = false;
            depositing_ = false;
            propelling_ = false;
            unit_->TurnTowardPoint(target_->GetX(), target_->GetY());
            DepositCarriedResources(simulation);
            return true;
        }

        pathRecalcCooldown_--;
        if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
            waypoints_.clear();
            if (pathfinding) {
                std::vector<std::pair<float,float>> worldPath;
                if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                target_->GetX(), target_->GetY(),
                                                std::max(0.0f, unit_->GetCollisionSize()),
                                                unit_->GetMovementType(), unit_, worldPath)) {
                    const float stepReach = std::max(1.0f,
                        unit_->GetSpeed() * simulation.GetSimStepTime()) * 2.0f;
                    for (size_t i = 1; i < worldPath.size(); i++) {
                        const float rdx = worldPath[i].first - unit_->GetX();
                        const float rdy = worldPath[i].second - unit_->GetY();
                        if (rdx * rdx + rdy * rdy > stepReach * stepReach)
                            waypoints_.push_back(worldPath[i]);
                    }
                }
            }
            pathRecalcCooldown_ = 8;
        }

        if (!waypoints_.empty()) {
            auto& wp = waypoints_.front();
            float wdx = wp.first - unit_->GetX();
            float wdy = wp.second - unit_->GetY();
            float wdist = std::sqrt(wdx * wdx + wdy * wdy);
            const float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
            if (wdist <= std::max(1.0f, speed)) {
                waypoints_.pop_front();
                propelling_ = true;
            } else {
                if (!unit_->TurnTowardPoint(wp.first, wp.second)) {
                    propelling_ = false;
                    return false;
                }
                float step = std::min(speed, wdist);
                const float nx = unit_->GetX() + (wdx / wdist) * step;
                const float ny = unit_->GetY() + (wdy / wdist) * step;
                const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
                if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                             unit_->GetMovementType(), unit_)) {
                    propelling_ = false;
                    waypoints_.clear();
                    pathRecalcCooldown_ = 0;
                    return false;
                }
                unit_->SetPosition(nx, ny);
                propelling_ = true;
            }
        } else {
            if (!unit_->TurnTowardPoint(target_->GetX(), target_->GetY())) {
                propelling_ = false;
                return false;
            }
            float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
            float step = std::min(speed, dist);
            const float nx = unit_->GetX() + (dx / dist) * step;
            const float ny = unit_->GetY() + (dy / dist) * step;
            const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
            if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                         unit_->GetMovementType(), unit_)) {
                propelling_ = false;
                return false;
            }
            unit_->SetPosition(nx, ny);
            propelling_ = true;
        }
        return false;
    }

    if (depositing_) {
        propelling_ = false;
        float dt = simulation.GetSimStepTime();
        depositTimer_ += dt;

        if (depositTimer_ >= depositDuration_) {
            DepositCarriedResources(simulation);
            return true;
        }
        return false;
    }

    propelling_ = false;
    return true;
}

#include "behavior_return_resources.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_return_resources.h"
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

bool CanAcceptReturnedResource(const CUnit* targetUnit, CResourceType resourceType) {
    if (!targetUnit) {
        return false;
    }
    auto* returnAbility = dynamic_cast<CAbilityReturnResources*>(targetUnit->GetAbility(War3ID("Aret")));
    if (!returnAbility) {
        return false;
    }
    switch (resourceType) {
        case CResourceType::GOLD:
            return returnAbility->AcceptsGold();
        case CResourceType::LUMBER:
            return returnAbility->AcceptsLumber();
        default:
            return false;
    }
}

CUnit* FindNearestMine(CSimulation& simulation, const CUnit* worker) {
    if (!worker) {
        return nullptr;
    }
    double bestDistSq = std::numeric_limits<double>::max();
    CUnit* bestTarget = nullptr;
    for (auto* otherUnit : simulation.GetUnits()) {
        if (!otherUnit || otherUnit->IsDead()) {
            continue;
        }
        if (!dynamic_cast<CAbilityGoldMine*>(otherUnit->GetAbility(War3ID("Agld")))) {
            continue;
        }
        const double distSq = otherUnit->DistanceSquared(worker->GetX(), worker->GetY());
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestTarget = otherUnit;
        }
    }
    return bestTarget;
}

CDestructable* FindNearestTree(CSimulation& simulation, const CUnit* worker, CWidget* origin) {
    if (!worker || !origin) {
        return nullptr;
    }
    auto* destructableData = simulation.GetDestructableData();
    if (!destructableData) {
        return nullptr;
    }

    double bestDistSq = std::numeric_limits<double>::max();
    CDestructable* bestTarget = nullptr;
    for (auto* destructable : simulation.GetDestructables()) {
        if (!destructable || destructable->IsDead()) {
            continue;
        }
        const auto* destructableType = destructableData->GetDestructable(destructable->GetTypeId());
        if (!destructableType || !destructableType->isTree) {
            continue;
        }
        const double dx = destructable->GetX() - origin->GetX();
        const double dy = destructable->GetY() - origin->GetY();
        const double distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestTarget = destructable;
        }
    }
    return bestTarget;
}

}

CBehaviorReturnResources::CBehaviorReturnResources() {}

void CBehaviorReturnResources::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

void CBehaviorReturnResources::FindNearestDropoff(CSimulation& simulation) {
    if (!unit_) return;

    auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit_->GetAbility(War3ID("Ahar")));
    const CResourceType resourceType = harvestAbility ? harvestAbility->GetCarriedResourceType() : CResourceType::NONE;
    if (resourceType == CResourceType::NONE) {
        target_ = nullptr;
        return;
    }

    float bestDist = 1e9f;
    CWidget* bestTarget = nullptr;

    for (auto* otherUnit : simulation.GetUnits()) {
        if (otherUnit->IsDead()) continue;
        if (otherUnit->GetPlayerIndex() != unit_->GetPlayerIndex()) continue;

        if (CanAcceptReturnedResource(otherUnit, resourceType)) {
            float dx = otherUnit->GetX() - unit_->GetX();
            float dy = otherUnit->GetY() - unit_->GetY();
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                bestTarget = otherUnit;
            }
        }
    }

    target_ = bestTarget;
}

void CBehaviorReturnResources::Begin(CSimulation& simulation) {
    (void)simulation;
    movingToDropoff_ = true;
    depositing_ = false;
    depositTimer_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    propelling_ = false;

    if (!target_) {
        FindNearestDropoff(simulation);
    }
}

void CBehaviorReturnResources::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
}

bool CBehaviorReturnResources::Update(CSimulation& simulation) {
    if (!unit_) return true;

    if (!target_ || target_->IsDead()) {
        // Try to find another dropoff
        FindNearestDropoff(simulation);
        if (!target_) {
            propelling_ = false;
            return true; // no dropoff found
        }
    }

    if (movingToDropoff_) {
        float dx = target_->GetX() - unit_->GetX();
        float dy = target_->GetY() - unit_->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);
        // Footprint-aware arrival: big town halls (collision ~176) can
        // never be approached within a fixed 128 units, so dock at the
        // footprint edge instead of orbiting it forever.
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

            // Deposit resources immediately matching warsmash CBehaviorReturnResources
            auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
            auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit_->GetAbility(War3ID("Ahar")));
            if (player && harvestAbility) {
                int amount = harvestAbility->GetCarriedAmount();
                CResourceType type = harvestAbility->GetCarriedResourceType();

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
            return true; // behavior done
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
                    // Drop already-reached waypoints (ref CBehaviorHarvest:
                    // degenerate refills treadmill forever otherwise).
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
            // Direct approach when waypoints are exhausted (same rationale
            // as CBehaviorHarvest): the step stays validated against the
            // pathing map.
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
            // Deposit resources
            auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
            auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit_->GetAbility(War3ID("Ahar")));
            if (player && harvestAbility) {
                int amount = harvestAbility->GetCarriedAmount();
                CResourceType type = harvestAbility->GetCarriedResourceType();

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

            return true; // behavior done
        }
        return false;
    }

    propelling_ = false;
    return true;
}

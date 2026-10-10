#include "behavior_harvest.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/orders/order.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include "data/destructabledata.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

CBehaviorHarvest::CBehaviorHarvest() {}

void CBehaviorHarvest::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
    if (target_ && unit_) {
        if (auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit_->GetAbility(War3ID("Ahar")))) {
            harvestAbility->SetLastTargetHandle(target_->GetHandleId());
        }
    }
}

void CBehaviorHarvest::Begin(CSimulation& simulation) {
    (void)simulation;
    movingToResource_ = true;
    gathering_ = false;
    returning_ = false;
    inMine_ = false;
    gatherTimer_ = 0;
    gatheredAmount_ = 0;
    popoutFromMineTurnTick_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    propelling_ = false;
}

void CBehaviorHarvest::End(CSimulation& simulation) {
    if (resourceType_ == CResourceType::GOLD && inMine_) {
        if (auto* targetUnit = dynamic_cast<CUnit*>(target_)) {
            MineAccessOf(targetUnit).Remove(this);
        }
        if (unit_) {
            unit_->SetHidden(false);
            unit_->SetInvulnerable(false);
        }
    }
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    inMine_ = false;
    popoutFromMineTurnTick_ = 0;
}

bool CBehaviorHarvest::Update(CSimulation& simulation) {
    if (!unit_) return true;

    const bool shouldReturnWithPayload = gatheredAmount_ > 0
        && (resourceType_ == CResourceType::GOLD || gatheredAmount_ >= capacity_);
    if (shouldReturnWithPayload) {
        propelling_ = false;
        auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit_->GetAbility(War3ID("Ahar")));
        if (harvestAbility) {
            harvestAbility->SetCarried(resourceType_, gatheredAmount_);
        }
        unit_->IssueOrder(std::make_unique<COrderNoTarget>(OrderIds::RETURN_RESOURCES));
        return true;
    }

    if (inMine_) {
        propelling_ = false;
        return false;
    }

    if (movingToResource_) {
        if (!target_ || target_->IsDead()) {
            if (resourceType_ == CResourceType::LUMBER) {
                FindNearestTree(simulation);
            }
            if (!target_) {
                propelling_ = false;
                return true;
            }
        }

        float dx = target_->GetX() - unit_->GetX();
        float dy = target_->GetY() - unit_->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);

        // Footprint-aware arrival (ref CBehaviorReturnResources): start
        // gathering at the target's edge instead of orbiting big footprints.
        float arrival = 128.0f;
        if (auto* targetUnit = dynamic_cast<CUnit*>(target_)) {
            arrival = std::max(220.0f, targetUnit->GetCollisionSize() + 96.0f);
            if (resourceType_ == CResourceType::GOLD || targetUnit->GetAbility(War3ID("Agld")) || targetUnit->GetTypeId() == War3ID("ngol")) {
                arrival = 256.0f;
            }
        }
        if (dist < arrival) {
            movingToResource_ = false;
            propelling_ = false;
            unit_->TurnTowardPoint(target_->GetX(), target_->GetY());

            // For gold mines, enter immediately if capacity allows (ref CBehaviorHarvest.java accept(CUnit))
            if (resourceType_ == CResourceType::GOLD) {
                auto* targetUnit = dynamic_cast<CUnit*>(target_);
                if (targetUnit) {
                    CMineAccess goldMine = MineAccessOf(targetUnit);
                    if (goldMine.ok()) {
                        if (goldMine.Gold() <= 0) {
                            return true;
                        }
                        if (goldMine.CanAdd()) {
                            goldMine.Add(this);
                            unit_->SetHidden(true);
                            unit_->SetInvulnerable(true);
                            inMine_ = true;
                            gathering_ = false;
                            popoutFromMineTurnTick_ = simulation.GetGameTurnTick() + std::max(1, static_cast<int>(
                                std::ceil(goldMine.Duration() / simulation.GetSimStepTime())));
                            return false;
                        }
                    }
                }
            }

            gathering_ = true;
            gatherTimer_ = 0;
            return false;
        }

        pathRecalcCooldown_--;
        auto* pathfinding = simulation.GetPathfinding();
        if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
            waypoints_.clear();
            if (pathfinding) {
                std::vector<std::pair<float,float>> worldPath;
                if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                target_->GetX(), target_->GetY(),
                                                std::max(0.0f, unit_->GetCollisionSize()),
                                                unit_->GetMovementType(), unit_, worldPath)) {
                    // Drop waypoints already reached: degenerate same-cell
                    // paths would otherwise refill every tick and treadmill
                    // in the pop branch forever (no turning, no movement).
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
                propelling_ = false;
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
                if (resourceType_ != CResourceType::GOLD) {
                    if (auto* collision = simulation.GetWorldCollision()) {
                        const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
                        if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit_)) {
                            propelling_ = false;
                            waypoints_.clear();
                            pathRecalcCooldown_ = 0;
                            return false;
                        }
                    }
                }
                unit_->SetPosition(nx, ny);
                propelling_ = true;
            }
        } else {
            // Direct approach when waypoints are exhausted: A* often
            // resolves start/end into the same cell (empty waypoint list)
            // while still hundreds of units out. The step below is still
            // validated against the pathing map + dynamic collision, so a
            // genuinely blocked unit stalls one tick and recalcs later.
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

    if (gathering_) {
        propelling_ = false;
        float dt = simulation.GetSimStepTime();
        gatherTimer_ += dt;

        if (gatherTimer_ >= gatherDuration_) {
            gatherTimer_ = 0;

            if (resourceType_ == CResourceType::GOLD) {
                auto* targetUnit = dynamic_cast<CUnit*>(target_);
                if (targetUnit) {
                    CMineAccess goldMine = MineAccessOf(targetUnit);
                    if (goldMine.ok()) {
                        if (goldMine.Gold() <= 0) {
                            std::cout << "Gold mine depleted!" << std::endl;
                            return true;
                        }

                        if (goldMine.CanAdd()) {
                            goldMine.Add(this);
                            unit_->SetHidden(true);
                            unit_->SetInvulnerable(true);
                            inMine_ = true;
                            gathering_ = false;
                            popoutFromMineTurnTick_ = simulation.GetGameTurnTick() + std::max(1, static_cast<int>(
                                std::ceil(goldMine.Duration() / simulation.GetSimStepTime())));
                            std::cout << "Worker entered mine, waiting until tick " << popoutFromMineTurnTick_ << std::endl;
                        } else {
                            std::cout << "Mine full, waiting..." << std::endl;
                        }
                    }
                }
            } else if (resourceType_ == CResourceType::LUMBER) {
                // Lumber chopping
                int chopAmount = 2;
                gatheredAmount_ = std::min(gatheredAmount_ + chopAmount, capacity_);
                if (auto* tree = dynamic_cast<CDestructable*>(target_)) {
                    tree->Damage(simulation, unit_, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", (float)chopAmount);
                    if (tree->IsDead() && gatheredAmount_ < capacity_) {
                        FindNearestTree(simulation);
                        if (target_) {
                            movingToResource_ = true;
                            gathering_ = false;
                            waypoints_.clear();
                            return false;
                        }
                    }
                }
                std::cout << "Chopped lumber: " << gatheredAmount_ << "/" << capacity_ << std::endl;
            }

            // Face resource
            if (target_) {
                unit_->TurnTowardPoint(target_->GetX(), target_->GetY());
            }
        }
        return false;
    }

    propelling_ = false;
    return true;
}

void CBehaviorHarvest::FindNearestTree(CSimulation& simulation) {
    if (!unit_) {
        target_ = nullptr;
        return;
    }

    auto* destructableData = simulation.GetDestructableData();
    if (!destructableData) {
        target_ = nullptr;
        return;
    }

    double bestDistSq = std::numeric_limits<double>::max();
    CWidget* bestTarget = nullptr;
    for (auto* destructable : simulation.GetDestructables()) {
        if (!destructable || destructable->IsDead()) {
            continue;
        }
        const auto* destructableType = destructableData->GetDestructable(destructable->GetTypeId());
        if (!destructableType || !destructableType->isTree) {
            continue;
        }

        const double distSq = destructable->DistanceSquared(unit_->GetX(), unit_->GetY());
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestTarget = destructable;
        }
    }

    target_ = bestTarget;
    waypoints_.clear();
    if (target_) {
        if (auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit_->GetAbility(War3ID("Ahar")))) {
            harvestAbility->SetLastTargetHandle(target_->GetHandleId());
        }
    }
}

void CBehaviorHarvest::PopoutFromMine(int goldMined) {
    if (!unit_) {
        return;
    }
    inMine_ = false;
    popoutFromMineTurnTick_ = 0;
    unit_->SetHidden(false);
    unit_->SetInvulnerable(false);
    gatheredAmount_ = goldMined;
}

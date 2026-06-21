#include "behavior_harvest.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/pathing/pathfinding.h"
#include <cmath>
#include <iostream>

CBehaviorHarvest::CBehaviorHarvest() {}

void CBehaviorHarvest::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

void CBehaviorHarvest::Begin(CSimulation& simulation) {
    (void)simulation;
    movingToResource_ = true;
    gathering_ = false;
    returning_ = false;
    gatherTimer_ = 0;
    gatheredAmount_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
}

void CBehaviorHarvest::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
}

bool CBehaviorHarvest::Update(CSimulation& simulation) {
    if (!unit_) return true;

    // If carrying resources, we're done - let the ability handle return
    if (gatheredAmount_ >= capacity_) {
        auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit_->GetAbility(War3ID("Ahar")));
        if (harvestAbility) {
            harvestAbility->SetCarried(resourceType_, gatheredAmount_);
        }
        return true; // behavior done, ability will issue return order
    }

    if (movingToResource_) {
        if (!target_ || target_->IsDead()) {
            if (resourceType_ == CResourceType::LUMBER) {
                FindNearestTree(simulation);
            }
            if (!target_) return true;
        }

        float dx = target_->GetX() - unit_->GetX();
        float dy = target_->GetY() - unit_->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 128.0f) {
            movingToResource_ = false;
            gathering_ = true;
            gatherTimer_ = 0;
            return false;
        }

        pathRecalcCooldown_--;
        if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
            waypoints_.clear();
            auto* pathfinding = simulation.GetPathfinding();
            if (pathfinding) {
                std::vector<std::pair<float,float>> worldPath;
                if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                target_->GetX(), target_->GetY(), worldPath)) {
                    for (size_t i = 1; i < worldPath.size(); i++)
                        waypoints_.push_back(worldPath[i]);
                }
            }
            pathRecalcCooldown_ = 8;
        }

        if (!waypoints_.empty()) {
            auto& wp = waypoints_.front();
            float wdx = wp.first - unit_->GetX();
            float wdy = wp.second - unit_->GetY();
            float wdist = std::sqrt(wdx * wdx + wdy * wdy);
            if (wdist < 48.0f) {
                waypoints_.pop_front();
            } else {
                float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
                float step = std::min(speed, wdist);
                unit_->SetPosition(unit_->GetX() + (wdx / wdist) * step,
                                   unit_->GetY() + (wdy / wdist) * step);
                float facing = std::atan2(wdx, wdy) * 180.0f / 3.14159f;
                unit_->SetFacing(facing);
            }
        } else {
            float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
            float step = std::min(speed, dist);
            unit_->SetPosition(unit_->GetX() + (dx / dist) * step,
                               unit_->GetY() + (dy / dist) * step);
            float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
            unit_->SetFacing(facing);
        }
        return false;
    }

    if (gathering_) {
        float dt = simulation.GetSimStepTime();
        gatherTimer_ += dt;

        if (gatherTimer_ >= gatherDuration_) {
            gatherTimer_ = 0;

            if (resourceType_ == CResourceType::GOLD) {
                // Gold mining: check mine capacity and gold
                auto* targetUnit = dynamic_cast<CUnit*>(target_);
                if (targetUnit) {
                    auto* goldMine = dynamic_cast<CAbilityGoldMine*>(targetUnit->GetAbility(War3ID("Agld")));
                    if (goldMine) {
                        if (goldMine->GetGold() <= 0) {
                            // Mine depleted
                            std::cout << "Gold mine depleted!" << std::endl;
                            return true;
                        }

                        if (goldMine->CanAddMiner()) {
                            // Enter the mine
                            goldMine->AddMiner();
                            unit_->SetHidden(true);
                            unit_->SetInvulnerable(true);

                            // Wait for mining duration
                            float miningTime = goldMine->GetMiningDuration();
                            int goldMined = std::min(goldMine->GetGoldPerTrip(), goldMine->GetGold());
                            goldMine->AddGold(-goldMined);
                            gatheredAmount_ = goldMined;

                            std::cout << "Worker entered mine, mining " << goldMined << " gold" << std::endl;

                            // Exit mine after duration
                            // For simplicity, we'll exit immediately and carry the gold
                            goldMine->RemoveMiner();
                            unit_->SetHidden(false);
                            unit_->SetInvulnerable(false);
                        } else {
                            // Mine is full, wait
                            std::cout << "Mine full, waiting..." << std::endl;
                        }
                    }
                }
            } else if (resourceType_ == CResourceType::LUMBER) {
                // Lumber chopping
                int chopAmount = 2;
                gatheredAmount_ = std::min(gatheredAmount_ + chopAmount, capacity_);
                std::cout << "Chopped lumber: " << gatheredAmount_ << "/" << capacity_ << std::endl;
            }

            // Face resource
            if (target_) {
                float dx = target_->GetX() - unit_->GetX();
                float dy = target_->GetY() - unit_->GetY();
                float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
                unit_->SetFacing(facing);
            }
        }
        return false;
    }

    return true;
}

void CBehaviorHarvest::FindNearestTree(CSimulation& simulation) {
    (void)simulation;
    // TODO: implement tree finding from destructable system
}

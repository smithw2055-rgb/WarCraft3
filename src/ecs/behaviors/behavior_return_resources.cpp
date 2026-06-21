#include "behavior_return_resources.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/pathing/pathfinding.h"
#include <cmath>
#include <iostream>

CBehaviorReturnResources::CBehaviorReturnResources() {}

void CBehaviorReturnResources::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

void CBehaviorReturnResources::FindNearestDropoff(CSimulation& simulation) {
    if (!unit_) return;

    float bestDist = 1e9f;
    CWidget* bestTarget = nullptr;

    for (auto* otherUnit : simulation.GetUnits()) {
        if (otherUnit->IsDead()) continue;
        if (otherUnit->GetPlayerIndex() != unit_->GetPlayerIndex()) continue;

        // Check if unit has return resources ability (town hall)
        auto* returnAbility = otherUnit->GetAbility(War3ID("Aret"));
        if (returnAbility) {
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

    if (!target_) {
        FindNearestDropoff(simulation);
    }
}

void CBehaviorReturnResources::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
}

bool CBehaviorReturnResources::Update(CSimulation& simulation) {
    if (!unit_) return true;

    if (!target_ || target_->IsDead()) {
        // Try to find another dropoff
        FindNearestDropoff(simulation);
        if (!target_) return true; // no dropoff found
    }

    if (movingToDropoff_) {
        float dx = target_->GetX() - unit_->GetX();
        float dy = target_->GetY() - unit_->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 128.0f) {
            movingToDropoff_ = false;
            depositing_ = true;
            depositTimer_ = 0;
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

    if (depositing_) {
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
                    player->AddGold(amount);
                } else if (type == CResourceType::LUMBER) {
                    player->AddLumber(amount);
                }

                harvestAbility->ClearCarried();

                // Auto-loop: go harvest again
                // TODO: find last harvest target or nearest resource
                // For now, just complete
            }

            return true; // behavior done
        }
        return false;
    }

    return true;
}

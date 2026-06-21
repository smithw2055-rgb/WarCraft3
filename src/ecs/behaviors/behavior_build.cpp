#include "behavior_build.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_build.h"
#include "ecs/pathing/pathfinding.h"
#include "data/unitdata.h"
#include <cmath>
#include <iostream>

CBehaviorBuild::CBehaviorBuild() {}

void CBehaviorBuild::SetBuildTarget(float x, float y, War3ID buildingTypeId, CBuildRace race) {
    buildX_ = x;
    buildY_ = y;
    buildingTypeId_ = buildingTypeId;
    race_ = race;

    switch (race) {
        case CBuildRace::HUMAN:     buildModel_ = CBuildModel::HUMAN; break;
        case CBuildRace::ORC:       buildModel_ = CBuildModel::ORC; break;
        case CBuildRace::UNDEAD:    buildModel_ = CBuildModel::UNDEAD; break;
        case CBuildRace::NIGHT_ELF: buildModel_ = CBuildModel::NIGHT_ELF; break;
        default: buildModel_ = CBuildModel::HUMAN; break;
    }
}

void CBehaviorBuild::Begin(CSimulation& simulation) {
    movingToSite_ = true;
    building_ = false;
    completed_ = false;
    constructionProgress_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;

    auto* unitData = simulation.GetUnitData();
    if (unitData) {
        auto* buildType = unitData->GetUnitType(buildingTypeId_);
        if (buildType) {
            buildTime_ = (float)buildType->GetBuildTime();
            if (buildTime_ <= 0) buildTime_ = 60.0f;
        }
    }
}

void CBehaviorBuild::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
}

bool CBehaviorBuild::Update(CSimulation& simulation) {
    if (!unit_ || completed_) return true;

    if (movingToSite_) {
        float dx = buildX_ - unit_->GetX();
        float dy = buildY_ - unit_->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 128.0f) {
            movingToSite_ = false;
            building_ = true;
            CreateBuilding(simulation);
            return false;
        }

        pathRecalcCooldown_--;
        if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
            waypoints_.clear();
            auto* pathfinding = simulation.GetPathfinding();
            if (pathfinding) {
                std::vector<std::pair<float,float>> worldPath;
                if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                buildX_, buildY_, worldPath)) {
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

    if (building_) {
        float dt = simulation.GetSimStepTime();

        // Race-specific construction progress
        switch (buildModel_) {
            case CBuildModel::HUMAN:
                // Human: worker stays outside and "repairs" the building
                // Construction progresses as long as worker is alive and near
                constructionProgress_ += dt;
                break;

            case CBuildModel::ORC:
            case CBuildModel::NIGHT_ELF:
                // Orc/Night Elf: worker is hidden inside, construction auto-progresses
                constructionProgress_ += dt;
                break;

            case CBuildModel::UNDEAD:
                // Undead: worker stays outside, plays work animation
                // Construction auto-progresses
                constructionProgress_ += dt;
                break;
        }

        // Update building health proportionally
        auto* building = simulation.GetUnit(createdUnitHandle_);
        if (building) {
            float progress = constructionProgress_ / buildTime_;
            float maxLife = building->GetMaxLife();
            float targetLife = maxLife * (0.1f + progress * 0.9f);
            building->SetLife(simulation, targetLife);
        }

        if (constructionProgress_ >= buildTime_) {
            CompleteBuilding(simulation);
            return true;
        }

        return false;
    }

    return true;
}

void CBehaviorBuild::CreateBuilding(CSimulation& simulation) {
    // Deduct resources from player
    auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
    auto* unitData = simulation.GetUnitData();
    if (player && unitData) {
        auto* buildType = unitData->GetUnitType(buildingTypeId_);
        if (buildType) {
            player->AddGold(-buildType->GetGoldCost());
            player->AddLumber(-buildType->GetLumberCost());
        }
    }

    // Create the building unit
    auto* building = simulation.CreateUnit(buildingTypeId_, unit_->GetPlayerIndex(),
                                            buildX_, buildY_, 0);
    if (building) {
        createdUnitHandle_ = building->GetHandleId();
        building->SetLife(simulation, building->GetMaxLife() * 0.1f);
        building->SetConstructing(true);
        building->SetConstructionPaused(false);

        // Race-specific worker handling
        switch (buildModel_) {
            case CBuildModel::HUMAN:
                // Human: worker stays outside, building is NOT paused
                // Worker will "repair" the building (construction progresses)
                building->SetConstructionPaused(false);
                break;

            case CBuildModel::ORC:
                // Orc: worker hides inside, building auto-constructs
                unit_->SetHidden(true);
                building->SetConstructionPaused(false);
                break;

            case CBuildModel::UNDEAD:
                // Undead: worker stays outside at build site
                // Worker plays work animation, building auto-constructs
                building->SetConstructionPaused(false);
                break;

            case CBuildModel::NIGHT_ELF:
                // Night Elf: worker hides inside (like Orc)
                // For Ancients, worker food is consumed
                unit_->SetHidden(true);
                building->SetConstructionPaused(false);
                break;
        }

        std::cout << "Building started: " << buildingTypeId_.ToString()
                  << " at (" << buildX_ << ", " << buildY_ << ")" << std::endl;
    }
}

void CBehaviorBuild::CompleteBuilding(CSimulation& simulation) {
    completed_ = true;

    auto* building = simulation.GetUnit(createdUnitHandle_);
    if (building) {
        building->SetLife(simulation, building->GetMaxLife());
        building->SetConstructing(false);
        building->SetConstructionProgress(0);

        // Add food cap from building
        auto* player = simulation.GetPlayer(building->GetPlayerIndex());
        if (player && building->GetFoodMade() > 0) {
            player->SetFoodCap(player->GetFoodCap() + building->GetFoodMade());
        }

        std::cout << "Building completed: " << buildingTypeId_.ToString() << std::endl;
    }

    // Race-specific worker handling on completion
    switch (buildModel_) {
        case CBuildModel::HUMAN:
            // Human: worker is already outside, just stop repairing
            break;

        case CBuildModel::ORC:
        case CBuildModel::NIGHT_ELF:
            // Orc/Night Elf: unhide worker and reposition outside
            unit_->SetHidden(false);
            if (building) {
                float offset = building->GetCollisionSize() + unit_->GetCollisionSize() + 32.0f;
                unit_->SetPosition(building->GetX() + offset, building->GetY());
            }
            break;

        case CBuildModel::UNDEAD:
            // Undead: worker is already outside, just stop working
            break;
    }
}

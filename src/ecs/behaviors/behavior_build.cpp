#include "behavior_build.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_build.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
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
        // Ref CAbilityNagaBuild reuses the orc build behavior.
        case CBuildRace::NAGA:      buildModel_ = CBuildModel::ORC; break;
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
    propelling_ = false;

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
    propelling_ = false;
}

bool CBehaviorBuild::Update(CSimulation& simulation) {
    if (!unit_ || completed_) return true;

    if (movingToSite_) {
        float dx = buildX_ - unit_->GetX();
        float dy = buildY_ - unit_->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);
        auto* pathfinding = simulation.GetPathfinding();

        if (dist < 128.0f) {
            movingToSite_ = false;
            building_ = true;
            propelling_ = false;
            unit_->TurnTowardPoint(buildX_, buildY_);
            CreateBuilding(simulation);
            return false;
        }

        pathRecalcCooldown_--;
        if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
            waypoints_.clear();
            if (pathfinding) {
                std::vector<std::pair<float,float>> worldPath;
                if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                buildX_, buildY_,
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
            }
        } else {
            // Direct approach when waypoints are exhausted (same rationale
            // as CBehaviorHarvest): the step stays validated against the
            // pathing map + dynamic collision.
            if (!unit_->TurnTowardPoint(buildX_, buildY_)) {
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

    if (building_) {
        propelling_ = false;
        unit_->TurnTowardPoint(buildX_, buildY_);
        float dt = simulation.GetSimStepTime();

        // Race-specific construction progress
        switch (buildModel_) {
            case CBuildModel::HUMAN:
                // Human: worker stays outside and works the building.
                // Construction stalls while the worker is dead or away
                // from the site (ref CBehaviorHumanBuild pause).
                if (unit_->IsDead()) {
                    return true;
                } {
                    const float dx = buildX_ - unit_->GetX();
                    const float dy = buildY_ - unit_->GetY();
                    if (dx * dx + dy * dy > 400.0f * 400.0f) {
                        return false;
                    }
                }
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
            building->SetConstructionProgress(constructionProgress_);
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

    propelling_ = false;
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
                // Night Elf: wisp hides inside during construction and is
                // consumed on completion (see CompleteBuilding).
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
        if (const auto& cb = simulation.GetConstructEventCallback()) {
            cb(building->GetHandleId(), true);
        }
    }

    // Race-specific worker handling on completion
    switch (buildModel_) {
        case CBuildModel::HUMAN:
            // Human: worker is already outside, just stop repairing
            break;

        case CBuildModel::ORC:
            // Orc: unhide peon and reposition outside
            unit_->SetHidden(false);
            if (building) {
                float offset = building->GetCollisionSize() + unit_->GetCollisionSize() + 32.0f;
                unit_->SetPosition(building->GetX() + offset, building->GetY());
            }
            break;

        case CBuildModel::NIGHT_ELF:
            // Night Elf: the wisp sacrifices itself (ref CBehaviorNightElfBuild).
            // It stays hidden and is removed instead of emerging.
            simulation.RemoveUnit(unit_);
            break;

        case CBuildModel::UNDEAD:
            // Undead: worker is already outside, just stop working
            break;
    }
}

void CBehaviorBuild::CancelConstruction(CSimulation& simulation) {
    if (completed_ || !unit_) return;
    completed_ = true;

    auto* building = simulation.GetUnit(createdUnitHandle_);
    // Full refund of the deducted cost (retail cancel); nothing was spent
    // yet when still walking to the site.
    if (building_) {
        auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
        auto* unitData = simulation.GetUnitData();
        if (player && unitData) {
            if (auto* buildType = unitData->GetUnitType(buildingTypeId_)) {
                player->AddGold(buildType->GetGoldCost());
                player->AddLumber(buildType->GetLumberCost());
            }
        }
    }
    // Hidden riders (orc peon, night-elf wisp) emerge; the wisp is only
    // consumed on completion, so cancel keeps it.
    if (buildModel_ == CBuildModel::ORC || buildModel_ == CBuildModel::NIGHT_ELF) {
        unit_->SetHidden(false);
    }
    if (building_ && building) {
        // Fire before removal so trigger matching still sees the structure.
        if (const auto& cb = simulation.GetConstructEventCallback()) {
            cb(building->GetHandleId(), false);
        }
        simulation.RemoveUnit(building);
        std::cout << "Building cancelled: " << buildingTypeId_.ToString() << std::endl;
    }
}

#include "csimulation.h"
#include "player.h"
#include "unit.h"
#include "handle_id.h"
#include "ability.h"
#include "abilities/ability_attack.h"
#include "abilities/ability_move.h"
#include "abilities/ability_hero.h"
#include "abilities/ability_inventory.h"
#include "abilities/ability_queue.h"
#include "abilities/ability_rally.h"
#include "abilities/ability_revive_hero.h"
#include "abilities/ability_upgrade.h"
#include "abilities/ability_harvest.h"
#include "abilities/ability_gold_mine.h"
#include "abilities/ability_return_resources.h"
#include "abilities/ability_sell_items.h"
#include "abilities/ability_build.h"
#include "pathing/world_collision.h"
#include "pathing/pathfinding.h"
#include "data/unitdata.h"
#include <iostream>
#include <algorithm>

CSimulation::CSimulation() {
    worldCollision_ = std::make_unique<CWorldCollision>();
    pathfindingProcessor_ = std::make_unique<CPathfindingProcessor>();
}

CSimulation::~CSimulation() = default;

void CSimulation::Update() {
    // Update units (includes regen, behavior processing, death detection)
    for (auto* unit : units_) {
        bool shouldRemove = unit->Update(*this);
        if (shouldRemove) {
            removedUnits_.push_back(unit);
        }
    }

    ProcessUnitRemovals();
    ProcessNewUnits();

    // Advance game time
    AdvanceGameTime(SIM_STEP_TIME);

    // Run post-update callbacks
    for (auto& cb : postUpdateCallbacks_) {
        cb();
    }
    postUpdateCallbacks_.clear();

    gameTurnTick_++;
}

CUnit* CSimulation::CreateUnit(War3ID typeId, int playerIndex,
                                float x, float y, float facing) {
    if (!unitData_) return nullptr;
    auto* unitType = unitData_->GetUnitType(typeId);
    if (!unitType) return nullptr;

    int handleId = handleIdAlloc_.CreateId();

    auto unit = std::make_unique<CUnit>(handleId, unitType, playerIndex,
                                         typeId, x, y, facing);

    // Add default abilities based on unit type
    // All units get attack and move abilities
    int abilityHandle = handleIdAlloc_.CreateId();
    unit->AddAbility(*this, std::make_unique<CAbilityAttack>(abilityHandle));

    abilityHandle = handleIdAlloc_.CreateId();
    unit->AddAbility(*this, std::make_unique<CAbilityMove>(abilityHandle));

    // Heroes get the hero ability
    if (unitType->IsHero()) {
        abilityHandle = handleIdAlloc_.CreateId();
        auto heroAbility = std::make_unique<CAbilityHero>(abilityHandle);
        unit->AddAbility(*this, std::move(heroAbility));
    }

    // Buildings get queue, rally, and upgrade abilities
    if (unitType->IsBuilding()) {
        abilityHandle = handleIdAlloc_.CreateId();
        unit->AddAbility(*this, std::make_unique<CAbilityQueue>(abilityHandle));

        abilityHandle = handleIdAlloc_.CreateId();
        unit->AddAbility(*this, std::make_unique<CAbilityRally>(abilityHandle));

        abilityHandle = handleIdAlloc_.CreateId();
        unit->AddAbility(*this, std::make_unique<CAbilityUpgrade>(abilityHandle));

        // Check if building has unitsTrained and populate queue
        auto* queueAbility = dynamic_cast<CAbilityQueue*>(unit->GetAbility(War3ID("Aque")));
        if (queueAbility) {
            for (auto& trainId : unitType->GetUnitsTrained()) {
                queueAbility->AddTrainedUnit(trainId);
            }
            for (auto& researchId : unitType->GetResearchesAvailable()) {
                queueAbility->AddResearch(researchId);
            }
        }

        // Check if building is a gold mine
        for (auto& abilityCode : unitType->GetAbilityList()) {
            if (abilityCode == War3ID("Agld")) {
                abilityHandle = handleIdAlloc_.CreateId();
                unit->AddAbility(*this, std::make_unique<CAbilityGoldMine>(abilityHandle));
                break;
            }
        }

        // Check if building is a town hall (has return resources)
        for (auto& abilityCode : unitType->GetAbilityList()) {
            if (abilityCode == War3ID("Aret")) {
                abilityHandle = handleIdAlloc_.CreateId();
                unit->AddAbility(*this, std::make_unique<CAbilityReturnResources>(abilityHandle));
                break;
            }
        }
    }

    // Workers get harvest and build abilities
    if (!unitType->IsBuilding()) {
        // Check for harvest ability
        for (auto& abilityCode : unitType->GetAbilityList()) {
            if (abilityCode == War3ID("Ahar")) {
                abilityHandle = handleIdAlloc_.CreateId();
                unit->AddAbility(*this, std::make_unique<CAbilityHarvest>(abilityHandle));
                break;
            }
        }

        // Check for build abilities (human, orc, undead, night elf)
        for (auto& abilityCode : unitType->GetAbilityList()) {
            if (abilityCode == War3ID("AHbu") || abilityCode == War3ID("AObu") ||
                abilityCode == War3ID("AUbu") || abilityCode == War3ID("AEbu")) {
                CBuildRace race = CBuildRace::HUMAN;
                if (abilityCode == War3ID("AObu")) race = CBuildRace::ORC;
                else if (abilityCode == War3ID("AUbu")) race = CBuildRace::UNDEAD;
                else if (abilityCode == War3ID("AEbu")) race = CBuildRace::NIGHT_ELF;
                abilityHandle = handleIdAlloc_.CreateId();
                unit->AddAbility(*this, std::make_unique<CAbilityBuild>(abilityHandle, abilityCode, race));
                break;
            }
        }
    }

    // Units with inventory get inventory ability
    for (auto& abilityCode : unitType->GetAbilityList()) {
        if (abilityCode == War3ID("AInv")) {
            abilityHandle = handleIdAlloc_.CreateId();
            unit->AddAbility(*this, std::make_unique<CAbilityInventory>(abilityHandle));
            break;
        }
    }

    CUnit* ptr = unit.get();
    unitMap_[handleId] = std::move(unit);
    newUnits_.push_back(ptr);

    return ptr;
}

CUnit* CSimulation::GetUnit(int handleId) const {
    auto it = unitMap_.find(handleId);
    return (it != unitMap_.end()) ? it->second.get() : nullptr;
}

void CSimulation::RemoveUnit(CUnit* unit) {
    removedUnits_.push_back(unit);
}

CPlayer* CSimulation::GetPlayer(int index) const {
    if (index < 0 || index >= (int)players_.size()) return nullptr;
    return players_[index].get();
}

void CSimulation::AddPlayer(std::unique_ptr<CPlayer> player) {
    players_.push_back(std::move(player));
}

void CSimulation::AdvanceGameTime(float dt) {
    float dayLength = gameplayConstants_.gameDayLength;
    gameTimeOfDay_ += dt;
    if (gameTimeOfDay_ >= dayLength) gameTimeOfDay_ -= dayLength;

    float gameHours = gameplayConstants_.gameDayHours;
    float timeOfDay = (gameTimeOfDay_ / dayLength) * gameHours;
    daytime_ = (timeOfDay >= gameplayConstants_.dawnTimeGameHours)
            && (timeOfDay < gameplayConstants_.duskTimeGameHours);
}

void CSimulation::AddPostUpdateCallback(std::function<void()> cb) {
    postUpdateCallbacks_.push_back(std::move(cb));
}

void CSimulation::ProcessUnitRemovals() {
    for (auto* unit : removedUnits_) {
        auto it = std::find(units_.begin(), units_.end(), unit);
        if (it != units_.end()) units_.erase(it);

        auto mit = std::find(newUnits_.begin(), newUnits_.end(), unit);
        if (mit != newUnits_.end()) newUnits_.erase(mit);

        unitMap_.erase(unit->GetHandleId());
    }
    removedUnits_.clear();
}

void CSimulation::ProcessNewUnits() {
    for (auto* unit : newUnits_) {
        units_.push_back(unit);
        // Register with player
        CPlayer* player = GetPlayer(unit->GetPlayerIndex());
        if (player) player->AddUnit(unit);
    }
    newUnits_.clear();
}

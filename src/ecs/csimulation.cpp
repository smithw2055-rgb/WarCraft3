#include "csimulation.h"
#include "player.h"
#include "unit.h"
#include "destructable.h"
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
#include "abilities/ability_repair.h"
#include "pathing/world_collision.h"
#include "pathing/pathfinding.h"
#include "data/unitdata.h"
#include "data/destructabledata.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <optional>

CSimulation::CSimulation() {
    worldCollision_ = std::make_unique<CWorldCollision>();
    pathfindingProcessor_ = std::make_unique<CPathfindingProcessor>();
    pathfindingProcessor_->SetWorldCollision(worldCollision_.get());
}

CSimulation::~CSimulation() = default;

namespace {

static bool IsRepairAbilityAlias(War3ID abilityCode) {
    return (abilityCode == War3ID("Arep")) || (abilityCode == War3ID("Ahrp"))
        || (abilityCode == War3ID("Aren")) || (abilityCode == War3ID("Arst"));
}

static std::optional<CBuildRace> BuildRaceFromUnitType(const CUnitType* unitType) {
    if (!unitType) {
        return std::nullopt;
    }
    const std::string& raceName = unitType->GetUnitRace().name;
    if (raceName == "human") {
        return CBuildRace::HUMAN;
    }
    if (raceName == "orc") {
        return CBuildRace::ORC;
    }
    if (raceName == "undead") {
        return CBuildRace::UNDEAD;
    }
    if (raceName == "nightelf" || raceName == "night elf") {
        return CBuildRace::NIGHT_ELF;
    }
    return std::nullopt;
}

static War3ID BuildRaceAbilityCode(CBuildRace race) {
    switch (race) {
    case CBuildRace::ORC:
        return War3ID("AObu");
    case CBuildRace::UNDEAD:
        return War3ID("AUbu");
    case CBuildRace::NIGHT_ELF:
        return War3ID("AEbu");
    case CBuildRace::HUMAN:
    default:
        return War3ID("AHbu");
    }
}

} // namespace

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
    ProcessDestructableRemovals();
    ProcessNewDestructables();

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
    // All units get attack; only movable units get move.
    int abilityHandle = handleIdAlloc_.CreateId();
    unit->AddAbility(*this, std::make_unique<CAbilityAttack>(abilityHandle));

    if (!unitType->IsBuilding()) {
        abilityHandle = handleIdAlloc_.CreateId();
        unit->AddAbility(*this, std::make_unique<CAbilityMove>(abilityHandle));
    }

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
        if (auto* upgradeAbility = dynamic_cast<CAbilityUpgrade*>(unit->GetAbility(War3ID("Aupg")))) {
            for (auto& upgradeId : unitType->GetUpgradesTo()) {
                upgradeAbility->AddUpgradeTo(upgradeId);
            }
        }

        if (!unitType->GetItemsSold().empty()) {
            abilityHandle = handleIdAlloc_.CreateId();
            unit->AddAbility(*this, std::make_unique<CAbilitySellItems>(
                abilityHandle, unitType->GetItemsSold()));
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

        if (!unitType->GetStructuresBuilt().empty() && !unit->GetAbility(War3ID("AHbu"))
            && !unit->GetAbility(War3ID("AObu")) && !unit->GetAbility(War3ID("AUbu"))
            && !unit->GetAbility(War3ID("AEbu"))) {
            if (const auto buildRace = BuildRaceFromUnitType(unitType)) {
                abilityHandle = handleIdAlloc_.CreateId();
                auto buildAbility = std::make_unique<CAbilityBuild>(
                    abilityHandle, BuildRaceAbilityCode(*buildRace), *buildRace);
                for (auto& structureId : unitType->GetStructuresBuilt()) {
                    buildAbility->AddBuildableStructure(structureId);
                }
                unit->AddAbility(*this, std::move(buildAbility));
            }
        }

        for (auto& abilityCode : unitType->GetAbilityList()) {
            if (IsRepairAbilityAlias(abilityCode)) {
                abilityHandle = handleIdAlloc_.CreateId();
                auto repairAbility = std::make_unique<CAbilityRepair>(abilityHandle);
                repairAbility->SetAlias(abilityCode);
                unit->AddAbility(*this, std::move(repairAbility));
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

CDestructable* CSimulation::CreateDestructable(War3ID typeId, float x, float y, float life) {
    int handleId = handleIdAlloc_.CreateId();
    auto destructable = std::make_unique<CDestructable>(handleId, typeId, x, y, life);
    if (destructableData_) {
        if (const auto* type = destructableData_->GetDestructable(typeId)) {
            destructable->SetMaxLife(static_cast<float>(type->maxLife));
            destructable->SetLife(*this, std::clamp(life, 0.0f, static_cast<float>(type->maxLife)));
        }
    }
    CDestructable* ptr = destructable.get();
    destructableMap_[handleId] = std::move(destructable);
    newDestructables_.push_back(ptr);
    return ptr;
}

CDestructable* CSimulation::GetDestructable(int handleId) const {
    auto it = destructableMap_.find(handleId);
    return (it != destructableMap_.end()) ? it->second.get() : nullptr;
}

void CSimulation::RemoveDestructable(CDestructable* destructable) {
    if (destructable) {
        removedDestructables_.push_back(destructable);
    }
}

CWidget* CSimulation::GetWidget(int handleId) const {
    if (auto* unit = GetUnit(handleId)) {
        return unit;
    }
    return GetDestructable(handleId);
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
        if (worldCollision_) {
            worldCollision_->RemoveUnit(unit);
        }

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
        if (worldCollision_) {
            worldCollision_->AddUnit(unit);
        }
        // Register with player
        CPlayer* player = GetPlayer(unit->GetPlayerIndex());
        if (player) player->AddUnit(unit);
    }
    newUnits_.clear();
}

void CSimulation::ProcessDestructableRemovals() {
    for (auto* destructable : removedDestructables_) {
        auto it = std::find(destructables_.begin(), destructables_.end(), destructable);
        if (it != destructables_.end()) {
            destructables_.erase(it);
        }

        auto nit = std::find(newDestructables_.begin(), newDestructables_.end(), destructable);
        if (nit != newDestructables_.end()) {
            newDestructables_.erase(nit);
        }

        destructableMap_.erase(destructable->GetHandleId());
    }
    removedDestructables_.clear();
}

void CSimulation::ProcessNewDestructables() {
    for (auto* destructable : newDestructables_) {
        destructables_.push_back(destructable);
    }
    newDestructables_.clear();
}

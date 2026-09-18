#include "csimulation.h"
#include "player.h"
#include "unit.h"
#include "destructable.h"
#include "item.h"
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
#include "data/itemdata.h"
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

static bool IsTownHallReturnResourceType(War3ID typeId) {
    return typeId == War3ID("htow") || typeId == War3ID("hkee") || typeId == War3ID("hcas")
        || typeId == War3ID("ogre") || typeId == War3ID("ostr") || typeId == War3ID("ofrt")
        || typeId == War3ID("unpl") || typeId == War3ID("unp1") || typeId == War3ID("unp2")
        || typeId == War3ID("etol") || typeId == War3ID("etoa") || typeId == War3ID("etoe");
}

static void ApplyItemTypeData(CItem& item, CSimulation& simulation) {
    const CItemData* itemData = simulation.GetItemData();
    if (!itemData) {
        return;
    }
    const CItemType* itemType = itemData->GetItem(item.GetTypeId());
    if (!itemType) {
        return;
    }
    item.SetName(itemType->name);
    item.SetIcon(itemType->iconPath);
    item.SetAbilityList(itemType->abilityList);
    item.SetMaxLife((float)itemType->maxLife);
    item.SetLife(simulation, (float)itemType->maxLife);
    item.SetGoldValue(itemType->goldCost);
    item.SetLumberValue(itemType->lumberCost);
    item.SetItemClass(itemType->itemClass);
    item.SetCharges(itemType->charges);
    item.SetPerishable(itemType->perishable);
    item.SetPowerup(itemType->powerup);
    item.SetUseAutomatically(itemType->useOnly);
    item.SetDroppable(itemType->droppable);
    item.SetPawnable(itemType->pawnable);
    item.SetDroppedOnDeath(itemType->dropOnDeath);
    if (itemType->charges > 0) {
        item.SetCharged(true);
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

    // Fire delayed script actions whose countdown elapsed.
    for (auto it = delayedActions_.begin(); it != delayedActions_.end();) {
        it->secondsLeft -= SIM_STEP_TIME;
        if (it->secondsLeft <= 0.0f) {
            auto fn = std::move(it->fn);
            it = delayedActions_.erase(it);
            if (fn) fn();
        } else {
            ++it;
        }
    }

    // Step projectiles after units so just-fired shots move the same tick.
    UpdateProjectiles();

    ProcessUnitRemovals();
    ProcessNewUnits();
    ProcessDestructableRemovals();
    ProcessNewDestructables();
    ProcessItemRemovals();

    // Advance game time
    AdvanceGameTime(SIM_STEP_TIME);

    // Run post-update callbacks
    for (auto& cb : postUpdateCallbacks_) {
        cb();
    }
    postUpdateCallbacks_.clear();

    gameTurnTick_++;
}

void CSimulation::UpdateProjectiles() {
    for (auto it = projectiles_.begin(); it != projectiles_.end();) {
        if (it->Update(*this)) {
            it = projectiles_.erase(it);
        } else {
            ++it;
        }
    }
}

float CSimulation::SimRand01() {
    // Numerical Recipes LCG; high 16 bits give a [0,1) float.
    simRandState_ = simRandState_ * 1664525u + 1013904223u;
    return (float)(simRandState_ >> 16) * (1.0f / 65536.0f);
}

bool CSimulation::IsPointBlighted(float x, float y) const {
    // W3E-baked blight bit first.
    if (terrainMap_) {
        if (const W3ETileCorner* corner = W3ECornerAt(*terrainMap_, x, y)) {
            if (corner->IsBlight()) return true;
        }
    }
    // Script overrides apply in order; last matching rect wins.
    bool blighted = false;
    for (const auto& r : blightRects_) {
        if (x >= r.minX && x <= r.maxX && y >= r.minY && y <= r.maxY) {
            blighted = r.blighted;
        }
    }
    for (const auto& c : blightCircles_) {
        const float dx = x - c.x, dy = y - c.y;
        if (dx * dx + dy * dy <= c.radius * c.radius) {
            blighted = c.blighted;
        }
    }
    return blighted;
}

CUnit* CSimulation::AdoptUnit(std::unique_ptr<CUnit> unit) {
    if (!unit) return nullptr;
    CUnit* ptr = unit.get();
    unitMap_[ptr->GetHandleId()] = std::move(unit);
    newUnits_.push_back(ptr);
    return ptr;
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
        bool hasReturnResources = IsTownHallReturnResourceType(typeId);
        for (auto& abilityCode : unitType->GetAbilityList()) {
            if (abilityCode == War3ID("Aret")) {
                hasReturnResources = true;
                break;
            }
        }
        if (hasReturnResources) {
            abilityHandle = handleIdAlloc_.CreateId();
            unit->AddAbility(*this, std::make_unique<CAbilityReturnResources>(abilityHandle));
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

CItem* CSimulation::CreateItem(War3ID typeId, float x, float y) {
    int handleId = handleIdAlloc_.CreateId();
    auto item = std::make_unique<CItem>(handleId, typeId, x, y);
    ApplyItemTypeData(*item, *this);
    CItem* ptr = item.get();
    itemMap_[handleId] = std::move(item);
    items_.push_back(ptr);
    if (worldCollision_) {
        worldCollision_->AddItem(ptr);
    }
    return ptr;
}

CItem* CSimulation::GetItem(int handleId) const {
    auto it = itemMap_.find(handleId);
    return (it != itemMap_.end()) ? it->second.get() : nullptr;
}

void CSimulation::RemoveItem(CItem* item) {
    if (item) {
        removedItems_.push_back(item);
    }
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
    if (auto* destructable = GetDestructable(handleId)) {
        return destructable;
    }
    return GetItem(handleId);
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
    if (timeOfDaySuspended_) return;
    float dayLength = gameplayConstants_.gameDayLength;
    gameTimeOfDay_ += dt * timeOfDayScale_;
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
        const bool wasActiveUnit = it != units_.end();
        if (wasActiveUnit) units_.erase(it);

        auto mit = std::find(newUnits_.begin(), newUnits_.end(), unit);
        if (mit != newUnits_.end()) newUnits_.erase(mit);

        if (wasActiveUnit) {
            if (CPlayer* player = GetPlayer(unit->GetPlayerIndex())) {
                player->RemoveUnit(unit);
                if (unit->GetFoodUsed() > 0) {
                    player->SetFoodUsed(std::max(0, player->GetFoodUsed() - unit->GetFoodUsed()));
                }
            }
        }

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
        if (player) {
            player->AddUnit(unit);
            if (unit->GetFoodUsed() > 0) {
                player->SetFoodUsed(player->GetFoodUsed() + unit->GetFoodUsed());
            }
        }
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

void CSimulation::ProcessItemRemovals() {
    for (auto* item : removedItems_) {
        const int handleId = item->GetHandleId();
        if (itemRemovedCallback_) {
            itemRemovedCallback_(handleId);
        }

        if (worldCollision_) {
            worldCollision_->RemoveItem(item);
        }

        auto it = std::find(items_.begin(), items_.end(), item);
        if (it != items_.end()) {
            items_.erase(it);
        }

        itemMap_.erase(handleId);
    }
    removedItems_.clear();
}

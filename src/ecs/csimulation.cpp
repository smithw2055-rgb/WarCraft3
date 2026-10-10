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
#include "ecs/orders/order.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
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
    if (paused_) return;
    fogSystem_.Tick(SIM_STEP_TIME);
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
    UpdateHazards();
    // Push stacked units apart so clustered spawns and followers cannot
    // hard-lock each other's collision checks forever (ref unit separation).
    ResolveUnitSeparation();

    ProcessUnitRemovals();
    ProcessNewUnits();
    ProcessDestructableRemovals();
    ProcessNewDestructables();
    ProcessItemRemovals();

    // Fog sight pass on its cadence, after pending units join so freshly
    // created/adopted units contribute the same tick (deterministic: units
    // iterate in units_ order, no RNG).
    if (gameTurnTick_ % CFogSystem::kSightTickPeriod == 0) {
        UpdateFogSight();
    }

    // Advance game time
    AdvanceGameTime(SIM_STEP_TIME);

    // Run post-update callbacks
    for (auto& cb : postUpdateCallbacks_) {
        cb();
    }
    postUpdateCallbacks_.clear();

    gameTurnTick_++;
}

void CSimulation::UpdateFogSight() {
    if (terrainMap_ && terrainMap_->tileWidth > 0 && terrainMap_->tileHeight > 0) {
        constexpr float kTile = 128.0f; // W3E tiles (cf. w3e.h kTileSize)
        fogSystem_.EnsureGrid(terrainMap_->tileWidth, terrainMap_->tileHeight,
                              terrainMap_->offsetX, terrainMap_->offsetY,
                              terrainMap_->offsetX + terrainMap_->tileWidth * kTile,
                              terrainMap_->offsetY + terrainMap_->tileHeight * kTile);
    }
    const bool night = IsNighttime();
    sightScratch_.clear();
    for (const auto* unit : units_) {
        if (!unit || unit->IsDead()) continue;
        const CUnitType* type = unit->GetUnitType();
        if (!type) continue;
        const int radius = night ? type->GetSightRadiusNight() : type->GetSightRadiusDay();
        if (radius <= 0) continue;
        CFogSightSource src;
        src.player = unit->GetPlayerIndex();
        src.x = unit->GetX();
        src.y = unit->GetY();
        src.radius = (float)radius;
        sightScratch_.push_back(src);
        for (int other : unit->SharedVisionPlayers()) {
            if (other == src.player || other < 0 || other >= CFogSystem::kMaxPlayers) continue;
            CFogSightSource shared = src;
            shared.player = other;
            sightScratch_.push_back(shared);
        }
    }
    fogSystem_.UpdateSight(sightScratch_.data(), (int)sightScratch_.size());
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

void CSimulation::ResolveUnitSeparation() {
    // Positional separation for overlapping movable units. Buildings never
    // move; a movable unit overlapping a building is pushed out alone.
    // Hidden/loaded/dead units are skipped. Displacement per pair is capped
    // so docking (harvest dropoff, repair) stays stable.
    constexpr float kMaxPushPerTick = 6.0f;
    const size_t n = units_.size();
    for (size_t i = 0; i < n; ++i) {
        CUnit* a = units_[i];
        if (!a || a->IsDead() || a->IsHidden() || a->IsLoadedAboard()) continue;
        const bool aStatic = a->IsBuilding() || !a->IsPathingEnabled();
        for (size_t j = i + 1; j < n; ++j) {
            CUnit* b = units_[j];
            if (!b || b->IsDead() || b->IsHidden() || b->IsLoadedAboard()) continue;
            const bool bStatic = b->IsBuilding() || !b->IsPathingEnabled();
            if (aStatic && bStatic) continue;
            const float ra = std::max(8.0f, a->GetCollisionSize());
            const float rb = std::max(8.0f, b->GetCollisionSize());
            const float minDist = ra + rb;
            float dx = b->GetX() - a->GetX();
            float dy = b->GetY() - a->GetY();
            float d2 = dx * dx + dy * dy;
            if (d2 >= minDist * minDist) continue;
            float dist = std::sqrt(d2);
            float nx = 0.0f;
            float ny = 0.0f;
            if (dist < 0.001f) {
                // Exactly stacked: separate along a deterministic axis
                // derived from handle order so the pair splits apart.
                nx = (a->GetHandleId() < b->GetHandleId()) ? -1.0f : 1.0f;
                ny = 0.0f;
                dist = 0.0f;
            } else {
                nx = dx / dist;
                ny = dy / dist;
            }
            const float overlap = minDist - dist;
            float push = std::min(overlap * 0.5f, kMaxPushPerTick);
            if (!aStatic && !bStatic) {
                a->SetPosition(a->GetX() - nx * push, a->GetY() - ny * push);
                b->SetPosition(b->GetX() + nx * push, b->GetY() + ny * push);
            } else if (!aStatic) {
                a->SetPosition(a->GetX() - nx * std::min(overlap, kMaxPushPerTick),
                               a->GetY() - ny * std::min(overlap, kMaxPushPerTick));
            } else {
                b->SetPosition(b->GetX() + nx * std::min(overlap, kMaxPushPerTick),
                               b->GetY() + ny * std::min(overlap, kMaxPushPerTick));
            }
        }
    }
}

void CSimulation::UpdateHazards() {    for (auto it = hazards_.begin(); it != hazards_.end();) {
        SimHazard& h = *it;
        // Wander step (tornadoes).
        if (h.speed > 0.0f) {
            const float dx = h.tx - h.x;
            const float dy = h.ty - h.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 1.0f) {
                const float step = std::min(h.speed * SIM_STEP_TIME, dist);
                h.x += (dx / dist) * step;
                h.y += (dy / dist) * step;
            }
        }
        bool consume = false;
        if (h.tick) {
            h.tickCount++;
            if (h.tickCount % std::max(1, h.tickPeriod) == 0) {
                consume = h.tick(*this, h);
            }
        }
        h.ticksLeft--;
        if (consume || h.ticksLeft <= 0) {
            it = hazards_.erase(it);
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

bool CSimulation::SetTerrainPathableCell(float x, float y, int pathType, bool pathable) {
    if (!pathfindingProcessor_ || !pathfindingProcessor_->HasPathingMap()) return false;
    WpmData* wpm = pathfindingProcessor_->GetPathingMap();
    if (!wpm) return false;
    const int cx = pathfindingProcessor_->WorldToCellX(x);
    const int cy = pathfindingProcessor_->WorldToCellY(y);
    if (cx < 0 || cy < 0 || cx >= wpm->width || cy >= wpm->height) return false;
    uint8_t flag = 0;
    switch (pathType) {
        case 1: // walk
        case 4: // peon harvest walks like foot
            flag = WpmData::UNWALKABLE;
            break;
        case 2: // fly
            flag = WpmData::UNFLYABLE;
            break;
        case 3: // build
            flag = WpmData::UNBUILDABLE;
            break;
        case 5: // blight
            flag = WpmData::BLIGHTED;
            break;
        case 6: // float (water-bound): unreadable water blocks floaters
            flag = WpmData::UNSWIMABLE;
            break;
        case 0: // any: walkability governs
        case 7: // amphibious uses walk cells too
        default:
            flag = WpmData::UNWALKABLE;
            break;
    }
    uint8_t& cell = wpm->pathing[(size_t)cy * (size_t)wpm->width + (size_t)cx];
    if (pathable) {
        cell = (uint8_t)(cell & ~flag);
    } else {
        cell = (uint8_t)(cell | flag);
    }
    return true;
}
bool CSimulation::IsPointBlighted(float x, float y) const {
    // W3E-baked blight bit first.
    if (terrainMap_) {
        if (const W3ETileCorner* corner = W3ECornerAt(*terrainMap_, x, y)) {
            if (corner->IsBlight()) return true;
        }
    }
    // Pathing-grid blight bit (map-baked or SetTerrainPathable).
    if (pathfindingProcessor_ && pathfindingProcessor_->HasPathingMap()) {
        WpmData* wpm = pathfindingProcessor_->GetPathingMap();
        if (wpm) {
            const int cx = pathfindingProcessor_->WorldToCellX(x);
            const int cy = pathfindingProcessor_->WorldToCellY(y);
            if (wpm->IsBlight(cx, cy)) return true;
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
    // Every order issued on this unit (JASS/UI/AI/net all call
    // CUnit::IssueOrder directly) flows into the sim order callback:
    // command log lives in IssueUnitOrder, JASS issued-order trigger
    // events live behind unitOrderCallback_.
    const int adoptedHandle = ptr->GetHandleId();
    ptr->SetOrderIssuedHook([this, adoptedHandle](int orderId, int targetKind, float x, float y, int targetHandle) {
        (void)targetKind;
        if (unitOrderCallback_) {
            unitOrderCallback_(adoptedHandle, orderId, targetHandle, x, y);
        }
    });
    ptr->SetUnitLoadedHook([this, adoptedHandle](int passengerHandle) {
        if (unitLoadedCallback_) {
            unitLoadedCallback_(adoptedHandle, passengerHandle);
        }
    });
    unitMap_[ptr->GetHandleId()] = std::move(unit);
    newUnits_.push_back(ptr);
    return ptr;
}

void CSimulation::AddGlobalShopStock(War3ID id, int current, int max, bool isUnit) {
    for (auto& entry : globalShopStock_) {
        if (entry.id == id && entry.isUnit == isUnit) {
            entry.current = current;
            entry.max = max;
            return;
        }
    }
    globalShopStock_.push_back({id, current, max, isUnit});
}

void CSimulation::RemoveGlobalShopStock(War3ID id, bool isUnit) {
    globalShopStock_.erase(std::remove_if(globalShopStock_.begin(), globalShopStock_.end(),
        [&](const GlobalShopStockEntry& entry) {
            return entry.id == id && entry.isUnit == isUnit;
        }), globalShopStock_.end());
}

CUnit* CSimulation::CreateUnit(War3ID typeId, int playerIndex,
                                float x, float y, float facing) {
    if (!unitData_) return nullptr;
    auto* unitType = unitData_->GetUnitType(typeId);
    if (!unitType) return nullptr;

    // Map-init dedupe: preplaced units exist both in war3mapUnits.doo and
    // in the map script's CreateNeutral*/CreatePlayer* functions, which the
    // engine runs on top of the doo load. During init only, fold an exact
    // duplicate (same type + owner within 64 units) into the existing unit
    // so creeps/mines/taverns are not doubled. Post-init creation (training,
    // summons, revives) is never deduped.
    if (mapInitPhase_) {
        for (CUnit* existing : units_) {
            if (!existing || existing->IsDead() || existing->GetTypeId() != typeId ||
                existing->GetPlayerIndex() != playerIndex) {
                continue;
            }
            const float dx = existing->GetX() - x;
            const float dy = existing->GetY() - y;
            if (dx * dx + dy * dy < 64.0f * 64.0f) {
                return existing;
            }
        }
        for (CUnit* pending : newUnits_) {
            if (!pending || pending->IsDead() || pending->GetTypeId() != typeId ||
                pending->GetPlayerIndex() != playerIndex) {
                continue;
            }
            const float dx = pending->GetX() - x;
            const float dy = pending->GetY() - y;
            if (dx * dx + dy * dy < 64.0f * 64.0f) {
                return pending;
            }
        }
    }

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
        // Race workers always harvest (ref melee economy): data rows for
        // some workers (e.g. acolyte) lack Ahar, which would leave AI and
        // player harvest orders permanently rejected. Acolytes cannot chop
        // lumber (retail), so their lumber capacity stays zero.
        if (!unit->GetAbility(War3ID("Ahar")) &&
            (typeId == War3ID("hpea") || typeId == War3ID("opeo") ||
             typeId == War3ID("uaco") || typeId == War3ID("ewsp"))) {
            abilityHandle = handleIdAlloc_.CreateId();
            auto harvest = std::make_unique<CAbilityHarvest>(abilityHandle);
            if (typeId == War3ID("uaco")) harvest->SetLumberCapacity(0);
            unit->AddAbility(*this, std::move(harvest));
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

namespace {

uint64_t HashU32(uint64_t h, uint32_t v) {
    // FNV-1a 64 over 4 LE bytes (same-binary comparison only).
    for (int i = 0; i < 4; i++) {
        h ^= (uint64_t)((v >> (i * 8)) & 0xFFu);
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t HashFloat(uint64_t h, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return HashU32(h, bits);
}

} // namespace

bool CSimulation::IssueUnitOrder(int unitHandle, std::unique_ptr<COrder> order) {
    if (!order) return false;
    CUnit* unit = GetUnit(unitHandle);
    if (!unit) return false;
    SimCommand cmd;
    cmd.turn = gameTurnTick_;
    cmd.player = unit->GetPlayerIndex();
    cmd.unit = unitHandle;
    cmd.orderId = order->GetOrderId();
    cmd.targetKind = (int32_t)order->GetTargetKind();
    cmd.targetHandle = -1;
    cmd.x = 0.0f;
    cmd.y = 0.0f;
    cmd.z = 0.0f;
    if (auto* pt = dynamic_cast<COrderTargetPoint*>(order.get())) {
        cmd.x = pt->GetX();
        cmd.y = pt->GetY();
        cmd.z = pt->GetZ();
    } else if (auto* wg = dynamic_cast<COrderTargetWidget*>(order.get())) {
        cmd.targetHandle = wg->GetTarget() ? wg->GetTarget()->GetHandleId() : -1;
    }
    commandLog_.push_back(cmd);
    const int orderId = cmd.orderId;
    unit->IssueOrder(std::move(order));
    // NOTE: no direct unitOrderCallback_ call here; CUnit::IssueOrder
    // invokes the AdoptUnit-installed hook above, which fires it exactly
    // once for every order path (sim, JASS, UI, AI, net).
    (void)orderId;
    return true;
}

uint64_t CSimulation::HashState() const {
    uint64_t h = 1469598103934665603ull; // FNV-1a offset
    h = HashU32(h, (uint32_t)gameTurnTick_);
    h = HashU32(h, simRandState_);
    h = HashU32(h, combatSystem_.GetRandState());
    std::vector<const CUnit*> sorted(units_.begin(), units_.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const CUnit* a, const CUnit* b) {
                  return a->GetHandleId() < b->GetHandleId();
              });
    h = HashU32(h, (uint32_t)sorted.size());
    for (const auto* u : sorted) {
        if (!u) continue;
        h = HashU32(h, (uint32_t)u->GetHandleId());
        h = HashU32(h, u->GetTypeId().value);
        h = HashU32(h, (uint32_t)u->GetPlayerIndex());
        h = HashFloat(h, u->GetX());
        h = HashFloat(h, u->GetY());
        h = HashFloat(h, u->GetLife());
    }
    h = HashU32(h, (uint32_t)projectiles_.size());
    for (const auto& p : projectiles_) {
        h = HashFloat(h, p.x);
        h = HashFloat(h, p.y);
    }
    h = HashU32(h, (uint32_t)hazards_.size());
    for (const auto& hz : hazards_) {
        h = HashFloat(h, hz.x);
        h = HashFloat(h, hz.y);
    }
    return h;
}

#include "ecs/sim_snapshot.h"
#include "csimulation.h"
#include "player.h"
#include "unit.h"
#include "destructable.h"
#include "item.h"
#include "handle_id.h"
#include "ability.h"
#include "pathing/world_collision.h"
#include "pathing/pathfinding.h"
#include "pathing/pathfinding_server.h"
#include "pathing/crowd_steering.h"
#include "data/unitdata.h"
#include "data/destructabledata.h"
#include "data/itemdata.h"
#include "ecs/orders/order.h"
#include "game/war3/war3_game_module.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

CSimulation::CSimulation() {
    worldCollision_ = std::make_unique<CWorldCollision>();
    pathfindingProcessor_ = std::make_unique<CPathfindingProcessor>();
    pathfindingProcessor_->SetWorldCollision(worldCollision_.get());
    pathfindingServer_ = std::make_unique<pathfinding::PathfindingServer>();
    crowdSteering_ = std::make_unique<pathfinding::CrowdSteeringSystem>();
    fogSystem_.SetOcclusionChecker([this](float fromX, float fromY, float toX, float toY, bool isFlying) -> bool {
        return IsLineOfSightBlocked(fromX, fromY, toX, toY, isFlying);
    });
    RegisterBuiltInSystems();
    SetSimulationModule(war3::War3GameModule::GetSharedDefaultInstance());
}

CSimulation::~CSimulation() {
    if (simModule_) {
        simModule_->OnDetachFromSimulation(*this);
    }
}

void CSimulation::SetSimulationModule(std::shared_ptr<engine::ISimulationModule> module) {
    if (simModule_ == module) return;
    if (simModule_) {
        simModule_->OnDetachFromSimulation(*this);
    }
    simModule_ = std::move(module);
    pureEngineMode_ = (dynamic_cast<engine::NullSimulationModule*>(simModule_.get()) != nullptr);
    combatSystem_.SetCombatModule(simModule_.get(), pureEngineMode_);
    if (simModule_) {
        simModule_->OnAttachToSimulation(*this);
        simModule_->OnConfigureCombatSystem(*this, combatSystem_);
    }
}

void CSimulation::SetPureEngineMode(bool pureEngine) {
    if (pureEngine) {
        SetSimulationModule(std::make_shared<engine::NullSimulationModule>());
        pureEngineMode_ = true;
    } else {
        SetSimulationModule(war3::War3GameModule::GetSharedDefaultInstance());
        pureEngineMode_ = false;
    }
}

void CSimulation::RegisterBuiltInSystems() {
    systemPipeline_.RegisterLambda("FogTimerSystem", SimSystemPhase::PreTick,
        [](CSimulation& sim, float dt) {
            sim.fogSystem_.Tick(dt);
        });

    systemPipeline_.RegisterLambda("UnitLogicSystem", SimSystemPhase::UnitLogic,
        [](CSimulation& sim, float /*dt*/) {
            for (auto* unit : sim.units_) {
                if (unit && unit->Update(sim)) {
                    sim.removedUnits_.push_back(unit);
                }
            }
        });

    systemPipeline_.RegisterLambda("DelayedActionSystem", SimSystemPhase::DelayedActions,
        [](CSimulation& sim, float dt) {
            for (auto it = sim.delayedActions_.begin(); it != sim.delayedActions_.end();) {
                it->secondsLeft -= dt;
                if (it->secondsLeft <= 0.0f) {
                    auto fn = std::move(it->fn);
                    it = sim.delayedActions_.erase(it);
                    if (fn) fn();
                } else {
                    ++it;
                }
            }
        });

    systemPipeline_.RegisterLambda("ProjectileSystem", SimSystemPhase::Projectiles,
        [](CSimulation& sim, float /*dt*/) {
            sim.UpdateProjectiles();
        });

    systemPipeline_.RegisterLambda("HazardSystem", SimSystemPhase::Hazards,
        [](CSimulation& sim, float /*dt*/) {
            sim.UpdateHazards();
        });

    systemPipeline_.RegisterLambda("EntityLifecycleSystem", SimSystemPhase::LifecycleCommit,
        [](CSimulation& sim, float /*dt*/) {
            sim.ProcessUnitRemovals();
            sim.ProcessNewUnits();
            sim.ProcessDestructableRemovals();
            sim.ProcessNewDestructables();
            sim.ProcessItemRemovals();
            sim.spatialGrid_.Rebuild(sim.units_);
            SimHotComponentSystem::SyncUnitsToHotPools(sim);
        });

    systemPipeline_.RegisterLambda("SpatialSeparationSystem", SimSystemPhase::SpatialSeparation,
        [](CSimulation& sim, float /*dt*/) {
            sim.ResolveUnitSeparation();
            sim.spatialGrid_.Rebuild(sim.units_);
        });

    systemPipeline_.RegisterLambda("FogSightSystem", SimSystemPhase::VisionAndFog,
        [](CSimulation& sim, float /*dt*/) {
            if (sim.gameTurnTick_ % CFogSystem::kSightTickPeriod == 0) {
                sim.UpdateFogSight();
            }
        });

    systemPipeline_.RegisterLambda("ClockAndPostCallbackSystem", SimSystemPhase::ClockAndPost,
        [](CSimulation& sim, float dt) {
            sim.AdvanceGameTime(dt);
            for (auto& cb : sim.postUpdateCallbacks_) {
                cb();
            }
            sim.postUpdateCallbacks_.clear();
        });
}

void CSimulation::Update() {
    if (paused_) return;
    systemPipeline_.ResetStepStats();
    systemPipeline_.ExecuteAll(*this, SIM_STEP_TIME);
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
        src.isFlying = unit->IsFlying();
        sightScratch_.push_back(src);
        for (int other : unit->SharedVisionPlayers()) {
            if (other == src.player || other < 0 || other >= CFogSystem::kMaxPlayers) continue;
            CFogSightSource shared = src;
            shared.player = other;
            sightScratch_.push_back(shared);
        }
        if (const CPlayer* owner = GetPlayer(src.player)) {
            for (size_t otherIdx = 0; otherIdx < players_.size() && otherIdx < (size_t)CFogSystem::kMaxPlayers; ++otherIdx) {
                if ((int)otherIdx == src.player) continue;
                const auto& otherPlayer = players_[otherIdx];
                if (!otherPlayer) continue;
                if (owner->HasAlliance(*otherPlayer, CAllianceType::SHARED_VISION) ||
                    owner->HasAlliance(*otherPlayer, CAllianceType::VISION)) {
                    const auto& sv = unit->SharedVisionPlayers();
                    if (std::find(sv.begin(), sv.end(), (int)otherIdx) == sv.end()) {
                        CFogSightSource shared = src;
                        shared.player = (int)otherIdx;
                        sightScratch_.push_back(shared);
                    }
                }
            }
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

    // Rebuild deterministic SpatialHashGrid before separation
    spatialGrid_.Rebuild(units_);

    // For large unit counts (> 48), use SpatialHashGrid O(N) neighborhood lookup;
    // for small unit counts, keep the exact array-order loop.
    const bool useSpatialBroadphase = (n > 48);
    std::vector<CUnit*> candidates;

    for (size_t i = 0; i < n; ++i) {
        CUnit* a = units_[i];
        if (!a || a->IsDead() || a->IsHidden() || a->IsLoadedAboard()) continue;
        const bool aStatic = a->IsBuilding() || !a->IsPathingEnabled();

        if (useSpatialBroadphase) {
            spatialGrid_.QueryPotentialColliders(a, candidates);
        }
        const size_t innerCount = useSpatialBroadphase ? candidates.size() : (n - (i + 1));
        for (size_t k = 0; k < innerCount; ++k) {
            CUnit* b = useSpatialBroadphase ? candidates[k] : units_[i + 1 + k];
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
    InvalidateNavigationCaches();
    return true;
}

void CSimulation::InvalidateNavigationCaches() {
    if (pathfindingServer_) {
        pathfindingServer_->InvalidateFlowFields();
    }
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

int CSimulation::GetTerrainHeight(float x, float y) const {
    if (terrainMap_) {
        if (const W3ETileCorner* corner = W3ECornerAt(*terrainMap_, x, y)) {
            return corner->LayerHeight();
        }
    }
    return 999;
}

bool CSimulation::CheckHighlandMiss(CUnit* attacker, const CWidget* target, bool isAttack) {
    if (!isAttack || !attacker || !target) return false;
    if (dynamic_cast<const CUnit*>(target) == nullptr) return false;
    if (GetTerrainHeight(attacker->GetX(), attacker->GetY()) <
        GetTerrainHeight(target->GetX(), target->GetY())) {
        if (SimRand01() < gameplayConstants_.chanceToMiss) return true;
    }
    return false;
}

bool CSimulation::IsLineOfSightBlocked(float fromX, float fromY, float toX, float toY, bool isFlying) const {
    if (isFlying) return false;

    // 1. Terrain cliff level check: low ground cannot see onto higher cliff
    const int fromCliff = GetTerrainHeight(fromX, fromY);
    const int toCliff = GetTerrainHeight(toX, toY);
    if (fromCliff != 999 && toCliff != 999 && toCliff > fromCliff) {
        return true;
    }

    if (!pathfindingProcessor_ || !pathfindingProcessor_->HasPathingMap()) {
        return false;
    }
    const WpmData* wpm = pathfindingProcessor_->GetPathingMap();
    if (!wpm) return false;

    const float dx = toX - fromX;
    const float dy = toY - fromY;
    const float distSq = dx * dx + dy * dy;
    constexpr float kStep = 32.0f; // WPM cell size
    if (distSq <= kStep * kStep) {
        return false;
    }

    const float dist = std::sqrt(distSq);
    const int numSteps = std::max(1, static_cast<int>(dist / kStep));
    const float invSteps = 1.0f / static_cast<float>(numSteps);

    for (int step = 1; step < numSteps; ++step) {
        const float t = static_cast<float>(step) * invSteps;
        const float sx = fromX + t * dx;
        const float sy = fromY + t * dy;

        if (fromCliff != 999 && terrainMap_) {
            const int sampleCliff = GetTerrainHeight(sx, sy);
            if (sampleCliff != 999 && sampleCliff > fromCliff) {
                return true; // Intermediate higher ground blocks vision
            }
        }

        const int cx = pathfindingProcessor_->WorldToCellX(sx);
        const int cy = pathfindingProcessor_->WorldToCellY(sy);
        if (cx >= 0 && cx < wpm->width && cy >= 0 && cy < wpm->height) {
            if (wpm->pathing[static_cast<size_t>(cy) * static_cast<size_t>(wpm->width) + static_cast<size_t>(cx)] & WpmData::BLOCKVISION) {
                return true; // Intermediate tree or vision blocker
            }
        }
    }

    return false;
}

void CSimulation::InstallUnitHooks(CUnit* ptr) {
    if (!ptr) return;
    ptr->BindRegistry(&ecsRegistry_);
    const int handle = ptr->GetHandleId();
    ptr->SetOrderIssuedHook([this, handle](int orderId, int targetKind, float x, float y, int targetHandle) {
        (void)targetKind;
        events_.onUnitOrder.Emit(handle, orderId, targetHandle, x, y);
    });
    ptr->SetUnitLoadedHook([this, handle](int passengerHandle) {
        events_.onUnitLoaded.Emit(handle, passengerHandle);
    });
}

CUnit* CSimulation::AdoptUnit(std::unique_ptr<CUnit> unit) {
    if (!unit) return nullptr;
    CUnit* ptr = unit.get();
    InstallUnitHooks(ptr);
    unitMap_[ptr->GetHandleId()] = std::move(unit);
    newUnits_.push_back(ptr);
    ptr->RegeneratePathingInstance(*this);
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
    CUnit* ptr = unit.get();

    // Phase 13: Delegate game-specific unit/building/worker/hero ability assembly
    // to the active ISimulationModule (e.g., War3GameModule or a dynamic plugin).
    if (simModule_) {
        simModule_->OnAssembleUnitAbilities(*this, *ptr, *unitType);
    }
    InstallUnitHooks(ptr);

    unitMap_[handleId] = std::move(unit);
    newUnits_.push_back(ptr);
    // Retail setupCreatedUnit: buildings blit their pathTex footprint now
    // (snapped inside RegeneratePathingInstance).
    ptr->RegeneratePathingInstance(*this);

    return ptr;
}

CUnit* CSimulation::GetUnit(int handleId) const {
    auto it = unitMap_.find(handleId);
    return (it != unitMap_.end()) ? it->second.get() : nullptr;
}

CDestructable* CSimulation::CreateDestructable(War3ID typeId, float x, float y, float life,
                                                 int rotationDeg) {
    int handleId = handleIdAlloc_.CreateId();
    auto destructable = std::make_unique<CDestructable>(handleId, typeId, x, y, life);
    if (simModule_) {
        simModule_->OnConfigureCreatedDestructable(*this, *destructable, life, rotationDeg);
    }
    CDestructable* ptr = destructable.get();
    destructableMap_[handleId] = std::move(destructable);
    newDestructables_.push_back(ptr);
    // Retail War3MapViewer: live pathTex now (or death pathTex when the
    // doodad spawns dead); occlusionHeight drives the vision block.
    ptr->BindPathing(*this, rotationDeg);
    return ptr;
}

CDestructable* CSimulation::GetDestructable(int handleId) const {
    auto it = destructableMap_.find(handleId);
    return (it != destructableMap_.end()) ? it->second.get() : nullptr;
}

CItem* CSimulation::CreateItem(War3ID typeId, float x, float y) {
    if (typeId.value == 0) return nullptr;
    if (itemData_ && !itemData_->HasItem(typeId)) {
        return nullptr;
    }
    int handleId = handleIdAlloc_.CreateId();
    auto item = std::make_unique<CItem>(handleId, typeId, x, y);
    if (simModule_) simModule_->OnConfigureCreatedItem(*this, *item);
    CItem* ptr = item.get();
    itemMap_[handleId] = std::move(item);
    items_.push_back(ptr);
    if (worldCollision_) {
        worldCollision_->AddItem(ptr);
    }
    events_.onItemSpawned.Emit(handleId);
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

int CSimulation::GetEffectiveTechLevel(int playerIndex, War3ID techId) const {
    if (simModule_) {
        return simModule_->GetEffectiveTechLevel(*this, playerIndex, techId.value);
    }
    if (const CPlayer* p = GetPlayer(playerIndex)) {
        return p->GetTechLevel(techId);
    }
    return 0;
}

void CSimulation::SetPlayerTechLevel(int playerIndex, War3ID techId, int level) {
    if (CPlayer* p = GetPlayer(playerIndex)) {
        p->SetTechLevel(*this, techId, level);
    }
}

void CSimulation::EvaluateVictoryDefeat() {
    if (simModule_) {
        simModule_->EvaluateVictoryDefeat(*this);
    }
}

bool CSimulation::IsPlayerCrippled(int playerIndex) const {
    if (simModule_) {
        return simModule_->IsPlayerCrippled(*this, playerIndex);
    }
    if (const CPlayer* p = GetPlayer(playerIndex)) {
        return p->IsCrippled();
    }
    return false;
}

bool CSimulation::CreatePlayerAI(int playerIndex, int difficulty) {
    if (simModule_) {
        return simModule_->CreatePlayerAI(*this, playerIndex, difficulty);
    }
    return false;
}

bool CSimulation::RemovePlayerAI(int playerIndex) {
    if (simModule_) {
        return simModule_->RemovePlayerAI(playerIndex);
    }
    return false;
}

void CSimulation::TickPlayerAIs(float dt) {
    if (simModule_) {
        simModule_->TickPlayerAIs(*this, dt);
    }
}

void CSimulation::AdvanceGameTime(float dt) {
    if (timeOfDaySuspended_) return;
    // gameTimeOfDay_ is kept in game hours (0..gameDayHours), matching the
    // JASS GetTimeOfDay/SetTimeOfDay contract. GameDayLength is the real
    // seconds a full day takes, so convert the wall-clock step to hours.
    const float gameHours = gameplayConstants_.gameDayHours > 0.0f
        ? gameplayConstants_.gameDayHours : 24.0f;
    const float dayLength = gameplayConstants_.gameDayLength > 0.0f
        ? gameplayConstants_.gameDayLength : gameHours;
    const float hoursPerSecond = gameHours / dayLength;

    gameTimeOfDay_ += dt * timeOfDayScale_ * hoursPerSecond;
    if (gameTimeOfDay_ >= gameHours) {
        gameTimeOfDay_ = std::fmod(gameTimeOfDay_, gameHours);
    } else if (gameTimeOfDay_ < 0.0f) {
        gameTimeOfDay_ = std::fmod(gameTimeOfDay_, gameHours) + gameHours;
    }

    daytime_ = (gameTimeOfDay_ >= gameplayConstants_.dawnTimeGameHours)
            && (gameTimeOfDay_ < gameplayConstants_.duskTimeGameHours);
}

void CSimulation::AddPostUpdateCallback(std::function<void()> cb) {
    postUpdateCallbacks_.push_back(std::move(cb));
}

void CSimulation::ProcessUnitRemovals() {
    for (auto* unit : removedUnits_) {
        // Retail removal drops the footprint (ref killPathingInstance on
        // removeUnit; death already cleared it, guard is idempotent).
        if (unit) unit->KillPathingInstance(*this);
        if (worldCollision_) {
            worldCollision_->RemoveUnit(unit);
        }

        auto it = std::find(units_.begin(), units_.end(), unit);
        const bool wasActiveUnit = it != units_.end();
        if (wasActiveUnit) units_.erase(it);

        auto mit = std::find(newUnits_.begin(), newUnits_.end(), unit);
        if (mit != newUnits_.end()) newUnits_.erase(mit);

        if (simModule_ && unit) {
            simModule_->OnUnitRemovedFromWorld(*this, *unit, wasActiveUnit);
        }

        const int removedHandle = unit->GetHandleId();
        events_.onUnitRemoved.Emit(removedHandle);
        ecsRegistry_.DestroyEntity(removedHandle);
        unitMap_.erase(removedHandle);
    }
    removedUnits_.clear();
}

void CSimulation::ProcessNewUnits() {
    for (auto* unit : newUnits_) {
        units_.push_back(unit);
        if (worldCollision_) {
            worldCollision_->AddUnit(unit);
        }
        if (simModule_ && unit) {
            simModule_->OnUnitCommittedToWorld(*this, *unit);
        }
        events_.onUnitSpawned.Emit(unit->GetHandleId());
    }
    newUnits_.clear();
}

void CSimulation::ProcessDestructableRemovals() {
    for (auto* destructable : removedDestructables_) {
        if (destructable) destructable->UnbindPathing(*this);
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
        events_.onItemRemoved.Emit(handleId);

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

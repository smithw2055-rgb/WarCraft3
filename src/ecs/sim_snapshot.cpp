#include "ecs/sim_snapshot.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "ecs/destructable.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/unit.h"

size_t SimHotComponentSystem::SyncUnitsToHotPools(CSimulation& sim) {
    SimEcsRegistry& reg = sim.Registry();
    auto& tfPool = reg.Pool<UnitHotTransformComponent>();
    auto& vtPool = reg.Pool<UnitHotVitalsComponent>();
    tfPool.Clear();
    vtPool.Clear();

    size_t count = 0;
    for (const CUnit* u : sim.GetUnits()) {
        if (!u) continue;
        const int h = u->GetHandleId();

        UnitHotTransformComponent tf;
        tf.x = u->GetX();
        tf.y = u->GetY();
        tf.facingDeg = u->GetFacing();
        tf.flyHeight = u->GetFlyHeight();
        tf.collisionSize = u->GetCollisionSize();
        tf.effectiveSpeed = u->GetSpeed();
        tf.turnRate = u->GetTurnRate();
        tf.playerIndex = u->GetPlayerIndex();
        tf.typeId = u->GetTypeId().value;
        tf.movementType = u->GetMovementType();
        tf.isBuilding = u->IsBuilding();
        tf.pathingEnabled = u->IsPathingEnabled();
        tfPool.Emplace(h, tf);

        UnitHotVitalsComponent vt;
        vt.life = u->GetLife();
        vt.maxLife = u->GetMaxLife();
        vt.mana = u->GetMana();
        vt.maxMana = u->GetMaxMana();
        vt.totalDefense = u->GetTotalDefense();
        vt.isDead = u->IsDead();
        vt.isHidden = u->IsHidden();
        vt.isInvulnerable = u->IsInvulnerable();
        vt.isPaused = u->IsPaused();
        vtPool.Emplace(h, vt);

        count++;
    }
    return count;
}

size_t SimHotComponentSystem::ApplyHotPoolsToUnits(CSimulation& sim) {
    SimEcsRegistry& reg = sim.Registry();
    auto& tfPool = reg.Pool<UnitHotTransformComponent>();
    auto& vtPool = reg.Pool<UnitHotVitalsComponent>();

    size_t updated = 0;
    for (CUnit* u : sim.GetUnits()) {
        if (!u) continue;
        const int h = u->GetHandleId();
        bool touched = false;

        if (const auto* tf = tfPool.TryGet(h)) {
            u->SetPosition(tf->x, tf->y);
            u->SetFacing(tf->facingDeg);
            u->SetFlyingHeight(tf->flyHeight);
            u->SetTurnRate(tf->turnRate);
            u->SetPlayerIndex(tf->playerIndex);
            u->SetPathingEnabled(tf->pathingEnabled);
            touched = true;
        }
        if (const auto* vt = vtPool.TryGet(h)) {
            u->SetLife(sim, vt->life);
            u->SetMana(vt->mana);
            u->SetMaxMana(vt->maxMana);
            u->SetHidden(vt->isHidden);
            u->SetInvulnerable(vt->isInvulnerable);
            u->SetPaused(vt->isPaused);
            touched = true;
        }
        if (touched) updated++;
    }
    sim.SpatialGrid().Rebuild(sim.GetUnits());
    return updated;
}

SimHotFleetMetrics SimHotComponentSystem::ComputeFleetMetricsFromHotPools(const SimEcsRegistry& reg,
                                                                          int playerFilter) {
    SimHotFleetMetrics out;
    const auto* tfPool = reg.TryPool<UnitHotTransformComponent>();
    const auto* vtPool = reg.TryPool<UnitHotVitalsComponent>();
    if (!tfPool || !vtPool) return out;

    const auto& entities = tfPool->Entities();
    const auto& transforms = tfPool->Components();
    double sumX = 0.0;
    double sumY = 0.0;

    for (size_t i = 0; i < transforms.size(); ++i) {
        const UnitHotTransformComponent& tf = transforms[i];
        if (playerFilter >= 0 && tf.playerIndex != playerFilter) continue;
        const int entityId = entities[i];
        const UnitHotVitalsComponent* vt = vtPool->TryGet(entityId);
        if (vt && vt->isDead) continue;

        out.activeUnitCount++;
        sumX += tf.x;
        sumY += tf.y;
        if (vt) {
            out.totalAliveLife += vt->life;
            out.totalMana += vt->mana;
        }
    }

    if (out.activeUnitCount > 0) {
        out.centroidX = static_cast<float>(sumX / static_cast<double>(out.activeUnitCount));
        out.centroidY = static_cast<float>(sumY / static_cast<double>(out.activeUnitCount));
    }
    return out;
}

SimStateSnapshot SimStateSnapshot::Capture(const CSimulation& sim) {
    SimStateSnapshot snap;
    snap.gameTurnTick = sim.gameTurnTick_;
    snap.simRandState = sim.simRandState_;
    snap.combatRandState = sim.combatSystem_.GetRandState();
    snap.gameTimeOfDay = sim.gameTimeOfDay_;
    snap.daytime = sim.daytime_;
    snap.timeOfDaySuspended = sim.timeOfDaySuspended_;
    snap.timeOfDayScale = sim.timeOfDayScale_;
    snap.heroXPSuspended = sim.heroXPSuspended_;
    snap.paused = sim.paused_;

    // Capture players
    snap.players.reserve(sim.players_.size());
    for (const auto& p : sim.players_) {
        if (!p) continue;
        SimPlayerSnapshot ps;
        ps.playerId = p->GetId();
        ps.gold = p->GetGold();
        ps.lumber = p->GetLumber();
        ps.foodUsed = p->GetFoodUsed();
        ps.foodCap = p->GetFoodCap();
        ps.heroTokens = p->GetHeroTokens();
        ps.goldUpkeepRate = p->GetGoldUpkeepRate();
        ps.lumberUpkeepRate = p->GetLumberUpkeepRate();
        snap.players.push_back(ps);
    }

    // Capture units sorted deterministically by handleId
    std::vector<const CUnit*> sortedUnits(sim.units_.begin(), sim.units_.end());
    std::sort(sortedUnits.begin(), sortedUnits.end(), [](const CUnit* a, const CUnit* b) {
        return a->GetHandleId() < b->GetHandleId();
    });
    snap.units.reserve(sortedUnits.size());
    const SimEcsRegistry& reg = sim.Registry();

    for (const CUnit* u : sortedUnits) {
        if (!u) continue;
        const int h = u->GetHandleId();
        SimUnitSnapshot us;
        us.handleId = h;
        us.typeId = u->GetTypeId().value;
        us.playerIndex = u->GetPlayerIndex();
        us.x = u->GetX();
        us.y = u->GetY();
        us.facingDeg = u->GetFacing();
        us.flyHeight = u->GetFlyHeight();
        us.life = u->GetLife();
        us.baseMaxLife = u->GetBaseMaxLife();
        us.bonusMaxLife = u->GetBonusMaxLife();
        us.mana = u->GetMana();
        us.maxMana = u->GetMaxMana();
        us.defense = u->GetDefense();
        us.baseSpeed = u->GetBaseSpeed();
        us.isDead = u->IsDead();
        us.isHidden = u->IsHidden();
        us.isInvulnerable = u->IsInvulnerable();
        us.isPaused = u->IsPaused();
        us.pathingEnabled = u->IsPathingEnabled();
        us.stateFlagsBitmask = u->GetStateFlagsBitmask();

        if (const auto* vc = reg.TryGet<UnitVisualComponent>(h)) {
            us.hasVisualComp = true;
            us.visualComp = *vc;
        }
        if (const auto* cc = reg.TryGet<UnitConstructionComponent>(h)) {
            us.hasConstructionComp = true;
            us.constructionComp = *cc;
        }
        if (const auto* sc = reg.TryGet<UnitStructureNavComponent>(h)) {
            us.hasStructureNavComp = true;
            us.structureNavComp = *sc;
        }
        snap.units.push_back(us);
    }

    // Capture items sorted by handleId
    std::vector<const CItem*> sortedItems(sim.items_.begin(), sim.items_.end());
    std::sort(sortedItems.begin(), sortedItems.end(), [](const CItem* a, const CItem* b) {
        return a->GetHandleId() < b->GetHandleId();
    });
    snap.items.reserve(sortedItems.size());
    for (const CItem* item : sortedItems) {
        if (!item) continue;
        SimItemSnapshot is;
        is.handleId = item->GetHandleId();
        is.typeId = item->GetTypeId().value;
        is.x = item->GetX();
        is.y = item->GetY();
        is.life = item->GetLife();
        is.maxLife = item->GetMaxLife();
        is.charges = item->GetCharges();
        is.hidden = item->IsHidden();
        is.invulnerable = item->IsInvulnerable();
        snap.items.push_back(is);
    }

    // Capture destructables sorted by handleId
    std::vector<const CDestructable*> sortedDests(sim.destructables_.begin(), sim.destructables_.end());
    std::sort(sortedDests.begin(), sortedDests.end(), [](const CDestructable* a, const CDestructable* b) {
        return a->GetHandleId() < b->GetHandleId();
    });
    snap.destructables.reserve(sortedDests.size());
    for (const CDestructable* d : sortedDests) {
        if (!d) continue;
        SimDestructableSnapshot ds;
        ds.handleId = d->GetHandleId();
        ds.typeId = d->GetTypeId().value;
        ds.x = d->GetX();
        ds.y = d->GetY();
        ds.life = d->GetLife();
        ds.maxLife = d->GetMaxLife();
        ds.occluderHeight = d->GetOccluderHeight();
        ds.dead = d->IsDead();
        ds.invulnerable = d->IsInvulnerable();
        snap.destructables.push_back(ds);
    }

    snap.projectiles = sim.projectiles_;
    snap.stateHash = sim.HashState();
    return snap;
}

bool SimStateSnapshot::Restore(CSimulation& sim) const {
    // 1. Cache CUnitType* pointers by typeId from existing units (for mapless unit tests where unitData_ is null)
    std::unordered_map<uint32_t, CUnitType*> knownTypes;
    for (const auto& [h, uPtr] : sim.unitMap_) {
        (void)h;
        if (uPtr && uPtr->GetUnitType()) {
            knownTypes[uPtr->GetTypeId().value] = uPtr->GetUnitType();
        }
    }

    // 2. Restore global simulation clock & RNG streams
    sim.gameTurnTick_ = gameTurnTick;
    sim.simRandState_ = simRandState;
    sim.combatSystem_.SetSeed(combatRandState);
    sim.gameTimeOfDay_ = gameTimeOfDay;
    sim.daytime_ = daytime;
    sim.timeOfDaySuspended_ = timeOfDaySuspended;
    sim.timeOfDayScale_ = timeOfDayScale;
    sim.heroXPSuspended_ = heroXPSuspended;
    sim.paused_ = paused;

    // 3. Restore player resources
    for (const SimPlayerSnapshot& ps : players) {
        if (CPlayer* p = sim.GetPlayer(ps.playerId)) {
            p->SetGold(ps.gold);
            p->SetLumber(ps.lumber);
            p->SetFoodUsed(ps.foodUsed);
            p->SetFoodCap(ps.foodCap);
            p->SetHeroTokens(ps.heroTokens);
            p->SetGoldUpkeepRate(ps.goldUpkeepRate);
            p->SetLumberUpkeepRate(ps.lumberUpkeepRate);
        }
    }

    // 4. Reconcile active units: remove units not in snapshot, update or recreate units in snapshot
    sim.newUnits_.clear();
    sim.removedUnits_.clear();
    sim.hazards_.clear();

    std::unordered_map<int, const SimUnitSnapshot*> targetUnitsByHandle;
    for (const auto& us : units) {
        targetUnitsByHandle[us.handleId] = &us;
    }

    // Remove units from sim.unitMap_ that do not exist in the snapshot
    for (auto it = sim.unitMap_.begin(); it != sim.unitMap_.end();) {
        if (targetUnitsByHandle.find(it->first) == targetUnitsByHandle.end()) {
            const int deadHandle = it->first;
            if (CUnit* u = it->second.get()) {
                if (CPlayer* owner = sim.GetPlayer(u->GetPlayerIndex())) {
                    owner->RemoveUnit(u);
                    owner->RemoveHero(u);
                }
            }
            sim.ecsRegistry_.DestroyEntity(deadHandle);
            it = sim.unitMap_.erase(it);
        } else {
            ++it;
        }
    }

    sim.units_.clear();
    sim.units_.reserve(units.size());

    for (const SimUnitSnapshot& us : units) {
        CUnit* u = nullptr;
        auto it = sim.unitMap_.find(us.handleId);
        if (it != sim.unitMap_.end() && it->second) {
            u = it->second.get();
        } else {
            CUnitType* uType = nullptr;
            auto kt = knownTypes.find(us.typeId);
            if (kt != knownTypes.end()) {
                uType = kt->second;
            }
            auto newUnit = std::make_unique<CUnit>(
                us.handleId, uType, us.playerIndex, War3ID(us.typeId), us.x, us.y, us.facingDeg);
            newUnit->BindRegistry(&sim.ecsRegistry_);
            sim.InstallUnitHooks(newUnit.get());
            u = newUnit.get();
            sim.unitMap_[us.handleId] = std::move(newUnit);
            if (CPlayer* owner = sim.GetPlayer(us.playerIndex)) {
                owner->AddUnit(u);
            }
        }

        u->SetPlayerIndex(us.playerIndex);
        u->SetPosition(us.x, us.y);
        u->SetFacing(us.facingDeg);
        u->SetFlyingHeight(us.flyHeight);
        u->Attributes().SetBaseMaxLife(us.baseMaxLife);
        u->Attributes().SetBaseSpeed(us.baseSpeed);
        u->SetDefense(us.defense);
        u->SetMaxMana(us.maxMana);
        u->SetMana(us.mana);
        
        u->CWidget::SetLife(sim, us.life);
        u->SetStateFlagsBitmask(us.stateFlagsBitmask);

        u->SetHidden(us.isHidden);
        u->SetInvulnerable(us.isInvulnerable);
        u->SetPaused(us.isPaused);
        u->SetPathingEnabled(us.pathingEnabled);


        if (us.hasVisualComp) {
            sim.ecsRegistry_.Emplace<UnitVisualComponent>(us.handleId, us.visualComp);
        } else {
            sim.ecsRegistry_.Remove<UnitVisualComponent>(us.handleId);
        }
        if (us.hasConstructionComp) {
            sim.ecsRegistry_.Emplace<UnitConstructionComponent>(us.handleId, us.constructionComp);
        } else {
            sim.ecsRegistry_.Remove<UnitConstructionComponent>(us.handleId);
        }
        if (us.hasStructureNavComp) {
            sim.ecsRegistry_.Emplace<UnitStructureNavComponent>(us.handleId, us.structureNavComp);
        } else {
            sim.ecsRegistry_.Remove<UnitStructureNavComponent>(us.handleId);
        }

        sim.units_.push_back(u);
    }

    sim.projectiles_ = projectiles;
    sim.spatialGrid_.Rebuild(sim.units_);
    SimHotComponentSystem::SyncUnitsToHotPools(sim);

    return sim.HashState() == stateHash;
}

namespace {

template <typename T>
void AppendPod(std::vector<uint8_t>& buf, const T& val) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

template <typename T>
bool ReadPod(const uint8_t* data, size_t size, size_t& offset, T& outVal) {
    if (offset + sizeof(T) > size) return false;
    std::memcpy(&outVal, data + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

} // namespace

std::vector<uint8_t> SimStateSnapshot::SerializeBinary() const {
    std::vector<uint8_t> out;
    out.reserve(128 + units.size() * sizeof(SimUnitSnapshot) + players.size() * sizeof(SimPlayerSnapshot));

    AppendPod(out, kMagic);
    AppendPod(out, kVersion);
    AppendPod(out, gameTurnTick);
    AppendPod(out, simRandState);
    AppendPod(out, combatRandState);
    AppendPod(out, gameTimeOfDay);
    AppendPod(out, daytime);
    AppendPod(out, timeOfDaySuspended);
    AppendPod(out, timeOfDayScale);
    AppendPod(out, heroXPSuspended);
    AppendPod(out, paused);
    AppendPod(out, stateHash);

    const uint32_t playerCount = static_cast<uint32_t>(players.size());
    AppendPod(out, playerCount);
    for (const auto& p : players) AppendPod(out, p);

    const uint32_t unitCount = static_cast<uint32_t>(units.size());
    AppendPod(out, unitCount);
    for (const auto& u : units) AppendPod(out, u);

    const uint32_t itemCount = static_cast<uint32_t>(items.size());
    AppendPod(out, itemCount);
    for (const auto& it : items) AppendPod(out, it);

    const uint32_t destCount = static_cast<uint32_t>(destructables.size());
    AppendPod(out, destCount);
    for (const auto& d : destructables) AppendPod(out, d);

    const uint32_t projCount = static_cast<uint32_t>(projectiles.size());
    AppendPod(out, projCount);
    for (const auto& pr : projectiles) AppendPod(out, pr);

    return out;
}

bool SimStateSnapshot::DeserializeBinary(const uint8_t* data, size_t size, SimStateSnapshot& out) {
    if (!data || size < 16) return false;
    size_t offset = 0;
    uint32_t magic = 0, version = 0;
    if (!ReadPod(data, size, offset, magic) || magic != kMagic) return false;
    if (!ReadPod(data, size, offset, version) || version != kVersion) return false;

    out = SimStateSnapshot{};
    if (!ReadPod(data, size, offset, out.gameTurnTick)) return false;
    if (!ReadPod(data, size, offset, out.simRandState)) return false;
    if (!ReadPod(data, size, offset, out.combatRandState)) return false;
    if (!ReadPod(data, size, offset, out.gameTimeOfDay)) return false;
    if (!ReadPod(data, size, offset, out.daytime)) return false;
    if (!ReadPod(data, size, offset, out.timeOfDaySuspended)) return false;
    if (!ReadPod(data, size, offset, out.timeOfDayScale)) return false;
    if (!ReadPod(data, size, offset, out.heroXPSuspended)) return false;
    if (!ReadPod(data, size, offset, out.paused)) return false;
    if (!ReadPod(data, size, offset, out.stateHash)) return false;

    uint32_t playerCount = 0;
    if (!ReadPod(data, size, offset, playerCount)) return false;
    out.players.resize(playerCount);
    for (uint32_t i = 0; i < playerCount; ++i) {
        if (!ReadPod(data, size, offset, out.players[i])) return false;
    }

    uint32_t unitCount = 0;
    if (!ReadPod(data, size, offset, unitCount)) return false;
    out.units.resize(unitCount);
    for (uint32_t i = 0; i < unitCount; ++i) {
        if (!ReadPod(data, size, offset, out.units[i])) return false;
    }

    uint32_t itemCount = 0;
    if (!ReadPod(data, size, offset, itemCount)) return false;
    out.items.resize(itemCount);
    for (uint32_t i = 0; i < itemCount; ++i) {
        if (!ReadPod(data, size, offset, out.items[i])) return false;
    }

    uint32_t destCount = 0;
    if (!ReadPod(data, size, offset, destCount)) return false;
    out.destructables.resize(destCount);
    for (uint32_t i = 0; i < destCount; ++i) {
        if (!ReadPod(data, size, offset, out.destructables[i])) return false;
    }

    uint32_t projCount = 0;
    if (!ReadPod(data, size, offset, projCount)) return false;
    out.projectiles.resize(projCount);
    for (uint32_t i = 0; i < projCount; ++i) {
        if (!ReadPod(data, size, offset, out.projectiles[i])) return false;
    }

    return offset == size;
}

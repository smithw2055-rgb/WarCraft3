#include "destructable.h"
#include "csimulation.h"
#include "data/destructabledata.h"
#include "data/itemdata.h"
#include "ecs/pathing/pathfinding.h"
#include "map/wpm.h"

CDestructable::CDestructable(int handleId, War3ID typeId, float x, float y, float life)
    : CWidget(handleId, x, y, life), typeId_(typeId) {}

namespace {

const CDestructableType* LookupType(CSimulation& sim, War3ID id) {
    if (auto* data = sim.GetDestructableData()) return data->GetDestructable(id);
    return nullptr;
}

WpmData* PathingMap(CSimulation& sim) {
    if (auto* pf = sim.GetPathfinding()) {
        if (pf->HasPathingMap()) return pf->GetPathingMap();
    }
    return nullptr;
}

} // namespace

void CDestructable::BindPathing(CSimulation& sim, int rotationDeg) {
    pathingRotationDeg_ = rotationDeg;
    UnbindPathing(sim);
    WpmData* wpm = PathingMap(sim);
    if (!wpm) return;
    const CDestructableType* type = LookupType(sim, typeId_);
    if (!type) return;
    const bool dead = IsDead();
    if (!dead && type->pathTexImage && type->pathTexImage->valid) {
        const bool blocksVision = type->occlusionHeight > 0.0f;
        pathingStampId_ = wpm->StampFootprint(GetX(), GetY(), pathingRotationDeg_,
                                              type->pathTexImage, blocksVision);
    } else if (dead && type->pathTexDeathImage && type->pathTexDeathImage->valid) {
        deathStampId_ = wpm->StampFootprint(GetX(), GetY(), pathingRotationDeg_,
                                            type->pathTexDeathImage, false);
    }
}

void CDestructable::RefreshPathingAfterLifeChange(CSimulation& sim) {
    WpmData* wpm = PathingMap(sim);
    const CDestructableType* type = LookupType(sim, typeId_);
    if (!wpm || !type) return;
    if (IsDead()) {
        // Retail kill(): drop the live footprint (and its vision block),
        // then raise the death footprint.
        if (pathingStampId_ >= 0) {
            wpm->UnstampFootprint(pathingStampId_);
            pathingStampId_ = -1;
        }
        if (deathStampId_ < 0 && type->pathTexDeathImage && type->pathTexDeathImage->valid) {
            deathStampId_ = wpm->StampFootprint(GetX(), GetY(), pathingRotationDeg_,
                                                type->pathTexDeathImage, false);
        }
    } else {
        // Retail revive (setLife back over zero): drop death, raise live.
        if (deathStampId_ >= 0) {
            wpm->UnstampFootprint(deathStampId_);
            deathStampId_ = -1;
        }
        if (pathingStampId_ < 0 && type->pathTexImage && type->pathTexImage->valid) {
            const bool blocksVision = type->occlusionHeight > 0.0f;
            pathingStampId_ = wpm->StampFootprint(GetX(), GetY(), pathingRotationDeg_,
                                                  type->pathTexImage, blocksVision);
        }
    }
}

void CDestructable::UnbindPathing(CSimulation& sim) {
    if (pathingStampId_ < 0 && deathStampId_ < 0) return;
    if (WpmData* wpm = PathingMap(sim)) {
        if (pathingStampId_ >= 0) wpm->UnstampFootprint(pathingStampId_);
        if (deathStampId_ >= 0) wpm->UnstampFootprint(deathStampId_);
    }
    pathingStampId_ = -1;
    deathStampId_ = -1;
}

void CDestructable::SetLife(CSimulation& simulation, float life) {
    const bool wasDead = IsDead();
    CWidget::SetLife(simulation, life);
    if (IsDead() == wasDead) return;
    RefreshPathingAfterLifeChange(simulation);
    // The killing blow (damage or script setLife) drops items immediately:
    // destructables have no corpse/decay ticks, so the transition itself is
    // the death moment (ref kill() side of setLife).
    if (IsDead()) DropDeathItems(simulation);
}

void CDestructable::DropDeathItems(CSimulation& simulation) {
    if (deathDrops_.empty()) return;
    for (const auto& drop : deathDrops_) {
        if (drop.itemId.value == 0 || drop.chancePercent <= 0) continue;
        if (drop.chancePercent < 100 &&
            simulation.SimRand01() * 100.0f >= (float)drop.chancePercent) {
            continue;
        }
        auto* itemData = simulation.GetItemData();
        if (!itemData || !itemData->HasItem(drop.itemId)) continue;
        simulation.CreateItem(drop.itemId, GetX(), GetY());
    }
}

float CDestructable::Damage(CSimulation& simulation, CUnit* source,
                             bool isAttack, bool isRanged,
                             CAttackType attackType, CDamageType damageType,
                             const std::string& weaponSoundType,
                             float damage, float bonusDamage) {
    if (invulnerable_) return 0;
    const bool wasDead = IsDead();
    float oldLife = life_;
    life_ -= (damage + bonusDamage);
    if (life_ < 0) life_ = 0;
    (void)source; (void)isAttack; (void)isRanged; (void)attackType;
    (void)damageType; (void)weaponSoundType;
    if (IsDead() != wasDead) {
        RefreshPathingAfterLifeChange(simulation);
        if (IsDead()) DropDeathItems(simulation);
    }
    return oldLife - life_;
}

bool CDestructable::CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                                     const std::vector<CTargetType>& targetsAllowed) const {
    (void)simulation; (void)source;
    // Ref CDestructable.canBeTargetedBy: the allowed set must contain every
    // targetedAs entry (tree/debris/wall/bridge fine branches collapse to
    // false here; the bool API carries no error receiver). Dead bodies need
    // DEAD; living ones pass unless the ability targets corpses only (ref
    // ALIVE check reduces to !DEAD: no C++ ALIVE enumerator exists).
    const CDestructableType* type = LookupType(simulation, typeId_);
    if (type) {
        for (CTargetType need : type->targetedAs) {
            bool found = false;
            for (CTargetType have : targetsAllowed) {
                if (have == need) { found = true; break; }
            }
            if (!found) return false;
        }
    }
    if (IsDead()) {
        for (CTargetType have : targetsAllowed) {
            if (have == CTargetType::DEAD) return true;
        }
        return false;
    }
    for (CTargetType have : targetsAllowed) {
        if (have == CTargetType::DEAD) return false;
    }
    return true;
}

#include "destructable.h"
#include "csimulation.h"
#include "data/destructabledata.h"
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
    if (IsDead() != wasDead) RefreshPathingAfterLifeChange(simulation);
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
    if (IsDead() != wasDead) RefreshPathingAfterLifeChange(simulation);
    return oldLife - life_;
}

bool CDestructable::CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                                     const std::vector<CTargetType>& targetsAllowed) const {
    (void)simulation; (void)source; (void)targetsAllowed;
    return !IsDead();
}

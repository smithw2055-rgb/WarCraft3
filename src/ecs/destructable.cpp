#include "destructable.h"

CDestructable::CDestructable(int handleId, War3ID typeId, float x, float y, float life)
    : CWidget(handleId, x, y, life), typeId_(typeId) {}

float CDestructable::Damage(CSimulation& simulation, CUnit* source,
                             bool isAttack, bool isRanged,
                             CAttackType attackType, CDamageType damageType,
                             const std::string& weaponSoundType,
                             float damage, float bonusDamage) {
    if (invulnerable_) return 0;
    float oldLife = life_;
    life_ -= (damage + bonusDamage);
    if (life_ < 0) life_ = 0;
    (void)source; (void)isAttack; (void)isRanged; (void)attackType;
    (void)damageType; (void)weaponSoundType;
    return oldLife - life_;
}

bool CDestructable::CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                                     const std::vector<CTargetType>& targetsAllowed) const {
    (void)simulation; (void)source; (void)targetsAllowed;
    return !IsDead();
}

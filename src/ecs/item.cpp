#include "item.h"

CItem::CItem(int handleId, War3ID typeId, float x, float y)
    : CWidget(handleId, x, y, 50.0f), typeId_(typeId) {}

bool CItem::CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                             const std::vector<CTargetType>& targetsAllowed) const {
    (void)simulation; (void)source; (void)targetsAllowed;
    return !IsDead();
}

float CItem::Damage(CSimulation& simulation, CUnit* source,
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

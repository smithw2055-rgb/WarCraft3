#pragma once
#include "ecs/widget.h"
#include "resource/slk.h"

class CDestructable : public CWidget {
public:
    CDestructable(int handleId, War3ID typeId, float x, float y, float life);

    War3ID GetTypeId() const { return typeId_; }
    float GetImpactZ() const override { return 0; }
    bool IsInvulnerable() const override { return invulnerable_; }

    float Damage(CSimulation& simulation, CUnit* source,
                 bool isAttack, bool isRanged,
                 CAttackType attackType, CDamageType damageType,
                 const std::string& weaponSoundType,
                 float damage, float bonusDamage = 0) override;

    bool CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                         const std::vector<CTargetType>& targetsAllowed) const override;

private:
    War3ID typeId_;
    bool invulnerable_ = false;
};

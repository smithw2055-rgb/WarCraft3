#pragma once
#include "ecs/buff.h"

// Slow debuff - reduces movement speed and attack speed
class CBuffSlow : public CBuff {
public:
    CBuffSlow(float duration, float slowPercent, int casterHandle = -1);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float slowPercent_;
    int originalSpeed_ = 0;
};

// Poison debuff - periodic damage
class CBuffPoison : public CBuff {
public:
    CBuffPoison(float duration, float damagePerTick, int casterHandle = -1);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
};

// Bloodlust buff - increases attack speed and movement speed
class CBuffBloodlust : public CBuff {
public:
    CBuffBloodlust(float duration, float speedBonus, float attackSpeedBonus);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float speedBonus_;
    float attackSpeedBonus_;
};

// Thorns aura buff - reflects melee damage
class CBuffThorns : public CBuff {
public:
    CBuffThorns(float duration, float reflectPercent);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float reflectPercent_;
};

// Divine Shield buff - invulnerable
class CBuffDivineShield : public CBuff {
public:
    CBuffDivineShield(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Stun debuff - cannot act
class CBuffStun : public CBuff {
public:
    CBuffStun(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Rejuvenation buff - periodic heal
class CBuffRejuvenation : public CBuff {
public:
    CBuffRejuvenation(float duration, float healPerTick);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float healPerTick_;
};

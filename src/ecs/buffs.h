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
    int appliedReduction_ = 0;
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
    int appliedSpeedBonus_ = 0;
};

// Thorns aura buff - reflects melee damage
class CBuffThorns : public CBuff {
public:
    CBuffThorns(float duration, float reflectPercent, War3ID id = War3ID("Bthr"));

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
    float GetReflectPercent() const { return reflectPercent_; }

private:
    float reflectPercent_;
};

// Vampiric aura buff - heals the attacker for a share of attack damage
// (ref CUnitDefaultLifestealListener; buff id BUav from object data).
class CBuffLifesteal : public CBuff {
public:
    CBuffLifesteal(float duration, float stealPercent);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
    float GetStealPercent() const { return stealPercent_; }

private:
    float stealPercent_;
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

// Sleep debuff - like stun, but any damage wakes the unit (ref sleep).
class CBuffSleep : public CBuff {
public:
    CBuffSleep(float duration);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;
};

// Silence debuff - cannot cast spells, but can move and attack (ref).
// Pure marker buff; casting is blocked in CBehaviorSpell.
class CBuffSilence : public CBuff {
public:
    CBuffSilence(float duration);

    CBuff* Clone() const override;
};

// Hex debuff - cannot attack or cast, but can still move (ref).
// Pure marker buff; attack/spell behaviors check IsHexed().
class CBuffHex : public CBuff {
public:
    CBuffHex(float duration);

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

// Timed life buff - marks a summoned unit and kills it on expiry.
// (ref CBuffTimedLife: summon spells attach this instead of a unit field;
// isTimedLifeBar() drives the blue dissipate bar in the renderer.)
class CBuffTimedLife : public CBuff {
public:
    CBuffTimedLife(War3ID alias, float duration);

    void OnRemove(CSimulation& game, CUnit* target) override;
    bool IsTimedLifeBar() const { return true; }
    CBuff* Clone() const override;
};

// Life drain channel - periodic damage to the bearer, healing the caster.
// (ref Life Drain: channelled, not the instant drain previously used.)
class CBuffLifeDrain : public CBuff {
public:
    CBuffLifeDrain(float duration, float drainPerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float drainPerTick_;
};

// Armor modifier - flat bonus armor while held (Frost Armor); negative
// values model armor-reducing curses (Faerie Fire).
class CBuffArmor : public CBuff {
public:
    CBuffArmor(War3ID id, float duration, int armorDelta, bool positive);

    void OnApply(CSimulation& game, CUnit* target) override;
    void OnRemove(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    int armorDelta_;
};

// Doom - periodic damage; if the bearer dies while doomed, a Doom Guard
// (nba2, ref UnitID1 of ANdo) rises for the caster's player.
class CBuffDoom : public CBuff {
public:
    CBuffDoom(float duration, float damagePerTick, int casterHandle);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    void OnDeath(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
};

// Lightning Shield - periodic Spiral damage to the bearer's enemies.
// (ref Lightning Shield: the bearer is spared here; retail also zaps
// the shielded unit itself, a visual-side approximation.)
class CBuffLightningShield : public CBuff {
public:
    CBuffLightningShield(float duration, float damagePerTick, float radius);

    void OnPeriodic(CSimulation& game, CUnit* target) override;
    CBuff* Clone() const override;

private:
    float damagePerTick_;
    float radius_;
};

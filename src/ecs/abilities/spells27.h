#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 27: state passives and the remaining unit/item actives that sit on
// existing flags (ethereal, magic immune, invisibility, damage multipliers).
// Economy, pairing, cargo, and waygates live in spells28.

struct CAbilityDefFull;

enum class CPassiveState {
    Ethereal,     // Aetl
    MagicImmune,  // Amim
    Resistant,    // Arsk
    Ghost         // Agho: invisible and no collision
};

// Permanent state while the ability is on the unit.
class CAbilityStatePassive : public CAbility {
public:
    CAbilityStatePassive(int handleId, War3ID code, CPassiveState state);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;

private:
    CPassiveState state_;
    float savedCollision_ = 0.0f;
    bool applied_ = false;
};

// Runed Bracers (AIsr). DataA is ALLATK, DataB is spell-damage reduction,
// so spells taken are multiplied by (1 - DataB).
class CAbilityItemSpellResist : public CAbilitySpellBase {
public:
    explicit CAbilityItemSpellResist(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    float attackBonus_ = 0.0f;
    float spellReduction_ = 0.33f;
    int appliedAttack_ = 0;
};

// reactions.json AIdd: DataB added to attack damage dealt, DataE multiplies
// magic and spell damage taken, DataA multiplies pierce taken.
// Pierce projectile reflection is the BIdd missile bounce in CProjectile.
class CAbilityItemMixedDefense : public CAbilitySpellBase {
public:
    explicit CAbilityItemMixedDefense(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    float pierceTaken_ = 0.80f;
    float dealtAdd_ = 0.20f;
    float spellTaken_ = 0.80f;
};

// Magic Defense (Amdf). Toggle: magic immunity and a slower attack.
class CAbilityMagicDefense : public CAbilitySpellBase {
public:
    explicit CAbilityMagicDefense(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    void TurnOff(CSimulation& game, CUnit* unit);
    float speedPenalty_ = 0.50f;
    float appliedFactor_ = 1.0f;
    bool active_ = false;
};

// Magic Leash (Amls). Stuns the target and deals DataA each second.
class CAbilityMagicLeash : public CAbilitySpellBase {
public:
    explicit CAbilityMagicLeash(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    float damagePerSecond_ = 10.0f;
    float duration_ = 10.0f;
};

// Replenish life (Arpl) or mana (Arpm) for wounded allies in the area.
class CAbilityReplenish : public CAbilitySpellBase {
public:
    CAbilityReplenish(int handleId, War3ID code, bool restoreLife);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    bool restoreLife_;
    float amount_ = 10.0f;
    float radius_ = 400.0f;
};

// Sphere (Asph) is attachment art. The ability exists so the code resolves.
class CAbilitySphere : public CAbility {
public:
    explicit CAbilitySphere(int handleId);
};

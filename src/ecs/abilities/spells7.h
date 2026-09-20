#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 7: common unit abilities (rawcodes validated against retail
// Units\AbilityData.slk): serpent ward summon, polymorph, inner fire,
// priest heal, dispel magic, rejuvenation, roar, defend.

// ===== Serpent Ward (Shadow Hunter, AOsw) - summon a ward =====
// Level-1 ward is osp1 (ref UnitData); wards stand until killed.
class CAbilitySerpentWard : public CAbilitySpellBase {
public:
    CAbilitySerpentWard(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    War3ID wardType_ = War3ID("osp1");
};

// ===== Polymorph (Sorceress, Aply) - sheep an enemy =====
// Reuses the Hex behavior (no attack/cast, can move) under its own id.
class CAbilityPolymorph : public CAbilitySpellBase {
public:
    CAbilityPolymorph(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 60.0f;
};

// ===== Inner Fire (Priest, Ainf) - timed damage + armor =====
class CAbilityInnerFire : public CAbilitySpellBase {
public:
    CAbilityInnerFire(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 30.0f;
    int bonusDamage_ = 5;
    int bonusArmor_ = 3;
};

// ===== Heal (Priest, Ahea) - single-target heal =====
class CAbilityHeal : public CAbilitySpellBase {
public:
    CAbilityHeal(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float healAmount_ = 200.0f;
};

// ===== Dispel Magic (Priest, Adis) - strip buffs in an area =====
class CAbilityDispelMagic : public CAbilitySpellBase {
public:
    CAbilityDispelMagic(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float radius_ = 250.0f; // ref Area1
    float summonDamage_ = 250.0f; // ref DataB level 1
};

// ===== Rejuvenation (Druid of the Claw, Arej) - heal over time =====
class CAbilityRejuvenation : public CAbilitySpellBase {
public:
    CAbilityRejuvenation(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 12.0f;
    float healPerTick_ = 10.0f;
};

// ===== Roar (Druid of the Claw, Aroa) - rally nearby allies =====
// Implemented as Inner Fire with zero armor on each ally in radius.
class CAbilityRoar : public CAbilitySpellBase {
public:
    CAbilityRoar(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 20.0f;
    int bonusDamage_ = 5;
    float radius_ = 50.0f; // ref Area1 (tight rally, not an aura)
};

// ===== Defend (Footman/Knight, Adef) - halve weapon damage taken =====
// Timed-buff approximation of the defend toggle; the reduction hook
// lives in CUnit::Damage.
class CAbilityDefend : public CAbilitySpellBase {
public:
    CAbilityDefend(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 30.0f;
};

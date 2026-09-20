#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 16 (abilities): Kodo Devour (+cargo digest), Devour Magic,
// Cold/Frost Arrows, Monsoon, Absorb Mana, Unholy Frenzy. Item->ability
// and unit-ability codes validated against retail SLKs.

// ===== Devour (Kodo Beast, Adev) - swallow a unit whole =====
// The victim rides hidden in cargo taking digest damage (DataA 5 dps)
// until dead; no release (ref). Ground non-hero enemies only.
class CAbilityDevour : public CAbilitySpellBase {
public:
    CAbilityDevour(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float digestPerTick_ = 5.0f;
    float digestDuration_ = 3600.0f; // session-long: no release
};

// ===== Devour Magic (Satyr, Advm) - dispel for mana =====
class CAbilityDevourMagic : public CAbilitySpellBase {
public:
    CAbilityDevourMagic(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float manaPerBuff_ = 50.0f;
};

// ===== Cold Arrows (AHca) - chilling poison arrows =====
// Sea Witch Frost Arrows (ANfa) share this base (ref X2 code column).
// Toggle mana cost is behavior-side; the proc lives in Damage.
class CAbilityColdArrows : public CAbilitySpellBase {
public:
    CAbilityColdArrows(int handleId);
    float GetSlowPercent() const { return slowPercent_; }
    float GetSlowDuration() const { return slowDuration_; }
    float GetPoisonPerTick() const { return poisonPerTick_; }
    float GetPoisonDuration() const { return poisonDuration_; }
private:
    float slowPercent_ = 0.3f;
    float slowDuration_ = 3.0f;
    float poisonPerTick_ = 5.0f;
    float poisonDuration_ = 6.0f;
};

// ===== Monsoon (ANmo) - raining waves =====
// Certain fields from retail (Cool 10, Cost 75, Area 300, Rng 500);
// damage is a documented guess.
class CAbilityMonsoon : public CAbilitySpellBase {
public:
    CAbilityMonsoon(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float damagePerWave_ = 50.0f;
    float radius_ = 300.0f;
    int waves_ = 4;
};

// ===== Absorb Mana (Obsidian Destroyer, Aabs) - pop a summon =====
class CAbilityAbsorbMana : public CAbilitySpellBase {
public:
    CAbilityAbsorbMana(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float manaGain_ = 100.0f;
};

// ===== Unholy Frenzy (Necromancer, Auhf) - raging haste =====
// Reuses the Berserk buff without the weak-skin half (documented
// approximation; retail trades life, not armor).
class CAbilityUnholyFrenzy : public CAbilitySpellBase {
public:
    CAbilityUnholyFrenzy(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 30.0f;
    float attackSpeedMul_ = 1.5f;
};

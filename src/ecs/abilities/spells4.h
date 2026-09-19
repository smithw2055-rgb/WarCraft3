#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 4 hero spells: passives via OnAdd/OnRemove, single-target curses,
// sacrifice, mass animation, doom, eruption waves.

// ===== Evasion (Demon Hunter, AEev) - passive dodge =====
class CAbilityEvasion : public CAbilitySpellBase {
public:
    CAbilityEvasion(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
private:
    float chance_ = 0.15f; // level-1 value; per-level tables are data work
};

// ===== Critical Strike (Blade Master, AOcr) - passive crit =====
class CAbilityCriticalStrike : public CAbilitySpellBase {
public:
    CAbilityCriticalStrike(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
private:
    float chance_ = 0.15f;
    float multiplier_ = 2.0f;
};

// ===== Bash (Mountain King, AHbh) - stun on attack landed =====
// Proc odds live here; the trigger sits in CUnit::Damage next to the
// thorns/lifesteal listeners (ref attack listeners).
class CAbilityBash : public CAbilitySpellBase {
public:
    CAbilityBash(int handleId);
    float GetChance() const { return chance_; }
    float GetDuration() const { return duration_; }
    void SetChance(float v) { chance_ = v; }
    void SetDuration(float v) { duration_ = v; }
private:
    float chance_ = 0.15f;
    float duration_ = 2.0f;
};

// ===== Slow (Sorceress, Aslo) - slow a unit =====
class CAbilitySlow : public CAbilitySpellBase {
public:
    CAbilitySlow(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 10.0f;
    float slowPercent_ = 0.3f;
};

// ===== Frost Armor (Lich, AUfa) - armor buff =====
class CAbilityFrostArmor : public CAbilitySpellBase {
public:
    CAbilityFrostArmor(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 30.0f;
    int armorBonus_ = 3;
};

// ===== Faerie Fire (Druid of the Talon, Afae) - armor curse =====
class CAbilityFaerieFire : public CAbilitySpellBase {
public:
    CAbilityFaerieFire(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 20.0f;
    int armorPenalty_ = -2;
    // Reveal-invisible half has no invisibility system to counter yet.
};

// ===== Death Pact (Death Knight, AUdp) - sacrifice for health =====
class CAbilityDeathPact : public CAbilitySpellBase {
public:
    CAbilityDeathPact(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
};

// ===== Animate Dead (Death Knight, AUan) - raise invulnerable dead =====
class CAbilityAnimateDead : public CAbilitySpellBase {
public:
    CAbilityAnimateDead(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float radius_ = 400.0f;
    float duration_ = 120.0f;
};

// ===== Doom (Pit Lord, ANdo) - damage over time, guard on death =====
class CAbilityDoom : public CAbilitySpellBase {
public:
    CAbilityDoom(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 20.0f;
    float damagePerSecond_ = 25.0f;
};

// ===== Volcano (Firelord, ANvc) - eruption damage waves =====
class CAbilityVolcano : public CAbilitySpellBase {
public:
    CAbilityVolcano(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float damage_ = 75.0f;
    float radius_ = 250.0f;
    int waves_ = 6;
};

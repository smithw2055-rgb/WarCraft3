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
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float chance_ = 0.10f; // ref DataA level 1
};

// ===== Critical Strike (Blade Master, AOcr) - passive crit =====
class CAbilityCriticalStrike : public CAbilitySpellBase {
public:
    CAbilityCriticalStrike(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float chance_ = 0.15f; // ref DataA level 1
    float multiplier_ = 2.0f; // ref DataB level 1
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
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float chance_ = 0.20f; // ref DataA level 1 (percent / 100)
    float duration_ = 2.0f; // ref Dur1
};

// ===== Slow (Sorceress, Aslo) - slow a unit =====
class CAbilitySlow : public CAbilitySpellBase {
public:
    CAbilitySlow(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 60.0f;
    float slowPercent_ = 0.6f;
};

// ===== Frost Armor (Lich, AUfa) - armor buff =====
class CAbilityFrostArmor : public CAbilitySpellBase {
public:
    CAbilityFrostArmor(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 5.0f; // ref Dur1 (spammed on a 2s cooldown)
    int armorBonus_ = 3; // ref DataB level 1
};

// ===== Faerie Fire (Druid of the Talon, Afae) - armor curse =====
class CAbilityFaerieFire : public CAbilitySpellBase {
public:
    CAbilityFaerieFire(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 90.0f; // ref Dur1
    int armorPenalty_ = -4; // ref DataA level 1
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
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 120.0f; // ref DataC level 1
    float damagePerSecond_ = 40.0f; // ref DataA level 1
};

// ===== Volcano (Firelord, ANvc) - eruption damage waves =====
class CAbilityVolcano : public CAbilitySpellBase {
public:
    CAbilityVolcano(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 100.0f; // ref DataE level 1
    float radius_ = 500.0f; // ref Area1
    int waves_ = 6; // wave count undocumented; scatter model guess
};

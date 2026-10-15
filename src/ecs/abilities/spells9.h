#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 9: orb attack modifiers (item->ability mapping from retail
// Units\ItemData.slk abilList) plus wand/scroll reuse of existing
// mechanics. Orbs install CAttackModifierEffects (one per attack, no
// stacking); CUnit::FireAttackModifiers runs the slot after a landed hit.

// ===== Orb of Frost (ofro -> AIob) - chilling attacks =====
// Installs a frost modifier into the attacker-side slot 0; the proc runs
// via CUnit::FireAttackModifiers, never hardcoded in Damage.
class CAbilityOrbOfFrost : public CAbilitySpellBase {
public:
    CAbilityOrbOfFrost(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetSlowPercent() const { return slowPercent_; }
    float GetDuration() const { return duration_; }
private:
    float slowPercent_ = 0.3f;
    float duration_ = 3.0f;
};

// ===== Orb of Corruption (ocor -> AIcb) - armor-stripping attacks =====
class CAbilityOrbOfCorruption : public CAbilitySpellBase {
public:
    CAbilityOrbOfCorruption(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetDuration() const { return duration_; }
    int GetArmorPenalty() const { return armorPenalty_; }
private:
    float duration_ = 5.0f;
    int armorPenalty_ = -3;
};

// ===== Orb of Fire (ofir -> AIfb) - bonus fire damage on attack =====
class CAbilityOrbOfFire : public CAbilitySpellBase {
public:
    CAbilityOrbOfFire(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetBonusDamage() const { return bonusDamage_; }
private:
    float bonusDamage_ = 15.0f;
};

// ===== Wand of Illusion (will -> AIil) - illusion of a target =====
class CAbilityIllusion : public CAbilitySpellBase {
public:
    CAbilityIllusion(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 60.0f;
};

// ===== Wand of Lightning Shield (wlsd -> AIls) =====
class CAbilityLightningShieldWand : public CAbilitySpellBase {
public:
    CAbilityLightningShieldWand(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 20.0f;
    float damagePerTick_ = 20.0f;
    float radius_ = 200.0f;
};

// ===== Scroll of the Beast (sror -> AIrr) - rally nearby allies =====
// Damage-only Inner Fire on each ally in radius (cf. Roar).
class CAbilityBeastScroll : public CAbilitySpellBase {
public:
    CAbilityBeastScroll(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 30.0f;
    int bonusDamage_ = 4;
    float radius_ = 300.0f;
};

// ===== Healing Wards (whwd -> AIhw) - place a healing ward =====
// Ward unit is ohwd (ref UnitData); the heal aura lasts 120s while the
// ward itself stands until killed.
class CAbilityHealingWard : public CAbilitySpellBase {
public:
    CAbilityHealingWard(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    War3ID wardType_ = War3ID("ohwd");
    float healPerTick_ = 10.0f;
    float auraDuration_ = 120.0f;
    float radius_ = 200.0f;
};

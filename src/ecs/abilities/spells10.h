#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 10: attack passives (cleave, searing arrows), ensnare root,
// ancestral spirit, witch-doctor healing ward, unstable concoction,
// abolish magic, cluster rockets. Rawcodes validated against retail
// Units\AbilityData.slk.

// ===== Cleaving Attack (Pit Lord, ANca) - splash on attack =====
// The splash hook lives in CUnit::Damage next to the modifier slot.
// The proc itself is a CAttackModifierEffect (see unit_attack.h).
class CAbilityCleavingAttack : public CAbilitySpellBase {
public:
    CAbilityCleavingAttack(int handleId);
    float GetSplashFraction() const override { return splashFraction_; }
    float GetSplashRadius() const override { return splashRadius_; }
private:
    float splashFraction_ = 0.5f;
    float splashRadius_ = 200.0f;
};

// ===== Searing Arrows (Priestess, AHfa) - bonus fire on attack =====
// Toggle mana cost is behavior-side; the proc is a slot effect.
class CAbilitySearingArrows : public CAbilitySpellBase {
public:
    CAbilitySearingArrows(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetBonusDamage() const { return bonusDamage_; }
private:
    float bonusDamage_ = 20.0f;
};

// ===== Ensnare (Raider, Aens) - root a unit =====
class CAbilityEnsnare : public CAbilitySpellBase {
public:
    CAbilityEnsnare(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 6.0f;
};

// ===== Ancestral Spirit (Spirit Walker, Aast) - revive a Tauren =====
// Only Tauren dead (otau) and Tauren Chieftain heroes (Otch) answer;
// the risen fights for the caster (cf. Charm ownership transfer).
class CAbilityAncestralSpirit : public CAbilitySpellBase {
public:
    CAbilityAncestralSpirit(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
};

// ===== Healing Ward (Witch Doctor, Ahwd) - place a healing ward =====
class CAbilityHealingWardUnit : public CAbilitySpellBase {
public:
    CAbilityHealingWardUnit(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    War3ID wardType_ = War3ID("ohwd");
    float healPerTick_ = 10.0f;
    float auraDuration_ = 120.0f;
    float radius_ = 200.0f;
};

// ===== Unstable Concoction (Batrider, Auco) - explode, self included =====
// The arm-then-detonate delay is folded into an instant blast here.
class CAbilityUnstableConcoction : public CAbilitySpellBase {
public:
    CAbilityUnstableConcoction(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float damage_ = 150.0f;
    float selfDamage_ = 100.0f;
    float radius_ = 250.0f;
};

// ===== Abolish Magic (Dryad, Aadm) - strip buffs in an area =====
class CAbilityAbolishMagic : public CAbilitySpellBase {
public:
    CAbilityAbolishMagic(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float radius_ = 200.0f;
};

// ===== Cluster Rockets (Tinker, ANcs) - two rocket clusters =====
class CAbilityClusterRockets : public CAbilitySpellBase {
public:
    CAbilityClusterRockets(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float damage_ = 75.0f;
    float radius_ = 150.0f;
};

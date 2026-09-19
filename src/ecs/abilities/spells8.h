#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 8: charged-item abilities (item->ability mapping validated
// against retail Units\ItemData.slk abilList column). All compose
// existing primitives; per-level values are approximations.

// ===== Scroll of Healing (shea -> AIha) - area heal =====
class CAbilityItemHealGroup : public CAbilitySpellBase {
public:
    CAbilityItemHealGroup(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float healAmount_ = 150.0f;
    float radius_ = 300.0f;
};

// ===== Scroll of Town Portal (stwp -> AItp) - group teleport =====
// Retail restricts the destination to town halls (UI-side); the
// movement itself matches Mass Teleport.
class CAbilityItemTownPortal : public CAbilitySpellBase {
public:
    CAbilityItemTownPortal(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float radius_ = 400.0f;
};

// ===== Potion of Lesser Invulnerability (pnvl -> AIvl) =====
class CAbilityItemInvulnerable : public CAbilitySpellBase {
public:
    CAbilityItemInvulnerable(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 5.0f;
};

// ===== Potion of Healing (phea -> AIh1) - self heal =====
class CAbilityItemHealSelf : public CAbilitySpellBase {
public:
    CAbilityItemHealSelf(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float healAmount_ = 400.0f;
};

// ===== Potion of Mana (pman -> AIm1) - self mana =====
class CAbilityItemManaSelf : public CAbilitySpellBase {
public:
    CAbilityItemManaSelf(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float manaAmount_ = 200.0f;
};

// ===== Lesser Clarity Potion (plcl -> AIpl) - mana regen =====
class CAbilityItemClarity : public CAbilitySpellBase {
public:
    CAbilityItemClarity(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 30.0f;
    float regenBonus_ = 1.0f;
};

// ===== Wand of Mana Stealing (woms -> Aste) - drain mana =====
class CAbilityItemManaSteal : public CAbilitySpellBase {
public:
    CAbilityItemManaSteal(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float stealAmount_ = 100.0f;
};

// ===== Scroll of Protection (spro -> AIda) - self armor =====
class CAbilityItemProtection : public CAbilitySpellBase {
public:
    CAbilityItemProtection(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 30.0f;
    int armorBonus_ = 3;
};

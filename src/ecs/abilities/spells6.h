#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 6: Goblin Alchemist kit (rawcodes ANhs/ANab/ANcr/ANtm, ref
// Units\AbilityData.slk) plus common unit abilities (Bloodlust Ablo,
// Purge Aprg, Lightning Shield Alsh, Raise Dead Arai, same source).

// ===== Healing Spray (Alchemist) - area heal for allies =====
class CAbilityHealingSpray : public CAbilitySpellBase {
public:
    CAbilityHealingSpray(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float healAmount_ = 100.0f;
    float radius_ = 200.0f;
};

// ===== Acid Bomb (Alchemist) - armor curse + poison =====
class CAbilityAcidBomb : public CAbilitySpellBase {
public:
    CAbilityAcidBomb(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 15.0f;
    int armorPenalty_ = -4;
    float poisonPerTick_ = 8.0f;
};

// ===== Chemical Rage (Alchemist) - self haste =====
class CAbilityChemicalRage : public CAbilitySpellBase {
public:
    CAbilityChemicalRage(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 20.0f;
    float speedBonus_ = 0.3f;
    float attackSpeedBonus_ = 0.4f;
};

// ===== Transmute (Alchemist) - kill an enemy for gold =====
class CAbilityTransmute : public CAbilitySpellBase {
public:
    CAbilityTransmute(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    int goldYield_ = 200;
};

// ===== Bloodlust (Shaman) - haste an ally =====
class CAbilityBloodlustCast : public CAbilitySpellBase {
public:
    CAbilityBloodlustCast(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 60.0f; // ref Dur1
    float speedBonus_ = 0.4f; // ref DataA level 1
    float attackSpeedBonus_ = 0.25f; // ref DataB level 1
};

// ===== Purge (Shaman) - dispel magic, slow, burn summons =====
class CAbilityPurge : public CAbilitySpellBase {
public:
    CAbilityPurge(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float slowPercent_ = 0.5f;
    float slowDuration_ = 4.0f;
    float summonDamage_ = 200.0f;
};

// ===== Lightning Shield (Shaman) - shocking aura on a unit =====
class CAbilityLightningShield : public CAbilitySpellBase {
public:
    CAbilityLightningShield(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 20.0f;
    float damagePerTick_ = 20.0f;
    float radius_ = 200.0f;
};

// ===== Raise Dead (Necromancer) - skeletons from nearby corpses =====
class CAbilityRaiseDead : public CAbilitySpellBase {
public:
    CAbilityRaiseDead(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID skeletonType_ = War3ID("uske"); // Skeleton Warrior, ref UnitData
    float radius_ = 500.0f;
    int maxTargets_ = 2; // ref DataA level 1
};

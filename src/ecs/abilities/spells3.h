#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 3 hero spells. All effects compose existing primitives (timed
// summons, area damage/heal chains, buffs, revive, ownership transfer);
// documented approximations are noted per class.

// ===== Mass Teleport (Archmage) - teleport friendly units in area =====
class CAbilityMassTeleport : public CAbilitySpellBase {
public:
    CAbilityMassTeleport(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float radius_ = 400.0f;
};

// ===== Summon Water Elemental (Archmage) - timed summon =====
class CAbilitySummonWaterElemental : public CAbilitySpellBase {
public:
    CAbilitySummonWaterElemental(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    War3ID elementalType_ = War3ID("hwat");
    int count_ = 1;
    float summonDuration_ = 60.0f;
};

// ===== Resurrect (Paladin) - revive dead units in area =====
class CAbilityResurrect : public CAbilitySpellBase {
public:
    CAbilityResurrect(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float radius_ = 400.0f;
    int maxTargets_ = 6;
};

// ===== Blink (Warden) - short teleport =====
class CAbilityBlink : public CAbilitySpellBase {
public:
    CAbilityBlink(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float maxRange_ = 1000.0f;
};

// ===== Mirror Image (Blademaster) - timed copies of the caster =====
class CAbilityMirrorImage : public CAbilitySpellBase {
public:
    CAbilityMirrorImage(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    int count_ = 2;
    float duration_ = 60.0f;
    // Images deal no bonus damage in retail; without a damage-suppression
    // flag they hit at full strength here (documented approximation).
};

// ===== Shockwave (Tauren Chieftain) - line damage =====
class CAbilityShockwave : public CAbilitySpellBase {
public:
    CAbilityShockwave(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 100.0f;
    float range_ = 700.0f;
    float width_ = 200.0f;
};

// ===== Starfall (Priestess of the Moon) - scattered AoE waves =====
class CAbilityStarfall : public CAbilitySpellBase {
public:
    CAbilityStarfall(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float damage_ = 50.0f;
    float radius_ = 400.0f;
    int waves_ = 8;
};

// ===== Tranquility (Keeper of the Grove) - area heal over time =====
class CAbilityTranquility : public CAbilitySpellBase {
public:
    CAbilityTranquility(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float healPerTick_ = 20.0f;
    float radius_ = 400.0f;
    float duration_ = 10.0f;
};

// ===== Reincarnation (Tauren Chieftain) - revive on death =====
class CAbilityReincarnation : public CAbilitySpellBase {
public:
    CAbilityReincarnation(int handleId);
    void OnDeath(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
private:
    int manaCost_ = 150;
    float reincarnateCooldown_ = 240.0f;
    float reincarnateRemaining_ = 0.0f;
};

// ===== Charm (Dark Ranger) - convert a low-level creep =====
class CAbilityCharm : public CAbilitySpellBase {
public:
    CAbilityCharm(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    int maxLevel_ = 5;
};

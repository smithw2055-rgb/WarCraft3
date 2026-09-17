#pragma once
#include "ecs/abilities/ability_spell_base.h"

// ===== Serenity (Archmage) - AOE mana restore =====
class CAbilitySerenity : public CAbilitySpellBase {
public:
    CAbilitySerenity(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float manaRestore_ = 100.0f;
    float radius_ = 250.0f;
};

// ===== Healing Wave (Shadow Hunter) - chain heal =====
class CAbilityHealingWave : public CAbilitySpellBase {
public:
    CAbilityHealingWave(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float healAmount_ = 100.0f;
    int bounces_ = 3;
    float bounceRange_ = 500.0f;
};

// ===== Hex (Shadow Hunter) - transform target =====
class CAbilityHex : public CAbilitySpellBase {
public:
    CAbilityHex(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 15.0f;
};

// ===== Chain Lightning (Far Seer) - chain damage =====
class CAbilityChainLightning : public CAbilitySpellBase {
public:
    CAbilityChainLightning(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 85.0f;
    int bounces_ = 4;
    float bounceRange_ = 500.0f;
    float damageReduction_ = 0.25f;
};

// ===== Feral Spirit (Far Seer) - summon wolves =====
class CAbilityFeralSpirit : public CAbilitySpellBase {
public:
    CAbilityFeralSpirit(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    War3ID wolfType_ = War3ID("nwlg");
    int count_ = 2;
    float summonDuration_ = 60.0f; // ref Feral Spirit wolf timed life
};

// ===== Earthquake (Far Seer) - AOE damage to buildings =====
class CAbilityEarthquake : public CAbilitySpellBase {
public:
    CAbilityEarthquake(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float damagePerSecond_ = 50.0f;
    float radius_ = 250.0f;
    float duration_ = 25.0f;
};

// ===== Carrion Swarm (Dread Lord) - line AOE damage =====
class CAbilityCarrionSwarm : public CAbilitySpellBase {
public:
    CAbilityCarrionSwarm(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 75.0f;
    float range_ = 700.0f;
    float width_ = 200.0f;
};

// ===== Sleep (Dread Lord) - put unit to sleep =====
class CAbilitySleep : public CAbilitySpellBase {
public:
    CAbilitySleep(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 20.0f;
};

// ===== Inferno (Dread Lord) - summon infernal =====
class CAbilityInferno : public CAbilitySpellBase {
public:
    CAbilityInferno(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    War3ID infernalType_ = War3ID("ninf");
    float stunDuration_ = 2.0f;
    float stunRadius_ = 250.0f;
    float summonDuration_ = 180.0f; // ref Inferno timed life
};

// ===== Silence (Dark Ranger) - prevent casting =====
class CAbilitySilence : public CAbilitySpellBase {
public:
    CAbilitySilence(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float duration_ = 16.0f;
    float radius_ = 200.0f;
};

// ===== Black Arrow (Dark Ranger) - attack modifier =====
class CAbilityBlackArrow : public CAbilitySpellBase {
public:
    CAbilityBlackArrow(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float bonusDamage_ = 15.0f;
};

// ===== Life Drain (Dark Ranger) - drain HP =====
class CAbilityLifeDrain : public CAbilitySpellBase {
public:
    CAbilityLifeDrain(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float drainPerSecond_ = 25.0f;
    float duration_ = 8.0f;
};

// ===== Finger of Death (Archimonde) - instant kill =====
class CAbilityFingerOfDeath : public CAbilitySpellBase {
public:
    CAbilityFingerOfDeath(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 50000.0f;
};

// ===== Dark Portal (Archimonde) - summon units =====
// Implemented as an AbilityBuilder chain (summon action), the first spell
// migrated off hardcoded DoEffect logic (ref abilitybuilder behavior/).
class CAbilityDarkPortal : public CAbilitySpellBase {
public:
    CAbilityDarkPortal(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    War3ID portalUnitType_ = War3ID("nfel"); // Felguard through the portal
    int summonCount_ = 2;
    float summonDuration_ = 120.0f;
    bool chainRegistered_ = false;
};

// ===== Rain of Chaos (Archimonde) - AOE damage =====
class CAbilityRainOfChaos : public CAbilitySpellBase {
public:
    CAbilityRainOfChaos(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float damage_ = 100.0f;
    float radius_ = 300.0f;
    int waves_ = 5;
};

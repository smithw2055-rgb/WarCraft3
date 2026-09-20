#pragma once
#include "ecs/abilities/ability_spell_base.h"

// ===== Healing Wave (Shadow Hunter) - chain heal =====
class CAbilityHealingWave : public CAbilitySpellBase {
public:
    CAbilityHealingWave(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
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
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 15.0f;
};

// ===== Chain Lightning (Far Seer) - chain damage =====
class CAbilityChainLightning : public CAbilitySpellBase {
public:
    CAbilityChainLightning(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
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
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID wolfType_ = War3ID("nwlg");
    int count_ = 2; // ref DataB level 1
    float summonDuration_ = 60.0f; // ref Feral Spirit wolf timed life
};

// ===== Earthquake (Far Seer) - AOE damage to buildings =====
class CAbilityEarthquake : public CAbilitySpellBase {
public:
    CAbilityEarthquake(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damagePerSecond_ = 50.0f; // ref DataB level 1
    float radius_ = 250.0f; // ref Area1 (== DataD)
    float duration_ = 25.0f;
};

// ===== Carrion Swarm (Dread Lord) - line AOE damage =====
class CAbilityCarrionSwarm : public CAbilitySpellBase {
public:
    CAbilityCarrionSwarm(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
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
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 20.0f;
};

// ===== Inferno (Dread Lord) - summon infernal =====
class CAbilityInferno : public CAbilitySpellBase {
public:
    CAbilityInferno(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
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
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float duration_ = 16.0f; // ref Dur1 (16/20/24)
    float radius_ = 200.0f; // ref Area1 (200/275/350)
};

// ===== Black Arrow (Dark Ranger) - attack modifier =====
class CAbilityBlackArrow : public CAbilitySpellBase {
public:
    CAbilityBlackArrow(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float bonusDamage_ = 2.0f; // ref DataA level 1 (2/10/20)
};

// ===== Life Drain (Dark Ranger) - drain HP =====
class CAbilityLifeDrain : public CAbilitySpellBase {
public:
    CAbilityLifeDrain(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float drainPerSecond_ = 25.0f; // ref DataA level 1 (25/40/55)
    float duration_ = 8.0f;
};

// ===== Finger of Death (Archimonde) - kills non-heroes =====
// Heroes take DataC 500 instead (ref).
class CAbilityFingerOfDeath : public CAbilitySpellBase {
public:
    CAbilityFingerOfDeath(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 50000.0f;
    float heroDamage_ = 500.0f; // ref DataC level 1
};

// ===== Dark Portal (Archimonde) - summon units =====
// Implemented as an AbilityBuilder chain (summon action), the first spell
// migrated off hardcoded DoEffect logic (ref abilitybuilder behavior/).
class CAbilityDarkPortal : public CAbilitySpellBase {
public:
    CAbilityDarkPortal(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    War3ID portalUnitType_ = War3ID("nfel"); // Felguard through the portal
    int summonCount_ = 3; // ref DataB level 1
    float summonDuration_ = 120.0f;
};

// ===== Rain of Chaos (Archimonde) - AOE damage =====
class CAbilityRainOfChaos : public CAbilitySpellBase {
public:
    CAbilityRainOfChaos(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 100.0f;
    float radius_ = 900.0f; // ref Area1
    int waves_ = 5;
};

#pragma once
#include "ecs/abilities/ability_spell_base.h"

// ===== Blizzard (Archmage) - AOE damage over time =====
class CAbilityBlizzard : public CAbilitySpellBase {
public:
    CAbilityBlizzard(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int waves_ = 6;
    float damagePerWave_ = 30.0f;
    float radius_ = 200.0f;
};

// ===== Holy Light (Paladin) - single target heal =====
class CAbilityHolyLight : public CAbilitySpellBase {
public:
    CAbilityHolyLight(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float healAmount_ = 200.0f;
};

// ===== Divine Shield (Paladin) - invulnerability =====
class CAbilityDivineShield : public CAbilitySpellBase {
public:
    CAbilityDivineShield(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 15.0f;
};

// ===== Thunder Clap (Mountain King) - AOE damage + slow =====
class CAbilityThunderClap : public CAbilitySpellBase {
public:
    CAbilityThunderClap(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 60.0f;
    float radius_ = 250.0f;
    float slowPercent_ = 0.5f;
    float slowDuration_ = 5.0f;
};

// ===== Storm Bolt (Mountain King) - single target damage + stun =====
class CAbilityStormBolt : public CAbilitySpellBase {
public:
    CAbilityStormBolt(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 100.0f;
    float stunDuration_ = 2.0f;
};

// ===== War Stomp (Tauren Chieftain) - AOE damage + stun =====
class CAbilityWarStomp : public CAbilitySpellBase {
public:
    CAbilityWarStomp(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 25.0f;
    float radius_ = 250.0f;
    float stunDuration_ = 3.0f;
};

// ===== Death Coil (Death Knight) - damage enemy or heal undead =====
class CAbilityDeathCoil : public CAbilitySpellBase {
public:
    CAbilityDeathCoil(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    // Heal follows DataA (200/400/600); enemy damage has no Data field
    // and stays hardcoded.
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 100.0f;
    float healAmount_ = 200.0f;
};

// ===== Frost Nova (Lich) - AOE damage + slow =====
class CAbilityFrostNova : public CAbilitySpellBase {
public:
    CAbilityFrostNova(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    // Primary damage follows DataB (flat 100), splash follows DataA.
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damage_ = 100.0f;
    float aoeDamage_ = 50.0f;
    float radius_ = 200.0f;
    float slowPercent_ = 0.5f;
    float slowDuration_ = 4.0f;
};

// ===== Mana Burn (Demon Hunter) - burns target mana and deals damage =====
class CAbilityManaBurn : public CAbilitySpellBase {
public:
    CAbilityManaBurn(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    int manaBurned_ = 100;
};

// ===== Entangling Roots (Keeper of the Grove) - damage + immobilize =====
class CAbilityEntanglingRoots : public CAbilitySpellBase {
public:
    CAbilityEntanglingRoots(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damagePerSecond_ = 15.0f;
    float duration_ = 3.0f;
};

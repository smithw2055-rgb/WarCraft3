#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 5 hero spells: remaining race/neutral nukes and tools composed
// from existing primitives (area waves, line damage+stun, passive armor
// with a reflect hook, sacrifice-for-mana, strike with slow+poison).

// ===== Flame Strike (Blood Mage, AHfs) - point damage waves =====
class CAbilityFlameStrike : public CAbilitySpellBase {
public:
    CAbilityFlameStrike(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float damagePerWave_ = 80.0f;
    float radius_ = 200.0f;
    int waves_ = 3;
};

// ===== Death and Decay (Lich, AUdd) - corrupts an area =====
// Retail hits everything standing in it (allies, enemies, buildings);
// the caster is exempt so the channel point stays castable.
class CAbilityDeathAndDecay : public CAbilitySpellBase {
public:
    CAbilityDeathAndDecay(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;
private:
    float damageFraction_ = 0.04f; // ref DataA: fraction of max life per wave
    float radius_ = 300.0f; // ref Area1 (300/400)
    int waves_ = 4;
};

// ===== Impale (Crypt Lord, AUim) - line damage + stun =====
class CAbilityImpale : public CAbilitySpellBase {
public:
    CAbilityImpale(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 80.0f;
    float range_ = 700.0f;
    float width_ = 200.0f;
    float stunDuration_ = 2.0f;
};

// ===== Fan of Knives (Warden, AEfk) - instant knives around caster =====
class CAbilityFanOfKnives : public CAbilitySpellBase {
public:
    CAbilityFanOfKnives(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float damage_ = 100.0f;
    float radius_ = 300.0f;
};

// ===== Rain of Fire (Pit Lord, ANrf) - point damage waves =====
class CAbilityRainOfFire : public CAbilitySpellBase {
public:
    CAbilityRainOfFire(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float damagePerWave_ = 30.0f;
    float radius_ = 200.0f;
    int waves_ = 6;
};

// ===== Spiked Carapace (Crypt Lord, AUts) - armor + melee reflect =====
// Armor rides the unit bonus field; the reflect hook lives in
// CUnit::Damage next to the thorns listener (ref attack listeners).
class CAbilitySpikedCarapace : public CAbilitySpellBase {
public:
    CAbilitySpikedCarapace(int handleId);
    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    float GetReflectPercent() const { return reflectPercent_; }
private:
    int armorBonus_ = 4;
    float reflectPercent_ = 0.25f;
};

// ===== Dark Ritual (Lich, AUdr) - sacrifice an ally for mana =====
class CAbilityDarkRitual : public CAbilitySpellBase {
public:
    CAbilityDarkRitual(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
};

// ===== Shadow Strike (Warden, AEsh) - damage + slow + poison =====
class CAbilityShadowStrike : public CAbilitySpellBase {
public:
    CAbilityShadowStrike(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 50.0f;
    float slowPercent_ = 0.5f;
    float slowDuration_ = 4.0f;
    float poisonPerTick_ = 10.0f;
    float poisonDuration_ = 10.0f;
};

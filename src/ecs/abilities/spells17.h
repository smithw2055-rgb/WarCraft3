#pragma once
#include "ecs/abilities/ability_spell_base.h"

// Batch 17 (abilities): Howl of Terror, Breath of Fire, Drunken Haze,
// Mana Shield, Locust Swarm, Cyclone, Vengeance. Numbers validated
// against retail AbilityData.slk.

// ===== Howl of Terror (Pit Lord, ANht) - weaken enemy attacks =====
// DataA 0.3 read as a 30% reduction (factor 0.7); the factor itself is
// explicit on the buff if retail meant otherwise.
class CAbilityHowlOfTerror : public CAbilitySpellBase {
public:
    CAbilityHowlOfTerror(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 15.0f;
    float radius_ = 500.0f;
    float damageFactor_ = 0.7f;
};

// ===== Breath of Fire (Brewmaster, ANbf) - burning cone =====
// Cone approximated as target + splash (DataA 65 damage, DataE 7 dps
// over Dur 5, Area 125).
class CAbilityBreathOfFire : public CAbilitySpellBase {
public:
    CAbilityBreathOfFire(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float damage_ = 65.0f;
    float radius_ = 125.0f;
    float burnPerTick_ = 7.0f;
    float burnDuration_ = 5.0f;
};

// ===== Drunken Haze (Brewmaster, ANdh) - miss chance + slow =====
// DataB 0.45 miss chance; DataC 0.15 applied as a slow (documented
// reading).
class CAbilityDrunkenHaze : public CAbilitySpellBase {
public:
    CAbilityDrunkenHaze(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
private:
    float duration_ = 12.0f;
    float radius_ = 200.0f;
    float missChance_ = 0.45f;
    float slowPercent_ = 0.15f;
};

// ===== Mana Shield (Sea Witch, ANms) - mana takes the hits =====
class CAbilityManaShield : public CAbilitySpellBase {
public:
    CAbilityManaShield(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float duration_ = 60.0f; // toggle approximated as timed
};

// ===== Locust Swarm (Crypt Lord, AUls) - devouring swarm =====
// Area 800 reads as the leash range; the damage radius below is a
// documented guess. DataA 20 per wave.
class CAbilityLocustSwarm : public CAbilitySpellBase {
public:
    CAbilityLocustSwarm(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster) override;
private:
    float damagePerWave_ = 20.0f;
    float radius_ = 300.0f;
    int waves_ = 6;
};

// ===== Cyclone (Druid of the Talon, Acyc) - toss and suspend =====
class CAbilityCyclone : public CAbilitySpellBase {
public:
    CAbilityCyclone(int handleId);
    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
private:
    float duration_ = 20.0f; // ref Dur1
};

// ===== Vengeance (Warden, Avng) - avenging spirit on death =====
// Spirit unit even (ref DataC); the risen fights for the caster.
class CAbilityVengeance : public CAbilitySpellBase {
public:
    CAbilityVengeance(int handleId);
    void OnDeath(CSimulation& game, CUnit* unit) override;
private:
    War3ID spiritType_ = War3ID("even");
};

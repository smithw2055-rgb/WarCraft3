#pragma once
#include <string>
#include "data/combat_types.h"
#include "ecs/fwd.h"

struct CUnitAttack {
    int index = 0;
    CAttackType attackType = CAttackType::NONE;
    CWeaponType weaponType = CWeaponType::NONE;

    int damageBase = 0;
    int damageDice = 0;
    int damageSidesPerDie = 0;
    int damageUpgradeAmount = 0;

    float cooldown = 1.0f;
    float damagePoint = 0.0f;
    float backswingPoint = 0.0f;

    float range = 100.0f;
    float minRange = 0.0f;

    float projectileSpeed = 0.0f;
    std::string projectileArt;

    float areaFull = 0.0f;
    float areaHalf = 0.0f;
    float areaQuarter = 0.0f;
    float damageFactorHalf = 0.5f;
    float damageFactorQuarter = 0.25f;

    int targetCount = 0;
    float damageLossFactor = 0.0f;
    float areaSpread = 0.0f;

    int attacksEnabled = 0;
    float damageSpillDistance = 0.0f;
    float damageSpillRadius = 0.0f;
    std::string weaponSound;
    std::string weaponSoundLoop;

    bool CanTargetAir() const { return true; }
    bool CanTargetGround() const { return true; }
    float GetRange() const { return range; }
};

// Attack modifier effect (ref CUnitAttackReplacementEffect): a data-driven
// per-attack proc installed by orb/arrow abilities (and future
// ability-builder actions) instead of hardcoded CUnit::Damage branches.
//
// One effect per attack index; installing a second one replaces the first
// (retail orb effects do not stack). The slot lives on the CUnit instance
// (not the shared CUnitType attack) so morphs/copies never leak it.
struct CAttackModifierEffect {
    virtual ~CAttackModifierEffect() = default;
    // Per-shot resource gate (ref chargeResources; e.g. a future mana cost
    // for toggled arrows). False suppresses the proc for this landed hit;
    // the weapon damage itself still lands.
    virtual bool ChargeResources(CUnit* attacker) {
        (void)attacker;
        return true;
    }
    // Post-hit proc after a landed weapon attack (procs deal non-attack
    // damage so they never re-proc).
    virtual void OnAttackLanded(CSimulation& game, CUnit* attacker,
                                CUnit* victim, float damageDealt) = 0;
    // Projectile visual overrides (ref getProjectileArt/Speed); null/zero
    // keeps the attack's own missile.
    virtual const char* ProjectileArtOverride() const { return nullptr; }
    virtual float ProjectileSpeedOverride() const { return 0.0f; }
};

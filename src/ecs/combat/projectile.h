#pragma once
#include <functional>
#include <string>
#include "data/combat_types.h"

class CSimulation;
class CUnit;
class CWidget;

// Single-target homing attack projectile (ref CUnitAttackMissile,
// simplified). INSTANT weapons never spawn projectiles (see behavior_attack).
//
// Identity is handle-based: units may die or be removed mid-flight, in which
// case the projectile fizzles (target) or still hits without credit (source).
struct CProjectile {
    int attackerHandleId = -1;
    int targetHandleId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f; // world units per second
    float damage = 0.0f; // rolled at fire time (dice + bonuses)
    CAttackType attackType = CAttackType::NORMAL;
    CDamageType damageType = CDamageType::NORMAL;
    bool isAttack = true;
    bool isRanged = true;
    std::string weaponSound;
    float maxTravel = 0.0f; // lifetime guard in world units
    float traveled = 0.0f;
    // Missile-splash (ref CUnitAttackMissileSplash): applied at impact.
    float splashFull = 0.0f;
    float splashHalf = 0.0f;
    float splashQuarter = 0.0f;
    float splashFactorHalf = 0.5f;
    float splashFactorQuarter = 0.25f;
    float splashBonus = 0.0f; // instance bonuses, applied unscaled in rings
    // Missile-bounce (ref CUnitAttackMissileBounce): extra targets chained.
    int bouncesLeft = 0;
    float bounceRange = 0.0f;
    float bounceLossFactor = 1.0f;

    // On-hit hook (ref spell-missile onHit, e.g. StormBolt stun): fired on
    // every damaging impact after base damage, never on fizzle. Source may
    // be null (killed mid-flight).
    std::function<void(CSimulation& sim, CUnit* source, CWidget* target)> onHit;

    // Effect-reaction gate (ref CheckUnitForAbilityProjReaction): when set,
    // a shielded enemy target consumes its shield and the missile fizzles
    // with no damage and no hook. Attack projectiles leave this off.
    bool checkEffectReaction = false;
    // Dragon Scale (AIdd): a pierce missile reflects once, back at the shooter.
    bool pierceReflected = false;

    // Advances one sim tick. Returns true when finished (impact or fizzle).
    bool Update(CSimulation& sim);
};

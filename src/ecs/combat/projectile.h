#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "data/combat_types.h"
#include "resource/slk.h"

class CSimulation;
class CUnit;
class CWidget;
struct CUnitAttack;

// ============================================================================
// CProjectile - Engine-Level Pluggable Projectile Component (Phase 23)
//
// - Compact memory layout: packs 6 boolean state flags (`isAttack`, `isRanged`,
//   `groundTarget`, `checkEffectReaction`, `pierceReflected`, `customImpactOnly`)
//   into a single 1-byte bitfield beside `attackType`, `damageType`, and
//   `War3ID projectileTypeId`, reducing `sizeof(CProjectile)` from 184 bytes to
//   168 bytes on 64-bit targets while adding custom projectile archetype identity.
// - Game-agnostic execution: pre-impact reactions/reflections, custom archetype
//   impacts, and bounce target selection are delegated to `engine::ISimulationModule`
//   (`OnProjectilePreImpact`, `OnProjectileCustomImpact`, `OnFindProjectileBounceTarget`).
// ============================================================================
struct CProjectile {
    int attackerHandleId = -1;
    int targetHandleId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f;  // world units per second
    float damage = 0.0f; // rolled at fire time (dice + bonuses)

    // Optional registered custom projectile archetype 4CC (`War3GameModule` v23).
    War3ID projectileTypeId;
    CAttackType attackType = CAttackType::NORMAL;
    CDamageType damageType = CDamageType::NORMAL;

    // 8-bit packed state bitfield (replaces 5 scattered bool fields + padding).
    bool isAttack : 1;
    bool isRanged : 1;
    // Ground destination (ref ARTILLERY firing at a point target, and
    // attack-ground orders): flies to (tx, ty) and detonates there instead
    // of homing a widget. Never fizzles for lack of a target.
    bool groundTarget : 1;
    // Effect-reaction gate (ref CheckUnitForAbilityProjReaction): when set,
    // a shielded enemy target consumes its shield and the missile fizzles
    // with no damage and no hook. Attack projectiles leave this off.
    bool checkEffectReaction : 1;
    // Dragon Scale (AIdd): a pierce missile reflects once, back at the shooter.
    bool pierceReflected : 1;
    // When true, skips standard weapon/spell damage and runs only custom impact / onHit.
    bool customImpactOnly : 1;
    uint8_t reservedFlags_ : 2;

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

    float tx = 0.0f;
    float ty = 0.0f;
    // Line spill (MLINE/ALINE corridor): corridor state for impact.
    float fx = 0.0f; // fire position (corridor direction reference)
    float fy = 0.0f;
    float spillDistance = 0.0f;
    float spillRadius = 0.0f;

    // On-hit hook (ref spell-missile onHit, e.g. StormBolt stun): fired on
    // every damaging impact after base damage, never on fizzle. Source may
    // be null (killed mid-flight).
    std::function<void(CSimulation& sim, CUnit* source, CWidget* target)> onHit;

    CProjectile()
        : isAttack(true)
        , isRanged(true)
        , groundTarget(false)
        , checkEffectReaction(false)
        , pierceReflected(false)
        , customImpactOnly(false)
        , reservedFlags_(0) {}

    // Unified factory for weapon-attack projectiles (eliminates duplicate setup
    // between CBehaviorAttack::FireAtPoint and CBehaviorAttack::Update).
    static CProjectile MakeAttackProjectile(const CUnit* attacker,
                                            const CUnitAttack& attack,
                                            float damageAmount,
                                            const std::string& weaponSound,
                                            CWidget* target,
                                            float targetX,
                                            float targetY,
                                            bool pointTarget);

    // Advances one sim tick. Returns true when finished (impact or fizzle).
    bool Update(CSimulation& sim);
};

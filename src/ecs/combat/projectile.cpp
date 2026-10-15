#include "projectile.h"
#include "ecs/csimulation.h"
#include "ecs/combat/combat_system.h"
#include "ecs/unit.h"
#include "ecs/unit_attack.h"
#include "ecs/widget.h"
#include "ecs/player.h"
#include <algorithm>
#include <cmath>

namespace {

bool IsProjectileEnemy(CSimulation& sim, CUnit* attacker, CUnit* other) {
    if (!attacker || !other || other == attacker) return false;
    CPlayer* ownerPlayer = sim.GetPlayer(attacker->GetPlayerIndex());
    if (ownerPlayer) {
        if (CPlayer* otherPlayer = sim.GetPlayer(other->GetPlayerIndex())) {
            return !ownerPlayer->HasAlliance(*otherPlayer, CAllianceType::ALLIED);
        }
        return false;
    }
    return other->GetPlayerIndex() != attacker->GetPlayerIndex();
}

// Ground detonation shared by attack-ground shells and fire-stamped
// artillery: splash rings and/or the line corridor at (px, py).
void DetonateGround(CSimulation& sim, CProjectile& shot, float px, float py) {
    CUnit* source = sim.GetUnit(shot.attackerHandleId);
    auto* combat = sim.GetCombatSystem();
    if (!combat) return;
    if (shot.splashHalf > 0.0f) {
        CUnitAttack splashAttack;
        splashAttack.areaFull = shot.splashFull;
        splashAttack.areaHalf = shot.splashHalf;
        splashAttack.areaQuarter = shot.splashQuarter;
        splashAttack.damageFactorHalf = shot.splashFactorHalf;
        splashAttack.damageFactorQuarter = shot.splashFactorQuarter;
        combat->ApplySplashDamageAtPoint(sim, source, px, py, shot.isAttack, shot.isRanged,
                                         shot.attackType, shot.damageType,
                                         shot.damage, shot.splashBonus, 0, 0, splashAttack,
                                         nullptr, 1.0f, false);
    }
    if (shot.spillRadius > 0.0f && shot.spillDistance > 0.0f) {
        combat->ApplyLineDamage(sim, source, px, py, shot.tx - shot.fx, shot.ty - shot.fy,
                                shot.isAttack, shot.isRanged,
                                shot.attackType, shot.damageType,
                                shot.damage, 0, 0, 0,
                                shot.spillDistance, shot.spillRadius);
    }
}

} // namespace

bool CProjectile::Update(CSimulation& sim) {
    // Ground shells fly to a fixed point and always detonate (ref ARTILLERY
    // firing at a point target): no widget, no fizzle.
    if (groundTarget) {
        const float dx = tx - x;
        const float dy = ty - y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const float step = std::max(0.0f, speed) * sim.GetSimStepTime();
        const float hitRadius = std::max(step, 16.0f);
        if (dist <= hitRadius) {
            x = tx;
            y = ty;
            DetonateGround(sim, *this, x, y);
            return true;
        }
        if (dist > 0.0f && step > 0.0f) {
            x += dx / dist * step;
            y += dy / dist * step;
            traveled += step;
        }
        if (traveled >= maxTravel) {
            DetonateGround(sim, *this, x, y);
            return true;
        }
        return false;
    }

    CWidget* target = sim.GetWidget(targetHandleId);
    if (!target || target->IsDead()) return true; // fizzle: no damage

    const float dx = target->GetX() - x;
    const float dy = target->GetY() - y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float step = std::max(0.0f, speed) * sim.GetSimStepTime();

    // Impact radius scales a bit with target size so fast missiles at
    // point-blank range don't orbit.
    const float hitRadius = std::max(step, 16.0f);
    if (dist <= hitRadius) {
        // Source may be gone (killed mid-flight): the missile still hits,
        // just without kill credit, like WC3.
        CUnit* source = sim.GetUnit(attackerHandleId);
        x = target->GetX();
        y = target->GetY();
        if (checkEffectReaction) {
            if (auto* victim = dynamic_cast<CUnit*>(target)) {
                if (!victim->CheckSpellEffectReaction(sim, source)) return true;
            }
        }
        if (!pierceReflected && attackType == CAttackType::PIERCE) {
            if (auto* victim = dynamic_cast<CUnit*>(target)) {
                if (victim->GetBuff(War3ID("BIdd")) && source
                    && source->GetHandleId() != victim->GetHandleId()) {
                    pierceReflected = true;
                    targetHandleId = source->GetHandleId();
                    attackerHandleId = victim->GetHandleId();
                    return false;
                }
            }
        }
        if (splashHalf > 0.0f) {
            CUnitAttack splashAttack;
            splashAttack.areaFull = splashFull;
            splashAttack.areaHalf = splashHalf;
            splashAttack.areaQuarter = splashQuarter;
            splashAttack.damageFactorHalf = splashFactorHalf;
            splashAttack.damageFactorQuarter = splashFactorQuarter;
            if (auto* combat = sim.GetCombatSystem()) {
                combat->ApplySplashDamage(sim, source, target, isAttack, isRanged,
                                          attackType, damageType,
                                          weaponSound, damage, splashBonus,
                                          0, 0, splashAttack);
            } else {
                target->Damage(sim, source, isAttack, isRanged, attackType,
                               damageType, weaponSound, damage, splashBonus);
            }
            // Line spill rides along when the missile carries both (the
            // pierced primary is excluded from its own corridor).
            if (spillRadius > 0.0f && spillDistance > 0.0f) {
                if (auto* combat = sim.GetCombatSystem()) {
                    combat->ApplyLineDamage(sim, source, x, y, x - fx, y - fy,
                                            isAttack, isRanged, attackType, damageType,
                                            damage, 0, 0, 0,
                                            spillDistance, spillRadius, target);
                }
            }
            if (onHit) onHit(sim, source, target);
            return true;
        }
        target->Damage(sim, source, isAttack, isRanged, attackType,
                       damageType, weaponSound, damage, 0);
        if (onHit) onHit(sim, source, target);
        // Line spill (MLINE/ALINE): corridor damage past the impact point
        // along the flight path; the pierced primary is excluded.
        if (spillRadius > 0.0f && spillDistance > 0.0f) {
            if (auto* combat = sim.GetCombatSystem()) {
                combat->ApplyLineDamage(sim, source, x, y, x - fx, y - fy,
                                        isAttack, isRanged, attackType, damageType,
                                        damage, 0, 0, 0,
                                        spillDistance, spillRadius, target);
            }
        }
        // Bounce: chain to the nearest other enemy in range with falloff.
        if (bouncesLeft > 0 && source) {
            CUnit* next = nullptr;
            float bestDistSq = bounceRange * bounceRange;
            for (CUnit* other : sim.GetUnits()) {
                if (!other || other->IsDead()) continue;
                if (other->GetHandleId() == targetHandleId) continue;
                if (!IsProjectileEnemy(sim, source, other)) continue;
                const float ox = other->GetX() - x;
                const float oy = other->GetY() - y;
                const float dSq = ox * ox + oy * oy;
                if (dSq < bestDistSq) {
                    bestDistSq = dSq;
                    next = other;
                }
            }
            if (next) {
                targetHandleId = next->GetHandleId();
                damage *= bounceLossFactor;
                bouncesLeft--;
                traveled = 0.0f;
                maxTravel = bounceRange + 512.0f;
                return false;
            }
        }
        return true;
    }

    if (dist > 0.0f && step > 0.0f) {
        x += dx / dist * step;
        y += dy / dist * step;
        traveled += step;
    }
    if (traveled >= maxTravel) return true; // stuck guard
    return false;
}

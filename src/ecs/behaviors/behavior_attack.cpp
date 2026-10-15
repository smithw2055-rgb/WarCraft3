#include "behavior_attack.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include <cmath>

CBehaviorAttack::CBehaviorAttack() {}

const CUnitAttack* CBehaviorAttack::GetPrimaryAttack() const {
    if (!unit_ || !unit_->GetUnitType()) {
        return nullptr;
    }
    const auto& attacks = unit_->GetUnitType()->GetAttacks();
    return attacks.empty() ? nullptr : &attacks.front();
}

void CBehaviorAttack::ResetAttackSwingState() {
    damagePointTick_ = 0;
    backswingEndTick_ = 0;
    attackAnimationDuration_ = 0.0f;
    damageAppliedThisSwing_ = false;
}

bool CBehaviorAttack::IsAttackAnimationActive(int currentTick) const {
    return backswingEndTick_ > 0 && currentTick < backswingEndTick_;
}

void CBehaviorAttack::Begin(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    cooldownEndTick_ = 0;
    attackAnimationStartTick_ = -1;
    propelling_ = false;
    ResetAttackSwingState();
}

void CBehaviorAttack::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    ResetAttackSwingState();
}

void CBehaviorAttack::SetTarget(CWidget* target) {
    target_ = target;
    pointTarget_ = false;
    waypoints_.clear();
}

void CBehaviorAttack::SetTargetPoint(float x, float y) {
    target_ = nullptr;
    pointTarget_ = true;
    pointX_ = x;
    pointY_ = y;
    waypoints_.clear();
}

void CBehaviorAttack::FireAtPoint(CSimulation& simulation, const CUnitAttack& attack,
                                  float damageAmount, const std::string& weaponSound,
                                  bool ranged, float x, float y) {
    const bool firesProjectile = ranged &&
        attack.weaponType != CWeaponType::INSTANT && attack.projectileSpeed > 0.0f;
    if (firesProjectile) {
        CProjectile shot;
        shot.attackerHandleId = unit_->GetHandleId();
        shot.targetHandleId = -1;
        shot.groundTarget = true;
        shot.x = unit_->GetX();
        shot.y = unit_->GetY();
        shot.fx = shot.x;
        shot.fy = shot.y;
        shot.tx = x;
        shot.ty = y;
        shot.speed = attack.projectileSpeed;
        shot.damage = damageAmount;
        shot.attackType = attack.attackType;
        shot.weaponSound = weaponSound;
        if (CWeaponTypeInfo::IsSplashWeapon(attack.weaponType) && attack.areaHalf > 0.0f) {
            shot.splashFull = attack.areaFull;
            shot.splashHalf = attack.areaHalf;
            shot.splashQuarter = attack.areaQuarter;
            shot.splashFactorHalf = attack.damageFactorHalf;
            shot.splashFactorQuarter = attack.damageFactorQuarter;
            shot.splashBonus = (float)unit_->GetBonusDamage();
        }
        if (CWeaponTypeInfo::IsLineWeapon(attack.weaponType) &&
            attack.damageSpillRadius > 0.0f && attack.damageSpillDistance > 0.0f) {
            shot.spillDistance = attack.damageSpillDistance;
            shot.spillRadius = attack.damageSpillRadius;
        }
        const float dx0 = x - shot.x;
        const float dy0 = y - shot.y;
        shot.maxTravel = std::sqrt(dx0 * dx0 + dy0 * dy0) + 512.0f;
        simulation.SpawnProjectile(std::move(shot));
        return;
    }
    auto* combat = simulation.GetCombatSystem();
    if (!combat) return;
    if (attack.areaHalf > 0.0f) {
        combat->ApplySplashDamageAtPoint(simulation, unit_, x, y, true, ranged,
                                         attack.attackType, CDamageType::NORMAL,
                                         damageAmount, 0, 0, 0, attack);
    } else if (CWeaponTypeInfo::IsLineWeapon(attack.weaponType) &&
               attack.damageSpillRadius > 0.0f && attack.damageSpillDistance > 0.0f) {
        combat->ApplyLineDamage(simulation, unit_, unit_->GetX(), unit_->GetY(),
                                x - unit_->GetX(), y - unit_->GetY(), true, ranged,
                                attack.attackType, CDamageType::NORMAL,
                                damageAmount, 0, 0, 0,
                                attack.damageSpillDistance, attack.damageSpillRadius);
    }
}

bool CBehaviorAttack::Update(CSimulation& simulation) {
    if (!unit_) return true;
    if (!target_ && !pointTarget_) return true;
    // Hexed units cannot attack (but may still move). Stay alive and idle
    // so the behavior resumes when the hex expires (ref hex).
    if (unit_->IsHexed() || unit_->AreAttacksDisabled()) {
        ResetAttackSwingState();
        propelling_ = false;
        return false;
    }
    // A target that turns invisible mid-chase is lost (ref invisibility).
    // Ground points never decloak.
    if (!pointTarget_) {
        if (auto* targetUnit = dynamic_cast<CUnit*>(target_)) {
            if (targetUnit->IsInvisibleTo(unit_)) {
                ResetAttackSwingState();
                propelling_ = false;
                return true;
            }
        }
    }
    const CUnitAttack* attack = GetPrimaryAttack();
    if (!attack) {
        return true;
    }
    attackRange_ = unit_->GetAttackRange(0);

    const float tx = pointTarget_ ? pointX_ : target_->GetX();
    const float ty = pointTarget_ ? pointY_ : target_->GetY();
    float dx = tx - unit_->GetX();
    float dy = ty - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    const int currentTick = simulation.GetGameTurnTick();

    if (dist > attackRange_) {
        // Rooted units hold position: no chasing, but the behavior stays
        // alive so in-range targets are still attacked (ref rooted combat).
        if (unit_->IsRooted()) {
            ResetAttackSwingState();
            propelling_ = false;
            return false;
        }
        ResetAttackSwingState();
        if (unit_->IsBuilding() || !unit_->GetMoveBehavior()) {
            propelling_ = false;
            return true;
        }

        auto* pathfinding = simulation.GetPathfinding();

        pathRecalcCooldown_--;
        if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
            static constexpr float kRecalcDist = 64.0f;
            bool shouldRecalculate = waypoints_.empty();
            if (!shouldRecalculate && !waypoints_.empty()) {
                const auto& lastWp = waypoints_.back();
                const float ddx = lastWp.first - tx;
                const float ddy = lastWp.second - ty;
                shouldRecalculate = (ddx * ddx + ddy * ddy) > (kRecalcDist * kRecalcDist);
            }
            if (shouldRecalculate) {
                waypoints_.clear();
                if (pathfinding) {
                    std::vector<std::pair<float,float>> worldPath;
                    if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                    tx, ty,
                                                    std::max(0.0f, unit_->GetCollisionSize()),
                                                    unit_->GetMovementType(), unit_, worldPath)) {
                        for (size_t i = 1; i < worldPath.size(); i++) {
                            waypoints_.push_back(worldPath[i]);
                        }
                    }
                }
            }
            pathRecalcCooldown_ = 5;
        }

        if (!waypoints_.empty()) {
            auto& wp = waypoints_.front();
            float wdx = wp.first - unit_->GetX();
            float wdy = wp.second - unit_->GetY();
            float wdist = std::sqrt(wdx * wdx + wdy * wdy);
            const float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
            if (wdist <= std::max(1.0f, speed)) {
                waypoints_.pop_front();
                propelling_ = false;
            } else {
                if (!unit_->TurnTowardPoint(wp.first, wp.second)) {
                    propelling_ = false;
                    return false;
                }
                float step = std::min(speed, wdist);
                float nx = unit_->GetX() + (wdx / wdist) * step;
                float ny = unit_->GetY() + (wdy / wdist) * step;
                const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
                if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                             unit_->GetMovementType(), unit_)) {
                    propelling_ = false;
                    waypoints_.clear();
                    pathRecalcCooldown_ = 0;
                    return false;
                }
                if (auto* collision = simulation.GetWorldCollision()) {
                    const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
                    if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit_)) {
                        propelling_ = false;
                        waypoints_.clear();
                        pathRecalcCooldown_ = 0;
                        return false;
                    }
                }
                unit_->SetPosition(nx, ny);
                propelling_ = true;
                return false;
            }
        }

        if (pathfinding && pathfinding->HasPathingMap()) {
            propelling_ = false;
            return false;
        }

        if (!unit_->TurnTowardPoint(tx, ty)) {
            propelling_ = false;
            return false;
        }
        float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
        float step = std::min(speed, dist - attackRange_ + 16.0f);
        float nx = unit_->GetX() + (dx / dist) * step;
        float ny = unit_->GetY() + (dy / dist) * step;
        const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
        if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                     unit_->GetMovementType(), unit_)) {
            propelling_ = false;
            return false;
        }
        if (auto* collision = simulation.GetWorldCollision()) {
            const float dynamicCollisionRadius = std::max(collisionRadius, 16.0f);
            if (collision->CheckCollision(nx, ny, dynamicCollisionRadius, unit_)) {
                propelling_ = false;
                return false;
            }
        }
        unit_->SetPosition(nx, ny);
        propelling_ = true;
        return false;
    }

    propelling_ = false;
    if (!unit_->TurnTowardPoint(tx, ty)) {
        return false;
    }

    if (!damageAppliedThisSwing_ && damagePointTick_ > 0 && currentTick >= damagePointTick_) {
        std::string weaponSound = attack->weaponSound;
        float damageAmount = 10.0f;
        const int dice = attack->damageDice;
        const int sides = attack->damageSidesPerDie;
        const int base = attack->damageBase;
        if (dice > 0 && sides > 0) {
            int roll = 0;
            for (int d = 0; d < dice; d++) {
                // Synced sim RNG (std::rand is banned: lockstep replays).
                roll += 1 + (int)(simulation.SimRand01() * (float)sides);
            }
            damageAmount = (float)(base + roll);
        }
        else {
            damageAmount = (float)base;
        }
        // Instance bonuses (items, upgrades, hero primary attribute) are part
        // of base damage, so armor reduction in CUnit::Damage applies to them.
        damageAmount += (float)unit_->GetBonusDamage();

        const bool ranged = CWeaponTypeInfo::IsRanged(attack->weaponType);
        // Attack-ground shells the fixed point (no widget): holding fire
        // that keeps shelling while the order stands.
        if (pointTarget_) {
            FireAtPoint(simulation, *attack, damageAmount, weaponSound, ranged, tx, ty);
            damageAppliedThisSwing_ = true;
            return false;
        }
        const bool firesProjectile = ranged &&
            attack->weaponType != CWeaponType::INSTANT && attack->projectileSpeed > 0.0f;
        if (firesProjectile) {
            // Dice are rolled at fire time; the missile carries final damage.
            CProjectile shot;
            shot.attackerHandleId = unit_->GetHandleId();
            shot.targetHandleId = target_->GetHandleId();
            shot.x = unit_->GetX();
            shot.y = unit_->GetY();
            shot.fx = shot.x;
            shot.fy = shot.y;
            shot.speed = attack->projectileSpeed;
            shot.damage = damageAmount;
            shot.attackType = attack->attackType;
            shot.weaponSound = weaponSound;
            if (CWeaponTypeInfo::IsSplashWeapon(attack->weaponType) && attack->areaHalf > 0.0f) {
                shot.splashFull = attack->areaFull;
                shot.splashHalf = attack->areaHalf;
                shot.splashQuarter = attack->areaQuarter;
                shot.splashFactorHalf = attack->damageFactorHalf;
                shot.splashFactorQuarter = attack->damageFactorQuarter;
                shot.splashBonus = (float)unit_->GetBonusDamage();
            }
            if (CWeaponTypeInfo::IsLineWeapon(attack->weaponType) &&
                attack->damageSpillRadius > 0.0f && attack->damageSpillDistance > 0.0f) {
                shot.spillDistance = attack->damageSpillDistance;
                shot.spillRadius = attack->damageSpillRadius;
            }
            if (attack->weaponType == CWeaponType::MISSILE_BOUNCE && attack->targetCount > 0) {
                shot.bouncesLeft = attack->targetCount;
                shot.bounceRange = attack->areaHalf > 0.0f ? attack->areaHalf : 500.0f;
                shot.bounceLossFactor = attack->damageLossFactor > 0.0f ? attack->damageLossFactor : 1.0f;
            }
            if (attack->weaponType == CWeaponType::ARTILLERY) {
                // Ref CBehaviorAttack: shells are ballistic to the
                // launch-time position, not homing (the point target is
                // stamped here so the shell still lands if the victim
                // moves or dies mid-flight).
                shot.groundTarget = true;
                shot.targetHandleId = -1;
                shot.tx = target_->GetX();
                shot.ty = target_->GetY();
            }
            const float dx0 = target_->GetX() - shot.x;
            const float dy0 = target_->GetY() - shot.y;
            shot.maxTravel = std::sqrt(dx0 * dx0 + dy0 * dy0) + 512.0f;
            simulation.SpawnProjectile(std::move(shot));
        } else if (attack->areaHalf > 0.0f) {
            auto* combat = simulation.GetCombatSystem();
            if (combat) {
                combat->ApplySplashDamage(simulation, unit_, target_, true, ranged,
                                          attack->attackType, CDamageType::NORMAL,
                                          weaponSound, damageAmount, 0, 0, 0, *attack);
            } else {
                target_->Damage(simulation, unit_, true, ranged,
                                attack->attackType, CDamageType::NORMAL,
                                weaponSound, damageAmount, 0);
            }
        } else {
            target_->Damage(simulation, unit_, true, ranged,
                            attack->attackType, CDamageType::NORMAL,
                            weaponSound, damageAmount, 0);
        }
        damageAppliedThisSwing_ = true;
        if (!pointTarget_ && target_ && target_->IsDead()) {
            return true; // target dead, behavior done
        }
    }

    if (backswingEndTick_ > 0 && currentTick >= backswingEndTick_) {
        ResetAttackSwingState();
    }

    if (currentTick >= cooldownEndTick_) {
        const float speedMul = std::max(0.01f, unit_->GetAttackSpeedMultiplier());
        const float effectiveCooldown = attack->cooldown / speedMul;
        const int cooldownTicks = std::max(1, (int)std::ceil(effectiveCooldown / simulation.GetSimStepTime()));
        const int damagePointTicks = std::max(0, (int)std::ceil(attack->damagePoint / simulation.GetSimStepTime()));
        const int backswingTicks = std::max(0, (int)std::ceil(attack->backswingPoint / simulation.GetSimStepTime()));

        cooldownEndTick_ = currentTick + cooldownTicks;
        damagePointTick_ = currentTick + damagePointTicks;
        backswingEndTick_ = currentTick + damagePointTicks + backswingTicks;
        attackAnimationStartTick_ = currentTick;
        attackAnimationDuration_ = std::max(attack->damagePoint + attack->backswingPoint, attack->cooldown);
        damageAppliedThisSwing_ = false;
    }

    return false;
}

#include "behavior_attack.h"
#include "behavior_move_into_range.h"
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
        simulation.SpawnProjectile(CProjectile::MakeAttackProjectile(
            unit_, attack, damageAmount, weaponSound, nullptr, x, y, true));
        return;
    }
    auto* combat = simulation.GetCombatSystem();
    if (!combat) return;
    if (attack.areaHalf > 0.0f) {
        combat->ApplySplashDamageAtPoint(simulation, unit_, x, y, true, ranged,
                                         attack.attackType, CDamageType::NORMAL,
                                         damageAmount, 0, 0, 0, attack,
                                         nullptr, 1.0f, false);
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
    if (!pointTarget_) {
        if (auto* targetUnit = ((target_) ? (target_)->AsUnit() : nullptr)) {
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
    const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), tx, ty);
    const int currentTick = simulation.GetGameTurnTick();

    if (dist > attackRange_) {
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

        MoveIntoRange::RefreshWaypointsIfNeeded(simulation, unit_, tx, ty,
                                                waypoints_, pathRecalcCooldown_, true, false);
        if (!waypoints_.empty()) {
            if (MoveIntoRange::StepFrontWaypoint(simulation, unit_, waypoints_,
                                                 pathRecalcCooldown_, propelling_, false)) {
                return false;
            }
        }
        MoveIntoRange::StepDirectValidated(simulation, unit_, tx, ty,
                                           dist - attackRange_ + 16.0f, propelling_);
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
                roll += 1 + (int)(simulation.SimRand01() * (float)sides);
            }
            damageAmount = (float)(base + roll);
        } else {
            damageAmount = (float)base;
        }
        damageAmount += (float)unit_->GetBonusDamage();

        const bool ranged = CWeaponTypeInfo::IsRanged(attack->weaponType);
        if (pointTarget_) {
            FireAtPoint(simulation, *attack, damageAmount, weaponSound, ranged, tx, ty);
            damageAppliedThisSwing_ = true;
            return false;
        }
        const bool firesProjectile = ranged &&
            attack->weaponType != CWeaponType::INSTANT && attack->projectileSpeed > 0.0f;
        if (firesProjectile) {
            simulation.SpawnProjectile(CProjectile::MakeAttackProjectile(
                unit_, *attack, damageAmount, weaponSound, target_, tx, ty, false));
        } else if (attack->areaHalf > 0.0f) {
            if (auto* combat = simulation.GetCombatSystem()) {
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
            return true;
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

std::unique_ptr<CBehavior> CBehavior::CreateAttack() { return std::make_unique<CBehaviorAttack>(); }

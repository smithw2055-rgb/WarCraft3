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
    waypoints_.clear();
}

bool CBehaviorAttack::Update(CSimulation& simulation) {
    if (!unit_ || !target_) return true;
    const CUnitAttack* attack = GetPrimaryAttack();
    if (!attack) {
        return true;
    }
    attackRange_ = attack->range;

    float dx = target_->GetX() - unit_->GetX();
    float dy = target_->GetY() - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    const int currentTick = simulation.GetGameTurnTick();

    if (dist > attackRange_) {
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
                const float ddx = lastWp.first - target_->GetX();
                const float ddy = lastWp.second - target_->GetY();
                shouldRecalculate = (ddx * ddx + ddy * ddy) > (kRecalcDist * kRecalcDist);
            }
            if (shouldRecalculate) {
                waypoints_.clear();
                if (pathfinding) {
                    std::vector<std::pair<float,float>> worldPath;
                    if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                    target_->GetX(), target_->GetY(),
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

        if (!unit_->TurnTowardPoint(target_->GetX(), target_->GetY())) {
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
    if (!unit_->TurnTowardPoint(target_->GetX(), target_->GetY())) {
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
                roll += (std::rand() % sides) + 1;
            }
            damageAmount = (float)(base + roll);
        }
        else {
            damageAmount = (float)base;
        }

        target_->Damage(simulation, unit_, true, false,
                        attack->attackType, CDamageType::NORMAL,
                        weaponSound, damageAmount, 0);
        damageAppliedThisSwing_ = true;
        if (target_->IsDead()) {
            return true; // target dead, behavior done
        }
    }

    if (backswingEndTick_ > 0 && currentTick >= backswingEndTick_) {
        ResetAttackSwingState();
    }

    if (currentTick >= cooldownEndTick_) {
        const int cooldownTicks = std::max(1, (int)std::ceil(attack->cooldown / simulation.GetSimStepTime()));
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

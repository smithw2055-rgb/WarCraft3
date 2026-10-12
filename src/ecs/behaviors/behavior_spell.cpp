#include "behavior_spell.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/pathing/world_collision.h"
#include <cmath>

CBehaviorSpell::CBehaviorSpell() {}

void CBehaviorSpell::SetTarget(CWidget* target) {
    target_ = target;
    hasPointTarget_ = false;
    waypoints_.clear();
}

void CBehaviorSpell::SetTargetPoint(float x, float y) {
    targetX_ = x;
    targetY_ = y;
    hasPointTarget_ = true;
    target_ = nullptr;
    waypoints_.clear();
}

void CBehaviorSpell::Begin(CSimulation& simulation) {
    (void)simulation;
    state_ = CastState::MOVING_TO_RANGE;
    castTimer_ = 0;
    backswingTimer_ = 0;
    effectApplied_ = false;
    spellAnimationStartTick_ = -1;
    spellAnimationDuration_ = std::max(0.05f, castPoint_ + backswingDuration_);
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    propelling_ = false;
}

void CBehaviorSpell::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
}

bool CBehaviorSpell::Update(CSimulation& simulation) {
    if (!unit_ || !spellAbility_) return true;
    // Cast fizzles if the caster is silenced/hexed mid-cast (ref conditions).
    if (unit_->IsSilenced() || unit_->IsHexed()) {
        propelling_ = false;
        return true;
    }

    float dt = simulation.GetSimStepTime();

    switch (state_) {
        case CastState::MOVING_TO_RANGE: {
            // Calculate distance to target
            float tx = hasPointTarget_ ? targetX_ : (target_ ? target_->GetX() : unit_->GetX());
            float ty = hasPointTarget_ ? targetY_ : (target_ ? target_->GetY() : unit_->GetY());
            float dx = tx - unit_->GetX();
            float dy = ty - unit_->GetY();
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= castRange_) {
                // In range, start facing
                state_ = CastState::FACING_TARGET;
                propelling_ = false;
                return false;
            }

            // Move toward target
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
                        if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(), tx, ty,
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
                float speed = unit_->GetSpeed() * dt;
                if (wdist <= std::max(1.0f, speed)) {
                    waypoints_.pop_front();
                    propelling_ = false;
                } else {
                    if (!unit_->TurnTowardPoint(wp.first, wp.second)) {
                        propelling_ = false;
                        return false;
                    }
                    float step = std::min(speed, wdist);
                    const float nx = unit_->GetX() + (wdx / wdist) * step;
                    const float ny = unit_->GetY() + (wdy / wdist) * step;
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
                }
            } else {
                if (pathfinding && pathfinding->HasPathingMap()) {
                    propelling_ = false;
                    return false;
                }
                if (!unit_->TurnTowardPoint(tx, ty)) {
                    propelling_ = false;
                    return false;
                }
                float speed = unit_->GetSpeed() * dt;
                float step = std::min(speed, dist);
                const float nx = unit_->GetX() + (dx / dist) * step;
                const float ny = unit_->GetY() + (dy / dist) * step;
                const float collisionRadius = std::max(0.0f, unit_->GetCollisionSize());
                if (pathfinding && !pathfinding->IsWorldPointPathableForUnit(nx, ny, collisionRadius,
                                                                             unit_->GetMovementType(), unit_)) {
                    propelling_ = false;
                    return false;
                }
                unit_->SetPosition(nx, ny);
                propelling_ = true;
            }
            return false;
        }

        case CastState::FACING_TARGET: {
            // Face the target
            float tx = hasPointTarget_ ? targetX_ : (target_ ? target_->GetX() : unit_->GetX());
            float ty = hasPointTarget_ ? targetY_ : (target_ ? target_->GetY() : unit_->GetY());
            propelling_ = false;
            if (!unit_->TurnTowardPoint(tx, ty)) {
                return false;
            }
            state_ = CastState::CASTING;
            castTimer_ = 0;
            spellAnimationStartTick_ = simulation.GetGameTurnTick();
            spellAnimationDuration_ = std::max(0.05f, castPoint_ + backswingDuration_);
            if (auto& cb = simulation.GetSpellEventCallback()) {
                cb(unit_->GetHandleId(), 0); // channel
                cb(unit_->GetHandleId(), 1); // cast
            }
            return false;
        }

        case CastState::CASTING: {
            propelling_ = false;
            castTimer_ += dt;
            if (castTimer_ >= castPoint_) {
                // Cast point reached - apply effect
                if (!effectApplied_) {
                    effectApplied_ = true;

                    // Deduct mana
                    unit_->SetMana(unit_->GetMana() - spellAbility_->GetManaCost());

                    // Start cooldown
                    spellAbility_->StartCooldown();

                    // Apply the spell effect
                    if (hasPointTarget_) {
                        spellAbility_->DoEffect(simulation, unit_, targetX_, targetY_);
                    } else if (target_) {
                        spellAbility_->DoEffect(simulation, unit_, target_);
                    } else {
                        spellAbility_->DoEffect(simulation, unit_);
                    }
                    if (auto& cb = simulation.GetSpellEventCallback()) {
                        cb(unit_->GetHandleId(), 2); // effect
                    }
                }
                state_ = CastState::BACKSWING;
                backswingTimer_ = 0;
            }
            return false;
        }

        case CastState::BACKSWING: {
            propelling_ = false;
            backswingTimer_ += dt;
            if (backswingTimer_ >= backswingDuration_) {
                state_ = CastState::DONE;
                if (auto& cb = simulation.GetSpellEventCallback()) {
                    cb(unit_->GetHandleId(), 3); // finish
                    cb(unit_->GetHandleId(), 4); // endcast
                }
                return true;
            }
            return false;
        }

        case CastState::DONE:
            propelling_ = false;
            return true;
    }

    propelling_ = false;
    return true;
}

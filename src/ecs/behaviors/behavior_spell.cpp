#include "behavior_spell.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/pathing/pathfinding.h"
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
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
}

void CBehaviorSpell::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
}

bool CBehaviorSpell::Update(CSimulation& simulation) {
    if (!unit_ || !spellAbility_) return true;

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
                return false;
            }

            // Move toward target
            pathRecalcCooldown_--;
            if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
                waypoints_.clear();
                auto* pathfinding = simulation.GetPathfinding();
                if (pathfinding) {
                    std::vector<std::pair<float,float>> worldPath;
                    if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(), tx, ty, worldPath)) {
                        for (size_t i = 1; i < worldPath.size(); i++)
                            waypoints_.push_back(worldPath[i]);
                    }
                }
                pathRecalcCooldown_ = 8;
            }

            if (!waypoints_.empty()) {
                auto& wp = waypoints_.front();
                float wdx = wp.first - unit_->GetX();
                float wdy = wp.second - unit_->GetY();
                float wdist = std::sqrt(wdx * wdx + wdy * wdy);
                if (wdist < 48.0f) {
                    waypoints_.pop_front();
                } else {
                    float speed = unit_->GetSpeed() * dt * 10.0f;
                    float step = std::min(speed, wdist);
                    unit_->SetPosition(unit_->GetX() + (wdx / wdist) * step,
                                       unit_->GetY() + (wdy / wdist) * step);
                }
            } else {
                float speed = unit_->GetSpeed() * dt * 10.0f;
                float step = std::min(speed, dist);
                unit_->SetPosition(unit_->GetX() + (dx / dist) * step,
                                   unit_->GetY() + (dy / dist) * step);
            }
            return false;
        }

        case CastState::FACING_TARGET: {
            // Face the target
            float tx = hasPointTarget_ ? targetX_ : (target_ ? target_->GetX() : unit_->GetX());
            float ty = hasPointTarget_ ? targetY_ : (target_ ? target_->GetY() : unit_->GetY());
            float dx = tx - unit_->GetX();
            float dy = ty - unit_->GetY();
            float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
            unit_->SetFacing(facing);
            state_ = CastState::CASTING;
            castTimer_ = 0;
            return false;
        }

        case CastState::CASTING: {
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
                }
                state_ = CastState::BACKSWING;
                backswingTimer_ = 0;
            }
            return false;
        }

        case CastState::BACKSWING: {
            backswingTimer_ += dt;
            if (backswingTimer_ >= backswingDuration_) {
                state_ = CastState::DONE;
                return true;
            }
            return false;
        }

        case CastState::DONE:
            return true;
    }

    return true;
}

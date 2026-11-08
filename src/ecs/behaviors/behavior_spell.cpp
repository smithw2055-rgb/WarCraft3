#include "behavior_spell.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/ability.h"
#include <cmath>

CBehaviorSpell::CBehaviorSpell() {}

namespace {

void FireSpellStage(CSimulation& simulation, CUnit* caster, CWidget* target,
                    bool hasPoint, float px, float py, int stage) {
    auto& cb = simulation.GetSpellEventCallback();
    if (!cb || !caster) return;
    const int targetHandle = target ? target->GetHandleId() : -1;
    const float x = hasPoint ? px : (target ? target->GetX() : caster->GetX());
    const float y = hasPoint ? py : (target ? target->GetY() : caster->GetY());
    cb(caster->GetHandleId(), stage, targetHandle, x, y);
}

} // namespace

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
    if (unit_->IsSilenced() || unit_->IsHexed()) {
        propelling_ = false;
        return true;
    }

    const float dt = simulation.GetSimStepTime();

    switch (state_) {
        case CastState::MOVING_TO_RANGE: {
            const float tx = hasPointTarget_ ? targetX_ : (target_ ? target_->GetX() : unit_->GetX());
            const float ty = hasPointTarget_ ? targetY_ : (target_ ? target_->GetY() : unit_->GetY());
            const float dx = tx - unit_->GetX();
            const float dy = ty - unit_->GetY();
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= castRange_) {
                state_ = CastState::FACING_TARGET;
                propelling_ = false;
                return false;
            }

            MoveIntoRange::RefreshWaypointsIfNeeded(simulation, unit_, tx, ty,
                                                    waypoints_, pathRecalcCooldown_, true, false);
            if (!waypoints_.empty()) {
                MoveIntoRange::StepFrontWaypoint(simulation, unit_, waypoints_,
                                                 pathRecalcCooldown_, propelling_, true);
            } else {
                auto* pathfinding = simulation.GetPathfinding();
                if (pathfinding && pathfinding->HasPathingMap()) {
                    propelling_ = false;
                    return false;
                }
                if (!unit_->TurnTowardPoint(tx, ty)) {
                    propelling_ = false;
                    return false;
                }
                const float speed = unit_->GetSpeed() * dt;
                const float step = std::min(speed, dist);
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
            const float tx = hasPointTarget_ ? targetX_ : (target_ ? target_->GetX() : unit_->GetX());
            const float ty = hasPointTarget_ ? targetY_ : (target_ ? target_->GetY() : unit_->GetY());
            propelling_ = false;
            if (!unit_->TurnTowardPoint(tx, ty)) {
                return false;
            }
            state_ = CastState::CASTING;
            castTimer_ = 0;
            spellAnimationStartTick_ = simulation.GetGameTurnTick();
            spellAnimationDuration_ = std::max(0.05f, castPoint_ + backswingDuration_);
            FireSpellStage(simulation, unit_, target_, hasPointTarget_, targetX_, targetY_, 0);
            FireSpellStage(simulation, unit_, target_, hasPointTarget_, targetX_, targetY_, 1);
            return false;
        }

        case CastState::CASTING: {
            propelling_ = false;
            castTimer_ += dt;
            if (castTimer_ >= castPoint_) {
                if (!effectApplied_) {
                    effectApplied_ = true;
                    unit_->SetMana(unit_->GetMana() - spellAbility_->GetManaCost());
                    spellAbility_->StartCooldown();
                    if (hasPointTarget_) {
                        spellAbility_->DoEffect(simulation, unit_, targetX_, targetY_);
                    } else if (target_) {
                        spellAbility_->DoEffect(simulation, unit_, target_);
                    } else {
                        spellAbility_->DoEffect(simulation, unit_);
                    }
                    FireSpellStage(simulation, unit_, target_, hasPointTarget_, targetX_, targetY_, 2);
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
                FireSpellStage(simulation, unit_, target_, hasPointTarget_, targetX_, targetY_, 3);
                FireSpellStage(simulation, unit_, target_, hasPointTarget_, targetX_, targetY_, 4);
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

CBehavior* CBehavior::CreateSpellCast(CUnit* caster, CAbility* spellAbility,
                                      CWidget* target, bool hasPoint, float x, float y,
                                      float castRange, float castPoint, float backswingDuration) {
    auto* behavior = new CBehaviorSpell();
    behavior->SetUnit(caster);
    behavior->SetSpellAbility(spellAbility);
    if (hasPoint) {
        behavior->SetTargetPoint(x, y);
    } else if (target) {
        behavior->SetTarget(target);
    }
    behavior->SetCastRange(castRange);
    behavior->SetCastPoint(castPoint);
    behavior->SetBackswingDuration(backswingDuration);
    return behavior;
}

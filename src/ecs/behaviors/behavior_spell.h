#pragma once
#include "ecs/behaviors/behavior.h"
#include <deque>

class CAbility;
class CWidget;

// CBehaviorSpell - handles the casting sequence for spells
// Moves into range -> faces target -> cast point -> effect -> backswing
class CBehaviorSpell : public CBehavior {
public:
    CBehaviorSpell();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::SPELL; }

    void SetSpellAbility(CAbility* ability) { spellAbility_ = ability; }
    void SetTarget(CWidget* target) override;
    CWidget* GetTarget() const override { return target_; }
    void SetTargetPoint(float x, float y) override;
    void SetCastRange(float v) { castRange_ = v; }
    void SetCastPoint(float v) { castPoint_ = v; }
    void SetBackswingDuration(float v) { backswingDuration_ = v; }
    bool TracksPropellingState() const override { return true; }
    bool IsPropelling() const override { return propelling_; }
    bool IsMovingToRange() const override { return state_ == CastState::MOVING_TO_RANGE; }
    bool IsFacingTarget() const override { return state_ == CastState::FACING_TARGET; }
    bool IsCasting() const override { return state_ == CastState::CASTING; }
    bool IsBackswing() const override { return state_ == CastState::BACKSWING; }
    bool IsSpellAnimationActive() const { return state_ == CastState::CASTING || state_ == CastState::BACKSWING; }
    int GetSpellAnimationStartTick() const { return spellAnimationStartTick_; }
    float GetSpellAnimationDuration() const override { return spellAnimationDuration_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    CAbility* spellAbility_ = nullptr;
    CWidget* target_ = nullptr;
    float targetX_ = 0, targetY_ = 0;
    bool hasPointTarget_ = false;

    // Casting state
    enum class CastState {
        MOVING_TO_RANGE,
        FACING_TARGET,
        CASTING,
        BACKSWING,
        DONE
    };
    CastState state_ = CastState::MOVING_TO_RANGE;
    float castRange_ = 600.0f;
    float castPoint_ = 0.3f;
    float castTimer_ = 0;
    float backswingTimer_ = 0;
    float backswingDuration_ = 0.5f;
    bool effectApplied_ = false;
    int spellAnimationStartTick_ = -1;
    float spellAnimationDuration_ = 0.8f;

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool propelling_ = false;
};

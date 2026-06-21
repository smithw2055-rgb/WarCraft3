#pragma once
#include "ecs/behaviors/behavior.h"
#include <deque>

class CAbilitySpellBase;
class CWidget;

// CBehaviorSpell - handles the casting sequence for spells
// Moves into range -> faces target -> cast point -> effect -> backswing
class CBehaviorSpell : public CBehavior {
public:
    CBehaviorSpell();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::SPELL; }

    void SetSpellAbility(CAbilitySpellBase* ability) { spellAbility_ = ability; }
    void SetTarget(CWidget* target);
    void SetTargetPoint(float x, float y);
    void SetCastRange(float v) { castRange_ = v; }
    void SetCastPoint(float v) { castPoint_ = v; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    CAbilitySpellBase* spellAbility_ = nullptr;
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

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
};

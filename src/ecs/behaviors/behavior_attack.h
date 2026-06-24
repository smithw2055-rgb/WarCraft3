#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/unit.h"
#include <deque>

class CBehaviorAttack : public CBehavior {
public:
    CBehaviorAttack();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::ATTACK; }

    void SetTarget(CWidget* target);
    CWidget* GetTarget() const { return target_; }
    bool IsPropelling() const { return propelling_; }
    bool IsAttackAnimationActive(int currentTick) const;
    int GetAttackAnimationStartTick() const { return attackAnimationStartTick_; }
    float GetAttackAnimationDuration() const { return attackAnimationDuration_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    const CUnitAttack* GetPrimaryAttack() const;
    void ResetAttackSwingState();

    CWidget* target_ = nullptr;
    float attackRange_ = 128.0f;
    float acquisitionRange_ = 256.0f;
    int cooldownEndTick_ = 0;
    int damagePointTick_ = 0;
    int backswingEndTick_ = 0;
    int attackAnimationStartTick_ = -1;
    float attackAnimationDuration_ = 0.0f;
    bool damageAppliedThisSwing_ = false;

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool propelling_ = false;
};

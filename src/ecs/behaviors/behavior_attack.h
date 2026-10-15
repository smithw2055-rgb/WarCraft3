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
    // Ground-point target (attack-ground for shelling weapons, ref
    // ARTILLERY/ALINE point orders). Mutually exclusive with SetTarget.
    void SetTargetPoint(float x, float y);
    bool HasPointTarget() const { return pointTarget_; }
    float GetPointX() const { return pointX_; }
    float GetPointY() const { return pointY_; }
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
    // Attack-ground shelling at a fixed point (no widget target).
    void FireAtPoint(CSimulation& simulation, const CUnitAttack& attack,
                     float damageAmount, const std::string& weaponSound,
                     bool ranged, float x, float y);

    CWidget* target_ = nullptr;
    // Attack-ground destination (valid only when pointTarget_).
    bool pointTarget_ = false;
    float pointX_ = 0.0f;
    float pointY_ = 0.0f;
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

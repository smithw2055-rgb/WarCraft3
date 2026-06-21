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

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    CWidget* target_ = nullptr;
    int lastAttackTick_ = 0;
    float attackRange_ = 128.0f;
    float acquisitionRange_ = 256.0f;

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
};

#pragma once
#include "ecs/behaviors/behavior.h"
#include <deque>

class CAbilityRepair;
class CWidget;

class CBehaviorRepair : public CBehavior {
public:
    CBehaviorRepair();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::BUILD; }

    void SetRepairAbility(CAbilityRepair* ability) { repairAbility_ = ability; }
    void SetTarget(CWidget* target);
    bool IsMovingToTarget() const { return movingToTarget_; }
    bool IsRepairing() const { return repairing_; }
    bool IsPropelling() const { return propelling_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool RecalculatePath(CSimulation& simulation);
    bool MoveTowardWaypoint(CSimulation& simulation);
    bool ChargeRepairResources(CSimulation& simulation);
    void RefreshRepairRates();

    CAbilityRepair* repairAbility_ = nullptr;
    CWidget* target_ = nullptr;

    bool movingToTarget_ = true;
    bool repairing_ = false;
    bool propelling_ = false;

    float totalGoldCharged_ = 0.0f;
    float totalLumberCharged_ = 0.0f;
    float goldPerUpdate_ = 0.0f;
    float lumberPerUpdate_ = 0.0f;
    float hpPerUpdate_ = 0.0f;

    std::deque<std::pair<float, float>> waypoints_;
    int pathRecalcCooldown_ = 0;
};

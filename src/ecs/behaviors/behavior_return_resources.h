#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/ability.h"
#include <deque>

// CBehaviorReturnResources - worker carries resources back to town hall
class CBehaviorReturnResources : public CBehavior {
public:
    CBehaviorReturnResources();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::RETURN_RESOURCES; }

    void SetTarget(CWidget* target) override;
    CWidget* GetTarget() const override { return target_; }
    void SetHarvestAbility(const CAbility* ability) { harvestAbility_ = ability; }
    bool IsMovingToDropoff() const override { return movingToDropoff_; }
    bool IsDepositing() const override { return depositing_; }
    bool TracksPropellingState() const override { return true; }
    bool IsPropelling() const override { return propelling_; }

    // Find nearest dropoff point (town hall)
    void FindNearestDropoff(CSimulation& simulation);

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    void DepositCarriedResources(CSimulation& simulation);

    CWidget* target_ = nullptr;
    const CAbility* harvestAbility_ = nullptr;

    // State
    bool movingToDropoff_ = true;
    bool depositing_ = false;
    float depositTimer_ = 0;
    float depositDuration_ = 0.5f;

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool propelling_ = false;
};

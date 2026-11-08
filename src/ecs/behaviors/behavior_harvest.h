#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/ability.h"
#include <deque>

// CBehaviorHarvest - worker moves to resource and gathers it
class CBehaviorHarvest : public CBehavior {
public:
    CBehaviorHarvest();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::HARVEST; }

    void SetTarget(CWidget* target) override;
    CWidget* GetTarget() const override { return target_; }
    void SetResourceType(CResourceType type) { resourceType_ = type; }
    void SetCapacity(int cap) { capacity_ = cap; }
    bool IsMovingToResource() const { return movingToResource_; }
    bool IsMovingToWorkSite() const override { return movingToResource_; }
    bool IsGathering() const { return gathering_; }
    bool IsActivelyWorking() const override { return gathering_; }
    bool TracksPropellingState() const override { return true; }
    bool IsPropelling() const override { return propelling_; }
    int GetGoldCapacity() const override { return resourceType_ == CResourceType::GOLD ? capacity_ : 0; }
    int GetPopoutFromMineTurnTick() const override { return popoutFromMineTurnTick_; }
    void PopoutFromMine(int goldMined) override;

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    void FindNearestTree(CSimulation& simulation);

    CWidget* target_ = nullptr;
    int targetHandle_ = -1;
    CResourceType resourceType_ = CResourceType::GOLD;
    int capacity_ = 10;

    // State
    bool movingToResource_ = true;
    bool gathering_ = false;
    bool returning_ = false;
    bool inMine_ = false;
    float gatherTimer_ = 0;
    float gatherDuration_ = 2.0f; // seconds per chop/mine tick
    int gatheredAmount_ = 0;
    int popoutFromMineTurnTick_ = 0;

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool propelling_ = false;
};

#pragma once
#include "ecs/behaviors/behavior.h"
#include "ecs/abilities/ability_harvest.h"
#include <deque>

// CBehaviorHarvest - worker moves to resource and gathers it
class CBehaviorHarvest : public CBehavior {
public:
    CBehaviorHarvest();
    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::HARVEST; }

    void SetTarget(CWidget* target);
    void SetResourceType(CResourceType type) { resourceType_ = type; }
    void SetCapacity(int cap) { capacity_ = cap; }
    bool IsMovingToResource() const { return movingToResource_; }
    bool IsGathering() const { return gathering_; }
    bool IsPropelling() const { return propelling_; }
    int GetGoldCapacity() const { return resourceType_ == CResourceType::GOLD ? capacity_ : 0; }
    int GetPopoutFromMineTurnTick() const { return popoutFromMineTurnTick_; }
    void PopoutFromMine(int goldMined);

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    void FindNearestTree(CSimulation& simulation);

    CWidget* target_ = nullptr;
    CResourceType resourceType_ = CResourceType::GOLD;
    int capacity_ = 10;

    // State
    bool movingToResource_ = true;
    bool gathering_ = false;
    bool returning_ = false;
    bool inMine_ = false;
    float gatherTimer_ = 0;
    float gatherDuration_ = 2.0f; // seconds per gather cycle
    int gatheredAmount_ = 0;
    int popoutFromMineTurnTick_ = 0;

    // Pathfinding
    std::deque<std::pair<float,float>> waypoints_;
    int pathRecalcCooldown_ = 0;
    bool propelling_ = false;
};

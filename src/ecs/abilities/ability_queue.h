#pragma once
#include "ecs/ability.h"
#include <vector>
#include <unordered_set>

// CAbilityQueue - training/research queue ability
// Allows buildings to queue units, research, and hero revives (CQueueItemType is defined in ecs/ability.h)
class CAbilityQueue : public CAbility {
public:
    static constexpr int MAX_QUEUE = MAX_QUEUE_SLOTS;

    CAbilityQueue(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;

    // Order handling
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;
    bool CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // Queue management (implements CAbility virtual production queue contract)
    bool QueueUnit(CSimulation& game, CUnit* building, War3ID unitTypeId) override;
    bool QueueResearch(CSimulation& game, CUnit* building, War3ID upgradeId) override;
    bool QueueReviveHero(CSimulation& game, CUnit* building, CUnit* hero) override;
    void CancelQueueItem(CSimulation& game, CUnit* building, int slot) override;
    void CancelAll(CSimulation& game, CUnit* building);

    // Queue state
    War3ID GetQueuedItem(int slot) const override;
    CQueueItemType GetQueuedItemType(int slot) const override;
    int GetQueueCount() const override;
    bool IsActivelyWorking() const override { return GetQueueCount() > 0; }
    float GetProgress() const override { return progress_; }
    float GetBuildTime() const override { return buildTime_; }

    // Configuration
    void AddTrainedUnit(War3ID id) override { unitsTrained_.insert(id); }
    void AddResearch(War3ID id) override { researchesAvailable_.insert(id); }
    const std::unordered_set<War3ID>& GetTrainedUnits() const override { return unitsTrained_; }
    const std::unordered_set<War3ID>& GetResearches() const override { return researchesAvailable_; }

private:
    void ProcessCompletion(CSimulation& game, CUnit* building);
    void ShiftQueue(CSimulation& game, CUnit* building);
    float CalculateBuildTime(CSimulation& game, CUnit* building, War3ID typeId, CQueueItemType type) const;

    // Queue arrays
    War3ID queue_[MAX_QUEUE] = {};
    CQueueItemType queueTypes_[MAX_QUEUE] = {};
    float progress_ = 0;
    float buildTime_ = 0;

    // Config
    std::unordered_set<War3ID> unitsTrained_;
    std::unordered_set<War3ID> researchesAvailable_;
};

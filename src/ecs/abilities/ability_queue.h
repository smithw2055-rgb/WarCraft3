#pragma once
#include "ecs/ability.h"
#include <vector>
#include <unordered_set>

// Queue item type
enum class CQueueItemType : uint8_t {
    NONE, UNIT, RESEARCH, HERO_REVIVE
};

// CAbilityQueue - training/research queue ability
// Allows buildings to queue units, research, and hero revives
class CAbilityQueue : public CAbility {
public:
    static constexpr int MAX_QUEUE = 5;

    CAbilityQueue(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;

    // Order handling
    bool CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // Queue management
    bool QueueUnit(CSimulation& game, CUnit* building, War3ID unitTypeId);
    bool QueueResearch(CSimulation& game, CUnit* building, War3ID upgradeId);
    bool QueueReviveHero(CSimulation& game, CUnit* building, CUnit* hero);
    void CancelQueueItem(CSimulation& game, CUnit* building, int slot);
    void CancelAll(CSimulation& game, CUnit* building);

    // Queue state
    War3ID GetQueuedItem(int slot) const;
    CQueueItemType GetQueuedItemType(int slot) const;
    int GetQueueCount() const;
    float GetProgress() const { return progress_; }
    float GetBuildTime() const { return buildTime_; }

    // Configuration
    void AddTrainedUnit(War3ID id) { unitsTrained_.insert(id); }
    void AddResearch(War3ID id) { researchesAvailable_.insert(id); }
    const std::unordered_set<War3ID>& GetTrainedUnits() const { return unitsTrained_; }
    const std::unordered_set<War3ID>& GetResearches() const { return researchesAvailable_; }

private:
    void ProcessCompletion(CSimulation& game, CUnit* building);
    void ShiftQueue();
    float CalculateBuildTime(CSimulation& game, War3ID typeId, CQueueItemType type) const;

    // Queue arrays
    War3ID queue_[MAX_QUEUE] = {};
    CQueueItemType queueTypes_[MAX_QUEUE] = {};
    float progress_ = 0;
    float buildTime_ = 0;

    // Config
    std::unordered_set<War3ID> unitsTrained_;
    std::unordered_set<War3ID> researchesAvailable_;
};

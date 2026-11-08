#pragma once
#include "ecs/ability.h"

// CAbilityHarvest - resource gathering ability
// Supports both gold mining and lumber chopping
class CAbilityHarvest : public CAbility {
public:
    CAbilityHarvest(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Order validation
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;

    // Begin harvesting
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // Resource state (implements CAbility virtual harvest capability contract)
    CResourceType GetCarriedResourceType() const override { return carriedAmount_ > 0 ? carriedType_ : CResourceType::NONE; }
    int GetCarriedAmount() const override { return carriedAmount_; }
    void SetCarried(CResourceType type, int amount) override { carriedType_ = type; carriedAmount_ = amount; }
    void ClearCarried() override { carriedType_ = CResourceType::NONE; carriedAmount_ = 0; }
    int GetGoldCapacity() const { return goldCapacity_; }
    int GetLumberCapacity() const { return lumberCapacity_; }
    int GetLastTargetHandle() const override { return lastTargetHandle_; }
    void SetLastTargetHandle(int handleId) override { lastTargetHandle_ = handleId; }

    // Configuration
    void SetGoldCapacity(int cap) { goldCapacity_ = cap; }
    void SetLumberCapacity(int cap) { lumberCapacity_ = cap; }
    void SetHarvestRange(float range) { harvestRange_ = range; }

    // Toggle state: if carrying resources, base order is RETURN_RESOURCES
    bool IsCarryingResources() const override { return carriedAmount_ > 0; }

private:
    CResourceType carriedType_ = CResourceType::NONE;
    int carriedAmount_ = 0;
    int goldCapacity_ = 10;
    int lumberCapacity_ = 10;
    float harvestRange_ = 128.0f;
    int lastTargetHandle_ = -1;
};

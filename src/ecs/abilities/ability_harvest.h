#pragma once
#include "ecs/ability.h"

// Resource types
enum class CResourceType : uint8_t {
    NONE, GOLD, LUMBER
};

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

    // Resource state
    CResourceType GetCarriedResourceType() const { return carriedType_; }
    int GetCarriedAmount() const { return carriedAmount_; }
    void SetCarried(CResourceType type, int amount) { carriedType_ = type; carriedAmount_ = amount; }
    void ClearCarried() { carriedType_ = CResourceType::NONE; carriedAmount_ = 0; }

    // Configuration
    void SetGoldCapacity(int cap) { goldCapacity_ = cap; }
    void SetLumberCapacity(int cap) { lumberCapacity_ = cap; }
    void SetHarvestRange(float range) { harvestRange_ = range; }

    // Toggle state: if carrying resources, base order is RETURN_RESOURCES
    bool IsCarryingResources() const { return carriedAmount_ > 0; }

private:
    CResourceType carriedType_ = CResourceType::NONE;
    int carriedAmount_ = 0;
    int goldCapacity_ = 10;
    int lumberCapacity_ = 10;
    float harvestRange_ = 128.0f;
    int lastTargetHandle_ = -1;
};

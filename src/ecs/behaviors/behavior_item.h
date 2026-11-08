#pragma once
#include "ecs/behaviors/behavior.h"

class CItem;
class CUnit;
class CWidget;

// Item behaviors (ref CBehaviorGetItem / CBehaviorDropItem /
// CBehaviorGiveItemToHero): walk-into-range then delegate to
// CAbilityInventory. Movement is straight-line stepping with collision
// checks (same simplification as CBehaviorBoardTransport); full
// pathfinding for maze chase is a documented follow-up.
class CBehaviorGetItem : public CBehavior {
public:
    explicit CBehaviorGetItem(float pickupRange = 96.0f);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::INVENTORY; }

    void SetTarget(CItem* item) { item_ = item; }
    CItem* GetItemTarget() const { return item_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool TryPickupNow(CSimulation& simulation);

    CItem* item_ = nullptr;
    float pickupRange_ = 96.0f;
};

class CBehaviorDropItem : public CBehavior {
public:
    // Drop `slot` at an explicit point (hasPoint=false drops at feet).
    CBehaviorDropItem(int slot, bool hasPoint = false, float x = 0.0f, float y = 0.0f);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::INVENTORY; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;

private:
    int slot_ = -1;
    bool hasPoint_ = false;
    float x_ = 0.0f;
    float y_ = 0.0f;
};

class CBehaviorGiveItem : public CBehavior {
public:
    explicit CBehaviorGiveItem(int slot, float giveRange = 150.0f);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::INVENTORY; }

    void SetTarget(CUnit* target) { target_ = target; }
    CUnit* GetUnitTarget() const { return target_; }
    int GetSlot() const { return slot_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool TryGiveNow(CSimulation& simulation);

    CUnit* target_ = nullptr;
    int slot_ = -1;
    float giveRange_ = 150.0f;
};

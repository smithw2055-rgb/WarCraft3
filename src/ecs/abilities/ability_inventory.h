#pragma once
#include "ecs/ability.h"
#include <algorithm>
#include <array>
#include <vector>

class CItem;

// CAbilityInventory - manages unit's item slots
class CAbilityInventory : public CAbility {
public:
    static constexpr int MAX_SLOTS = MAX_INVENTORY_SLOTS;

    CAbilityInventory(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;

    // Item management (implements CAbility virtual inventory capability contract)
    bool CanPickUpItem(CSimulation& game, CUnit* unit, CItem* item) const;
    bool PickUpItem(CSimulation& game, CUnit* unit, CItem* item) override;
    bool PickUpItem(CSimulation& game, CUnit* unit, CItem* item, int slotPreference) override;
    bool DropItem(CSimulation& game, CUnit* unit, int slot) override;
    bool DropItemAt(CSimulation& game, CUnit* unit, int slot, float x, float y) override;
    bool DropItem(CSimulation& game, CUnit* unit, CItem* item) override;
    bool DropItemAt(CSimulation& game, CUnit* unit, CItem* item, float x, float y) override;
    bool RemoveItemAt(CSimulation& game, CUnit* unit, int slot);
    bool UseItem(CSimulation& game, CUnit* unit, int slot) override;
    bool UseItem(CSimulation& game, CUnit* unit, CItem* item) override;
    bool MoveItemToSlot(CItem* item, int slot) override;

    // Slot access
    CItem* GetItem(int slot) const override;
    int GetItemCount() const override;
    int GetItemCapacity() const override { return itemCapacity_; }
    int FindEmptySlot() const;
    int FindEmptySlot(int slotPreference) const;
    int FindItemSlot(CItem* item) const override;
    bool HasItem(CItem* item) const override;

    // Configuration
    void SetCanDrop(bool v) override { canDrop_ = v; }
    void SetCanPickUp(bool v) { canPickUp_ = v; }
    void SetCanUse(bool v) { canUse_ = v; }
    void SetDropOnDeath(bool v) { dropOnDeath_ = v; }
    void SetItemCapacity(int cap) override { itemCapacity_ = std::min(cap, MAX_SLOTS); }

    // Order handling
    bool CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;

private:
    std::array<CItem*, MAX_SLOTS> items_ = {};
    int itemCapacity_ = MAX_SLOTS;
    bool canDrop_ = true;
    bool canPickUp_ = true;
    bool canUse_ = true;
    bool dropOnDeath_ = true;
};

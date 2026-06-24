#pragma once
#include "ecs/ability.h"
#include <array>
#include <vector>

class CItem;

// CAbilityInventory - manages unit's item slots
class CAbilityInventory : public CAbility {
public:
    static constexpr int MAX_SLOTS = 6;

    CAbilityInventory(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;

    // Item management
    bool CanPickUpItem(CSimulation& game, CUnit* unit, CItem* item) const;
    bool PickUpItem(CSimulation& game, CUnit* unit, CItem* item);
    bool DropItem(CSimulation& game, CUnit* unit, int slot);
    bool DropItemAt(CSimulation& game, CUnit* unit, int slot, float x, float y);
    bool DropItem(CSimulation& game, CUnit* unit, CItem* item);
    bool DropItemAt(CSimulation& game, CUnit* unit, CItem* item, float x, float y);
    bool UseItem(CSimulation& game, CUnit* unit, int slot);
    bool UseItem(CSimulation& game, CUnit* unit, CItem* item);
    bool MoveItemToSlot(CItem* item, int slot);

    // Slot access
    CItem* GetItem(int slot) const;
    int GetItemCount() const;
    int FindEmptySlot() const;
    int FindItemSlot(CItem* item) const;
    bool HasItem(CItem* item) const;

    // Configuration
    void SetCanDrop(bool v) { canDrop_ = v; }
    void SetCanPickUp(bool v) { canPickUp_ = v; }
    void SetCanUse(bool v) { canUse_ = v; }
    void SetDropOnDeath(bool v) { dropOnDeath_ = v; }
    void SetItemCapacity(int cap) { itemCapacity_ = std::min(cap, MAX_SLOTS); }

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

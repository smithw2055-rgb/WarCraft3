#include "ability_inventory.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/item.h"
#include "ecs/item_bonuses.h"
#include <iostream>

CAbilityInventory::CAbilityInventory(int handleId)
    : CAbility(handleId, War3ID("AInv"), CAbilityCategory::INVENTORY) {
    iconShowing_ = false;
}

void CAbilityInventory::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    items_.fill(nullptr);
}

void CAbilityInventory::OnRemove(CSimulation& game, CUnit* unit) {
    if (dropOnDeath_) {
        for (int i = 0; i < itemCapacity_; i++) {
            if (items_[i]) {
                DropItem(game, unit, i);
            }
        }
    }
}

void CAbilityInventory::OnDeath(CSimulation& game, CUnit* unit) {
    if (dropOnDeath_) {
        for (int i = 0; i < itemCapacity_; i++) {
            if (items_[i]) {
                DropItem(game, unit, i);
            }
        }
    }
}

bool CAbilityInventory::CanPickUpItem(CSimulation& game, CUnit* unit, CItem* item) const {
    (void)game; (void)unit;
    if (!canPickUp_ || !item) return false;
    return FindEmptySlot() >= 0;
}

bool CAbilityInventory::PickUpItem(CSimulation& game, CUnit* unit, CItem* item) {
    if (!CanPickUpItem(game, unit, item)) return false;

    int slot = FindEmptySlot();
    if (slot < 0) return false;

    // Auto-use powerups
    if (item->IsUseAutomatically() || item->IsPowerup()) {
        // Apply item effects immediately
        std::cout << "Auto-used powerup item" << std::endl;
        // TODO: apply item abilities
        item->SetLife(game, 0); // consume the item
        return true;
    }

    items_[slot] = item;
    item->SetInInventory(true);
    item->SetInventorySlot(slot);
    item->SetOwner(unit);

    // Grant item abilities to unit
    for (auto& abilityId : item->GetAbilityList()) {
        // TODO: create ability from data and add to unit
        std::cout << "Granted item ability: " << abilityId.ToString() << std::endl;
    }

    std::cout << "Picked up item in slot " << slot << std::endl;
    return true;
}

bool CAbilityInventory::DropItem(CSimulation& game, CUnit* unit, int slot) {
    (void)game;
    if (!canDrop_ || slot < 0 || slot >= itemCapacity_) return false;
    if (!items_[slot]) return false;

    CItem* item = items_[slot];
    items_[slot] = nullptr;

    // Remove item abilities from unit
    for (auto& abilityId : item->GetAbilityList()) {
        // TODO: remove ability from unit
        std::cout << "Removed item ability: " << abilityId.ToString() << std::endl;
    }

    // Place item at unit's position
    item->SetPosition(unit->GetX(), unit->GetY());
    item->SetInInventory(false);
    item->SetInventorySlot(-1);
    item->SetOwner(nullptr);

    std::cout << "Dropped item from slot " << slot << std::endl;
    return true;
}

bool CAbilityInventory::UseItem(CSimulation& game, CUnit* unit, int slot) {
    (void)game;
    if (!canUse_ || slot < 0 || slot >= itemCapacity_) return false;
    if (!items_[slot]) return false;

    CItem* item = items_[slot];

    // Use item's ability (potion, scroll, etc.)
    // TODO: find and execute item's active ability
    std::cout << "Used item in slot " << slot << std::endl;

    // Consume charges if perishable
    if (item->IsPerishable() || item->IsCharged()) {
        item->UseCharge();
        if (item->GetCharges() <= 0) {
            // Item consumed, remove from inventory
            items_[slot] = nullptr;
            item->SetLife(game, 0); // destroy the item
            std::cout << "Item consumed (no charges left)" << std::endl;
        }
    }

    return true;
}

CItem* CAbilityInventory::GetItem(int slot) const {
    if (slot < 0 || slot >= itemCapacity_) return nullptr;
    return items_[slot];
}

int CAbilityInventory::GetItemCount() const {
    int count = 0;
    for (int i = 0; i < itemCapacity_; i++) {
        if (items_[i]) count++;
    }
    return count;
}

int CAbilityInventory::FindEmptySlot() const {
    for (int i = 0; i < itemCapacity_; i++) {
        if (!items_[i]) return i;
    }
    return -1;
}

bool CAbilityInventory::HasItem(CItem* item) const {
    for (int i = 0; i < itemCapacity_; i++) {
        if (items_[i] == item) return true;
    }
    return false;
}

bool CAbilityInventory::CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)game; (void)caster; (void)orderId; (void)target;
    // Item use/drop orders are handled here
    // TODO: implement item order interception
    return true;
}

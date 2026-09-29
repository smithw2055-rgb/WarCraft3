#include "ability_inventory.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/item.h"
#include "ecs/item_bonuses.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/ability_spell_base.h"
#include "ecs/orders/order.h"
#include "ecs/behaviors/behavior.h"
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
    return PickUpItem(game, unit, item, 0);
}

bool CAbilityInventory::PickUpItem(CSimulation& game, CUnit* unit, CItem* item, int slotPreference) {
    if (!CanPickUpItem(game, unit, item)) return false;

    int slot = FindEmptySlot(slotPreference);
    if (slot < 0) return false;

    // Auto-use powerups
    if (item->IsUseAutomatically() || item->IsPowerup()) {
        // Apply item effects immediately. Permanent tome stats need
        // ItemData->CItem bonus plumbing (not yet available); charged
        // abilities on powerups resolve through the grant path below
        // when the item has an ability list.
        std::cout << "Auto-used powerup item" << std::endl;
        if (unit) {
            ApplyItemBonuses(game, unit, item->GetBonuses());
            for (auto& abilityId : item->GetAbilityList()) {
                auto created = AbilityFactory::CreateAbility(game.AllocateHandleId(), abilityId);
                auto* spell = dynamic_cast<CAbilitySpellBase*>(created.get());
                if (spell) spell->DoEffect(game, unit);
            }
        }
        item->SetInInventory(false);
        item->SetInventorySlot(-1);
        item->SetOwner(nullptr);
        item->SetHidden(true);
        item->SetLife(game, 0); // consume the item
        if (const auto& cb = game.GetItemEventCallback()) {
            cb(unit ? unit->GetHandleId() : -1, item->GetHandleId(), CSimulation::ITEM_EVENT_PICKUP);
            cb(unit ? unit->GetHandleId() : -1, item->GetHandleId(), CSimulation::ITEM_EVENT_USE);
        }
        game.RemoveItem(item);
        return true;
    }

    items_[slot] = item;
    item->SetInInventory(true);
    item->SetInventorySlot(slot);
    item->SetOwner(unit);
    item->SetHidden(true);
    ApplyItemBonuses(game, unit, item->GetBonuses());

    // Grant item abilities to unit (ref inventory ability add).
    for (auto& abilityId : item->GetAbilityList()) {
        if (!unit || unit->GetAbility(abilityId)) continue;
        if (auto created = AbilityFactory::CreateAbility(game.AllocateHandleId(), abilityId)) {
            unit->AddAbility(game, std::move(created));
        }
    }

    std::cout << "Picked up item in slot " << slot << std::endl;
    if (const auto& cb = game.GetItemEventCallback()) {
        cb(unit ? unit->GetHandleId() : -1, item->GetHandleId(), CSimulation::ITEM_EVENT_PICKUP);
    }
    return true;
}

bool CAbilityInventory::DropItem(CSimulation& game, CUnit* unit, int slot) {
    return DropItemAt(game, unit, slot, unit ? unit->GetX() : 0.0f, unit ? unit->GetY() : 0.0f);
}

bool CAbilityInventory::DropItemAt(CSimulation& game, CUnit* unit, int slot, float x, float y) {
    (void)game;
    if (!unit) return false;
    if (!canDrop_ || slot < 0 || slot >= itemCapacity_) return false;
    if (!items_[slot]) return false;

    CItem* item = items_[slot];
    items_[slot] = nullptr;
    RemoveItemBonuses(game, unit, item->GetBonuses());

    // Remove item abilities from unit (only those no other source grants;
    // GetAbilityList holds this item's contribution).
    for (auto& abilityId : item->GetAbilityList()) {
        unit->RemoveAbility(game, abilityId);
    }

    item->SetPosition(x, y);
    item->SetInInventory(false);
    item->SetInventorySlot(-1);
    item->SetOwner(nullptr);
    item->SetHidden(false);

    std::cout << "Dropped item from slot " << slot << std::endl;
    if (const auto& cb = game.GetItemEventCallback()) {
        cb(unit->GetHandleId(), item->GetHandleId(), CSimulation::ITEM_EVENT_DROP);
    }
    return true;
}

bool CAbilityInventory::DropItem(CSimulation& game, CUnit* unit, CItem* item) {
    return DropItemAt(game, unit, item, unit ? unit->GetX() : 0.0f, unit ? unit->GetY() : 0.0f);
}

bool CAbilityInventory::DropItemAt(CSimulation& game, CUnit* unit, CItem* item, float x, float y) {
    const int slot = FindItemSlot(item);
    if (slot < 0) return false;
    return DropItemAt(game, unit, slot, x, y);
}

bool CAbilityInventory::UseItem(CSimulation& game, CUnit* unit, int slot) {
    (void)game;
    if (!canUse_ || slot < 0 || slot >= itemCapacity_) return false;
    if (!items_[slot]) return false;

    CItem* item = items_[slot];

    // Execute the item's active ability when the unit carries it as a spell
    // (potions/scrolls with no-target effects). Targeted item powers are
    // issued as orders and resolve through CheckBeforeQueue below.
    for (auto& abilityId : item->GetAbilityList()) {
        CAbility* ability = unit ? unit->GetAbility(abilityId) : nullptr;
        // CheckCanTargetNoTarget covers disabled/cooldown/mana for spells.
        if (!ability || !ability->CheckCanTargetNoTarget(game, unit, 0)) continue;
        if (ability->CheckCanTargetNoTarget(game, unit, 0)) {
            if (CBehavior* behavior = ability->BeginNoTarget(game, unit, 0)) {
                behavior->SetUnit(unit);
                behavior->Begin(game);
                unit->SetCurrentBehavior(std::unique_ptr<CBehavior>(behavior));
                break;
            }
        }
    }
    if (const auto& cb = game.GetItemEventCallback()) {
        cb(unit ? unit->GetHandleId() : -1, item->GetHandleId(), CSimulation::ITEM_EVENT_USE);
    }

    // Consume charges if perishable
    if (item->IsPerishable() || item->IsCharged()) {
        item->UseCharge();
        if (item->GetCharges() <= 0) {
            // Item consumed, remove from inventory
            items_[slot] = nullptr;
            item->SetInInventory(false);
            item->SetInventorySlot(-1);
            item->SetOwner(nullptr);
            item->SetHidden(true);
            item->SetLife(game, 0); // destroy the item
            game.RemoveItem(item);
            std::cout << "Item consumed (no charges left)" << std::endl;
        }
    }

    return true;
}

bool CAbilityInventory::UseItem(CSimulation& game, CUnit* unit, CItem* item) {
    const int slot = FindItemSlot(item);
    if (slot < 0) return false;
    return UseItem(game, unit, slot);
}

bool CAbilityInventory::MoveItemToSlot(CItem* item, int slot) {
    if (slot < 0 || slot >= itemCapacity_ || !item) return false;
    const int sourceSlot = FindItemSlot(item);
    if (sourceSlot < 0) return false;
    if (sourceSlot == slot) return true;

    CItem* displaced = items_[slot];
    items_[slot] = item;
    items_[sourceSlot] = displaced;
    item->SetInventorySlot(slot);
    if (displaced) {
        displaced->SetInventorySlot(sourceSlot);
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
    return FindEmptySlot(0);
}

int CAbilityInventory::FindEmptySlot(int slotPreference) const {
    if (itemCapacity_ <= 0) return -1;
    int start = slotPreference % itemCapacity_;
    if (start < 0) start += itemCapacity_;
    for (int i = 0; i < itemCapacity_; i++) {
        const int itemIndex = (i + start) % itemCapacity_;
        if (!items_[itemIndex]) return itemIndex;
    }
    return -1;
}

int CAbilityInventory::FindItemSlot(CItem* item) const {
    if (!item) return -1;
    for (int i = 0; i < itemCapacity_; i++) {
        if (items_[i] == item) return i;
    }
    return -1;
}

bool CAbilityInventory::HasItem(CItem* item) const {
    return FindItemSlot(item) >= 0;
}

bool CAbilityInventory::CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    if (!caster) return false;
    // Item orders are executed by the inventory and consumed so they never
    // reach the generic order queue (ref inventory intercept).
    if (orderId == OrderIds::ITEM_USE) {
        if (auto* item = dynamic_cast<CItem*>(target)) {
            if (FindItemSlot(item) >= 0) UseItem(game, caster, item);
        }
        return false;
    }
    if (orderId == OrderIds::ITEM_DROP) {
        if (auto* item = dynamic_cast<CItem*>(target)) {
            if (FindItemSlot(item) >= 0) DropItem(game, caster, item);
        }
        return false;
    }
    if (orderId == OrderIds::ITEM_GIVE) {
        if (auto* item = dynamic_cast<CItem*>(target)) {
            // Giving is handled by the giver dropping; pickup is a separate
            // order on the receiver. Consume here to avoid double handling.
            (void)item;
        }
        return false;
    }
    return true;
}

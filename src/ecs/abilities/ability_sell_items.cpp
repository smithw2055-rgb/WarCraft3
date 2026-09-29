#include "ability_sell_items.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/item.h"
#include "ecs/abilities/ability_inventory.h"
#include "data/itemdata.h"
#include <iostream>
#include <algorithm>

CAbilitySellItems::CAbilitySellItems(int handleId)
    : CAbility(handleId, War3ID("Asei"), CAbilityCategory::SELL_ITEMS) {
    iconShowing_ = false;
}

CAbilitySellItems::CAbilitySellItems(int handleId, const std::vector<War3ID>& itemsSold)
    : CAbilitySellItems(handleId) {
    itemsSold_ = itemsSold;
}

void CAbilitySellItems::OnAdd(CSimulation& game, CUnit* unit) {
    (void)unit;
    for (const auto& entry : game.GetGlobalShopStock()) {
        if (entry.isUnit) AddStockUnit(entry.id, entry.current, entry.max);
        else AddStockItem(entry.id, entry.current, entry.max);
    }
}

bool CAbilitySellItems::CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    // OrderId encodes item index: (orderId & 0xFF00) >> 8 - 1
    int itemIndex = ((orderId & 0xFF00) >> 8) - 1;
    if (itemIndex < 0 || itemIndex >= (int)itemsSold_.size()) {
        return true; // not a valid item order
    }

    War3ID itemTypeId = itemsSold_[itemIndex];
    const CItemType* itemType = game.GetItemData() ? game.GetItemData()->GetItem(itemTypeId) : nullptr;

    // Find a hero for the player to give the item to
    int playerIndex = orderId & 0xFF;
    if (playerIndex < 0 || playerIndex >= game.GetPlayerCount()) {
        playerIndex = caster->GetPlayerIndex();
    }
    auto* player = game.GetPlayer(playerIndex);
    if (!player) return false;

    // Check if player can afford it
    int itemGoldCost = itemType ? itemType->goldCost : 0;
    int itemLumberCost = itemType ? itemType->lumberCost : 0;
    if (player->GetGold() < itemGoldCost) {
        std::cout << "Not enough gold to buy item!" << std::endl;
        return false;
    }
    if (player->GetLumber() < itemLumberCost) {
        std::cout << "Not enough lumber to buy item!" << std::endl;
        return false;
    }

    // Find a hero with inventory
    CUnit* hero = nullptr;
    for (auto* unit : player->GetUnits()) {
        if (unit->IsHero() && !unit->IsDead()) {
            auto* inventory = dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv")));
            if (inventory && inventory->FindEmptySlot() >= 0) {
                hero = unit;
                break;
            }
        }
    }

    if (!hero) {
        std::cout << "No hero with empty inventory found!" << std::endl;
        return false;
    }

    // Stock gate (after all failure exits): only applies when a stock
    // entry exists for this id; legacy unlimited shops are unaffected.
    if (FindStock(itemTypeId) && !ConsumeStock(itemTypeId)) {
        std::cout << "Out of stock: " << itemTypeId.ToString() << std::endl;
        return false;
    }

    // Deduct gold
    player->AddGold(-itemGoldCost);
    player->AddLumber(-itemLumberCost);

    // Create item at shop location
    auto* item = game.CreateItem(itemTypeId, caster->GetX(), caster->GetY());
    if (!item) return false;

    // Give item to hero
    auto* inventory = dynamic_cast<CAbilityInventory*>(hero->GetAbility(War3ID("AInv")));
    if (inventory) {
        inventory->PickUpItem(game, hero, item);
        std::cout << "Sold item " << itemTypeId.ToString() << " to hero" << std::endl;
    }

    return false; // order consumed
}

void CAbilitySellItems::AddStockItem(War3ID id, int current, int max) {
    for (auto& e : stock_) {
        if (e.id == id && !e.isUnit) {
            e.current = current;
            e.max = max;
            e.timer = 0.0f;
            return;
        }
    }
    stock_.push_back({id, current, max, 0.0f, false});
}

void CAbilitySellItems::AddStockUnit(War3ID id, int current, int max) {
    for (auto& e : stock_) {
        if (e.id == id && e.isUnit) {
            e.current = current;
            e.max = max;
            e.timer = 0.0f;
            return;
        }
    }
    stock_.push_back({id, current, max, 0.0f, true});
}

bool CAbilitySellItems::RemoveStock(War3ID id) {
    const size_t before = stock_.size();
    stock_.erase(std::remove_if(stock_.begin(), stock_.end(),
        [&](const StockEntry& e) { return e.id == id; }), stock_.end());
    return stock_.size() != before;
}

const CAbilitySellItems::StockEntry* CAbilitySellItems::FindStock(War3ID id) const {
    for (const auto& e : stock_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

bool CAbilitySellItems::ConsumeStock(War3ID id) {
    for (auto& e : stock_) {
        if (e.id == id) {
            if (e.current <= 0) return false;
            e.current--;
            return true;
        }
    }
    return true; // no entry: unlimited legacy behavior
}

void CAbilitySellItems::OnTick(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    if (stockReplenishInterval_ <= 0.0f) return;
    const float dt = game.GetSimStepTime();
    for (auto& e : stock_) {
        if (e.current >= e.max) {
            e.timer = 0.0f;
            continue;
        }
        e.timer += dt;
        while (e.timer >= stockReplenishInterval_ && e.current < e.max) {
            e.timer -= stockReplenishInterval_;
            e.current++;
        }
    }
}

#include "ability_sell_items.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/item.h"
#include "ecs/abilities/ability_inventory.h"
#include "data/itemdata.h"
#include "data/unitdata.h"
#include "ecs/unit_type.h"
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
    // Unit-slot hire orders carry the 0x10000 bit (item orders do not).
    if (orderId & 0x10000) {
        return HireUnit(game, caster, orderId);
    }
    // Pawn orders carry the 0x20000 bit: seller holds the item in the slot.
    if (orderId & 0x20000) {
        int playerIndex = orderId & 0xFF;
        if (playerIndex < 0 || playerIndex >= game.GetPlayerCount()) {
            playerIndex = caster ? caster->GetPlayerIndex() : -1;
        }
        const int slot = ((orderId & 0xFF00) >> 8) - 1;
        CUnit* seller = nullptr;
        if (auto* player = game.GetPlayer(playerIndex)) {
            for (auto* unit : player->GetUnits()) {
                if (!unit || unit->IsDead()) continue;
                auto* inv = dynamic_cast<CAbilityInventory*>(unit->GetAbility(War3ID("AInv")));
                if (inv && slot >= 0 && slot < inv->GetItemCapacity() && inv->GetItem(slot)) {
                    seller = unit;
                    break;
                }
            }
        }
        if (!seller) return true; // not a pawn order (no holder)
        return PawnItem(game, caster, seller, slot);
    }
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
        // Shop sale event (ref SELL_ITEM): buyer hero, seller shop, sold item.
        if (const auto& cb = game.GetShopEventCallback()) {
            cb(hero->GetHandleId(), caster->GetHandleId(), item->GetHandleId());
        }
    }

    return false; // order consumed
}

bool CAbilitySellItems::HireUnit(CSimulation& game, CUnit* caster, int orderId) {
    if (!caster) return false;
    int playerIndex = orderId & 0xFF;
    if (playerIndex < 0 || playerIndex >= game.GetPlayerCount()) {
        playerIndex = caster->GetPlayerIndex();
    }
    auto* player = game.GetPlayer(playerIndex);
    if (!player) return false;
    // Unit-stock catalog in listed order (ref shop stock entries).
    int slot = ((orderId & 0xFF00) >> 8) - 1;
    int stockIdx = -1;
    {
        int seen = 0;
        for (size_t i = 0; i < stock_.size(); i++) {
            if (!stock_[i].isUnit) continue;
            if (seen == slot) {
                stockIdx = (int)i;
                break;
            }
            seen++;
        }
    }
    if (stockIdx < 0) return true; // not a valid unit order
    const War3ID typeId = stock_[stockIdx].id;
    const CUnitType* type =
        game.GetUnitData() ? game.GetUnitData()->GetUnitType(typeId) : nullptr;
    if (!type) return false;
    const int goldCost = type->GetGoldCost();
    const int lumberCost = type->GetLumberCost();
    if (player->GetGold() < goldCost) {
        std::cout << "Not enough gold to hire unit!" << std::endl;
        return false;
    }
    if (player->GetLumber() < lumberCost) {
        std::cout << "Not enough lumber to hire unit!" << std::endl;
        return false;
    }
    if (stock_[stockIdx].current <= 0) {
        std::cout << "Out of stock: " << typeId.ToString() << std::endl;
        return false;
    }
    stock_[stockIdx].current--;

    player->AddGold(-goldCost);
    player->AddLumber(-lumberCost);

    // Muster beside the shop, facing it (retail hires pop out alongside).
    CUnit* hired = game.CreateUnit(typeId, playerIndex, caster->GetX() + 128.0f,
                                   caster->GetY(), 270.0f);
    if (!hired) {
        player->AddGold(goldCost);
        player->AddLumber(lumberCost);
        stock_[stockIdx].current++;
        return false;
    }
    std::cout << "Hired unit " << typeId.ToString() << std::endl;
    // Merc hire event (ref SELL): buyer is player-level (-1), seller shop.
    if (const auto& cb = game.GetShopUnitEventCallback()) {
        cb(-1, caster->GetHandleId(), hired->GetHandleId());
    }
    return false; // order consumed
}

bool CAbilitySellItems::PawnItem(CSimulation& game, CUnit* shop, CUnit* seller, int slot) {
    if (!shop || !seller) return false;
    auto* inv = dynamic_cast<CAbilityInventory*>(seller->GetAbility(War3ID("AInv")));
    if (!inv) return false;
    CItem* item =
        (slot >= 0 && slot < inv->GetItemCapacity()) ? inv->GetItem(slot) : nullptr;
    if (!item) return false;
    if (!item->IsPawnable()) {
        std::cout << "Item cannot be pawned!" << std::endl;
        return false;
    }
    const CItemType* type =
        game.GetItemData() ? game.GetItemData()->GetItem(item->GetTypeId()) : nullptr;
    const int goldCost = type ? type->goldCost : 0;
    const int value = (int)(goldCost * game.GetGameplayConstants().pawnItemRate);
    if (!inv->RemoveItemAt(game, seller, slot)) return false;
    const int itemHandle = item->GetHandleId();
    game.RemoveItem(item);
    if (auto* player = game.GetPlayer(seller->GetPlayerIndex())) {
        player->AddGold(value);
    }
    std::cout << "Pawned item for " << value << " gold" << std::endl;
    // Pawn event (ref PAWN_ITEM): seller pawner, buyer shop.
    if (const auto& cb = game.GetPawnEventCallback()) {
        cb(shop->GetHandleId(), seller->GetHandleId(), itemHandle);
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

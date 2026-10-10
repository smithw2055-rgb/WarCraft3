#pragma once
#include "ecs/ability.h"
#include <vector>

// CAbilitySellItems - shop selling ability
// Allows buildings to sell items to players
class CAbilitySellItems : public CAbility {
public:
    CAbilitySellItems(int handleId);
    CAbilitySellItems(int handleId, const std::vector<War3ID>& itemsSold);

    void OnAdd(CSimulation& game, CUnit* unit) override;

    // Order handling
    bool CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Configuration
    void AddSoldItem(War3ID id) { itemsSold_.push_back(id); }
    const std::vector<War3ID>& GetSoldItems() const { return itemsSold_; }

    // Stock management (ref shop stock with replenish interval).
    // Entries added via AddItemToStock natives gate purchases; shops with
    // no entry for an id keep the legacy unlimited behavior.
    struct StockEntry {
        War3ID id;
        int current = 0;
        int max = 0;
        float timer = 0.0f;
        bool isUnit = false;
    };
    void AddStockItem(War3ID id, int current, int max);
    void AddStockUnit(War3ID id, int current, int max);
    bool RemoveStock(War3ID id);
    const StockEntry* FindStock(War3ID id) const;
    bool ConsumeStock(War3ID id);

    // Stock management
    void SetStockMax(int v) { stockMax_ = v; }
    void SetStockReplenishInterval(float v) { stockReplenishInterval_ = v; }

private:
    // Mercenary hire from unit-stock slots (order carries 0x10000).
    bool HireUnit(CSimulation& game, CUnit* caster, int orderId);
    // Pawn an inventory item back to this shop for gold (ref pawn shops).
    bool PawnItem(CSimulation& game, CUnit* shop, CUnit* seller, int slot);

    std::vector<War3ID> itemsSold_;
    std::vector<StockEntry> stock_;
    int stockMax_ = 1;
    float stockReplenishInterval_ = 30.0f;
};

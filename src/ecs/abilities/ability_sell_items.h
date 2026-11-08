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

    // Stock management (implements CAbility virtual shop stock contract)
    struct StockEntry {
        War3ID id;
        int current = 0;
        int max = 0;
        float timer = 0.0f;
        bool isUnit = false;
    };
    void AddStockItem(War3ID id, int current, int max) override;
    void AddStockUnit(War3ID id, int current, int max) override;
    bool RemoveStock(War3ID id) override;
    const StockEntry* FindStock(War3ID id) const;
    bool ConsumeStock(War3ID id);

    // Stock management
    void SetStockMax(int v) override { stockMax_ = v; }
    void SetStockReplenishInterval(float v) override { stockReplenishInterval_ = v; }

private:
    bool HireUnit(CSimulation& game, CUnit* caster, int orderId);
    bool PawnItem(CSimulation& game, CUnit* shop, CUnit* seller, int slot);

    std::vector<War3ID> itemsSold_;
    std::vector<StockEntry> stock_;
    int stockMax_ = 1;
    float stockReplenishInterval_ = 30.0f;
};

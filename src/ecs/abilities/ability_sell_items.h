#pragma once
#include "ecs/ability.h"
#include <vector>

// CAbilitySellItems - shop selling ability
// Allows buildings to sell items to players
class CAbilitySellItems : public CAbility {
public:
    CAbilitySellItems(int handleId);

    // Order handling
    bool CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // Configuration
    void AddSoldItem(War3ID id) { itemsSold_.push_back(id); }
    const std::vector<War3ID>& GetSoldItems() const { return itemsSold_; }

    // Stock management
    void SetStockMax(int v) { stockMax_ = v; }
    void SetStockReplenishInterval(float v) { stockReplenishInterval_ = v; }

private:
    std::vector<War3ID> itemsSold_;
    int stockMax_ = 1;
    float stockReplenishInterval_ = 30.0f;
};

#pragma once
#include "ecs/ability.h"
#include <unordered_set>

// CAbilityUpgrade - building upgrade ability
// Allows buildings to upgrade to higher tier versions
class CAbilityUpgrade : public CAbility {
public:
    CAbilityUpgrade(int handleId);

    // Order handling
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;
    void OnTick(CSimulation& game, CUnit* unit) override;
    bool IsUpgrading() const { return upgrading_; }

    // Configuration
    void AddUpgradeTo(War3ID id) { upgradesTo_.insert(id); }
    const std::unordered_set<War3ID>& GetUpgradesTo() const override { return upgradesTo_; }

private:
    std::unordered_set<War3ID> upgradesTo_;
    War3ID pendingId_;
    float progress_ = 0.0f;
    float buildTime_ = 0.0f;
    bool upgrading_ = false;
};

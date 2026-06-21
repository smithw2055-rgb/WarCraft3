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

    // Configuration
    void AddUpgradeTo(War3ID id) { upgradesTo_.insert(id); }
    const std::unordered_set<War3ID>& GetUpgradesTo() const { return upgradesTo_; }

private:
    std::unordered_set<War3ID> upgradesTo_;
};

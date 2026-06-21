#pragma once
#include "ecs/ability.h"

// CAbilityRepair - worker repairs buildings
class CAbilityRepair : public CAbility {
public:
    CAbilityRepair(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnRemove(CSimulation& game, CUnit* unit) override;

    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;

    // Configuration
    float GetRepairRate() const { return repairRate_; }
    void SetRepairRate(float v) { repairRate_ = v; }
    float GetRepairRange() const { return repairRange_; }
    void SetRepairRange(float v) { repairRange_ = v; }
    int GetGoldCostPerHP() const { return goldCostPerHP_; }
    void SetGoldCostPerHP(int v) { goldCostPerHP_ = v; }
    int GetLumberCostPerHP() const { return lumberCostPerHP_; }
    void SetLumberCostPerHP(int v) { lumberCostPerHP_ = v; }

private:
    float repairRate_ = 10.0f; // HP per second
    float repairRange_ = 128.0f;
    int goldCostPerHP_ = 1;
    int lumberCostPerHP_ = 0;
};

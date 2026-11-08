#pragma once
#include "ecs/ability.h"
#include <vector>

// CAbilityLoad - transport loads units
class CAbilityLoad : public CAbility {
public:
    CAbilityLoad(int handleId);

    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;

    // Configuration
    int GetCargoCapacity() const override { return cargoCapacity_; }
    void SetCargoCapacity(int v) override { cargoCapacity_ = v; }
    float GetLoadRange() const { return loadRange_; }
    void SetLoadRange(float v) override { loadRange_ = v; }

private:
    int cargoCapacity_ = 8;
    float loadRange_ = 256.0f;
};

// CAbilityUnload - transport unloads units
class CAbilityUnload : public CAbility {
public:
    CAbilityUnload(int handleId);

    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) override;
};

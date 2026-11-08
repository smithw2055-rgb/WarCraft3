#pragma once
#include "ecs/ability.h"

// CAbilityRally - rally point ability
// Allows buildings to set rally points for trained units
class CAbilityRally : public CAbility {
public:
    CAbilityRally(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;

    // Order handling
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;

    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) override;

    // Rally point access (implements CAbility virtual rally capability contract)
    CWidget* GetRallyTarget() const override { return rallyTarget_; }
    float GetRallyX() const override { return rallyX_; }
    float GetRallyY() const override { return rallyY_; }
    bool HasRallyPoint() const override { return rallyTarget_ != nullptr || hasRallyPoint_; }
    bool IsExplicitlySet() const override { return explicitlySet_; }

    void ApplyToTrainedUnit(CSimulation& game, CUnit* building, CUnit* trainedUnit) const;
    void ClearRallyPoint();

private:
    CWidget* rallyTarget_ = nullptr;
    float rallyX_ = 0, rallyY_ = 0;
    bool hasRallyPoint_ = false;
    bool explicitlySet_ = false;
};

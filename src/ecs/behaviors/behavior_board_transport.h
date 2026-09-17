#pragma once
#include "ecs/behaviors/behavior.h"

class CWidget;

// CBehaviorBoardTransport - transport moves adjacent to the passenger, then
// loads it (ref CBehaviorBoardTransport, minus the MoveIntoRangeFor pairing
// machinery: range comes from CAbilityLoad directly).
class CBehaviorBoardTransport : public CBehavior {
public:
    CBehaviorBoardTransport(float loadRange, int cargoCapacity);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::BOARD_TRANSPORT; }

    void SetTarget(CWidget* target) { target_ = target; }
    CWidget* GetTarget() const { return target_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    bool TryLoadNow(CSimulation& simulation);

    CWidget* target_ = nullptr;
    float loadRange_ = 256.0f;
    int cargoCapacity_ = 8;
};

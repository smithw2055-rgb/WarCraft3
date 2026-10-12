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

// CBehaviorLoad - passenger walks to the transport then boards it.
// Ref Java cargohold/CBehaviorLoad (passenger perspective). Complements
// BoardTransport (transport perspective) for SMART right-click boarding.
class CBehaviorLoad : public CBehavior {
public:
    CBehaviorLoad(float loadRange);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::BOARD_TRANSPORT; }

    void SetTarget(CWidget* target) { target_ = target; }
    CWidget* GetTarget() const { return target_; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    CWidget* target_ = nullptr;
    float loadRange_ = 256.0f;
};

// CBehaviorDrop - timed unload, one passenger per duration interval.
// Ref Java cargohold/CBehaviorDrop (duration pacing, unloadall order).
class CBehaviorDrop : public CBehavior {
public:
    CBehaviorDrop(float x, float y, float durationSec = 0.5f);

    CBehaviorCategory GetCategory() const override { return CBehaviorCategory::SPELL; }

    bool Update(CSimulation& simulation) override;
    void Begin(CSimulation& simulation) override;
    void End(CSimulation& simulation) override;

private:
    float x_ = 0.0f, y_ = 0.0f;
    float durationSec_ = 0.5f;
    float accum_ = 0.0f;
    int dropIndex_ = 0;
};

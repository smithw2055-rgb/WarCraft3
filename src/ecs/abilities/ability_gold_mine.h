#pragma once
#include "ecs/ability.h"
#include <vector>

// CAbilityGoldMine - gold mine ability
// Tracks gold amount and active miners
class CAbilityGoldMine : public CAbility {
public:
    CAbilityGoldMine(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Gold management
    int GetGold() const { return gold_; }
    void SetGold(int v) { gold_ = v; }
    void AddGold(int v) { gold_ += v; }

    // Mining capacity
    int GetMiningCapacity() const { return miningCapacity_; }
    int GetActiveMinerCount() const { return activeMinerCount_; }
    bool CanAddMiner() const { return activeMinerCount_ < miningCapacity_; }
    void AddMiner() { activeMinerCount_++; }
    void RemoveMiner() { if (activeMinerCount_ > 0) activeMinerCount_--; }

    // Mining duration (seconds)
    float GetMiningDuration() const { return miningDuration_; }
    void SetMiningDuration(float v) { miningDuration_ = v; }

    // Gold per trip
    int GetGoldPerTrip() const { return goldPerTrip_; }
    void SetGoldPerTrip(int v) { goldPerTrip_ = v; }

private:
    int gold_ = 12500; // standard WC3 gold mine
    int miningCapacity_ = 3;
    int activeMinerCount_ = 0;
    float miningDuration_ = 5.0f; // seconds per mining trip
    int goldPerTrip_ = 10;
};

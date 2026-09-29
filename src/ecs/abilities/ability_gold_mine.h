#pragma once
#include "ecs/ability.h"
#include <vector>

class CBehaviorHarvest;

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
    int GetActiveMinerCount() const { return static_cast<int>(activeMiners_.size()); }
    bool CanAddMiner() const { return GetActiveMinerCount() < miningCapacity_; }
    void AddMiner(CBehaviorHarvest* miner);
    void RemoveMiner(CBehaviorHarvest* miner);

    // Mining duration (seconds)
    float GetMiningDuration() const { return miningDuration_; }
    void SetMiningDuration(float v) { miningDuration_ = v; }

    // Gold per trip
    int GetGoldPerTrip() const { return goldPerTrip_; }
    void SetGoldPerTrip(int v) { goldPerTrip_ = v; }

private:
    int gold_ = 12500; // standard WC3 gold mine
    int miningCapacity_ = 3;
    float miningDuration_ = 5.0f; // seconds per mining trip
    int goldPerTrip_ = 10;
    std::vector<CBehaviorHarvest*> activeMiners_;
};

// Overlayed gold mine (Agl2). Gold and depletion live on the hidden parent
// mine. Workers enter this unit; when the parent is empty the overlay dies
// and the parent is restored.
class CAbilityOverlayedGoldMine : public CAbility {
public:
    explicit CAbilityOverlayedGoldMine(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;

    void SetParentMine(CUnit* parentUnit, CAbilityGoldMine* parent);
    CUnit* GetParentUnit() const { return parentUnit_; }
    CAbilityGoldMine* GetParentMine() const { return parent_; }

    int GetGold() const;
    void SetGold(int v);
    int GetMiningCapacity() const { return miningCapacity_; }
    int GetActiveMinerCount() const { return static_cast<int>(activeMiners_.size()); }
    bool CanAddMiner() const { return GetActiveMinerCount() < miningCapacity_; }
    void AddMiner(CBehaviorHarvest* miner);
    void RemoveMiner(CBehaviorHarvest* miner);
    float GetMiningDuration() const { return miningDuration_; }
    void SetMiningDuration(float v) { miningDuration_ = v; }

private:
    void RestoreParent();
    CUnit* parentUnit_ = nullptr;
    CAbilityGoldMine* parent_ = nullptr;
    int miningCapacity_ = 5;
    float miningDuration_ = 5.0f;
    std::vector<CBehaviorHarvest*> activeMiners_;
};

// Harvest face of a mine: the overlay when present, otherwise the base mine.
struct CMineAccess {
    CAbilityGoldMine* base = nullptr;
    CAbilityOverlayedGoldMine* overlay = nullptr;
    bool ok() const { return base || overlay; }
    int Gold() const;
    bool CanAdd() const;
    void Add(CBehaviorHarvest* miner);
    void Remove(CBehaviorHarvest* miner);
    float Duration() const;
};

CMineAccess MineAccessOf(CUnit* unit);

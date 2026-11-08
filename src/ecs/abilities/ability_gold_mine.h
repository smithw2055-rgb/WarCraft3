#pragma once
#include "ecs/ability.h"
#include <vector>

class CBehavior;

// CAbilityGoldMine - gold mine ability
// Tracks gold amount and active miners
class CAbilityGoldMine : public CAbility {
public:
    CAbilityGoldMine(int handleId);

    void OnAdd(CSimulation& game, CUnit* unit) override;
    void OnTick(CSimulation& game, CUnit* unit) override;

    // Gold management (implements CAbility virtual mine contract)
    bool IsResourceMine() const override { return true; }
    int GetGold() const override { return gold_; }
    void SetGold(int v) override { gold_ = v; }
    void AddGold(int v) { gold_ += v; }

    // Mining capacity
    int GetMiningCapacity() const { return miningCapacity_; }
    void SetMiningCapacity(int v) { miningCapacity_ = v; }
    int GetActiveMinerCount() const { return static_cast<int>(activeMiners_.size()); }
    bool IsActivelyWorking() const override { return GetActiveMinerCount() > 0; }
    bool CanAddMiner() const override { return GetActiveMinerCount() < miningCapacity_; }
    void AddMiner(CBehavior* miner) override;
    void RemoveMiner(CBehavior* miner) override;

    // Mining duration (seconds)
    float GetMiningDuration() const override { return miningDuration_; }
    void SetMiningDuration(float v) { miningDuration_ = v; }

    // Gold per trip
    int GetGoldPerTrip() const { return goldPerTrip_; }
    void SetGoldPerTrip(int v) { goldPerTrip_ = v; }

private:
    int gold_ = 12500; // standard WC3 gold mine
    int miningCapacity_ = 5;
    float miningDuration_ = 5.0f; // seconds per mining trip
    int goldPerTrip_ = 10;
    std::vector<CBehavior*> activeMiners_;
};

// Overlayed gold mine (Agl2). Gold and depletion live on the hidden parent
// mine. Workers enter this unit; when the parent is empty the overlay dies
// and the parent is restored.
class CAbilityOverlayedGoldMine : public CAbility {
public:
    explicit CAbilityOverlayedGoldMine(int handleId);
    void OnTick(CSimulation& game, CUnit* unit) override;
    void OnDeath(CSimulation& game, CUnit* unit) override;

    void SetParentMine(CUnit* parentUnit, CAbilityGoldMine* parent = nullptr);
    void BindParentMineUnit(CUnit* parentUnit) override { SetParentMine(parentUnit, nullptr); }
    CUnit* GetParentUnit() const { return parentUnit_; }
    CAbilityGoldMine* GetParentMine() const { return parent_; }

    bool IsResourceMine() const override { return true; }
    int GetGold() const override;
    void SetGold(int v) override;
    int GetMiningCapacity() const { return miningCapacity_; }
    int GetActiveMinerCount() const { return static_cast<int>(activeMiners_.size()); }
    bool IsActivelyWorking() const override { return GetActiveMinerCount() > 0; }
    bool CanAddMiner() const override { return GetActiveMinerCount() < miningCapacity_; }
    void AddMiner(CBehavior* miner) override;
    void RemoveMiner(CBehavior* miner) override;
    float GetMiningDuration() const override { return miningDuration_; }
    void SetMiningDuration(float v) { miningDuration_ = v; }

private:
    void RestoreParent();
    CUnit* parentUnit_ = nullptr;
    CAbilityGoldMine* parent_ = nullptr;
    int miningCapacity_ = 5;
    float miningDuration_ = 5.0f;
    std::vector<CBehavior*> activeMiners_;
};

// Harvest face of a mine: the overlay when present, otherwise the base mine.
struct CMineAccess {
    CAbility* mine = nullptr;
    bool ok() const { return mine != nullptr; }
    int Gold() const;
    bool CanAdd() const;
    void Add(CBehavior* miner);
    void Remove(CBehavior* miner);
    float Duration() const;
};

CMineAccess MineAccessOf(CUnit* unit);

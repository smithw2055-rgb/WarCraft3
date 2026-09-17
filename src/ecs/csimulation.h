#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include "ecs/fwd.h"
#include "ecs/unit.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/player.h"
#include "ecs/handle_id.h"
#include "ecs/combat/combat_system.h"
#include "ecs/combat/projectile.h"
#include "data/gameplay_constants.h"

class AudioSystem;

class CSimulation {
public:
    static constexpr int ITEM_EVENT_DROP = 85;
    static constexpr int ITEM_EVENT_PICKUP = 86;
    static constexpr int ITEM_EVENT_USE = 87;

    CSimulation();
    ~CSimulation();

    // System registration
    void SetUnitData(CUnitData* data) { unitData_ = data; }
    CUnitData* GetUnitData() const { return unitData_; }
    void SetDestructableData(CDestructableData* data) { destructableData_ = data; }
    CDestructableData* GetDestructableData() const { return destructableData_; }
    void SetItemData(CItemData* data) { itemData_ = data; }
    CItemData* GetItemData() const { return itemData_; }
    void SetUpgradeData(CUpgradeData* data) { upgradeData_ = data; }
    CUpgradeData* GetUpgradeData() const { return upgradeData_; }
    void SetGameplayConstants(const CGameplayConstants& c) {
        gameplayConstants_ = c;
        combatSystem_.Configure(c.GetDefenseArmor(), c.damageBonus);
    }
    const CGameplayConstants& GetGameplayConstants() const { return gameplayConstants_; }

    // Game loop
    void Update();

    // Unit management
    virtual CUnit* CreateUnit(War3ID typeId, int playerIndex, float x, float y, float facing);
    int AllocateHandleId() { return handleIdAlloc_.CreateId(); }
    CUnit* GetUnit(int handleId) const;
    virtual void RemoveUnit(CUnit* unit);
    const std::vector<CUnit*>& GetUnits() const { return units_; }
    std::vector<CUnit*>& GetUnits() { return units_; }
    virtual CDestructable* CreateDestructable(War3ID typeId, float x, float y, float life);
    CDestructable* GetDestructable(int handleId) const;
    virtual void RemoveDestructable(CDestructable* destructable);
    const std::vector<CDestructable*>& GetDestructables() const { return destructables_; }
    virtual CItem* CreateItem(War3ID typeId, float x, float y);
    CItem* GetItem(int handleId) const;
    virtual void RemoveItem(CItem* item);
    const std::vector<CItem*>& GetItems() const { return items_; }
    CWidget* GetWidget(int handleId) const;

    // Player management
    CPlayer* GetPlayer(int index) const;
    int GetPlayerCount() const { return (int)players_.size(); }
    void AddPlayer(std::unique_ptr<CPlayer> player);

    // Timing
    int GetGameTurnTick() const { return gameTurnTick_; }
    float GetSimStepTime() const { return SIM_STEP_TIME; }
    static constexpr float SIM_STEP_TIME = 1.0f / 20.0f;

    // Game time
    float GetGameTimeOfDay() const { return gameTimeOfDay_; }
    void SetGameTimeOfDay(float t) { gameTimeOfDay_ = t; }
    void AdvanceGameTime(float dt);
    // Time-of-day control (ref SuspendTimeOfDay / time-of-day scale).
    bool IsTimeOfDaySuspended() const { return timeOfDaySuspended_; }
    void SetTimeOfDaySuspended(bool v) { timeOfDaySuspended_ = v; }
    float GetTimeOfDayScale() const { return timeOfDayScale_; }
    void SetTimeOfDayScale(float v) { timeOfDayScale_ = v; }
    // Hero XP control (SuspendHeroXP blocks AddXP gains map-wide).
    bool IsHeroXPSuspended() const { return heroXPSuspended_; }
    void SetHeroXPSuspended(bool v) { heroXPSuspended_ = v; }

    // Day/night
    bool IsDaytime() const { return daytime_; }
    bool IsNighttime() const { return !daytime_; }

    // Audio
    void SetAudioSystem(AudioSystem* audio) { audio_ = audio; }
    AudioSystem* GetAudioSystem() const { return audio_; }

    // Pathing
    CWorldCollision* GetWorldCollision() const { return worldCollision_.get(); }
    CPathfindingProcessor* GetPathfinding() const { return pathfindingProcessor_.get(); }
    void SetPathingMap(const WpmData* wpm, float worldOffsetX = 0.0f, float worldOffsetY = 0.0f) {
        if (pathfindingProcessor_) {
            pathfindingProcessor_->SetPathingMap(wpm, worldOffsetX, worldOffsetY);
        }
    }

    // Combat
    CCombatSystem* GetCombatSystem() { return &combatSystem_; }
    const CCombatSystem* GetCombatSystem() const { return &combatSystem_; }

    // Deterministic synced RNG (ref GameTurnManager lockstep: every client
    // must roll the same sequence; std::rand is banned in sim code).
    // LCG, seeded fixed per game so replays stay reproducible.
    float SimRand01();
    float SimRandRange(float lo, float hi) { return lo + (hi - lo) * SimRand01(); }

    // Projectiles (see combat/projectile.h). Spawned by ranged attacks at
    // their damage point; stepped in Update() after units.
    void SpawnProjectile(CProjectile&& p) { projectiles_.push_back(std::move(p)); }
    const std::vector<CProjectile>& GetProjectiles() const { return projectiles_; }
    void UpdateProjectiles();

    // Takes ownership of an externally constructed unit (tests, editor).
    // Visible to lookups immediately, joins units_ on the next Update.
    CUnit* AdoptUnit(std::unique_ptr<CUnit> unit);

    // Callbacks
    void AddPostUpdateCallback(std::function<void()> cb);
    void SetUnitDeathCallback(std::function<void(int unitHandle, int killerHandle, int playerIndex)> cb) {
        unitDeathCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int)>& GetUnitDeathCallback() const { return unitDeathCallback_; }
    void SetUnitDamageCallback(std::function<void(int unitHandle, int sourceHandle, float damage)> cb) {
        unitDamageCallback_ = std::move(cb);
    }
    const std::function<void(int, int, float)>& GetUnitDamageCallback() const { return unitDamageCallback_; }
    void SetUnitOrderCallback(std::function<void(int unitHandle, int orderId, int targetHandle, float x, float y)> cb) {
        unitOrderCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int, float, float)>& GetUnitOrderCallback() const { return unitOrderCallback_; }
    void SetItemEventCallback(std::function<void(int unitHandle, int itemHandle, int eventType)> cb) {
        itemEventCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int)>& GetItemEventCallback() const { return itemEventCallback_; }
    void SetItemRemovedCallback(std::function<void(int itemHandle)> cb) {
        itemRemovedCallback_ = std::move(cb);
    }
    const std::function<void(int)>& GetItemRemovedCallback() const { return itemRemovedCallback_; }
    void SetUnitTrainedCallback(std::function<void(int buildingHandle, int trainedHandle)> cb) {
        unitTrainedCallback_ = std::move(cb);
    }
    const std::function<void(int, int)>& GetUnitTrainedCallback() const { return unitTrainedCallback_; }
    void SetUnitSummonedCallback(std::function<void(int summonerHandle, int summonedHandle)> cb) {
        unitSummonedCallback_ = std::move(cb);
    }
    const std::function<void(int, int)>& GetUnitSummonedCallback() const { return unitSummonedCallback_; }
    void SetResearchFinishedCallback(std::function<void(int unitHandle, int upgradeId)> cb) {
        researchFinishedCallback_ = std::move(cb);
    }
    const std::function<void(int, int)>& GetResearchFinishedCallback() const {
        return researchFinishedCallback_;
    }
    void SetHeroRevivedCallback(std::function<void(int reviverHandle, int revivedHandle)> cb) {
        heroRevivedCallback_ = std::move(cb);
    }
    const std::function<void(int, int)>& GetHeroRevivedCallback() const { return heroRevivedCallback_; }

private:
    void ProcessUnitRemovals();
    void ProcessNewUnits();
    void ProcessDestructableRemovals();
    void ProcessNewDestructables();
    void ProcessItemRemovals();

    int gameTurnTick_ = 0;

    // Game state
    float gameTimeOfDay_ = 12.0f; // noon
    bool daytime_ = true;
    bool timeOfDaySuspended_ = false;
    float timeOfDayScale_ = 1.0f;
    bool heroXPSuspended_ = false;
    CGameplayConstants gameplayConstants_;

    // Unit lists (using raw pointers managed by unique_ptr in maps)
    std::vector<CUnit*> units_;
    std::vector<CUnit*> newUnits_;
    std::vector<CUnit*> removedUnits_;
    std::vector<CProjectile> projectiles_;
    std::unordered_map<int, std::unique_ptr<CUnit>> unitMap_;
    std::vector<CDestructable*> destructables_;
    std::vector<CDestructable*> newDestructables_;
    std::vector<CDestructable*> removedDestructables_;
    std::unordered_map<int, std::unique_ptr<CDestructable>> destructableMap_;
    std::vector<CItem*> items_;
    std::vector<CItem*> removedItems_;
    std::unordered_map<int, std::unique_ptr<CItem>> itemMap_;

    // Players
    std::vector<std::unique_ptr<CPlayer>> players_;

    // Systems
    CUnitData* unitData_ = nullptr;
    CDestructableData* destructableData_ = nullptr;
    CAbilityData* abilityData_ = nullptr;
    CItemData* itemData_ = nullptr;
    CUpgradeData* upgradeData_ = nullptr;

    CCombatSystem combatSystem_;
    uint32_t simRandState_ = 0x12345678u; // deterministic sim RNG (LCG)
    std::unique_ptr<CWorldCollision> worldCollision_;    std::unique_ptr<CPathfindingProcessor> pathfindingProcessor_;
    AudioSystem* audio_ = nullptr;
    HandleIdAllocator handleIdAlloc_;

    // Post-update callbacks
    std::vector<std::function<void()>> postUpdateCallbacks_;
    std::function<void(int, int, int)> unitDeathCallback_;
    std::function<void(int, int, float)> unitDamageCallback_;
    std::function<void(int, int, int, float, float)> unitOrderCallback_;
    std::function<void(int, int, int)> itemEventCallback_;
    std::function<void(int)> itemRemovedCallback_;
    std::function<void(int, int)> unitTrainedCallback_;
    std::function<void(int, int)> unitSummonedCallback_;
    std::function<void(int, int)> researchFinishedCallback_;
    std::function<void(int, int)> heroRevivedCallback_;
};

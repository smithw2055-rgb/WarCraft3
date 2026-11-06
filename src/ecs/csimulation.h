#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include "ecs/fwd.h"
#include "ecs/sim_events.h"
#include "ecs/ecs_registry.h"
#include "ecs/spatial_grid.h"
#include "ecs/sim_systems.h"
#include "ecs/fog_state.h"
#include "ecs/lockstep.h"
#include "ecs/unit.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/player.h"
#include "ecs/handle_id.h"
#include "ecs/combat/combat_system.h"
#include "ecs/combat/projectile.h"
#include "data/gameplay_constants.h"
#include "map/w3e.h"

class AudioSystem;

class CSimulation {
public:
    static constexpr int ITEM_EVENT_DROP = 85;
    static constexpr int ITEM_EVENT_PICKUP = 86;
    static constexpr int ITEM_EVENT_USE = 87;

    using CombatTextKind = SimCombatTextKind;

    CSimulation();
    ~CSimulation();

    // Multicast EventBus access (Godot Signal / EventBus modernization)
    SimEventBus& Events() { return events_; }
    const SimEventBus& Events() const { return events_; }

    // Sparse-Set ECS Component Registry access (Phase 3 & 5 modernization)
    SimEcsRegistry& Registry() { return ecsRegistry_; }
    const SimEcsRegistry& Registry() const { return ecsRegistry_; }

    // Spatial Hash Grid access (Phase 5 Stage C modernization)
    SpatialHashGrid& SpatialGrid() { return spatialGrid_; }
    const SpatialHashGrid& SpatialGrid() const { return spatialGrid_; }
    std::vector<CUnit*> QueryUnitsInRadius(float x, float y, float radius) const {
        return spatialGrid_.QueryRadius(x, y, radius, true);
    }

    // Modular Simulation System Pipeline access (Phase 5 Stage C modernization)
    SimulationSystemPipeline& SystemPipeline() { return systemPipeline_; }
    const SimulationSystemPipeline& SystemPipeline() const { return systemPipeline_; }

    // System registration
    void SetUnitData(CUnitData* data) { unitData_ = data; }
    CUnitData* GetUnitData() const { return unitData_; }
    void SetDestructableData(CDestructableData* data) { destructableData_ = data; }
    CDestructableData* GetDestructableData() const { return destructableData_; }
    void SetDoodadData(CDoodadData* data) { doodadData_ = data; }
    CDoodadData* GetDoodadData() const { return doodadData_; }
    void SetItemData(CItemData* data) { itemData_ = data; }
    CItemData* GetItemData() const { return itemData_; }
    void SetUpgradeData(CUpgradeData* data) { upgradeData_ = data; }
    CUpgradeData* GetUpgradeData() const { return upgradeData_; }
    void SetAbilityData(CAbilityData* data) { abilityData_ = data; }
    CAbilityData* GetAbilityData() const { return abilityData_; }
    void SetGameplayConstants(const CGameplayConstants& c) {
        gameplayConstants_ = c;
        combatSystem_.Configure(c.GetDefenseArmor(), c.damageBonus);
        combatSystem_.ConfigureMagicRules(c.etherealDamageBonusMagic,
                                          c.etherealDamageBonusSpells,
                                          c.magicImmuneResistsDamage,
                                          c.magicImmuneResistsUltimates,
                                          c.etherealDamageBonusAlly);
    }
    const CGameplayConstants& GetGameplayConstants() const { return gameplayConstants_; }

    // Game loop
    void Update();

    // Pause (ref PauseGame): frozen ticks; VM threads keep their own clock.
    void SetPaused(bool v) { paused_ = v; }
    bool IsPaused() const { return paused_; }

    // Unit management
    virtual CUnit* CreateUnit(War3ID typeId, int playerIndex, float x, float y, float facing);
    // Map-init dedupe window (ref CreateUnit): true while loading units.doo
    // and running the map script's placement functions; false in live play.
    void SetMapInitPhase(bool v) { mapInitPhase_ = v; }
    bool IsMapInitPhase() const { return mapInitPhase_; }
    int AllocateHandleId() { return handleIdAlloc_.CreateId(); }
    CUnit* GetUnit(int handleId) const;
    virtual void RemoveUnit(CUnit* unit);
    const std::vector<CUnit*>& GetUnits() const { return units_; }
    std::vector<CUnit*>& GetUnits() { return units_; }
    virtual CDestructable* CreateDestructable(War3ID typeId, float x, float y, float life,
                                              int rotationDeg = 0);
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

    // Lockstep (ref GameTurnManager): player commands funnel here, are
    // logged with the current turn, then forwarded. Returns false when
    // the unit handle does not resolve.
    bool IssueUnitOrder(int unitHandle, std::unique_ptr<COrder> order);
    const std::vector<SimCommand>& LoggedCommands() const { return commandLog_; }
    void ClearCommandLog() { commandLog_.clear(); }

    // Deterministic state hash for desync detection: turn, RNG state,
    // units (sorted by handle) and live projectiles. Floats hash by bit
    // pattern; valid for same-binary comparison only.
    uint64_t HashState() const;

    uint32_t GetRandState() const { return simRandState_; }
    void SetRandState(uint32_t state) { simRandState_ = state; }
    // Reseed the whole game RNG (sim stream + combat stream) together;
    // JASS SetRandomSeed funnels here so replays reproduce.
    void SeedRng(uint32_t seed) {
        simRandState_ = seed;
        combatSystem_.SetSeed(seed);
    }

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

    // Terrain reference for JASS terrain queries (GetTerrainType and
    // friends). Set by the map loader next to SetPathingMap; may be null in
    // tests, in which case queries return safe defaults. Non-const because
    // SetTerrainType edits tiles in place (loader-owned, version-bumped).
    void SetTerrainMap(W3EMap* w3e) { terrainMap_ = w3e; }
    W3EMap* GetTerrainMap() const { return terrainMap_; }
    // Cliff layer at a world point (ref SimulationRenderController /
    // getTerrainHeight: corner LayerHeight, 999 when off-map or mapless so
    // off-map attackers never count as downhill).
    int GetTerrainHeight(float x, float y) const;
    // Uphill accuracy (ref CUnitDefaultAccuracyCheckListener): weapon
    // attacks from a lower cliff layer at a higher unit miss with
    // ChanceToMiss. Rolls the sim stream; no-op (no RNG consumed) on flat
    // ground, vs non-units, or for non-attacks.
    bool CheckHighlandMiss(CUnit* attacker, const CWidget* target, bool isAttack);
    // Line-of-sight obstruction check (cliff height and WPM vision blocker checks).
    bool IsLineOfSightBlocked(float fromX, float fromY, float toX, float toY, bool isFlying = false) const;

    // Water deformation toggle (ref SetWaterDeforms). The water mesh
    // adds a traveling wave while this is true.
    void SetWaterDeforms(bool v) { waterDeforms_ = v; }
    bool GetWaterDeforms() const { return waterDeforms_; }

    // Script pathing-cell edit (ref SetTerrainPathable). Pathing-type
    // ordinals match IsTerrainPathable (0 any,1 walk,2 fly,3 build,
    // 4 peon,5 blight,6 float,7 amphibious); flag=false clears the block.
    bool SetTerrainPathableCell(float x, float y, int pathType, bool pathable);

    // Script blight overrides from SetBlight* natives (ref blight state).
    // W3E-baked blight bits are read from the terrain map; these add
    // (blighted=true) or clear (false) blight on top. Last match wins.
    struct BlightRect {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        bool blighted = true;
    };
    struct BlightCircle {
        float x = 0, y = 0, radius = 0;
        bool blighted = true;
    };
    void SetBlightRect(float minX, float minY, float maxX, float maxY, bool blighted) {
        blightRects_.push_back({minX, minY, maxX, maxY, blighted});
    }
    void SetBlightCircle(float x, float y, float radius, bool blighted) {
        blightCircles_.push_back({x, y, radius, blighted});
    }
    bool IsPointBlighted(float x, float y) const;

    // Pathing
    CWorldCollision* GetWorldCollision() const { return worldCollision_.get(); }
    CPathfindingProcessor* GetPathfinding() const { return pathfindingProcessor_.get(); }
    void SetPathingMap(WpmData* wpm, float worldOffsetX = 0.0f, float worldOffsetY = 0.0f) {
        if (pathfindingProcessor_) {
            pathfindingProcessor_->SetPathingMap(wpm, worldOffsetX, worldOffsetY);
        }
    }

    // AddItemToAllStock / AddUnitToAllStock. Shops created later copy this
    // list when their sell ability is added.
    struct GlobalShopStockEntry {
        War3ID id;
        int current = 0;
        int max = 0;
        bool isUnit = false;
    };
    void AddGlobalShopStock(War3ID id, int current, int max, bool isUnit);
    void RemoveGlobalShopStock(War3ID id, bool isUnit);
    const std::vector<GlobalShopStockEntry>& GetGlobalShopStock() const { return globalShopStock_; }

    // Combat
    CCombatSystem* GetCombatSystem() { return &combatSystem_; }
    const CCombatSystem* GetCombatSystem() const { return &combatSystem_; }

    // Scripted fog (modifiers + forced paints; see fog_state.h).
    CFogSystem* GetFogSystem() { return &fogSystem_; }
    const CFogSystem* GetFogSystem() const { return &fogSystem_; }
    // Rebuild the sight grid from live units (day/night radii).
    void UpdateFogSight();

    // Delayed actions for timed script effects (ref UnitDamagePoint delay).
    // Fired from Update() once their countdown elapses; deterministic.
    void AddDelayedAction(float delaySeconds, std::function<void()> fn) {
        delayedActions_.push_back({delaySeconds, std::move(fn)});
    }

    // Timed hazards (Stasis Trap mines, Tornado wanderers): position +
    // radius areas that tick periodically until expiry or consumption.
    struct SimHazard {
        float x = 0.0f, y = 0.0f;
        float radius = 0.0f;
        int ticksLeft = 0;
        int tickPeriod = 1;
        int tickCount = 0;
        float tx = 0.0f, ty = 0.0f, speed = 0.0f;
        std::function<bool(CSimulation&, SimHazard&)> tick;
    };
    void AddHazard(SimHazard hazard) { hazards_.push_back(std::move(hazard)); }
    const std::vector<SimHazard>& GetHazards() const { return hazards_; }

    // Deterministic synced RNG
    float SimRand01();
    float SimRandRange(float lo, float hi) { return lo + (hi - lo) * SimRand01(); }

    // Projectiles
    void SpawnProjectile(CProjectile&& p) { projectiles_.push_back(std::move(p)); }
    const std::vector<CProjectile>& GetProjectiles() const { return projectiles_; }
    void UpdateProjectiles();

    // Takes ownership of an externally constructed unit (tests, editor).
    CUnit* AdoptUnit(std::unique_ptr<CUnit> unit);
    void InstallUnitHooks(CUnit* unit);

    // ============================================================
    // Callbacks & Multicast Signal Adapters
    // All legacy Set*Callback / Get*Callback methods transparently
    // route through SimEventBus (events_) so both legacy callers
    // and modern Signal::Connect subscribers receive events.
    // ============================================================
    void AddPostUpdateCallback(std::function<void()> cb);

    void SetUnitDeathCallback(std::function<void(int unitHandle, int killerHandle, int playerIndex)> cb) {
        events_.onUnitDeath.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int)>& GetUnitDeathCallback() const {
        return events_.onUnitDeath.GetDispatcher();
    }

    void SetUnitDamageCallback(std::function<void(int unitHandle, int sourceHandle, float damage)> cb) {
        events_.onUnitDamage.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, float)>& GetUnitDamageCallback() const {
        return events_.onUnitDamage.GetDispatcher();
    }

    void SetUnitOrderCallback(std::function<void(int unitHandle, int orderId, int targetHandle, float x, float y)> cb) {
        events_.onUnitOrder.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int, float, float)>& GetUnitOrderCallback() const {
        return events_.onUnitOrder.GetDispatcher();
    }

    void SetItemEventCallback(std::function<void(int unitHandle, int itemHandle, int eventType)> cb) {
        events_.onItemEvent.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int)>& GetItemEventCallback() const {
        return events_.onItemEvent.GetDispatcher();
    }

    void SetUnitLoadedCallback(std::function<void(int transportHandle, int passengerHandle)> cb) {
        events_.onUnitLoaded.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int)>& GetUnitLoadedCallback() const {
        return events_.onUnitLoaded.GetDispatcher();
    }

    void SetUnitDecayCallback(std::function<void(int unitHandle)> cb) {
        events_.onUnitDecay.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int)>& GetUnitDecayCallback() const {
        return events_.onUnitDecay.GetDispatcher();
    }

    void SetShopEventCallback(std::function<void(int buyerHandle, int sellerHandle, int itemHandle)> cb) {
        events_.onShopItemSold.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int)>& GetShopEventCallback() const {
        return events_.onShopItemSold.GetDispatcher();
    }

    void SetShopUnitEventCallback(std::function<void(int buyerHandle, int sellerHandle, int hiredHandle)> cb) {
        events_.onShopUnitSold.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int)>& GetShopUnitEventCallback() const {
        return events_.onShopUnitSold.GetDispatcher();
    }

    void SetPawnEventCallback(std::function<void(int buyerHandle, int sellerHandle, int itemHandle)> cb) {
        events_.onItemPawned.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int)>& GetPawnEventCallback() const {
        return events_.onItemPawned.GetDispatcher();
    }

    void SetHeroSkillCallback(std::function<void(int unitHandle, int skillId, int newLevel)> cb) {
        events_.onHeroSkillLearned.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int)>& GetHeroSkillCallback() const {
        return events_.onHeroSkillLearned.GetDispatcher();
    }

    void SetCombatTextCallback(
        std::function<void(int attackerHandle, int victimHandle, CombatTextKind kind, float amount)> cb) {
        events_.onCombatText.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, CombatTextKind, float)>& GetCombatTextCallback() const {
        return events_.onCombatText.GetDispatcher();
    }
    void FireCombatText(int attackerHandle, int victimHandle, CombatTextKind kind, float amount) {
        events_.onCombatText.Emit(attackerHandle, victimHandle, kind, amount);
    }

    void SetQueueEventCallback(std::function<void(int buildingHandle, int typeId, int kind, bool started)> cb) {
        events_.onQueueEvent.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int, bool)>& GetQueueEventCallback() const {
        return events_.onQueueEvent.GetDispatcher();
    }

    void SetConstructEventCallback(std::function<void(int buildingHandle, bool finished)> cb) {
        events_.onConstructEvent.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, bool)>& GetConstructEventCallback() const {
        return events_.onConstructEvent.GetDispatcher();
    }

    void SetDeathExplodeCallback(std::function<void(int unitHandle, War3ID buffId)> cb) {
        events_.onDeathExplode.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, War3ID)>& GetDeathExplodeCallback() const {
        return events_.onDeathExplode.GetDispatcher();
    }
    void FireDeathExplode(int unitHandle, War3ID buffId) {
        events_.onDeathExplode.Emit(unitHandle, buffId);
    }

    void SetOwnerChangedCallback(std::function<void(int unitHandle, int prevOwnerIndex)> cb) {
        events_.onOwnerChanged.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int)>& GetOwnerChangedCallback() const {
        return events_.onOwnerChanged.GetDispatcher();
    }

    void SetItemRemovedCallback(std::function<void(int itemHandle)> cb) {
        events_.onItemRemoved.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int)>& GetItemRemovedCallback() const {
        return events_.onItemRemoved.GetDispatcher();
    }

    void SetUnitTrainedCallback(std::function<void(int buildingHandle, int trainedHandle)> cb) {
        events_.onUnitTrained.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int)>& GetUnitTrainedCallback() const {
        return events_.onUnitTrained.GetDispatcher();
    }

    void SetUnitSummonedCallback(std::function<void(int summonerHandle, int summonedHandle)> cb) {
        events_.onUnitSummoned.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int)>& GetUnitSummonedCallback() const {
        return events_.onUnitSummoned.GetDispatcher();
    }

    void SetResearchFinishedCallback(std::function<void(int unitHandle, int upgradeId)> cb) {
        events_.onResearchFinished.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int)>& GetResearchFinishedCallback() const {
        return events_.onResearchFinished.GetDispatcher();
    }

    void SetHeroRevivedCallback(std::function<void(int reviverHandle, int revivedHandle)> cb) {
        events_.onHeroRevived.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int)>& GetHeroRevivedCallback() const {
        return events_.onHeroRevived.GetDispatcher();
    }

    void SetSpellEventCallback(std::function<void(int casterHandle, int stage, int targetSimHandle,
                                                  float x, float y)> cb) {
        events_.onSpellEvent.SetLegacyCallback(std::move(cb));
    }
    const std::function<void(int, int, int, float, float)>& GetSpellEventCallback() const {
        return events_.onSpellEvent.GetDispatcher();
    }

private:
    void RegisterBuiltInSystems();
    void ProcessUnitRemovals();
    void ProcessNewUnits();
    void ResolveUnitSeparation();
    void ProcessDestructableRemovals();
    void ProcessNewDestructables();
    void ProcessItemRemovals();

    int gameTurnTick_ = 0;
    bool paused_ = false;
    bool mapInitPhase_ = false;

    // Game state
    float gameTimeOfDay_ = 12.0f; // noon
    bool daytime_ = true;
    bool timeOfDaySuspended_ = false;
    float timeOfDayScale_ = 1.0f;
    bool heroXPSuspended_ = false;
    CGameplayConstants gameplayConstants_;

    // Unit lists
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
    CDoodadData* doodadData_ = nullptr;
    CAbilityData* abilityData_ = nullptr;
    CItemData* itemData_ = nullptr;
    CUpgradeData* upgradeData_ = nullptr;

    CCombatSystem combatSystem_;
    CFogSystem fogSystem_;
    std::vector<CFogSightSource> sightScratch_;
    uint32_t simRandState_ = 0x12345678u;
    std::vector<SimCommand> commandLog_;
    W3EMap* terrainMap_ = nullptr;
    bool waterDeforms_ = true;
    std::vector<BlightRect> blightRects_;
    std::vector<BlightCircle> blightCircles_;
    struct DelayedAction {
        float secondsLeft = 0.0f;
        std::function<void()> fn;
    };
    std::vector<DelayedAction> delayedActions_;
    std::vector<SimHazard> hazards_;
    void UpdateHazards();
    std::unique_ptr<CWorldCollision> worldCollision_;
    std::unique_ptr<CPathfindingProcessor> pathfindingProcessor_;
    std::vector<GlobalShopStockEntry> globalShopStock_;
    AudioSystem* audio_ = nullptr;
    HandleIdAllocator handleIdAlloc_;

    // Post-update callbacks, central multicast EventBus, Sparse-Set ECS & Spatial Grid
    std::vector<std::function<void()>> postUpdateCallbacks_;
    SimEventBus events_;
    SimEcsRegistry ecsRegistry_;
    SpatialHashGrid spatialGrid_{256.0f};
    SimulationSystemPipeline systemPipeline_;
};

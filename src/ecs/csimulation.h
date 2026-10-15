#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include "ecs/fwd.h"
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

    CSimulation();
    ~CSimulation();

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
    // Callbacks must resolve units by handle (raw pointers may dangle) and
    // must only use SimRand for game-affecting rolls. Returns true from
    // tick to consume the hazard early (mine triggered).
    struct SimHazard {
        float x = 0.0f, y = 0.0f;
        float radius = 0.0f;
        int ticksLeft = 0;
        int tickPeriod = 1;
        int tickCount = 0;
        // Wander (tornadoes): stepped toward (tx, ty) every tick.
        float tx = 0.0f, ty = 0.0f, speed = 0.0f;
        std::function<bool(CSimulation&, SimHazard&)> tick;
    };
    void AddHazard(SimHazard hazard) { hazards_.push_back(std::move(hazard)); }
    const std::vector<SimHazard>& GetHazards() const { return hazards_; }

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
    // Transport boarding (transport handle, passenger handle): JASS LOADED.
    void SetUnitLoadedCallback(std::function<void(int transportHandle, int passengerHandle)> cb) {
        unitLoadedCallback_ = std::move(cb);
    }
    const std::function<void(int, int)>& GetUnitLoadedCallback() const { return unitLoadedCallback_; }
    // Corpse begins decaying (retail EVENT_UNIT_DECAY): unit handle.
    void SetUnitDecayCallback(std::function<void(int unitHandle)> cb) {
        unitDecayCallback_ = std::move(cb);
    }
    const std::function<void(int)>& GetUnitDecayCallback() const { return unitDecayCallback_; }
    // Shop sales need the seller too (buyer/seller/item sim handles).
    void SetShopEventCallback(std::function<void(int buyerHandle, int sellerHandle, int itemHandle)> cb) {
        shopEventCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int)>& GetShopEventCallback() const { return shopEventCallback_; }
    // Mercenary hire (buyer may be -1; seller shop and hired unit sim handles).
    void SetShopUnitEventCallback(std::function<void(int buyerHandle, int sellerHandle, int hiredHandle)> cb) {
        shopUnitEventCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int)>& GetShopUnitEventCallback() const {
        return shopUnitEventCallback_;
    }
    // Pawn-back sales (buyer shop, seller pawner, item sim handles).
    void SetPawnEventCallback(std::function<void(int buyerHandle, int sellerHandle, int itemHandle)> cb) {
        pawnEventCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int)>& GetPawnEventCallback() const { return pawnEventCallback_; }
    // Hero skill learning (unit handle, skill rawcode, new skill level).
    void SetHeroSkillCallback(std::function<void(int unitHandle, int skillId, int newLevel)> cb) {
        heroSkillCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int)>& GetHeroSkillCallback() const { return heroSkillCallback_; }
    // Floating combat text (ref spawnTextTag for miss/crit/bash): the sim
    // only reports the event; presentation (JASS text tags) subscribes.
    // MISS amount is 0, CRIT amount is the final damage number, BASH
    // amount is the bonus damage (0 until bash carries a damage bonus).
    enum class CombatTextKind : uint8_t { MISS, CRIT, BASH };
    void SetCombatTextCallback(
        std::function<void(int attackerHandle, int victimHandle, CombatTextKind kind, float amount)> cb) {
        combatTextCallback_ = std::move(cb);
    }
    const std::function<void(int, int, CombatTextKind, float)>& GetCombatTextCallback() const {
        return combatTextCallback_;
    }
    void FireCombatText(int attackerHandle, int victimHandle, CombatTextKind kind, float amount) {
        if (combatTextCallback_) combatTextCallback_(attackerHandle, victimHandle, kind, amount);
    }
    // Queue lifecycle (building handle, type rawcode, kind 0 = train,
    // 1 = research, 2 = upgrade morph; started false = cancelled, except
    // upgrades where it means finished).
    void SetQueueEventCallback(std::function<void(int buildingHandle, int typeId, int kind, bool started)> cb) {
        queueEventCallback_ = std::move(cb);
    }
    const std::function<void(int, int, int, bool)>& GetQueueEventCallback() const {
        return queueEventCallback_;
    }
    // Construction lifecycle (building handle, true = finished).
    void SetConstructEventCallback(std::function<void(int buildingHandle, bool finished)> cb) {
        constructEventCallback_ = std::move(cb);
    }
    const std::function<void(int, bool)>& GetConstructEventCallback() const {
        return constructEventCallback_;
    }
    // Mid-construction death explosion (ref createDeathExplodeEffect: pure
    // presentation, fired when a unit dies while IsConstructing). The sim
    // only reports (unit handle + explode buff id); the renderer spawns the
    // art. Fires with a zero buff id when none was set via JASS.
    void SetDeathExplodeCallback(std::function<void(int unitHandle, War3ID buffId)> cb) {
        deathExplodeCallback_ = std::move(cb);
    }
    const std::function<void(int, War3ID)>& GetDeathExplodeCallback() const {
        return deathExplodeCallback_;
    }
    void FireDeathExplode(int unitHandle, War3ID buffId) {
        if (deathExplodeCallback_) deathExplodeCallback_(unitHandle, buffId);
    }
    // Sim-side ownership changes (charm/possession transfers): unit handle
    // plus previous owner player index. JASS SetUnitOwner fires directly.
    void SetOwnerChangedCallback(std::function<void(int unitHandle, int prevOwnerIndex)> cb) {
        ownerChangedCallback_ = std::move(cb);
    }
    const std::function<void(int, int)>& GetOwnerChangedCallback() const { return ownerChangedCallback_; }
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
    // Spell lifecycle (caster handle, stage 0=channel 1=cast 2=effect 3=finish 4=endcast).
    void SetSpellEventCallback(std::function<void(int casterHandle, int stage)> cb) {
        spellEventCallback_ = std::move(cb);
    }
    const std::function<void(int, int)>& GetSpellEventCallback() const { return spellEventCallback_; }

private:
    void ProcessUnitRemovals();
    void ProcessNewUnits();
    // Separation steering: positional push-apart for overlapping movable
    // units (deterministic: units_ order, no RNG).
    void ResolveUnitSeparation();    void ProcessDestructableRemovals();
    void ProcessNewDestructables();
    void ProcessItemRemovals();

    int gameTurnTick_ = 0;
    bool paused_ = false;
    // Map-init dedupe window backing flag (default off).
    bool mapInitPhase_ = false;

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
    CDoodadData* doodadData_ = nullptr;
    CAbilityData* abilityData_ = nullptr;
    CItemData* itemData_ = nullptr;
    CUpgradeData* upgradeData_ = nullptr;

    CCombatSystem combatSystem_;
    CFogSystem fogSystem_;
    std::vector<CFogSightSource> sightScratch_;
    uint32_t simRandState_ = 0x12345678u; // deterministic sim RNG (LCG)
    std::vector<SimCommand> commandLog_;
    W3EMap* terrainMap_ = nullptr;  // terrain queries (owned by loader)
    bool waterDeforms_ = true;
    std::vector<BlightRect> blightRects_; // SetBlight* overrides
    std::vector<BlightCircle> blightCircles_;
    struct DelayedAction {
        float secondsLeft = 0.0f;
        std::function<void()> fn;
    };
    std::vector<DelayedAction> delayedActions_;
    std::vector<SimHazard> hazards_;
    void UpdateHazards();
    std::unique_ptr<CWorldCollision> worldCollision_;    std::unique_ptr<CPathfindingProcessor> pathfindingProcessor_;
    std::vector<GlobalShopStockEntry> globalShopStock_;
    AudioSystem* audio_ = nullptr;
    HandleIdAllocator handleIdAlloc_;

    // Post-update callbacks
    std::vector<std::function<void()>> postUpdateCallbacks_;
    std::function<void(int, int, int)> unitDeathCallback_;
    std::function<void(int, int, float)> unitDamageCallback_;
    std::function<void(int, int, int, float, float)> unitOrderCallback_;
    std::function<void(int, int, int)> itemEventCallback_;
    std::function<void(int, int)> unitLoadedCallback_;
    std::function<void(int)> unitDecayCallback_;
    std::function<void(int, int, int)> shopEventCallback_;
    std::function<void(int, int, int)> shopUnitEventCallback_;
    std::function<void(int, int, int)> pawnEventCallback_;
    std::function<void(int, int, int)> heroSkillCallback_;
    std::function<void(int, int, CombatTextKind, float)> combatTextCallback_;
    std::function<void(int, bool)> constructEventCallback_;
    std::function<void(int, War3ID)> deathExplodeCallback_;
    std::function<void(int, int, int, bool)> queueEventCallback_;
    std::function<void(int, int)> ownerChangedCallback_;
    std::function<void(int)> itemRemovedCallback_;
    std::function<void(int, int)> unitTrainedCallback_;
    std::function<void(int, int)> unitSummonedCallback_;
    std::function<void(int, int)> researchFinishedCallback_;
    std::function<void(int, int)> heroRevivedCallback_;
    std::function<void(int, int)> spellEventCallback_;
};

#pragma once
#include "ecs/widget.h"
#include "ecs/unit_type.h"
#include "ecs/player.h"
#include "ecs/abilities.h"
#include "ecs/buff.h"
#include "ecs/behaviors/behavior.h"
#include "ecs/behaviors/behavior_stop.h"
#include "ecs/behaviors/behavior_move.h"
#include "ecs/behaviors/behavior_follow.h"
#include "ecs/behaviors/behavior_patrol.h"
#include "ecs/behaviors/behavior_hold_position.h"
#include "ecs/behaviors/behavior_attack.h"
#include "ecs/behaviors/behavior_attack_move.h"
#include "ecs/orders/order.h"
#include "data/combat_types.h"
#include "ecs/abilities/ability_aura.h"
#include "ecs/attribute_aggregator.h"
#include "ecs/unit_components.h"
#include "ecs/ecs_registry.h"
#include <vector>
#include <queue>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class CAbilityAttack;
class CAbilityMove;
class CAbilityHero;
class CAbilityInventory;

class CUnit : public CWidget {
public:
    CUnit(int handleId, CUnitType* unitType, int playerIndex,
          War3ID typeId, float x, float y, float facing);
    ~CUnit() override;

    // Bind this unit's orthogonal components to a central SimEcsRegistry (Phase 5 True ECS Pooling).
    // Any components already populated before adoption are migrated into the registry's contiguous pools.
    void BindRegistry(SimEcsRegistry* registry);
    SimEcsRegistry* GetBoundRegistry() const { return registry_; }

    // Type info
    CUnitType* GetUnitType() const { return unitType_; }
    War3ID GetTypeId() const { return typeId_; }
    int GetPlayerIndex() const { return playerIndex_; }
    void SetPlayerIndex(int playerIndex) { playerIndex_ = playerIndex; }

    // Movement
    float GetFacing() const { return facing_; }
    void SetFacing(float f) { facing_ = f; }
    float GetTurnRate() const { return turnRate_; }
    void SetTurnRate(float v) { turnRate_ = v; }
    // Propulsion window override (-1 = use unit type default; ref setPropWindow).
    float GetPropWindow() const { return propWindow_; }
    void SetPropWindow(float v) { propWindow_ = v; }
    float GetEffectivePropWindow() const;
    // Pathing toggle (ref setUnitPathing / CUnit.isPathingEnabled).
    bool IsPathingEnabled() const { return pathingEnabled_; }
    void SetPathingEnabled(bool v) { pathingEnabled_ = v; }
    PathingGridMovementType GetMovementType() const { return movementType_; }
    bool IsFlying() const { return movementType_ == PathingGridMovementType::FLY; }
    float GetSpeed() const;
    void SetSpeed(int s);
    UnitAttributeAggregator& Attributes() { return attrs_; }
    const UnitAttributeAggregator& Attributes() const { return attrs_; }

    // Orthogonal ECS Component Accessors (Phase 5 True Sparse-Set ECS Storage)
    // Components are lazily allocated on demand in the bound SimEcsRegistry's
    // SparseComponentPool<T>, while const getters return a shared default singleton
    // if the unit has never mutated that component!
    UnitConstructionComponent& ConstructionComp() { return ActiveRegistry().GetOrCreate<UnitConstructionComponent>(handleId_); }
    const UnitConstructionComponent& ConstructionComp() const {
        if (const auto* c = ActiveRegistry().TryGet<UnitConstructionComponent>(handleId_)) return *c;
        static const UnitConstructionComponent kDefault{};
        return kDefault;
    }
    UnitCargoComponent& CargoComp() { return ActiveRegistry().GetOrCreate<UnitCargoComponent>(handleId_); }
    const UnitCargoComponent& CargoComp() const {
        if (const auto* c = ActiveRegistry().TryGet<UnitCargoComponent>(handleId_)) return *c;
        static const UnitCargoComponent kDefault{};
        return kDefault;
    }
    UnitMorphComponent& MorphComp() { return ActiveRegistry().GetOrCreate<UnitMorphComponent>(handleId_); }
    const UnitMorphComponent& MorphComp() const {
        if (const auto* c = ActiveRegistry().TryGet<UnitMorphComponent>(handleId_)) return *c;
        static const UnitMorphComponent kDefault{};
        return kDefault;
    }
    UnitStructureNavComponent& StructureNavComp() { return ActiveRegistry().GetOrCreate<UnitStructureNavComponent>(handleId_); }
    const UnitStructureNavComponent& StructureNavComp() const {
        if (const auto* c = ActiveRegistry().TryGet<UnitStructureNavComponent>(handleId_)) return *c;
        static const UnitStructureNavComponent kDefault{};
        return kDefault;
    }
    UnitVisualComponent& VisualComp() { return ActiveRegistry().GetOrCreate<UnitVisualComponent>(handleId_); }
    const UnitVisualComponent& VisualComp() const {
        if (const auto* c = ActiveRegistry().TryGet<UnitVisualComponent>(handleId_)) return *c;
        static const UnitVisualComponent kDefault{};
        return kDefault;
    }
    UnitCorpseDropComponent& CorpseDropComp() { return ActiveRegistry().GetOrCreate<UnitCorpseDropComponent>(handleId_); }
    const UnitCorpseDropComponent& CorpseDropComp() const {
        if (const auto* c = ActiveRegistry().TryGet<UnitCorpseDropComponent>(handleId_)) return *c;
        static const UnitCorpseDropComponent kDefault{};
        return kDefault;
    }

    int GetBaseSpeed() const { return attrs_.GetBaseSpeed(); }
    void AddBonusSpeed(int v) { attrs_.AddBonusSpeed(v); }
    void AddSpeedModifier(War3ID id, float percent) { attrs_.AddSpeedModifier(id, percent); }
    void RemoveSpeedModifier(War3ID id) { attrs_.RemoveSpeedModifier(id); }
    bool TurnTowardPoint(float targetX, float targetY);

    // Position (override from CWidget)
    void SetPosition(float x, float y);
    void SetPositionAndCheckUnstuck(float x, float y, CSimulation& simulation);

    // Life / mana
    float GetMana() const { return mana_; }
    void SetMana(float v) { mana_ = v; }
    int GetMaxMana() const { return maxMana_; }
    void SetMaxMana(int v) { maxMana_ = v; }
    int GetBaseMaxLife() const { return attrs_.GetBaseMaxLife(); }
    float GetBonusMaxLife() const { return attrs_.GetBonusMaxLife(); }
    void AddBonusMaxLife(float v);
    int GetBonusMaxMana() const { return attrs_.GetBonusMaxMana(); }
    void AddBonusMaxMana(int v);
    void SetHeroBonusLife(float v);
    void SetHeroBonusMana(int v);
    int GetDefense() const { return attrs_.GetCurrentDefense(); }
    void SetDefense(int v) { attrs_.SetCurrentDefense(v); }
    int GetTotalDefense() const { return attrs_.ComputeTotalDefense(); }
    float GetTempDefenseBonus() const { return attrs_.GetTempDefenseBonus(); }
    void SetTempDefenseBonus(float v) { attrs_.SetTempDefenseBonus(v); }
    void AddTempDefenseBonus(float v) { attrs_.AddTempDefenseBonus(v); }
    War3ID GetExplodeOnDeathBuffId() const { return explodeBuffId_; }
    void SetExplodeOnDeathBuffId(War3ID id) { explodeBuffId_ = id; }
    CDefenseType GetDefenseType() const;

    // Instance bonus stats routed through UnitAttributeAggregator
    float GetBonusDamage() const { return attrs_.ComputeTotalBonusDamage(); }
    void AddBonusDamage(int v) { attrs_.AddBonusDamage(v); }
    void SetHeroBonusDamage(int v) { attrs_.SetHeroBonusDamage(v); }
    int GetBonusArmor() const { return attrs_.GetBonusArmor(); }
    void AddBonusArmor(int v) { attrs_.AddBonusArmor(v); }
    float GetBonusLifeRegen() const { return attrs_.GetBonusLifeRegen(); }
    void AddBonusLifeRegen(float v) { attrs_.AddBonusLifeRegen(v); }
    float GetBonusManaRegen() const { return attrs_.GetBonusManaRegen(); }
    void AddBonusManaRegen(float v) { attrs_.AddBonusManaRegen(v); }
    // Non-stacking aura stat bonuses (ref NonStackingStatBonus): one entry
    // per source buff per (stat, percent) slot; only the strongest applies.
    // Implemented in unit.cpp; driven by CBuffAuraStat add/remove.
    void AddAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId, float value,
                           int caster = -1);
    void RemoveAuraStatBonus(CAuraEffectType type, bool percent, War3ID buffId,
                             int caster = -1);
    void RecomputeAuraStat(CAuraEffectType type, bool percent);
    // Recompute every aura-stat slot (ref computeDerivedFields sweep;
    // backs the RecomputeStatBonusesOnUnit native).
    void RecomputeAllAuraStats();
    // Evasion / critical strike sources (ref evasionListeners + crit
    // listeners: every source rolls independently). Same-id installs
    // replace; removals only clear their own id, so Drunken Brawler dodge
    // no longer wipes Demon Hunter evasion (and vice versa).
    using EvasionSource = UnitEvasionSource;
    using CritSource = UnitCritSource;
    void AddEvasionSource(War3ID id, float chance) { attrs_.AddEvasionSource(id, chance); }
    void RemoveEvasionSource(War3ID id) { attrs_.RemoveEvasionSource(id); }
    const std::vector<EvasionSource>& GetEvasionSources() const { return attrs_.GetEvasionSources(); }
    float GetEvasionChance() const { return attrs_.ComputeEvasionChance(); }
    void SetEvasionChance(float v) { attrs_.SetEvasionChance(v); }
    void AddCritSource(War3ID id, float chance, float multiplier) { attrs_.AddCritSource(id, chance, multiplier); }
    void RemoveCritSource(War3ID id) { attrs_.RemoveCritSource(id); }
    const std::vector<CritSource>& GetCritSources() const { return attrs_.GetCritSources(); }
    float GetCritChance() const { return attrs_.ComputeCritChance(); }
    float GetCritMultiplier() const { return attrs_.ComputeCritMultiplier(); }
    void SetCriticalStrike(float chance, float multiplier) { attrs_.SetCriticalStrike(chance, multiplier); }
    float GetAttackSpeedMultiplier() const { return attrs_.ComputeAttackSpeedMultiplier(); }
    void MultiplyAttackSpeed(float m) { attrs_.MultiplyAttackSpeed(m); }
    void DivideAttackSpeed(float m) { attrs_.DivideAttackSpeed(m); }
    void AddStackingAttackSpeed(float bonus) { attrs_.AddStackingAttackSpeed(bonus); }
    // Non-stacking attack bonuses (ref NonStackingStatBuff: same key takes
    // max, different keys sum; key = buff/ability id, e.g. Binf/Broa/BHav
    // for damage, Bblo/Babr/Bmdf/aura ids for speed). Flat and pct damage
    // ride separate maps (ref MELEEATK vs MELEEATKPCT both applying); the
    // pct map stores caller-converted flat values. Zero/empty writes erase.
    void SetNonStackingDamageBonus(War3ID key, int flatValue);
    void SetNonStackingDamagePctBonus(War3ID key, int flatValue);
    void ClearNonStackingDamageBonus(War3ID key);
    int GetNonStackingDamageTotal() const;
    void SetNonStackingAttackSpeed(War3ID key, float bonus);
    void ClearNonStackingAttackSpeed(War3ID key);
    float GetNonStackingAttackSpeedTotal() const;
    // Per-attack-index range bonus (index 0 = primary attack)
    float GetAttackRange(int index) const;
    void AddAttackRangeBonus(int index, float v);
    void AddAttackRangeBonusAll(float v);

    // Regen
    void ApplyRegen(CSimulation& simulation);

    // ===== Ability System =====
    // Add/remove abilities
    void AddAbility(CSimulation& game, std::unique_ptr<CAbility> ability);
    void RemoveAbility(CSimulation& game, int abilityHandleId);
    bool RemoveAbility(CSimulation& game, War3ID abilityCode);
    CAbility* GetAbility(War3ID code) const;
    const std::vector<CAbility*>& GetAbilities() const { return abilities_; }

    // Legacy ability defs (deprecated, kept for backward compat)
    const std::vector<CAbilityDef>& GetAbilityDefs() const { return abilityDefs_; }
    void AddAbilityDef(const CAbilityDef& def) { abilityDefs_.push_back(def); }

    // ===== Buff System =====
    void AddBuff(std::unique_ptr<CBuff> buff, CSimulation& game);
    void RemoveBuff(War3ID buffId, CSimulation& game);
    // Aura-grade removal: erases the buff from one caster only (same-id
    // buffs from other casters survive). No-op when absent.
    void RemoveBuffByIdAndCaster(War3ID buffId, int caster, CSimulation& game);
    CBuff* GetBuff(War3ID buffId) const;
    bool HasBuff(War3ID buffId) const;
    const std::vector<std::unique_ptr<CBuff>>& GetBuffs() const { return buffs_; }
    void UpdateBuffs(CSimulation& game);
    // Predicate removal for UnitRemoveBuffs(Ex): matched buffs get OnRemove
    // then erase. positive/negative/timedLife filter; pass -1/ignore flags
    // through for categories without backing (documented at call sites).
    // Timed life is never dispellable magic: dispels pass keepTimedLife so
    // unsummoning stays on Banish-style effects, since stripping timed
    // life via OnRemove kills the bearer.
    void RemoveBuffsIf(CSimulation& game, bool removePositive, bool removeNegative,
                       bool timedLifeOnly, bool keepTimedLife = false);
    // Crowd-control / summon queries (type-based so custom buff ids from
    // object data still work; canonical ids: BOhx/BUsl/BNsi/timed-life).
    bool IsHexed() const;
    bool IsSilenced() const;
    bool IsAsleep() const;
    bool IsSummoned() const;
    // Illusion flag (ref IsUnitIllusion): set on Mirror Image / Wand of
    // Illusion copies at summon time; never cleared while alive.
    bool IsIllusion() const { return illusion_; }
    void SetIsIllusion(bool v) { illusion_ = v; }
    // Spell effect reaction (ref CheckUnitForAbilityEffectReaction):
    // returns true when an enemy-targeted spell effect may land. A present
    // Spell Shield (BNss) is consumed and the effect fizzles instead.
    // Allied casters bypass the shield; missing players count as hostile.
    bool CheckSpellEffectReaction(CSimulation& game, CUnit* caster);
    // Sleep capability (ref UnitAddSleep/UnitCanSleep): Sleep effects only
    // land when both flags hold. Both default true (retail default).
    bool CanSleep() const { return canSleep_ && canSleepPerm_; }
    void SetCanSleep(bool v) { canSleep_ = v; }
    bool CanSleepPerm() const { return canSleepPerm_; }
    void SetCanSleepPerm(bool v) { canSleepPerm_ = v; }
    // Instance unit-type flags (ref UnitAddType/UnitRemoveType raw ids).
    // Stored per instance; consulted by future targeting/type checks.
    void AddUnitTypeFlag(int typeId) { addedUnitTypes_.insert(typeId); removedUnitTypes_.erase(typeId); }
    void RemoveUnitTypeFlag(int typeId) { removedUnitTypes_.insert(typeId); addedUnitTypes_.erase(typeId); }
    bool HasUnitTypeFlag(int typeId) const { return addedUnitTypes_.count(typeId) > 0; }
    // Town-alarm opt-out (ref UnitIgnoreAlarm): no alarm system consumes
    // this yet; retained so script intent survives.
    bool IsAlarmIgnored() const { return ignoreAlarm_; }
    void SetIgnoreAlarm(bool v) { ignoreAlarm_ = v; }
    // Command-card alt icon (ref UnitSetUsesAltIcon): consumed by UI.
    bool UsesAltIcon() const { return VisualComp().useAltIcon; }
    void SetUsesAltIcon(bool v) { VisualComp().useAltIcon = v; }
    // Upgrade morph progress percent 0..100 (ref UnitSetUpgradeProgress):
    // consumed once the morph API lands; construction twin is live.
    float GetUpgradeProgress() const { return ConstructionComp().upgradeProgress; }
    void SetUpgradeProgress(float v) { ConstructionComp().upgradeProgress = v; }

    // ===== Behavior System =====
    // Current behavior
    CBehavior* GetCurrentBehavior() const { return currentBehavior_; }
    void SetCurrentBehavior(std::unique_ptr<CBehavior> behavior);
    void IssueOrder(std::unique_ptr<COrder> order, bool queue = false);
    void ClearOrders();
    void Stop(CSimulation& simulation);
    // Last issued order id (ref GetUnitCurrentOrder); 0 when none issued.
    int GetLastOrderId() const { return lastOrderId_; }
    // Order-issued hook (order id, target kind, point x/y, target sim
    // handle or -1). Installed by CSimulation::AdoptUnit so every order —
    // JASS, UI, AI, net — flows into the sim's unitOrderCallback_ (JASS
    // issued-order trigger events + command log live there).
    using OrderIssuedHook = std::function<void(int, int, float, float, int)>;
    void SetOrderIssuedHook(OrderIssuedHook hook) { orderIssuedHook_ = std::move(hook); }
    // Load hook (passenger sim handle). Installed by AdoptUnit; the sim
    // forwards fresh boardings to unitLoadedCallback_ (JASS LOADED event).
    // Fires only when the passenger actually boards (not already aboard).
    using UnitLoadedHook = std::function<void(int)>;
    void SetUnitLoadedHook(UnitLoadedHook hook) { unitLoadedHook_ = std::move(hook); }

    // Behavior storage (set by abilities on Add)
    CBehaviorMove* GetMoveBehavior() const { return moveBehavior_.get(); }
    CBehaviorFollow* GetFollowBehavior() const { return followBehavior_.get(); }
    CBehaviorPatrol* GetPatrolBehavior() const { return patrolBehavior_.get(); }
    CBehaviorHoldPosition* GetHoldPositionBehavior() const { return holdPositionBehavior_.get(); }
    CBehaviorAttack* GetAttackBehavior() const { return attackBehavior_.get(); }
    CBehaviorAttackMove* GetAttackMoveBehavior() const { return attackMoveBehavior_.get(); }
    CBehavior* GetDefaultBehavior() const { return defaultBehavior_.get(); }

    void SetMoveBehavior(std::unique_ptr<CBehaviorMove> b);
    void SetFollowBehavior(std::unique_ptr<CBehaviorFollow> b);
    void SetPatrolBehavior(std::unique_ptr<CBehaviorPatrol> b);
    void SetHoldPositionBehavior(std::unique_ptr<CBehaviorHoldPosition> b);
    void SetAttackBehavior(std::unique_ptr<CBehaviorAttack> b);
    void SetAttackMoveBehavior(std::unique_ptr<CBehaviorAttackMove> b);
    void SetDefaultBehavior(std::unique_ptr<CBehavior> b);

    // Poll next order from queue, returns behavior or nullptr
    CBehavior* PollNextOrderBehavior(CSimulation& simulation);
    // True when the unit has queued orders waiting (AI uses this plus an
    // idle behavior to detect workers needing assignments).
    bool HasQueuedOrders() const;

    // Update - called each simulation tick
    bool Update(CSimulation& simulation);

    // Damage
    float Damage(CSimulation& simulation, CUnit* source,
                 bool isAttack, bool isRanged,
                 CAttackType attackType, CDamageType damageType,
                 const std::string& weaponSoundType,
                 float damage, float bonusDamage = 0) override;

    // Properties
    bool IsBuilding() const { return isBuilding_; }
    bool IsHero() const { return isHero_; }
    bool IsInvulnerable() const override { return invulnerable_; }
    void SetInvulnerable(bool v) { invulnerable_ = v; }

    // State
    void SetHidden(bool v) { hidden_ = v; }
    bool IsHidden() const { return hidden_; }
    // Paused units stay in the world but skip behavior and ability ticks.
    void SetPaused(bool v) { paused_ = v; }
    bool IsPaused() const { return paused_; }

    // Construction state
    bool IsConstructing() const { return ConstructionComp().constructing; }
    void SetConstructing(bool v) { ConstructionComp().constructing = v; }
    // Root state (Night Elf ancients): rooted units cannot move but can
    // still attack; movement behaviors complete immediately while rooted.
    bool IsRooted() const { return rooted_; }
    void SetRooted(bool v) { rooted_ = v; }
    // Invisibility is hidden from other players unless revealed (Faerie Fire)
    // or a detector is within sight range.
    bool IsInvisible() const { return invisible_; }
    void SetInvisible(bool v) { invisible_ = v; }
    // Detectors (shades, sentry wards) pierce stealth within sight range.
    bool IsDetector() const { return detector_; }
    void SetDetector(bool v) { detector_ = v; }
    // Players who also see through this unit's sight (UnitShareVision).
    void SetSharesVisionWith(int player, bool share);
    const std::unordered_set<int>& SharedVisionPlayers() const { return sharedVision_; }
    // Ethereal (Banish): immune to physical (NORMAL damage), takes bonus
    // from MAGIC/SPELLS (ref CUnitDefaultEtherealDamageModListener).
    bool IsEthereal() const { return ethereal_; }
    void SetEthereal(bool v) { ethereal_ = v; }
    // Magic immunity (Anti-Magic Shell, spell immunity): resists MAGIC and
    // non-universal SPELLS (ref CUnitDefaultMagicImmuneDamageModListener).
    bool IsMagicImmune() const { return magicImmune_; }
    void SetMagicImmune(bool v) { magicImmune_ = v; }
    bool IsInvisibleTo(const CUnit* viewer) const;
    // Stealth pre-check for fog rendering: invisible flag or a Wind Walk
    // buff. Detector/Faerie-Fire resolution still needs IsInvisibleTo(viewer).
    bool IsStealthed() const;
    bool IsConstructionPaused() const { return ConstructionComp().constructionPaused; }
    void SetConstructionPaused(bool v) { ConstructionComp().constructionPaused = v; }
    float GetConstructionProgress() const { return ConstructionComp().constructionProgress; }
    void SetConstructionProgress(float v) { ConstructionComp().constructionProgress = v; }

    // Hero level (per-unit instance, not from unit type)
    int GetHeroLevel() const { return heroLevel_; }
    void SetHeroLevel(int lvl) { heroLevel_ = lvl; }

    // Death drops (ref units.doo dropSets + w3i droppedItemTable): items
    // created at the corpse on the death tick, each rolling its own
    // percent chance independently (sets are editor groupings only).
    using DeathDropItem = UnitDeathDropItem;
    void SetDeathDrops(std::vector<DeathDropItem> drops) {
        CorpseDropComp().deathDrops = std::move(drops);
    }
    const std::vector<DeathDropItem>& GetDeathDrops() const { return CorpseDropComp().deathDrops; }
    // Rolls the death drops and creates the won items (ref drop chance %).
    void DropDeathItems(CSimulation& simulation);

    // Death
    int GetDeathTurnTick() const { return CorpseDropComp().deathTurnTick; }
    void SetDeathTurnTick(int t) { CorpseDropComp().deathTurnTick = t; }
    bool IsCorpse() const { return CorpseDropComp().corpse; }
    bool IsBoneCorpse() const { return CorpseDropComp().boneCorpse; }
    // Killer at the fatal blow (player index, -1 = none/environment; ref
    // kill(simulation, source)): feeds the killer-side death-vision reveal.
    // Recorded in Damage on the killing hit, consumed on the death tick.
    int GetDeathKillerPlayer() const { return CorpseDropComp().deathKillerPlayer; }
    void SetDeathKillerPlayer(int v) { CorpseDropComp().deathKillerPlayer = v; }
    // Construction worker link (ref CUnit.worker/workerInside): orc/ne
    // builders hide inside the foundation; death pops them back out
    // (PopoutWorker), completion/cancel clears the link.
    void SetWorker(int workerHandleId, bool inside) {
        auto& c = ConstructionComp();
        c.workerHandleId = workerHandleId;
        c.workerInside = inside;
    }
    void ClearWorker() {
        if (auto* c = ActiveRegistry().TryGet<UnitConstructionComponent>(handleId_)) {
            c->workerHandleId = -1;
            c->workerInside = false;
            c->constructionConsumesWorker = false;
        }
    }
    int GetWorkerHandle() const { return ConstructionComp().workerHandleId; }
    bool IsWorkerInside() const { return ConstructionComp().workerInside; }
    // Night-elf consume (ref constructionConsumesWorker, set only for
    // ancients; wisp food is zeroed at build start and restored on popout).
    void SetConstructionConsumesWorker(bool v) { ConstructionComp().constructionConsumesWorker = v; }
    bool IsConstructionConsumesWorker() const { return ConstructionComp().constructionConsumesWorker; }
    // Emerge the hidden construction worker, if any (ref popoutWorker):
    // unhide/unpause/uninvulnerable, nudge beside the ruin, restore
    // consumed-worker food. Always clears the link.
    void PopoutWorker(CSimulation& simulation);
    // Decay suspend (UnitSuspendDecay): frozen corpses never advance.
    bool IsDecaySuspended() const { return CorpseDropComp().decaySuspended; }
    void SetDecaySuspended(bool v) { CorpseDropComp().decaySuspended = v; }
    float GetEndingDecayTime(const CSimulation& simulation) const;

    // Collision
    float GetCollisionSize() const { return collisionSize_; }
    void SetCollisionSize(float v) { collisionSize_ = v; }

    // Cargo (transported units ride along hidden inside this unit)
    const std::vector<CUnit*>& GetCargoUnits() const { return CargoComp().cargoUnits; }
    bool IsLoadedAboard() const { return CargoComp().loadedAboard; }
    bool LoadUnit(CUnit* passenger);
    void UnloadUnitAt(CUnit* passenger, float x, float y);
    // Burrow cargo (Abun) locks attacks until a peon is inside.
    bool AreAttacksDisabled() const { return CargoComp().attacksDisabled; }
    void SetAttacksDisabled(bool v) { CargoComp().attacksDisabled = v; }
    bool CanBeTargetedBy(CSimulation& simulation, CUnit* source,
                         const std::vector<CTargetType>& targetsAllowed) const override;

    // Resource / food
    int GetFoodUsed() const { return useFood_ ? foodUsed_ : 0; }
    int GetFoodMade() const { return foodMade_; }
    void SetFoodUsed(int v) { foodUsed_ = v; }
    void SetFoodMade(int v) { foodMade_ = v; }
    // Whether this unit counts toward food usage (ref SetUnitUseFood).
    // Toggling preserves the base value; sim add/remove accounting reads
    // through GetFoodUsed, so toggle-then-die can drift by design (noted).
    bool UsesFood() const { return useFood_; }
    void SetUseFood(bool v) { useFood_ = v; }
    // Revive a dead hero at a position (ref ReviveHero).
    void Revive(CSimulation& game, float x, float y);
    // Alternate-form morph (Metamorphosis, Bear/Raven/Robo forms):
    // swaps the type snapshot, saving the original for Unmorph().
    void MorphTo(CUnitType* form, War3ID formId);
    void Unmorph();
    bool IsMorphed() const { return MorphComp().morphed; }
    // Attack modifier slot (ref attackReplacement): one data-driven proc
    // effect per attack index, installed by orb/arrow abilities. Installing
    // replaces whatever was there (retail orbs do not stack). Behaviors
    // only ever fire the primary attack, so Damage consults index 0.
    void SetAttackModifier(int attackIndex, War3ID installer,
                           std::unique_ptr<CAttackModifierEffect> effect);
    // Clears only when the recorded installer matches (stale removes from
    // a replaced orb are ignored).
    void ClearAttackModifier(int attackIndex, War3ID installer);
    CAttackModifierEffect* GetAttackModifier(int attackIndex);
    const CAttackModifierEffect* GetAttackModifier(int attackIndex) const;
    // Runs the primary-attack modifier after a landed weapon hit (gated by
    // ChargeResources; no-op when the slot is empty).
    void FireAttackModifiers(CSimulation& game, CUnit* victim, float damageDealt);

    // Fly height
    float GetDefaultFlyHeight() const { return flyHeight_; }
    void SetFlyingHeight(float h) { SetFlyHeight(h); }

    // Visual state (ref CUnit: scaling values, time scale, vertex color).
    // Stored in SimEcsRegistry<UnitVisualComponent> so JASS setters persist;
    // the renderer reads them when drawing (no render dependency here).
    float GetScale() const { return VisualComp().scale; }
    void SetScale(float s) { VisualComp().scale = s; }
    float GetTimeScale() const { return VisualComp().timeScale; }
    void SetTimeScale(float s) { VisualComp().timeScale = s; }
    void SetVertexColor(int r, int g, int b, int a) {
        auto& vc = VisualComp();
        vc.vertexR = r; vc.vertexG = g; vc.vertexB = b; vc.vertexA = a;
    }
    void GetVertexColor(int& r, int& g, int& b, int& a) const {
        const auto& vc = VisualComp();
        r = vc.vertexR; g = vc.vertexG; b = vc.vertexB; a = vc.vertexA;
    }
    void SetPlayerColor(int color) { VisualComp().playerColor = color; }
    int GetPlayerColor() const { return VisualComp().playerColor; }
    // Acquire-range override (-1 = use unit type default)
    float GetAcquireRange() const { return acquireRange_; }
    void SetAcquireRange(float range) { acquireRange_ = range; }
    // Rally point (ref CUnit rallyPoint; orderId 0 = none).
    float GetRallyX() const { return StructureNavComp().rallyX; }
    float GetRallyY() const { return StructureNavComp().rallyY; }
    int GetRallyOrderId() const { return StructureNavComp().rallyOrderId; }
    void SetRallyPoint(float x, float y, int orderId = 0) {
        auto& sn = StructureNavComp();
        sn.rallyX = x; sn.rallyY = y; sn.rallyOrderId = orderId;
    }
    bool IsWaygateActive() const { return StructureNavComp().waygateActive; }
    void SetWaygateActive(bool v) { StructureNavComp().waygateActive = v; }
    float GetWaygateX() const { return StructureNavComp().waygateX; }
    float GetWaygateY() const { return StructureNavComp().waygateY; }
    void SetWaygateDestination(float x, float y) {
        auto& sn = StructureNavComp();
        sn.waygateX = x; sn.waygateY = y;
    }
    void RegeneratePathingInstance(CSimulation& game);
    void KillPathingInstance(CSimulation& game);
    int GetPathingStampId() const { return StructureNavComp().pathingStampId; }

private:
    static SimEcsRegistry& DetachedRegistry();
    SimEcsRegistry& ActiveRegistry() const {
        return registry_ ? *registry_ : DetachedRegistry();
    }

    void PerformDefaultBehavior(CSimulation& simulation);
    bool TryAutocast(CSimulation& simulation);
    // Kill rewards (bounty dice + hero XP split) fired once at the
    // killing blow. Source may be nullptr (degeneration/decay).
    void AwardBountyAndXP(CSimulation& simulation, CUnit* source);
    // Return fire / flee when damaged while idle (ref damage() else
    // branch). No-op while busy; P0-2 refines the gate with
    // interruptable().
    void MaybeReturnFire(CSimulation& simulation, CUnit* source);
    void ConvertOrderToBehavior(CSimulation& simulation, std::unique_ptr<COrder> order);
    void RefreshMaxLifeTotal();
    void RefreshMaxManaTotal();

    SimEcsRegistry* registry_ = nullptr;
    CUnitType* unitType_;
    War3ID typeId_;
    int playerIndex_;
    float facing_;

    // Centralized attribute & modifier aggregator (Phase 2 modernization)
    UnitAttributeAggregator attrs_;
    int speed_ = 0;
    float mana_ = 0.0f;
    int maxMana_ = 0;
    War3ID explodeBuffId_ = War3ID(0u);
    float turnRate_;
    PathingGridMovementType movementType_;
    float collisionSize_;

    // Core flags
    bool isBuilding_;
    bool isHero_;
    bool hidden_ = false;
    bool paused_ = false;
    bool invulnerable_ = false;
    bool rooted_ = false;
    bool invisible_ = false;
    bool detector_ = false;
    std::unordered_set<int> sharedVision_;
    bool ethereal_ = false;
    bool magicImmune_ = false;
    bool illusion_ = false;
    int foodUsed_ = 0;
    int foodMade_ = 0;
    bool useFood_ = true;
    int heroLevel_ = 1;
    float acquireRange_ = -1.0f;
    float propWindow_ = -1.0f;
    bool pathingEnabled_ = true;

    // Abilities
    std::vector<CAbility*> abilities_; // raw pointers to owned abilities
    std::vector<std::unique_ptr<CAbility>> ownedAbilities_;
    std::vector<CAbilityDef> abilityDefs_; // legacy

    // Buffs
    std::vector<std::unique_ptr<CBuff>> buffs_;

    // Instance unit-type flag overrides (UnitAddType/UnitRemoveType).
    std::unordered_set<int> addedUnitTypes_;
    std::unordered_set<int> removedUnitTypes_;
    bool canSleep_ = true;
    bool canSleepPerm_ = true;
    bool ignoreAlarm_ = false;

    // Behavior storage (owned by unit, created by abilities)
    std::unique_ptr<CBehaviorMove> moveBehavior_;
    std::unique_ptr<CBehaviorFollow> followBehavior_;
    std::unique_ptr<CBehaviorPatrol> patrolBehavior_;
    std::unique_ptr<CBehaviorHoldPosition> holdPositionBehavior_;
    std::unique_ptr<CBehaviorAttack> attackBehavior_;
    std::unique_ptr<CBehaviorAttackMove> attackMoveBehavior_;
    std::unique_ptr<CBehavior> defaultBehavior_;

    // Current behavior and order queue
    std::unique_ptr<CBehavior> ownedCurrentBehavior_;
    CBehavior* currentBehavior_ = nullptr;
    std::queue<std::unique_ptr<COrder>> orderQueue_;
    int lastOrderId_ = 0;
    OrderIssuedHook orderIssuedHook_;
    UnitLoadedHook unitLoadedHook_;

    // Attack modifier slots by attack index (ref attackReplacement).
    struct AttackModifierSlot {
        War3ID installer = War3ID(0u);
        std::unique_ptr<CAttackModifierEffect> effect;
    };
    std::unordered_map<int, AttackModifierSlot> attackModifiers_;
};

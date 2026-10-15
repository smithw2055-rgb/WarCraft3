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
    float GetSpeed() const { return speed_; }
    void SetSpeed(int s) { speed_ = s; }
    bool TurnTowardPoint(float targetX, float targetY);

    // Position (override from CWidget)
    void SetPosition(float x, float y);
    void SetPositionAndCheckUnstuck(float x, float y, CSimulation& simulation);

    // Life / mana
    float GetMana() const { return mana_; }
    void SetMana(float v) { mana_ = v; }
    int GetMaxMana() const { return maxMana_; }
    void SetMaxMana(int v) { maxMana_ = v; }
    int GetBaseMaxLife() const { return baseMaxLife_; }
    // Layered max life/mana: type base + hero bonus + item/upgrade bonus.
    // Unit code must mutate layers via Add*/SetHeroBonus*, never via
    // SetMaxLife/SetMaxMana, so layers can't wipe each other.
    float GetBonusMaxLife() const { return bonusMaxLife_; }
    void AddBonusMaxLife(float v);
    int GetBonusMaxMana() const { return bonusMaxMana_; }
    void AddBonusMaxMana(int v);
    void SetHeroBonusLife(float v);
    void SetHeroBonusMana(int v);
    int GetDefense() const { return currentDefense_; }
    void SetDefense(int v) { currentDefense_ = v; }
    // Total defense including item/upgrade bonus armor (ref GetTotalDefense;
    // temporary script bonus from UnitAddDefenseBonus rides along so combat
    // honors Avatar/Devotion-style script armor).
    int GetTotalDefense() const { return currentDefense_ + bonusArmor_ + (int)tempDefenseBonus_; }
    // Temporary script defense bonus (ref temporaryDefenseBonus): mutated by
    // UnitAddDefenseBonus/SetTemporaryDefenseBonus, read by GetUnitDefenseBonus.
    float GetTempDefenseBonus() const { return tempDefenseBonus_; }
    void SetTempDefenseBonus(float v) { tempDefenseBonus_ = v; }
    void AddTempDefenseBonus(float v) { tempDefenseBonus_ += v; }
    // Buff applied as a death explosion visual (ref explodesOnDeathBuffId;
    // stored for the effect pass, cf. JASS SetUnitExplodeOnDeathBuffId).
    War3ID GetExplodeOnDeathBuffId() const { return explodeBuffId_; }
    void SetExplodeOnDeathBuffId(War3ID id) { explodeBuffId_ = id; }
    CDefenseType GetDefenseType() const;

    // Instance bonus stats (items, upgrades, buffs). These stack on top of
    // the static CUnitType data and must never mutate the shared unit type.
    int GetBonusDamage() const { return bonusDamage_ + heroBonusDamage_; }
    void AddBonusDamage(int v) { bonusDamage_ += v; }
    void SetHeroBonusDamage(int v) { heroBonusDamage_ = v; }
    int GetBonusArmor() const { return bonusArmor_; }
    void AddBonusArmor(int v) { bonusArmor_ += v; }
    float GetBonusLifeRegen() const { return bonusLifeRegen_; }
    void AddBonusLifeRegen(float v) { bonusLifeRegen_ += v; }
    float GetBonusManaRegen() const { return bonusManaRegen_; }
    void AddBonusManaRegen(float v) { bonusManaRegen_ += v; }
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
    struct EvasionSource {
        War3ID id = War3ID(0u);
        float chance = 0.0f;
    };
    struct CritSource {
        War3ID id = War3ID(0u);
        float chance = 0.0f;
        float multiplier = 1.0f;
    };
    void AddEvasionSource(War3ID id, float chance);
    void RemoveEvasionSource(War3ID id);
    const std::vector<EvasionSource>& GetEvasionSources() const { return evasionSources_; }
    // Combined dodge probability 1 - Π(1 - p) (for UI/inspect).
    float GetEvasionChance() const;
    // Legacy single-value shim (tests): clears all sources, installs one
    // anonymous source.
    void SetEvasionChance(float v);
    void AddCritSource(War3ID id, float chance, float multiplier);
    void RemoveCritSource(War3ID id);
    const std::vector<CritSource>& GetCritSources() const { return critSources_; }
    // Display pair: chance/multiplier of the likeliest source (exact when
    // only one source is installed).
    float GetCritChance() const;
    float GetCritMultiplier() const;
    // Legacy single-value shim (tests): clears all sources, installs one
    // anonymous source.
    void SetCriticalStrike(float chance, float multiplier);
    float GetAttackSpeedMultiplier() const { return attackSpeedMul_; }
    void MultiplyAttackSpeed(float m) { attackSpeedMul_ *= m; }
    void DivideAttackSpeed(float m) { if (m != 0.0f) attackSpeedMul_ /= m; }
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
    bool UsesAltIcon() const { return useAltIcon_; }
    void SetUsesAltIcon(bool v) { useAltIcon_ = v; }
    // Upgrade morph progress percent 0..100 (ref UnitSetUpgradeProgress):
    // consumed once the morph API lands; construction twin is live.
    float GetUpgradeProgress() const { return upgradeProgress_; }
    void SetUpgradeProgress(float v) { upgradeProgress_ = v; }

    // ===== Behavior System =====
    // Current behavior
    CBehavior* GetCurrentBehavior() const { return currentBehavior_; }
    void SetCurrentBehavior(std::unique_ptr<CBehavior> behavior);
    void IssueOrder(std::unique_ptr<COrder> order);
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
    bool IsConstructing() const { return constructing_; }
    void SetConstructing(bool v) { constructing_ = v; }
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
    bool IsConstructionPaused() const { return constructionPaused_; }
    void SetConstructionPaused(bool v) { constructionPaused_ = v; }
    float GetConstructionProgress() const { return constructionProgress_; }
    void SetConstructionProgress(float v) { constructionProgress_ = v; }

    // Hero level (per-unit instance, not from unit type)
    int GetHeroLevel() const { return heroLevel_; }
    void SetHeroLevel(int lvl) { heroLevel_ = lvl; }

    // Death
    int GetDeathTurnTick() const { return deathTurnTick_; }
    void SetDeathTurnTick(int t) { deathTurnTick_ = t; }
    bool IsCorpse() const { return corpse_; }
    bool IsBoneCorpse() const { return boneCorpse_; }
    // Decay suspend (UnitSuspendDecay): frozen corpses never advance.
    bool IsDecaySuspended() const { return decaySuspended_; }
    void SetDecaySuspended(bool v) { decaySuspended_ = v; }
    float GetEndingDecayTime(const CSimulation& simulation) const;

    // Collision
    float GetCollisionSize() const { return collisionSize_; }
    void SetCollisionSize(float v) { collisionSize_ = v; }

    // Cargo (transported units ride along hidden inside this unit)
    const std::vector<CUnit*>& GetCargoUnits() const { return cargoUnits_; }
    bool IsLoadedAboard() const { return loadedAboard_; }
    bool LoadUnit(CUnit* passenger);
    void UnloadUnitAt(CUnit* passenger, float x, float y);
    // Burrow cargo (Abun) locks attacks until a peon is inside.
    bool AreAttacksDisabled() const { return attacksDisabled_; }
    void SetAttacksDisabled(bool v) { attacksDisabled_ = v; }
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
    void Revive(CSimulation& game, float x, float y);    // Alternate-form morph (Metamorphosis, Bear/Raven/Robo forms):
    // swaps the type snapshot, saving the original for Unmorph().
    void MorphTo(CUnitType* form, War3ID formId);
    void Unmorph();
    bool IsMorphed() const { return morphed_; }
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
    // Stored on the sim unit so JASS setters persist; the renderer reads
    // them when drawing (no render dependency here).
    float GetScale() const { return scale_; }
    void SetScale(float s) { scale_ = s; }
    float GetTimeScale() const { return timeScale_; }
    void SetTimeScale(float s) { timeScale_ = s; }
    void SetVertexColor(int r, int g, int b, int a) {
        vertexR_ = r; vertexG_ = g; vertexB_ = b; vertexA_ = a;
    }
    void GetVertexColor(int& r, int& g, int& b, int& a) const {
        r = vertexR_; g = vertexG_; b = vertexB_; a = vertexA_;
    }
    void SetPlayerColor(int color) { playerColor_ = color; }
    int GetPlayerColor() const { return playerColor_; }
    // Acquire-range override (-1 = use unit type default)
    float GetAcquireRange() const { return acquireRange_; }
    void SetAcquireRange(float range) { acquireRange_ = range; }
    // Rally point (ref CUnit rallyPoint; orderId 0 = none).
    float GetRallyX() const { return rallyX_; }
    float GetRallyY() const { return rallyY_; }
    int GetRallyOrderId() const { return rallyOrderId_; }
    void SetRallyPoint(float x, float y, int orderId = 0) {
        rallyX_ = x; rallyY_ = y; rallyOrderId_ = orderId;
    }
    // Waygate destination (ref CAbilityWayGate; stored on the unit since
    // the C++ port has no dedicated waygate ability class yet).
    bool IsWaygateActive() const { return waygateActive_; }
    void SetWaygateActive(bool v) { waygateActive_ = v; }
    float GetWaygateX() const { return waygateX_; }
    float GetWaygateY() const { return waygateY_; }
    void SetWaygateDestination(float x, float y) { waygateX_ = x; waygateY_ = y; }
    // Building footprint stamp (ref CUnit.pathingInstance). Buildings OR
    // their pathTex footprint on creation (snapped, rotation 270); death
    // and removal unstamp. Ancients toggle via Root/Uproot.
    void RegeneratePathingInstance(CSimulation& game);
    void KillPathingInstance(CSimulation& game);
    int GetPathingStampId() const { return pathingStampId_; }

private:
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

    CUnitType* unitType_;
    War3ID typeId_;
    int playerIndex_;
    float facing_;

    // Stats
    int speed_;
    int baseMaxLife_;
    int baseMaxMana_;
    float mana_;
    int maxMana_;
    float bonusMaxLife_ = 0.0f;
    int bonusMaxMana_ = 0;
    float heroBonusLife_ = 0.0f;
    int heroBonusMana_ = 0;
    int currentDefense_;
    int bonusDamage_ = 0;
    int heroBonusDamage_ = 0;
    int bonusArmor_ = 0;
    float tempDefenseBonus_ = 0.0f;
    War3ID explodeBuffId_ = War3ID(0u);
    float bonusLifeRegen_ = 0.0f;
    float bonusManaRegen_ = 0.0f;
    float attackSpeedMul_ = 1.0f;
    // Non-stacking aura sources per (stat, percent) slot, plus the
    // currently applied effective delta (additive stats) or factor
    // (multiplicative stats) so recompute can undo precisely.
    struct AuraStatSource {
        War3ID buffId;
        int caster = -1;
        float value = 0.0f;
    };
    std::map<std::pair<int, bool>, std::vector<AuraStatSource>> auraStats_;
    std::map<std::pair<int, bool>, float> auraApplied_;
    std::vector<EvasionSource> evasionSources_;
    std::vector<CritSource> critSources_;
    std::vector<float> attackRangeBonus_;
    float turnRate_;
    PathingGridMovementType movementType_;
    float collisionSize_;

    // State
    bool isBuilding_;
    bool isHero_;
    bool hidden_ = false;
    bool paused_ = false;
    bool invulnerable_ = false;
    bool constructing_ = false;
    bool rooted_ = false;
    bool invisible_ = false;
    bool detector_ = false;
    std::unordered_set<int> sharedVision_;
    bool ethereal_ = false;
    bool magicImmune_ = false;
    // Morph snapshot (ref alternate-form units).
    bool morphed_ = false;
    CUnitType* originalType_ = nullptr;
    War3ID originalTypeId_ = War3ID();
    int savedSpeed_ = 0;
    int savedBaseMaxLife_ = 0;
    int savedBaseMaxMana_ = 0;
    int savedDefense_ = 0;
    float savedTurnRate_ = 0.0f;
    PathingGridMovementType savedMovementType_ = PathingGridMovementType::FOOT;
    float savedCollisionSize_ = 0.0f;
    bool savedBuilding_ = false;
    float savedFlyHeight_ = 0.0f;
    bool constructionPaused_ = false;
    float constructionProgress_ = 0;
    int deathTurnTick_ = 0;
    bool corpse_ = false;
    bool boneCorpse_ = false;
    bool decaySuspended_ = false;
    int foodUsed_ = 0;
    int foodMade_ = 0;
    bool useFood_ = true;

    // Hero
    int heroLevel_ = 1;

    // Visual state defaults
    float scale_ = 1.0f;
    float timeScale_ = 1.0f;
    int vertexR_ = 255;
    int vertexG_ = 255;
    int vertexB_ = 255;
    int vertexA_ = 255;
    int playerColor_ = -1;
    float acquireRange_ = -1.0f;
    float propWindow_ = -1.0f;
    bool pathingEnabled_ = true;
    float rallyX_ = 0.0f;
    float rallyY_ = 0.0f;
    int rallyOrderId_ = 0;
    bool waygateActive_ = false;
    float waygateX_ = 0.0f;
    float waygateY_ = 0.0f;
    // WPM footprint stamp id from RegeneratePathingInstance (-1 = none).
    int pathingStampId_ = -1;

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
    bool useAltIcon_ = false;
    float upgradeProgress_ = 0.0f;

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

    // Transport cargo (passengers hidden aboard this unit)
    std::vector<CUnit*> cargoUnits_;
    bool loadedAboard_ = false;
    bool attacksDisabled_ = false;

    // Attack modifier slots by attack index (ref attackReplacement).
    struct AttackModifierSlot {
        War3ID installer = War3ID(0u);
        std::unique_ptr<CAttackModifierEffect> effect;
    };
    std::unordered_map<int, AttackModifierSlot> attackModifiers_;
};

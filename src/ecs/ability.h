#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include "ecs/fwd.h"
#include "resource/slk.h"

class CBehavior;
class CBehavior;
class CItem;
class COrder;
class COrderTargetPoint;
class COrderTargetWidget;
class COrderNoTarget;

// Resource types carried by workers or harvested from resource nodes
enum class CResourceType : uint8_t {
    NONE, GOLD, LUMBER
};

// Build race type (moved from ability_build.h in Phase 17 so generic build
// behaviors and simulation modules do not depend on concrete ability headers)
enum class CBuildRace : uint8_t {
    HUMAN, ORC, UNDEAD, NIGHT_ELF, NAGA, NEUTRAL
};

// Spell targeting type (moved from ability_spell_base.h in Phase 19 so UI,
// scripting, and core layers never include ability_spell_base.h directly)
enum class CSpellTargetType : uint8_t {
    NO_TARGET,      // Instant cast, no target needed
    TARGET_UNIT,    // Must target a unit
    TARGET_POINT,   // Must target a point
    TARGET_BOTH     // Can target unit or point
};

// Production queue item type (moved from ability_queue.h in Phase 19 so UI
// and scripting layers never include ability_queue.h directly)
enum class CQueueItemType : uint8_t {
    NONE, UNIT, RESEARCH, HERO_REVIVE
};

// Hero attribute stat value (moved from ability_hero.h in Phase 19 so
// scripting and UI layers query hero attributes through CAbility virtual contract)
struct HeroStatValue {
    int base = 0;
    float perLevelFactor = 0.0f;
    int bonus = 0;

    int GetCurrentBase(int level) const {
        return base + static_cast<int>(perLevelFactor * static_cast<float>(level - 1));
    }
    int GetCurrent(int level) const {
        return GetCurrentBase(level) + bonus;
    }
};

// Aura effect type & bonus definition (moved from ability_aura.h in Phase 17
// so CUnit, CBuff, and UnitAttributeAggregator have zero concrete ability headers)
enum class CAuraEffectType : uint8_t {
    MOVE_SPEED, ATTACK_SPEED, ARMOR, DAMAGE, LIFE_REGEN, MANA_REGEN
};

struct CAuraBonus {
    CAuraEffectType type = CAuraEffectType::MOVE_SPEED;
    float value = 0;
    bool isPercent = false;
};

// Ability category (matches Java CAbilityCategory)
enum class CAbilityCategory : uint8_t {
    CORE, ATTACK, MOVEMENT, BUFF, ITEM, HERO, BUILD, INVENTORY,
    HARVEST, CARGO_HOLD, UPGRADE, QUEUE, RALLY, NEUTRAL_BUILDING,
    GOLD_MINE, SELL_ITEMS, ROOT, MOON_WELL, BLIGHT, SPELL
};

// Autocast acquisition kind (ref AutocastType + autoAcquireAutocastTargets).
enum class CAutocastKind : uint8_t {
    NONE = 0,
    LOWEST_HP_ALLY = 1, // heals: lowest-hp wounded ally in range
    NEAREST_ENEMY = 2,  // debuffs: nearest visible enemy in range
};

inline CAutocastKind GetAutocastKind(War3ID code) {
    switch (code.value) {
        case 0x41686561u: // Ahea
        case 0x41486862u: // AHhb
        case 0x414f6877u: // AOhw
        case 0x4172656au: // Arej
        case 0x414e6873u: // ANhs
        case 0x41496463u: // AIdc
            return CAutocastKind::LOWEST_HP_ALLY;
        case 0x41736c6fu: // Aslo
        case 0x41666165u: // Afae
        case 0x41706c79u: // Aply
        case 0x414f6878u: // AOhx
            return CAutocastKind::NEAREST_ENEMY;
        default:
            return CAutocastKind::NONE;
    }
}

// Disable type bitmask
enum class CAbilityDisableType : uint8_t {
    NONE = 0,
    REQUIREMENTS = 1,
    CONSTRUCTION = 2,
    TRANSFORMATION = 4,
    TRIGGER = 8,
    ATTACK_DISABLED = 16,
    PLAYER = 32
};

inline CAbilityDisableType operator|(CAbilityDisableType a, CAbilityDisableType b) {
    return static_cast<CAbilityDisableType>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool HasDisableFlag(uint8_t flags, CAbilityDisableType flag) {
    return (flags & static_cast<uint8_t>(flag)) != 0;
}

// Compact bitmask flags for CAbility instance state (Phase 18 Stage RR)
enum class AbilityStateFlag : uint8_t {
    NONE                = 0,
    PHYSICAL            = 1u << 0,
    PERMANENT           = 1u << 1,
    ICON_SHOWING        = 1u << 2,
    AUTOCAST_ACTIVE     = 1u << 3,
    PASSIVE_SPELL       = 1u << 4,
    GENERIC_PLACEHOLDER = 1u << 5,
};

// ============================================================
// CAbility - base class for all abilities
//
// Abilities validate orders and create/return behaviors.
// Phase 17–19: Provides the complete virtual capability contract for
// spell casting/cooldowns, worker resource gathering/dropoff, mining,
// repair, production queues, build menus, building upgrades, rally points,
// inventory slots, hero progression/stats, and shop stock so `src/ecs/`,
// `src/ecs/behaviors/*`, `src/scripting/*`, `src/ui/*`, `src/core/*`, and
// `src/map/*` have ZERO concrete WarCraft 3 ability header `#include`s.
// ============================================================
class CAbility {
public:
    static constexpr int MAX_QUEUE_SLOTS     = 5;
    static constexpr int MAX_INVENTORY_SLOTS = 6;

    CAbility(int handleId, War3ID code, CAbilityCategory category);
    virtual ~CAbility() = default;

    // Identity
    int GetHandleId() const { return handleId_; }
    War3ID GetCode() const { return code_; }
    War3ID GetAlias() const { return alias_; }
    void SetAlias(War3ID alias) { alias_ = alias; }
    CAbilityCategory GetCategory() const { return category_; }
    virtual int GetLevel() const { return static_cast<int>(level_); }
    virtual void SetLevel(int level) { level_ = static_cast<uint16_t>(level < 0 ? 0 : level); }

    // State
    bool IsDisabled() const { return disabled_ != 0; }
    bool IsPhysical() const { return physical_ != 0; }
    bool IsPermanent() const { return permanent_ != 0; }
    bool IsIconShowing() const { return iconShowing_ != 0; }
    bool IsPassiveSpell() const { return passiveSpell_ != 0; }
    bool IsGenericPlaceholder() const { return genericPlaceholder_ != 0; }
    uint8_t GetDisabledFlags() const { return disabled_; }

    void SetDisabled(bool disabled, CAbilityDisableType type);
    void SetPhysical(bool v) { physical_ = v ? 1u : 0u; }
    void SetPermanent(bool v) { permanent_ = v ? 1u : 0u; }
    void SetIconShowing(bool v) { iconShowing_ = v ? 1u : 0u; }
    void SetPassiveSpell(bool v) { passiveSpell_ = v ? 1u : 0u; }
    void SetGenericPlaceholder(bool v) { genericPlaceholder_ = v ? 1u : 0u; }
    bool IsAutocastActive() const { return autocastActive_ != 0; }
    void SetAutocastActive(bool v) { autocastActive_ = v ? 1u : 0u; }

    // O(1) bitmask inspection/restoration for ability state flags
    uint8_t GetAbilityStateFlagsBitmask() const {
        uint8_t mask = 0;
        if (physical_)           mask |= static_cast<uint8_t>(AbilityStateFlag::PHYSICAL);
        if (permanent_)          mask |= static_cast<uint8_t>(AbilityStateFlag::PERMANENT);
        if (iconShowing_)        mask |= static_cast<uint8_t>(AbilityStateFlag::ICON_SHOWING);
        if (autocastActive_)     mask |= static_cast<uint8_t>(AbilityStateFlag::AUTOCAST_ACTIVE);
        if (passiveSpell_)       mask |= static_cast<uint8_t>(AbilityStateFlag::PASSIVE_SPELL);
        if (genericPlaceholder_) mask |= static_cast<uint8_t>(AbilityStateFlag::GENERIC_PLACEHOLDER);
        return mask;
    }
    void SetAbilityStateFlagsBitmask(uint8_t mask) {
        physical_           = (mask & static_cast<uint8_t>(AbilityStateFlag::PHYSICAL)) ? 1u : 0u;
        permanent_          = (mask & static_cast<uint8_t>(AbilityStateFlag::PERMANENT)) ? 1u : 0u;
        iconShowing_        = (mask & static_cast<uint8_t>(AbilityStateFlag::ICON_SHOWING)) ? 1u : 0u;
        autocastActive_     = (mask & static_cast<uint8_t>(AbilityStateFlag::AUTOCAST_ACTIVE)) ? 1u : 0u;
        passiveSpell_       = (mask & static_cast<uint8_t>(AbilityStateFlag::PASSIVE_SPELL)) ? 1u : 0u;
        genericPlaceholder_ = (mask & static_cast<uint8_t>(AbilityStateFlag::GENERIC_PLACEHOLDER)) ? 1u : 0u;
    }

    // Base engine order id for this ability (ref getBaseOrderId); 0 when
    // the ability is not triggered by orders (passives, item powers).
    virtual int GetBaseOrderId() const { return 0; }

    // Lifecycle hooks
    virtual void OnAdd(CSimulation& game, CUnit* unit);
    virtual void OnRemove(CSimulation& game, CUnit* unit);
    virtual void OnTick(CSimulation& game, CUnit* unit);
    virtual void OnDeath(CSimulation& game, CUnit* unit);
    virtual void OnLevelChanged(CSimulation& game, CUnit* unit);

    // Order validation
    virtual bool CheckCanUse(CSimulation& game, CUnit* unit, int orderId) const;
    virtual bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const;
    virtual bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const;
    virtual bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const;

    // Check if requirements are met (tech tree)
    virtual bool IsRequirementsMet(CSimulation& game, CUnit* unit) const;

    // Order interception (returns false to consume order without queueing)
    virtual bool CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, CWidget* target);
    virtual bool CheckBeforeQueue(CSimulation& game, CUnit* caster, int orderId, float x, float y);
    virtual bool CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId);

    // Begin execution - returns a behavior the unit should start performing
    virtual CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target);
    virtual CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y);
    virtual CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId);

    // ===== Phase 17–19 Generic Capability Interfaces =====

    // 1. Spell Casting & Cooldown Interface (overridden by CAbilitySpellBase)
    virtual bool IsSpellAbility() const { return false; }
    virtual CSpellTargetType GetTargetType() const { return CSpellTargetType::NO_TARGET; }
    virtual void SetTargetType(CSpellTargetType /*v*/) {}
    virtual float GetCastRange() const { return 0.0f; }
    virtual void SetCastRange(float /*v*/) {}
    virtual float GetCastPoint() const { return 0.0f; }
    virtual void SetCastPoint(float /*v*/) {}
    virtual float GetCooldown() const { return 0.0f; }
    virtual void SetCooldown(float /*v*/) {}
    virtual float GetCooldownRemaining() const { return 0.0f; }
    virtual int GetManaCost() const { return 0; }
    virtual void SetManaCost(int /*v*/) {}
    virtual void StartCooldown() {}
    virtual void ResetCooldown() {}
    virtual void UpdateCooldown(float /*dt*/) {}
    virtual void DoEffect(CSimulation& /*game*/, CUnit* /*caster*/, CWidget* /*target*/) {}
    virtual void DoEffect(CSimulation& /*game*/, CUnit* /*caster*/, float /*x*/, float /*y*/) {}
    virtual void DoEffect(CSimulation& /*game*/, CUnit* /*caster*/) {}

    // 2. Worker Resource Harvest & Dropoff Interface (overridden by CAbilityHarvest / CAbilityReturnResources)
    virtual CResourceType GetCarriedResourceType() const { return CResourceType::NONE; }
    virtual int GetCarriedAmount() const { return 0; }
    virtual void SetCarried(CResourceType /*type*/, int /*amount*/) {}
    virtual void ClearCarried() {}
    virtual int GetLastTargetHandle() const { return -1; }
    virtual void SetLastTargetHandle(int /*handleId*/) {}
    virtual bool IsCarryingResources() const { return GetCarriedAmount() > 0; }
    virtual bool AcceptsResourceDropoff(CResourceType /*type*/) const { return false; }

    // 3. Resource Mine Interface (overridden by CAbilityGoldMine, CAbilityOverlayedGoldMine, CAbilityBlightedGoldMine)
    virtual bool IsResourceMine() const { return false; }
    virtual int GetGold() const { return 0; }
    virtual void SetGold(int /*v*/) {}
    virtual bool CanAddMiner() const { return false; }
    virtual void AddMiner(CBehavior* /*miner*/) {}
    virtual void RemoveMiner(CBehavior* /*miner*/) {}
    virtual float GetMiningDuration() const { return 5.0f; }

    // 4. Repair & Working Animation State Interface (overridden by CAbilityRepair, CAbilityQueue, CAbilityGoldMine)
    virtual bool IsRepairAbility() const { return false; }
    virtual float GetRepairRate() const { return 10.0f; }
    virtual float GetRepairRange() const { return 128.0f; }
    virtual bool IsActivelyWorking() const { return false; }

    // 5. Inventory Slot Interface (overridden by CAbilityInventory)
    virtual bool PickUpItem(CSimulation& /*game*/, CUnit* /*unit*/, CItem* /*item*/) { return false; }
    virtual bool PickUpItem(CSimulation& game, CUnit* unit, CItem* item, int /*slotPreference*/) {
        return PickUpItem(game, unit, item);
    }
    virtual bool DropItem(CSimulation& /*game*/, CUnit* /*unit*/, int /*slot*/) { return false; }
    virtual bool DropItemAt(CSimulation& /*game*/, CUnit* /*unit*/, int /*slot*/, float /*x*/, float /*y*/) { return false; }
    virtual bool DropItem(CSimulation& /*game*/, CUnit* /*unit*/, CItem* /*item*/) { return false; }
    virtual bool DropItemAt(CSimulation& /*game*/, CUnit* /*unit*/, CItem* /*item*/, float /*x*/, float /*y*/) { return false; }
    virtual bool RemoveItemAt(CSimulation& /*game*/, CUnit* /*unit*/, int /*slot*/) { return false; }
    virtual bool UseItem(CSimulation& /*game*/, CUnit* /*unit*/, int /*slot*/) { return false; }
    virtual bool UseItem(CSimulation& /*game*/, CUnit* /*unit*/, CItem* /*item*/) { return false; }
    virtual bool MoveItemToSlot(CItem* /*item*/, int /*slot*/) { return false; }
    virtual CItem* GetItem(int /*slot*/) const { return nullptr; }
    virtual int FindEmptySlot() const { return -1; }
    virtual int FindItemSlot(CItem* /*item*/) const { return -1; }
    virtual bool HasItem(CItem* item) const { return FindItemSlot(item) >= 0; }
    virtual int GetItemCount() const { return 0; }
    virtual int GetItemCapacity() const { return 0; }
    virtual void SetItemCapacity(int /*cap*/) {}
    virtual void SetCanDrop(bool /*v*/) {}

    // 6. Production Queue, Build Menu, Upgrade & Rally Interface (overridden by CAbilityQueue, CAbilityBuild, CAbilityUpgrade, CAbilityRally)
    static const std::unordered_set<War3ID>& EmptyWar3IdSet();
    virtual bool QueueUnit(CSimulation& /*game*/, CUnit* /*building*/, War3ID /*unitTypeId*/) { return false; }
    virtual bool QueueResearch(CSimulation& /*game*/, CUnit* /*building*/, War3ID /*upgradeId*/) { return false; }
    virtual bool QueueReviveHero(CSimulation& /*game*/, CUnit* /*building*/, CUnit* /*hero*/) { return false; }
    virtual void CancelQueueItem(CSimulation& /*game*/, CUnit* /*building*/, int /*slot*/) {}
    virtual War3ID GetQueuedItem(int /*slot*/) const { return War3ID(); }
    virtual CQueueItemType GetQueuedItemType(int /*slot*/) const { return CQueueItemType::NONE; }
    virtual int GetQueueCount() const { return 0; }
    virtual float GetProgress() const { return 0.0f; }
    virtual float GetBuildTime() const { return 0.0f; }
    virtual const std::unordered_set<War3ID>& GetBuildableStructures() const { return EmptyWar3IdSet(); }
    virtual const std::unordered_set<War3ID>& GetTrainedUnits() const { return EmptyWar3IdSet(); }
    virtual const std::unordered_set<War3ID>& GetResearches() const { return EmptyWar3IdSet(); }
    virtual const std::unordered_set<War3ID>& GetUpgradesTo() const { return EmptyWar3IdSet(); }
    virtual CWidget* GetRallyTarget() const { return nullptr; }
    virtual float GetRallyX() const { return 0.0f; }
    virtual float GetRallyY() const { return 0.0f; }
    virtual bool HasRallyPoint() const { return false; }
    virtual bool IsExplicitlySet() const { return false; }

    // 7. Hero Progression & Shop Stock Interface (overridden by CAbilityHero and CAbilitySellItems)
    static const HeroStatValue& EmptyHeroStat();
    static const std::string& EmptyProperName();
    virtual int GetXP() const { return 0; }
    virtual void AddXP(CSimulation& /*game*/, CUnit* /*unit*/, int /*amount*/) {}
    virtual void SetXP(CSimulation& /*game*/, CUnit* /*unit*/, int /*value*/) {}
    virtual int GetHeroLevel() const { return 1; }
    virtual void SetHeroLevel(CSimulation& /*game*/, CUnit* /*unit*/, int /*level*/) {}
    virtual bool StripLevels(CSimulation& /*game*/, CUnit* /*unit*/, int /*levels*/) { return false; }
    virtual bool SelectHeroSkill(CSimulation& /*game*/, CUnit* /*unit*/, War3ID /*skillCode*/) { return false; }
    virtual int GetSkillPoints() const { return 0; }
    virtual void SetSkillPoints(int /*v*/) {}
    virtual const HeroStatValue& GetStrength() const { return EmptyHeroStat(); }
    virtual const HeroStatValue& GetAgility() const { return EmptyHeroStat(); }
    virtual const HeroStatValue& GetIntelligence() const { return EmptyHeroStat(); }
    virtual void SetStrengthBase(int /*v*/) {}
    virtual void SetAgilityBase(int /*v*/) {}
    virtual void SetIntelligenceBase(int /*v*/) {}
    virtual void AddStrengthBonus(CSimulation& /*game*/, CUnit* /*unit*/, int /*v*/) {}
    virtual void AddAgilityBonus(CSimulation& /*game*/, CUnit* /*unit*/, int /*v*/) {}
    virtual void AddIntelligenceBonus(CSimulation& /*game*/, CUnit* /*unit*/, int /*v*/) {}
    virtual const std::string& GetProperName() const { return EmptyProperName(); }
    virtual void SetProperName(const std::string& /*name*/) {}
    virtual const std::unordered_set<War3ID>& GetLearnableSkills() const { return EmptyWar3IdSet(); }
    virtual void AddStockItem(War3ID /*id*/, int /*current*/, int /*max*/) {}
    virtual void AddStockUnit(War3ID /*id*/, int /*current*/, int /*max*/) {}
    virtual bool RemoveStock(War3ID /*id*/) { return false; }
    virtual void SetStockMax(int /*v*/) {}
    virtual void SetStockReplenishInterval(float /*v*/) {}
    virtual void AddLearnableSkill(War3ID /*skillCode*/) {}

    // 8. Phase 22 Stage ZZ2: Extended Queue/Upgrade/Rally/Cargo/Mine/Pairing/Combat Passive Contracts
    virtual void AddTrainedUnit(War3ID /*unitTypeId*/) {}
    virtual void AddResearch(War3ID /*upgradeId*/) {}
    virtual void AddUpgradeTo(War3ID /*targetTypeId*/) {}
    virtual void ApplyToTrainedUnit(CSimulation& /*game*/, CUnit* /*building*/, CUnit* /*trainedUnit*/) const {}
    virtual void ApplyData(const struct CAbilityDefFull& /*def*/) {}
    virtual bool IsOnCooldown() const { return GetCooldownRemaining() > 0.0f; }

    // Cargo Hold & Transport Load virtual contract
    virtual int GetCargoCapacity() const { return 0; }
    virtual void SetCargoCapacity(int /*v*/) {}
    virtual void SetLoadRange(float /*v*/) {}
    virtual bool TryLoad(CUnit* /*holder*/, CUnit* /*passenger*/) { return false; }
    virtual void UnloadAll(CUnit* /*holder*/) {}

    // Blighted & Overlayed Mine virtual contract
    virtual bool AddMiner() { return false; }
    virtual void RemoveMiner() {}
    virtual void BindParentMineUnit(CUnit* /*parentUnit*/) {}

    // Militia / Pairing conversion virtual contract
    virtual bool Convert(CSimulation& /*game*/, CUnit* /*self*/) { return false; }
    virtual void SetConverted(bool /*v*/) {}

    // Passive Combat Procs virtual contract (Spiked Carapace, Bash, Cleaving Attack, Pulverize)
    virtual float GetReflectPercent() const { return 0.0f; }
    virtual float GetProcChance() const { return 0.0f; }
    virtual float GetProcDuration() const { return 0.0f; }
    virtual float GetSplashFraction() const { return 0.0f; }
    virtual float GetSplashRadius() const { return 0.0f; }

protected:
    int handleId_;
    War3ID code_;
    War3ID alias_;
    CAbilityCategory category_;
    uint8_t disabled_ = 0;
    uint16_t level_              : 10;
    uint16_t physical_           : 1;
    uint16_t permanent_          : 1;
    uint16_t iconShowing_        : 1;
    uint16_t autocastActive_     : 1;
    uint16_t passiveSpell_       : 1;
    uint16_t genericPlaceholder_ : 1;
};

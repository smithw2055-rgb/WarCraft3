#pragma once
#include <cstdint>
#include <string>
#include "ecs/fwd.h"
#include "resource/slk.h"

class CBehavior;
class CBehaviorHarvest;
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
// Central table so the sim (idle acquisition) and the UI (right-click
// toggle) agree on which rawcodes participate.
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
    NONE            = 0,
    PHYSICAL        = 1u << 0,
    PERMANENT       = 1u << 1,
    ICON_SHOWING    = 1u << 2,
    AUTOCAST_ACTIVE = 1u << 3,
    PASSIVE_SPELL   = 1u << 4,
};

// ============================================================
// CAbility - base class for all abilities
//
// Abilities validate orders and create/return behaviors.
// Phase 17: Provides virtual capability query/action interfaces for
// casting, resource gathering/dropoff, mining, repair, work state,
// and inventory management so generic ECS behaviors (`src/ecs/behaviors/*`)
// and presentation helpers never include concrete WarCraft 3 ability headers.
// Phase 18: Packs `level_` (11-bit) and boolean flags (`physical_`,
// `permanent_`, `iconShowing_`, `autocastActive_`, `passiveSpell_`) into a
// shared `uint16_t` bitfield alongside `category_` and `disabled_`, shrinking
// `sizeof(CAbility)` to 24 bytes (down from 40 bytes).
// ============================================================
class CAbility {
public:
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
    uint8_t GetDisabledFlags() const { return disabled_; }

    void SetDisabled(bool disabled, CAbilityDisableType type);
    void SetPhysical(bool v) { physical_ = v ? 1u : 0u; }
    void SetPermanent(bool v) { permanent_ = v ? 1u : 0u; }
    void SetIconShowing(bool v) { iconShowing_ = v ? 1u : 0u; }
    void SetPassiveSpell(bool v) { passiveSpell_ = v ? 1u : 0u; }
    bool IsAutocastActive() const { return autocastActive_ != 0; }
    void SetAutocastActive(bool v) { autocastActive_ = v ? 1u : 0u; }

    // O(1) bitmask inspection/restoration for ability state flags
    uint8_t GetAbilityStateFlagsBitmask() const {
        uint8_t mask = 0;
        if (physical_)       mask |= static_cast<uint8_t>(AbilityStateFlag::PHYSICAL);
        if (permanent_)      mask |= static_cast<uint8_t>(AbilityStateFlag::PERMANENT);
        if (iconShowing_)    mask |= static_cast<uint8_t>(AbilityStateFlag::ICON_SHOWING);
        if (autocastActive_) mask |= static_cast<uint8_t>(AbilityStateFlag::AUTOCAST_ACTIVE);
        if (passiveSpell_)   mask |= static_cast<uint8_t>(AbilityStateFlag::PASSIVE_SPELL);
        return mask;
    }
    void SetAbilityStateFlagsBitmask(uint8_t mask) {
        physical_       = (mask & static_cast<uint8_t>(AbilityStateFlag::PHYSICAL)) ? 1u : 0u;
        permanent_      = (mask & static_cast<uint8_t>(AbilityStateFlag::PERMANENT)) ? 1u : 0u;
        iconShowing_    = (mask & static_cast<uint8_t>(AbilityStateFlag::ICON_SHOWING)) ? 1u : 0u;
        autocastActive_ = (mask & static_cast<uint8_t>(AbilityStateFlag::AUTOCAST_ACTIVE)) ? 1u : 0u;
        passiveSpell_   = (mask & static_cast<uint8_t>(AbilityStateFlag::PASSIVE_SPELL)) ? 1u : 0u;
    }

    // Base engine order id for this ability (ref getBaseOrderId); 0 when
    // the ability is not triggered by orders (passives, item powers).
    virtual int GetBaseOrderId() const { return 0; }

    // Lifecycle hooks
    virtual void OnAdd(CSimulation& game, CUnit* unit);
    virtual void OnRemove(CSimulation& game, CUnit* unit);
    virtual void OnTick(CSimulation& game, CUnit* unit);
    virtual void OnDeath(CSimulation& game, CUnit* unit);
    // Hero skill level-up refresh: unapply then re-apply so passives
    // that pushed member values at OnAdd time (evasion, crit, carapace)
    // pick up the new level's numbers. Implementations must keep the
    // OnRemove/OnAdd pair idempotent.
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
    // Ownership of returned behavior is transferred to caller
    virtual CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target);
    virtual CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y);
    virtual CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId);

    // ===== Phase 17 Generic Capability Interfaces (Decouples Behaviors from Concrete Abilities) =====

    // 1. Spell Casting & Cooldown Interface (overridden by CAbilitySpellBase)
    virtual float GetCastRange() const { return 0.0f; }
    virtual float GetCastPoint() const { return 0.0f; }
    virtual int GetManaCost() const { return 0; }
    virtual void StartCooldown() {}
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

    // 3. Resource Mine Interface (overridden by CAbilityGoldMine & CAbilityOverlayedGoldMine)
    virtual bool IsResourceMine() const { return false; }
    virtual int GetGold() const { return 0; }
    virtual void SetGold(int /*v*/) {}
    virtual bool CanAddMiner() const { return false; }
    virtual void AddMiner(CBehaviorHarvest* /*miner*/) {}
    virtual void RemoveMiner(CBehaviorHarvest* /*miner*/) {}
    virtual float GetMiningDuration() const { return 5.0f; }

    // 4. Repair & Working Animation State Interface (overridden by CAbilityRepair, CAbilityQueue, CAbilityGoldMine)
    virtual float GetRepairRate() const { return 10.0f; }
    virtual float GetRepairRange() const { return 128.0f; }
    virtual bool IsActivelyWorking() const { return false; }

    // 5. Inventory Slot Interface (overridden by CAbilityInventory)
    virtual bool PickUpItem(CSimulation& /*game*/, CUnit* /*unit*/, CItem* /*item*/) { return false; }
    virtual bool DropItem(CSimulation& /*game*/, CUnit* /*unit*/, int /*slot*/) { return false; }
    virtual bool DropItemAt(CSimulation& /*game*/, CUnit* /*unit*/, int /*slot*/, float /*x*/, float /*y*/) { return false; }
    virtual CItem* GetItem(int /*slot*/) const { return nullptr; }
    virtual int FindItemSlot(CItem* /*item*/) const { return -1; }

protected:
    int handleId_;
    War3ID code_;
    War3ID alias_;
    CAbilityCategory category_;
    uint8_t disabled_ = 0;
    uint16_t level_          : 11;
    uint16_t physical_       : 1;
    uint16_t permanent_      : 1;
    uint16_t iconShowing_    : 1;
    uint16_t autocastActive_ : 1;
    uint16_t passiveSpell_   : 1;
};

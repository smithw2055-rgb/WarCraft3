#pragma once
#include <cstdint>
#include <string>
#include "ecs/fwd.h"
#include "resource/slk.h"

class CBehavior;
class COrder;
class COrderTargetPoint;
class COrderTargetWidget;
class COrderNoTarget;

// Resource types carried by workers or harvested from resource nodes
enum class CResourceType : uint8_t {
    NONE, GOLD, LUMBER
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

// ============================================================
// CAbility - base class for all abilities
//
// Abilities validate orders and create/return behaviors.
// They do NOT execute per-tick logic themselves (most have
// empty OnTick). Behaviors are the per-tick execution units.
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
    virtual int GetLevel() const { return level_; }
    virtual void SetLevel(int level) { level_ = level; }

    // State
    bool IsDisabled() const { return disabled_ != 0; }
    bool IsPhysical() const { return physical_; }
    bool IsPermanent() const { return permanent_; }
    bool IsIconShowing() const { return iconShowing_; }
    uint8_t GetDisabledFlags() const { return disabled_; }

    void SetDisabled(bool disabled, CAbilityDisableType type);
    void SetPhysical(bool v) { physical_ = v; }
    void SetPermanent(bool v) { permanent_ = v; }
    void SetIconShowing(bool v) { iconShowing_ = v; }
    bool IsAutocastActive() const { return autocastActive_; }
    void SetAutocastActive(bool v) { autocastActive_ = v; }

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

protected:
    int handleId_;
    War3ID code_;
    War3ID alias_;
    CAbilityCategory category_;
    int level_ = 1;
    uint8_t disabled_ = 0;
    bool physical_ = false;
    bool permanent_ = false;
    bool iconShowing_ = true;
    bool autocastActive_ = false;
};

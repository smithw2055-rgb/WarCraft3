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

// Ability category (matches Java CAbilityCategory)
enum class CAbilityCategory : uint8_t {
    CORE, ATTACK, MOVEMENT, BUFF, ITEM, HERO, BUILD, INVENTORY,
    HARVEST, CARGO_HOLD, UPGRADE, QUEUE, RALLY, NEUTRAL_BUILDING,
    GOLD_MINE, SELL_ITEMS, ROOT, MOON_WELL, BLIGHT, SPELL
};

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
};

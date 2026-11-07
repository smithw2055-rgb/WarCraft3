#pragma once
#include <cstdint>
#include <string>
#include "resource/slk.h"

class CUnit;
class CSimulation;

// Buff stacking type
enum class CBuffStackType : uint8_t {
    NONE,       // No stacking, refresh duration only
    STACK,      // Stack count increases
    REPLACE     // Replace old buff entirely
};

// Buff target type
enum class CBuffTargetType : uint8_t {
    SELF,       // Applied to caster
    TARGET,     // Applied to single target
    AOE         // Applied to area of effect
};

// ============================================================
// Stage Z1: Zero-allocation POD descriptor for data-driven
// numerical/periodic buffs (Unity DOTS GameplayEffect style).
// ============================================================
struct DataDrivenBuffSpec {
    War3ID id = War3ID(0u);
    float duration = 0.0f;
    bool isPositive = true;
    CBuffStackType stackType = CBuffStackType::NONE;
    int maxStacks = 1;
    int casterHandleId = -1;

    // Stat modifiers applied via UnitAttributeAggregator
    float moveSpeedPercent = 0.0f;     // e.g. -0.25f for 25% slow, +0.30f for 30% haste
    float attackSpeedBonus = 0.0f;     // e.g. +0.40f for 40% IAS, -0.25f for slow
    float armorDelta = 0.0f;           // e.g. +5.0f or -4.0f armor
    int flatDamageBonus = 0;           // e.g. +15 attack damage

    // Periodic tick effect (periodicDamage > 0 deals damage, < 0 heals target)
    bool periodic = false;
    float periodicInterval = 1.0f;
    float periodicDamage = 0.0f;
};

struct DataDrivenBuffRecord {
    DataDrivenBuffSpec spec;
    float remaining = 0.0f;
    float periodicTimer = 0.0f;
    int stackCount = 1;
    float appliedArmorTotal = 0.0f;
    bool suspended = false;
    bool active = false;
};

// ============================================================
// CBuff - base class for all buffs/debuffs
// ============================================================
class CBuff {
public:
    CBuff(War3ID id, float duration, bool isPositive);
    virtual ~CBuff() = default;

    // Identity
    War3ID GetId() const { return id_; }
    bool IsPositive() const { return isPositive_; }
    bool IsNegative() const { return !isPositive_; }

    // Duration
    float GetDuration() const { return duration_; }
    float GetRemaining() const { return remaining_; }
    bool IsExpired() const { return remaining_ <= 0; }
    void SetDuration(float v) { duration_ = v; }
    void SetRemaining(float v) { remaining_ = v; }
    void RefreshDuration() { remaining_ = duration_; }
    virtual void OnRefresh(CSimulation& game, CUnit* target, const CBuff& newBuff);
    // Resistant Skin quarters a negative buff before it is applied.
    void ScaleDuration(float factor) {
        if (factor < 0.0f) factor = 0.0f;
        duration_ *= factor;
        remaining_ *= factor;
    }

    // Stacking
    CBuffStackType GetStackType() const { return stackType_; }
    void SetStackType(CBuffStackType v) { stackType_ = v; }
    // Aura merge: same-id buffs from DIFFERENT casters coexist (strongest
    // wins via the target's source table); same caster refreshes. Only
    // aura buffs opt in (ref per-source aura buffs).
    virtual bool MergeByCaster() const { return false; }
    int GetStackCount() const { return stackCount_; }
    int GetMaxStacks() const { return maxStacks_; }
    void SetMaxStacks(int v) { maxStacks_ = v; }
    bool AddStack();
    void RemoveStack();

    // Periodic effect
    bool IsPeriodic() const { return periodic_; }
    void SetPeriodic(bool v, float interval = 1.0f) { periodic_ = v; periodicInterval_ = interval; }
    float GetPeriodicInterval() const { return periodicInterval_; }

    // Caster
    int GetCasterHandleId() const { return casterHandleId_; }
    void SetCasterHandleId(int v) { casterHandleId_ = v; }

    // Suspension (ref UnitPauseTimedLife): suspended buffs neither tick
    // down nor fire periodic effects until resumed.
    bool IsSuspended() const { return suspended_; }
    void SetSuspended(bool v) { suspended_ = v; }

    // Lifecycle
    virtual void OnApply(CSimulation& game, CUnit* target);
    virtual void OnRemove(CSimulation& game, CUnit* target);
    virtual void OnTick(CSimulation& game, CUnit* target);
    virtual void OnPeriodic(CSimulation& game, CUnit* target);
    virtual void OnDeath(CSimulation& game, CUnit* target);

    // Clone for stacking
    virtual CBuff* Clone() const = 0;

protected:
    War3ID id_;
    float duration_;
    float remaining_;
    bool isPositive_;
    CBuffStackType stackType_ = CBuffStackType::NONE;
    int stackCount_ = 1;
    int maxStacks_ = 1;
    bool periodic_ = false;
    float periodicInterval_ = 1.0f;
    float periodicTimer_ = 0;
    int casterHandleId_ = -1;
    bool suspended_ = false;
};

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
    void SetRemaining(float v) { remaining_ = v; }
    void RefreshDuration() { remaining_ = duration_; }
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

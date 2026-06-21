#pragma once
#include "ecs/ability.h"

// Spell targeting type
enum class CSpellTargetType : uint8_t {
    NO_TARGET,      // Instant cast, no target needed
    TARGET_UNIT,    // Must target a unit
    TARGET_POINT,   // Must target a point
    TARGET_BOTH     // Can target unit or point
};

// ============================================================
// CAbilitySpellBase - base class for all spell abilities
//
// Spell lifecycle:
//   1. CheckCanUse() - validates mana, cooldown, disabled
//   2. CheckCanTarget() - validates target type
//   3. Begin() -> returns CBehaviorSpell
//   4. CBehaviorSpell handles:
//      a. Move into cast range
//      b. Face target
//      c. Play cast animation (cast point)
//      d. Deduct mana, start cooldown
//      e. Call DoEffect() - the actual spell effect
//      f. Play backswing animation
// ============================================================
class CAbilitySpellBase : public CAbility {
public:
    CAbilitySpellBase(int handleId, War3ID code, CAbilityCategory category);

    // Configuration
    CSpellTargetType GetTargetType() const { return targetType_; }
    void SetTargetType(CSpellTargetType v) { targetType_ = v; }
    float GetCastRange() const { return castRange_; }
    void SetCastRange(float v) { castRange_ = v; }
    float GetCastPoint() const { return castPoint_; }
    void SetCastPoint(float v) { castPoint_ = v; }
    float GetCooldown() const { return cooldown_; }
    void SetCooldown(float v) { cooldown_ = v; }
    int GetManaCost() const { return manaCost_; }
    void SetManaCost(int v) { manaCost_ = v; }
    int GetLevel() const { return level_; }
    void SetLevel(int v) { level_ = v; }
    int GetMaxLevel() const { return maxLevel_; }
    void SetMaxLevel(int v) { maxLevel_ = v; }

    // Order validation
    bool CheckCanUse(CSimulation& game, CUnit* unit, int orderId) const override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;

    // Begin casting
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // The actual spell effect - override in subclasses
    virtual void DoEffect(CSimulation& game, CUnit* caster, CWidget* target);
    virtual void DoEffect(CSimulation& game, CUnit* caster, float x, float y);
    virtual void DoEffect(CSimulation& game, CUnit* caster);

    // Cooldown state
    bool IsOnCooldown() const { return cooldownRemaining_ > 0; }
    void StartCooldown() { cooldownRemaining_ = cooldown_; }
    void UpdateCooldown(float dt) { if (cooldownRemaining_ > 0) cooldownRemaining_ -= dt; }
    float GetCooldownRemaining() const { return cooldownRemaining_; }

protected:
    CSpellTargetType targetType_ = CSpellTargetType::NO_TARGET;
    float castRange_ = 600.0f;
    float castPoint_ = 0.3f;
    float cooldown_ = 0;
    int manaCost_ = 0;
    int level_ = 1;
    int maxLevel_ = 3;
    float cooldownRemaining_ = 0;
};

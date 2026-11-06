#pragma once
#include "ecs/ability.h"
#include "ecs/abilities/ability_builder.h"

struct CAbilityDefFull;

// Spell targeting type
enum class CSpellTargetType : uint8_t {
    NO_TARGET,      // Instant cast, no target needed
    TARGET_UNIT,    // Must target a unit
    TARGET_POINT,   // Must target a point
    TARGET_BOTH     // Can target unit or point
};

// ============================================================
// CAbilitySpellBase - base class for all spell abilities
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
    int GetLevel() const override { return level_; }
    void SetLevel(int v) override;
    int GetMaxLevel() const { return maxLevel_; }
    void SetMaxLevel(int v) { maxLevel_ = v; }
    void ApplyData(const CAbilityDefFull& def);

    // Order validation
    int GetBaseOrderId() const override;
    bool CheckCanUse(CSimulation& game, CUnit* unit, int orderId) const override;
    bool CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const override;
    bool CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const override;
    bool CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const override;

    // Begin casting
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) override;
    CBehavior* Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) override;
    CBehavior* BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) override;

    // The actual spell effect - defaults to dispatching any registered
    // ABEffectChain in AbilityBuilderRegistry::Instance(), or overridden in subclasses.
    virtual void DoEffect(CSimulation& game, CUnit* caster, CWidget* target);
    virtual void DoEffect(CSimulation& game, CUnit* caster, float x, float y);
    virtual void DoEffect(CSimulation& game, CUnit* caster);

    // Cooldown state
    bool IsOnCooldown() const { return cooldownRemaining_ > 0; }
    void StartCooldown() { cooldownRemaining_ = cooldown_; }
    void ResetCooldown() { cooldownRemaining_ = 0.0f; }
    void UpdateCooldown(float dt) { if (cooldownRemaining_ > 0) cooldownRemaining_ -= dt; }
    float GetCooldownRemaining() const { return cooldownRemaining_; }

protected:
    virtual void ApplyEffectData(const CAbilityDefFull& def, int level);

    CSpellTargetType targetType_ = CSpellTargetType::NO_TARGET;
    float castRange_ = 600.0f;
    float castPoint_ = 0.3f;
    float cooldown_ = 0;
    int manaCost_ = 0;
    int level_ = 1;
    int maxLevel_ = 3;
    float cooldownRemaining_ = 0;
    const CAbilityDefFull* dataDef_ = nullptr;
};

// ============================================================
// CAbilityDataDrivenSpell - Unity ScriptableObject / Godot Resource
// style data-driven spell ability instantiated from ABSpellDescriptor.
// ============================================================
class CAbilityDataDrivenSpell : public CAbilitySpellBase {
public:
    CAbilityDataDrivenSpell(int handleId, War3ID code, const ABSpellDescriptor& desc);

    const ABSpellEffectContext& GetEffectContext() const { return ctx_; }
    const ABSpellDescriptor& GetDescriptor() const { return desc_; }

    void DoEffect(CSimulation& game, CUnit* caster, CWidget* target) override;
    void DoEffect(CSimulation& game, CUnit* caster, float x, float y) override;
    void DoEffect(CSimulation& game, CUnit* caster) override;

protected:
    void ApplyEffectData(const CAbilityDefFull& def, int level) override;

private:
    ABSpellDescriptor desc_;
    ABSpellEffectContext ctx_;
};

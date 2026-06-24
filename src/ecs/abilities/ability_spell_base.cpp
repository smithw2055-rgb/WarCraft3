#include "ability_spell_base.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_spell.h"
#include <iostream>

CAbilitySpellBase::CAbilitySpellBase(int handleId, War3ID code, CAbilityCategory category)
    : CAbility(handleId, code, category) {
    iconShowing_ = true;
}

bool CAbilitySpellBase::CheckCanUse(CSimulation& game, CUnit* unit, int orderId) const {
    (void)orderId;
    if (IsDisabled()) return false;
    if (IsOnCooldown()) return false;
    if (unit->GetMana() < manaCost_) return false;
    return true;
}

bool CAbilitySpellBase::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    (void)orderId;
    if (!CheckCanUse(game, unit, orderId)) return false;
    if (targetType_ == CSpellTargetType::NO_TARGET) return false;
    if (targetType_ == CSpellTargetType::TARGET_POINT) return false;
    return target != nullptr && !target->IsDead();
}

bool CAbilitySpellBase::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)x; (void)y;
    if (!CheckCanUse(game, unit, orderId)) return false;
    return targetType_ == CSpellTargetType::TARGET_POINT || targetType_ == CSpellTargetType::TARGET_BOTH;
}

bool CAbilitySpellBase::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    if (!CheckCanUse(game, unit, orderId)) return false;
    return targetType_ == CSpellTargetType::NO_TARGET;
}

CBehavior* CAbilitySpellBase::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)orderId;
    if (!target) return nullptr;

    // Create spell behavior
    auto* behavior = new CBehaviorSpell();
    behavior->SetUnit(caster);
    behavior->SetSpellAbility(this);
    behavior->SetTarget(target);
    behavior->SetCastRange(castRange_);
    behavior->SetCastPoint(castPoint_);
    if (auto* unitType = caster->GetUnitType()) {
        behavior->SetBackswingDuration(std::max(0.0f, unitType->GetCastBackswingPoint()));
    }
    return behavior;
}

CBehavior* CAbilitySpellBase::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)orderId;

    auto* behavior = new CBehaviorSpell();
    behavior->SetUnit(caster);
    behavior->SetSpellAbility(this);
    behavior->SetTargetPoint(x, y);
    behavior->SetCastRange(castRange_);
    behavior->SetCastPoint(castPoint_);
    if (auto* unitType = caster->GetUnitType()) {
        behavior->SetBackswingDuration(std::max(0.0f, unitType->GetCastBackswingPoint()));
    }
    return behavior;
}

CBehavior* CAbilitySpellBase::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)orderId;

    auto* behavior = new CBehaviorSpell();
    behavior->SetUnit(caster);
    behavior->SetSpellAbility(this);
    behavior->SetCastRange(0);
    behavior->SetCastPoint(castPoint_);
    if (auto* unitType = caster->GetUnitType()) {
        behavior->SetBackswingDuration(std::max(0.0f, unitType->GetCastBackswingPoint()));
    }
    return behavior;
}

void CAbilitySpellBase::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)game; (void)caster; (void)target;
}

void CAbilitySpellBase::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    (void)game; (void)caster; (void)x; (void)y;
}

void CAbilitySpellBase::DoEffect(CSimulation& game, CUnit* caster) {
    (void)game; (void)caster;
}

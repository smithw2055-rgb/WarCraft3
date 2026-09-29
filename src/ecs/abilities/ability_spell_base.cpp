#include "ability_spell_base.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_spell.h"
#include "ecs/orders/order.h"
#include "data/abilitydata.h"
#include <iostream>

CAbilitySpellBase::CAbilitySpellBase(int handleId, War3ID code, CAbilityCategory category)
    : CAbility(handleId, code, category) {
    iconShowing_ = true;
}

void CAbilitySpellBase::SetLevel(int v) {
    level_ = v;
    if (dataDef_) ApplyData(*dataDef_);
}

void CAbilitySpellBase::ApplyData(const CAbilityDefFull& def) {
    dataDef_ = &def;
    maxLevel_ = def.levels;
    cooldown_ = def.CooldownAt(level_);
    manaCost_ = def.ManaCostAt(level_);
    castRange_ = def.RangeAt(level_);
    castPoint_ = def.CastTimeAt(level_);
    ApplyEffectData(def, level_);
}

void CAbilitySpellBase::ApplyEffectData(const CAbilityDefFull& def, int level) {
    (void)def; (void)level;
}

int CAbilitySpellBase::GetBaseOrderId() const {
    return SpellBaseOrderId(code_);
}

bool CAbilitySpellBase::CheckCanUse(CSimulation& game, CUnit* unit, int orderId) const {
    if (IsDisabled()) return false;
    if (IsOnCooldown()) return false;
    if (!unit) return false;
    // Order routing (ref ability order checks): a nonzero order must be
    // this spell's base order (or toggle pair), or the alias order for
    // code-variant abilities (ANin answering for AUin Inferno, Adet for
    // Atru True Sight). Id 0 preserves legacy callers via positional match.
    if (!SpellMatchesOrder(code_, orderId) && !SpellMatchesOrder(alias_, orderId)) return false;
    // Silenced units cannot cast; hexed units can neither attack nor cast
    // (ref target checks ABConditionIsUnitPassAllAbilityTargetChecks).
    if (unit->IsSilenced() || unit->IsHexed()) return false;
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

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
    (void)game;
    if (IsDisabled()) return false;
    if (IsOnCooldown()) return false;
    if (!unit) return false;
    if (!SpellMatchesOrder(code_, orderId) && !SpellMatchesOrder(alias_, orderId)) return false;
    if (unit->IsSilenced() || unit->IsHexed()) return false;
    if (unit->GetMana() < manaCost_) return false;
    return true;
}

bool CAbilitySpellBase::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
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
    (void)game; (void)orderId;
    if (!target) return nullptr;

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
    (void)game; (void)orderId;

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
    (void)game; (void)orderId;

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
    const ABEffectChain* chain = AbilityBuilderRegistry::Instance().Find(alias_);
    if (!chain) chain = AbilityBuilderRegistry::Instance().Find(code_);
    if (chain) chain->Run(game, caster, target, false, 0.0f, 0.0f);
}

void CAbilitySpellBase::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    const ABEffectChain* chain = AbilityBuilderRegistry::Instance().Find(alias_);
    if (!chain) chain = AbilityBuilderRegistry::Instance().Find(code_);
    if (chain) chain->Run(game, caster, nullptr, true, x, y);
}

void CAbilitySpellBase::DoEffect(CSimulation& game, CUnit* caster) {
    const ABEffectChain* chain = AbilityBuilderRegistry::Instance().Find(alias_);
    if (!chain) chain = AbilityBuilderRegistry::Instance().Find(code_);
    if (chain) chain->Run(game, caster, nullptr, false, 0.0f, 0.0f);
}

// ============================================================
// CAbilityDataDrivenSpell implementation
// ============================================================
CAbilityDataDrivenSpell::CAbilityDataDrivenSpell(int handleId, War3ID code, const ABSpellDescriptor& desc)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL)
    , desc_(desc) {
    SetTargetType(static_cast<CSpellTargetType>(desc.targetType));
    SetCastRange(desc.defaultRange);
    SetCastPoint(desc.defaultCastPoint);
    SetCooldown(desc.defaultCooldown);
    SetManaCost(desc.defaultManaCost);
    SetMaxLevel(desc.maxLevel);
    ctx_.level = 1;
    ctx_.duration = desc.defaultDuration;
    ctx_.area = desc.defaultArea;
    for (int i = 0; i < 9; ++i) {
        ctx_.data[i] = desc.defaultData[i];
    }
}

void CAbilityDataDrivenSpell::ApplyEffectData(const CAbilityDefFull& def, int level) {
    ctx_.level = level;
    const float dur = def.DurationAt(level);
    ctx_.duration = (dur > 0.0f) ? dur : desc_.defaultDuration;
    const float area = def.AreaAt(level);
    ctx_.area = (area > 0.0f) ? area : desc_.defaultArea;
    for (int i = 0; i < 9; ++i) {
        ctx_.data[i] = def.DataFloatAt(level, i, desc_.defaultData[i]);
    }
}

void CAbilityDataDrivenSpell::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ctx_.level = level_;
    if (desc_.onEffect) {
        desc_.onEffect(ctx_, game, caster, target, false, 0.0f, 0.0f);
        return;
    }
    CAbilitySpellBase::DoEffect(game, caster, target);
}

void CAbilityDataDrivenSpell::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    ctx_.level = level_;
    if (desc_.onEffect) {
        desc_.onEffect(ctx_, game, caster, nullptr, true, x, y);
        return;
    }
    CAbilitySpellBase::DoEffect(game, caster, x, y);
}

void CAbilityDataDrivenSpell::DoEffect(CSimulation& game, CUnit* caster) {
    ctx_.level = level_;
    if (desc_.onEffect) {
        desc_.onEffect(ctx_, game, caster, nullptr, false, 0.0f, 0.0f);
        return;
    }
    CAbilitySpellBase::DoEffect(game, caster);
}

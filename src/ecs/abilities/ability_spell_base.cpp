#include "ability_spell_base.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/orders/order.h"
#include "data/abilitydata.h"
#include <iostream>

CAbilitySpellBase::CAbilitySpellBase(int handleId, War3ID code, CAbilityCategory category)
    : CAbility(handleId, code, category) {
    iconShowing_ = true;
}

void CAbilitySpellBase::SetLevel(int v) {
    level_ = static_cast<uint16_t>(v < 0 ? 0 : v);
    if (dataDef_) ApplyData(*dataDef_);
}

void CAbilitySpellBase::ApplyData(const CAbilityDefFull& def) {
    dataDef_ = &def;
    maxLevel_ = static_cast<int16_t>(def.levels);
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
    if (IsDisabled() || IsPassiveSpell()) return false;
    if (IsOnCooldown()) return false;
    if (!unit) return false;
    const int baseOrder = GetBaseOrderId();
    const bool matchesOrder = SpellMatchesOrder(code_, orderId) ||
                              SpellMatchesOrder(alias_, orderId) ||
                              (orderId != 0 && orderId == baseOrder);
    if (!matchesOrder) return false;
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
    const float backswing = (caster && caster->GetUnitType()) ? std::max(0.0f, caster->GetUnitType()->GetCastBackswingPoint()) : 0.5f;
    return CBehavior::CreateSpellCast(caster, this, target, false, 0.0f, 0.0f, castRange_, castPoint_, backswing);
}

CBehavior* CAbilitySpellBase::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)game; (void)orderId;
    const float backswing = (caster && caster->GetUnitType()) ? std::max(0.0f, caster->GetUnitType()->GetCastBackswingPoint()) : 0.5f;
    return CBehavior::CreateSpellCast(caster, this, nullptr, true, x, y, castRange_, castPoint_, backswing);
}

CBehavior* CAbilitySpellBase::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)orderId;
    const float backswing = (caster && caster->GetUnitType()) ? std::max(0.0f, caster->GetUnitType()->GetCastBackswingPoint()) : 0.5f;
    return CBehavior::CreateSpellCast(caster, this, nullptr, false, 0.0f, 0.0f, 0.0f, castPoint_, backswing);
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
// Phase 17–18 Declarative Spell Effect Primitives
// ============================================================

void CAbilitySpellBase::CastBuffOnTarget(CSimulation& game, CUnit* caster, CWidget* target,
                                         ABActionAddBuffTarget::BuffFactory factory) {
    ABActionAddBuffTarget action(std::move(factory));
    action.Run(game, caster, target, false, 0.0f, 0.0f);
}

void CAbilitySpellBase::CastBuffOnSelf(CSimulation& game, CUnit* caster,
                                       ABActionAddBuffTarget::BuffFactory factory) {
    if (!caster) return;
    ABActionAddBuffTarget action(std::move(factory));
    action.Run(game, caster, caster, false, 0.0f, 0.0f);
}

void CAbilitySpellBase::CastPointDamageWaves(CSimulation& game, CUnit* caster, float x, float y,
                                             int waves, float radius, float dmgPerWave,
                                             CAttackType atkType, CDamageType dmgType,
                                             ABFactionFilter filter) {
    if (!caster) return;
    for (int w = 0; w < waves; ++w) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(dmgPerWave, atkType, dmgType));
        ABActionForEachInRange wave(radius, std::move(perUnit));
        wave.SetFactionFilter(filter);
        wave.Run(game, caster, nullptr, true, x, y);
    }
}

void CAbilitySpellBase::CastCasterAreaDamageWaves(CSimulation& game, CUnit* caster,
                                                  int waves, float radius, float dmgPerWave,
                                                  CAttackType atkType, CDamageType dmgType,
                                                  ABFactionFilter filter) {
    if (!caster) return;
    for (int w = 0; w < waves; ++w) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(dmgPerWave, atkType, dmgType));
        ABActionForEachInRange wave(radius, std::move(perUnit));
        wave.SetFactionFilter(filter);
        wave.Run(game, caster, nullptr, false, 0.0f, 0.0f);
    }
}

void CAbilitySpellBase::CastPointAreaBuff(CSimulation& game, CUnit* caster, float x, float y,
                                          float radius, ABActionAddBuffTarget::BuffFactory factory,
                                          ABFactionFilter filter, bool includeCaster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(std::move(factory)));
    ABActionForEachInRange area(radius, std::move(perUnit), includeCaster);
    area.SetFactionFilter(filter);
    area.Run(game, caster, nullptr, true, x, y);
}

void CAbilitySpellBase::CastCasterAreaBuff(CSimulation& game, CUnit* caster,
                                           float radius, ABActionAddBuffTarget::BuffFactory factory,
                                           ABFactionFilter filter, bool includeCaster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(std::move(factory)));
    ABActionForEachInRange area(radius, std::move(perUnit), includeCaster);
    area.SetFactionFilter(filter);
    area.Run(game, caster, nullptr, false, 0.0f, 0.0f);
}

void CAbilitySpellBase::CastLineDamageAndStun(CSimulation& game, CUnit* caster,
                                              CWidget* unitTarget, bool hasPoint, float x, float y,
                                              float range, float width, float damage, float stunDuration,
                                              CAttackType atkType, CDamageType dmgType,
                                              ABFactionFilter filter) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(damage, atkType, dmgType));
    if (stunDuration > 0.0f) {
        perUnit.Add(std::make_unique<ABActionStunTarget>(stunDuration));
    }
    ABActionForEachInLine line(range, width, std::move(perUnit));
    line.SetFactionFilter(filter);
    line.Run(game, caster, unitTarget, hasPoint, x, y);
}

void CAbilitySpellBase::CastTimedSpawnerUnit(CSimulation& game, CUnit* caster, float x, float y,
                                             War3ID unitType, War3ID buffCode, float lifeDuration,
                                             War3ID spawnType, float spawnInterval,
                                             float boomDamage, float boomRadius) {
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    ABActionCreateUnitWithBuff factory(
        unitType, x, y,
        {BF([buffCode, lifeDuration]() {
             return std::make_unique<CBuffTimedLife>(buffCode, lifeDuration);
         }),
         BF([lifeDuration, spawnType, spawnInterval, boomDamage, boomRadius]() {
             return std::make_unique<CBuffSpawner>(
                 lifeDuration, spawnType, spawnInterval, boomDamage, boomRadius);
         })});
    factory.Run(game, caster, nullptr, true, x, y);
}

void CAbilitySpellBase::CastPointAreaHeal(CSimulation& game, CUnit* caster, float x, float y,
                                          float radius, float amount,
                                          ABFactionFilter filter, bool includeCaster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionHealTarget>(amount));
    ABActionForEachInRange heal(radius, std::move(perUnit), includeCaster);
    heal.SetFactionFilter(filter);
    heal.Run(game, caster, nullptr, true, x, y);
}

void CAbilitySpellBase::ForEachPassiveAllyInRange(CSimulation& game, CUnit* caster,
                                                  float cx, float cy, float radius,
                                                  bool includeCaster,
                                                  const std::function<void(CUnit*)>& fn) {
    if (!caster || !fn) return;
    CPlayer* owner = game.GetPlayer(caster->GetPlayerIndex());
    const float radiusSq = radius * radius;
    for (CUnit* other : game.GetUnits()) {
        if (!other || other->IsDead()) continue;
        if (!includeCaster && other == caster) continue;
        const float dx = other->GetX() - cx;
        const float dy = other->GetY() - cy;
        if (dx * dx + dy * dy > radiusSq) continue;
        const bool self = (other == caster);
        CPlayer* otherPlayer = game.GetPlayer(other->GetPlayerIndex());
        const bool ally = owner && otherPlayer &&
                          owner->HasAlliance(*otherPlayer, CAllianceType::PASSIVE);
        if (!self && !ally) continue;
        fn(other);
    }
}

bool CAbilitySpellBase::IsFriendlyToPlayer(CSimulation& game, int playerIndex, const CUnit* other) {
    if (!other) return true;
    if (other->GetPlayerIndex() == playerIndex) return true;
    CPlayer* owner = game.GetPlayer(playerIndex);
    CPlayer* theirs = game.GetPlayer(other->GetPlayerIndex());
    return owner && theirs && owner->HasAlliance(*theirs, CAllianceType::ALLIED);
}

void CAttackBonusDamageEffect::OnAttackLanded(CSimulation& game, CUnit* attacker,
                                              CUnit* victim, float damageDealt) {
    (void)damageDealt;
    if (!victim) return;
    victim->Damage(game, attacker, false, false,
                   CAttackType::SPELLS, damageType_, "", bonusDamage_, 0.0f);
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

int CAbilityDataDrivenSpell::GetBaseOrderId() const {
    const int base = SpellBaseOrderId(code_);
    return base != 0 ? base : static_cast<int>(code_.value);
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

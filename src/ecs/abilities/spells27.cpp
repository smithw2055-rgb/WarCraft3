#include "spells27.h"
#include "data/abilitydata.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include <algorithm>
#include <cmath>

namespace {

float ClampedFactor(float penalty) {
    float factor = 1.0f - penalty;
    if (factor < 0.05f) factor = 0.05f;
    if (factor > 1.0f) factor = 1.0f;
    return factor;
}

} // namespace

CAbilityStatePassive::CAbilityStatePassive(int handleId, War3ID code, CPassiveState state)
    : CAbility(handleId, code, CAbilityCategory::SPELL), state_(state) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityStatePassive::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit || applied_) return;
    applied_ = true;
    switch (state_) {
    case CPassiveState::Ethereal:
        unit->AddBuff(std::make_unique<CBuffEthereal>(1.0e9f, War3ID("Betl")), game);
        break;
    case CPassiveState::MagicImmune:
        unit->AddBuff(std::make_unique<CBuffMagicImmune>(1.0e9f, War3ID("Bmim")), game);
        break;
    case CPassiveState::Resistant:
        unit->AddBuff(std::make_unique<CBuffResistant>(), game);
        break;
    case CPassiveState::Ghost:
        savedCollision_ = unit->GetCollisionSize();
        unit->SetInvisible(true);
        unit->SetCollisionSize(0.0f);
        break;
    }
}

void CAbilityStatePassive::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit || !applied_) return;
    applied_ = false;
    switch (state_) {
    case CPassiveState::Ethereal:
        unit->RemoveBuff(War3ID("Betl"), game);
        break;
    case CPassiveState::MagicImmune:
        unit->RemoveBuff(War3ID("Bmim"), game);
        break;
    case CPassiveState::Resistant:
        unit->RemoveBuff(War3ID("Brsk"), game);
        break;
    case CPassiveState::Ghost:
        unit->SetInvisible(false);
        unit->SetCollisionSize(savedCollision_);
        break;
    }
}

CAbilityItemSpellResist::CAbilityItemSpellResist(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIsr"), CAbilityCategory::ITEM) {
    iconShowing_ = false;
    permanent_ = true;
    SetPassiveSpell(true);
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemSpellResist::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    appliedAttack_ = (int)std::lround(attackBonus_);
    if (appliedAttack_ != 0) unit->AddBonusDamage(appliedAttack_);
    float taken = std::max(0.0f, 1.0f - spellReduction_);
    unit->AddBuff(std::make_unique<CBuffDamageMod>(War3ID("BIsr"), taken, 1.0f, 1.0f, 0.0f), game);
}

void CAbilityItemSpellResist::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (appliedAttack_ != 0) unit->AddBonusDamage(-appliedAttack_);
    appliedAttack_ = 0;
    unit->RemoveBuff(War3ID("BIsr"), game);
}

void CAbilityItemSpellResist::ApplyEffectData(const CAbilityDefFull& def, int level) {
    attackBonus_ = def.DataFloatAt(level, 0, attackBonus_);
    spellReduction_ = def.DataFloatAt(level, 1, spellReduction_);
}

CAbilityItemMixedDefense::CAbilityItemMixedDefense(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIdd"), CAbilityCategory::ITEM) {
    iconShowing_ = false;
    permanent_ = true;
    SetPassiveSpell(true);
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemMixedDefense::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    unit->AddBuff(std::make_unique<CBuffDamageMod>(
        War3ID("BIdd"), spellTaken_, spellTaken_, pierceTaken_, dealtAdd_), game);
}

void CAbilityItemMixedDefense::OnRemove(CSimulation& game, CUnit* unit) {
    if (unit) unit->RemoveBuff(War3ID("BIdd"), game);
}

void CAbilityItemMixedDefense::ApplyEffectData(const CAbilityDefFull& def, int level) {
    pierceTaken_ = def.DataFloatAt(level, 0, pierceTaken_);
    dealtAdd_ = def.DataFloatAt(level, 1, dealtAdd_);
    spellTaken_ = def.DataFloatAt(level, 4, spellTaken_);
}

CAbilityMagicDefense::CAbilityMagicDefense(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Amdf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(0.0f);
    SetManaCost(0);
}

void CAbilityMagicDefense::TurnOff(CSimulation& game, CUnit* unit) {
    if (!unit || !active_) return;
    unit->RemoveBuff(War3ID("Bmdf"), game);
    unit->ClearNonStackingAttackSpeed(War3ID("Bmdf"));
    active_ = false;
    appliedFactor_ = 1.0f;
}

void CAbilityMagicDefense::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (active_) {
        TurnOff(game, caster);
        return;
    }
    caster->AddBuff(std::make_unique<CBuffMagicImmune>(1.0e9f, War3ID("Bmdf")), game);
    appliedFactor_ = ClampedFactor(speedPenalty_);
    caster->SetNonStackingAttackSpeed(War3ID("Bmdf"), appliedFactor_ - 1.0f);
    active_ = true;
}

void CAbilityMagicDefense::OnRemove(CSimulation& game, CUnit* unit) {
    TurnOff(game, unit);
}

void CAbilityMagicDefense::ApplyEffectData(const CAbilityDefFull& def, int level) {
    speedPenalty_ = def.DataFloatAt(level, 3, speedPenalty_);
}

CAbilityMagicLeash::CAbilityMagicLeash(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Amls"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(12.0f);
    SetManaCost(50);
}

void CAbilityMagicLeash::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (!unit || unit->IsDead()) return;
    if (!unit->CheckSpellEffectReaction(game, caster)) return;
    unit->AddBuff(std::make_unique<CBuffStun>(duration_), game);
    unit->AddBuff(std::make_unique<CBuffPoison>(duration_, damagePerSecond_), game);
    unit->Damage(game, caster, false, false, CAttackType::SPELLS, CDamageType::MAGIC,
                 "", damagePerSecond_, 0);
}

void CAbilityMagicLeash::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_);
    duration_ = def.DurationAt(level);
}

CAbilityReplenish::CAbilityReplenish(int handleId, War3ID code, bool restoreLife)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL)
    , restoreLife_(restoreLife) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetManaCost(5);
    SetCooldown(1.0f);
}

void CAbilityReplenish::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster || amount_ <= 0.0f) return;
    ForEachPassiveAllyInRange(game, caster, caster->GetX(), caster->GetY(), radius_, true,
                              [&game, restoreLife = restoreLife_, amount = amount_](CUnit* other) {
                                  if (restoreLife) {
                                      const float missing = other->GetMaxLife() - other->GetLife();
                                      if (missing > 0.0f) {
                                          other->SetLife(game, other->GetLife() + std::min(amount, missing));
                                      }
                                  } else {
                                      const float missing = static_cast<float>(other->GetMaxMana()) - other->GetMana();
                                      if (missing > 0.0f) {
                                          other->SetMana(other->GetMana() + std::min(amount, missing));
                                      }
                                  }
                              });
}

void CAbilityReplenish::ApplyEffectData(const CAbilityDefFull& def, int level) {
    amount_ = def.DataFloatAt(level, 0, amount_);
    radius_ = def.AreaAt(level);
}

CAbilitySphere::CAbilitySphere(int handleId)
    : CAbility(handleId, War3ID("Asph"), CAbilityCategory::SPELL) {
    iconShowing_ = false;
    permanent_ = true;
}

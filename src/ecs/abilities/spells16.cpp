#include "spells16.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/unit_type.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Devour =====
CAbilityDevour::CAbilityDevour(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Adev"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(100.0f);
}

void CAbilityDevour::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead() || target == caster) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || targetUnit->IsHero()) return;
    if (targetUnit->GetPlayerIndex() == caster->GetPlayerIndex()) return;
    if (!targetUnit->GetUnitType()) return;
    // Ground meat only (ref targs); flyers cannot be swallowed.
    if (targetUnit->GetUnitType()->GetMovementType() == PathingGridMovementType::FLY) return;
    if (!caster->LoadUnit(targetUnit)) return;
    targetUnit->AddBuff(std::make_unique<CBuffDigest>(
        digestDuration_, digestPerTick_), game);
}

void CAbilityDevour::ApplyEffectData(const CAbilityDefFull& def, int level) {
    digestPerTick_ = def.DataFloatAt(level, 0, digestPerTick_);
}

// ===== Devour Magic =====
CAbilityDevourMagic::CAbilityDevourMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Advm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityDevourMagic::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    const size_t before = targetUnit->GetBuffs().size();
    targetUnit->RemoveBuffsIf(game, true, true, false);
    const size_t eaten = before - targetUnit->GetBuffs().size();
    if (eaten > 0 && caster && !caster->IsDead()) {
        float mana = caster->GetMana() + manaPerBuff_ * (float)eaten;
        if (mana > (float)caster->GetMaxMana()) mana = (float)caster->GetMaxMana();
        caster->SetMana(mana);
    }
}

// ===== Cold Arrows =====
CAbilityColdArrows::CAbilityColdArrows(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHca"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityColdArrows::ApplyEffectData(const CAbilityDefFull& def, int level) {
    poisonPerTick_ = def.DataFloatAt(level, 0, poisonPerTick_);
    slowPercent_ = def.DataFloatAt(level, 1, slowPercent_);
    slowDuration_ = def.DurationAt(level);
    poisonDuration_ = def.DataFloatAt(level, 3, poisonDuration_);
}

// ===== Monsoon =====
CAbilityMonsoon::CAbilityMonsoon(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANmo"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityMonsoon::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
            const float dx = unit->GetX() - x;
            const float dy = unit->GetY() - y;
            if (dx * dx + dy * dy <= radius_ * radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::COLD,
                             "", damagePerWave_, 0);
            }
        }
    }
}

void CAbilityMonsoon::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerWave_ = def.DataFloatAt(level, 0, damagePerWave_);
    radius_ = def.AreaAt(level);
}

// ===== Absorb Mana =====
CAbilityAbsorbMana::CAbilityAbsorbMana(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aabs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
}

void CAbilityAbsorbMana::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || !targetUnit->IsSummoned()) return;
    targetUnit->Damage(game, caster, false, false,
                       CAttackType::SPELLS, CDamageType::MAGIC,
                       "", 1e9f, 0);
    float mana = caster->GetMana() + manaGain_;
    if (mana > (float)caster->GetMaxMana()) mana = (float)caster->GetMaxMana();
    caster->SetMana(mana);
}

// ===== Unholy Frenzy =====
CAbilityUnholyFrenzy::CAbilityUnholyFrenzy(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Auhf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityUnholyFrenzy::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffBerserk>(
        duration_, attackSpeedMul_, 1.0f), game);
}

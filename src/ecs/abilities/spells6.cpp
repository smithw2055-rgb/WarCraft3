#include "spells6.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Healing Spray =====
CAbilityHealingSpray::CAbilityHealingSpray(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANhs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
    SetCooldown(9.0f);
    SetManaCost(75);
}

void CAbilityHealingSpray::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - x;
        const float dy = unit->GetY() - y;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            float life = unit->GetLife() + healAmount_;
            if (life > unit->GetMaxLife()) life = unit->GetMaxLife();
            unit->SetLife(game, life);
        }
    }
}

// ===== Acid Bomb =====
CAbilityAcidBomb::CAbilityAcidBomb(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANab"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityAcidBomb::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffArmor>(
        War3ID("Bacb"), duration_, armorPenalty_, false), game);
    targetUnit->AddBuff(std::make_unique<CBuffPoison>(
        duration_, poisonPerTick_, caster ? caster->GetHandleId() : -1), game);
}

// ===== Chemical Rage =====
CAbilityChemicalRage::CAbilityChemicalRage(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANcr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(20.0f);
    SetManaCost(50);
}

void CAbilityChemicalRage::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffBloodlust>(
        duration_, speedBonus_, attackSpeedBonus_), game);
}

// ===== Transmute =====
CAbilityTransmute::CAbilityTransmute(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANtm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(25);
}

void CAbilityTransmute::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || targetUnit->IsHero()) return;
    if (targetUnit->GetPlayerIndex() == caster->GetPlayerIndex()) return;
    targetUnit->Damage(game, caster, false, false,
                       CAttackType::SPELLS, CDamageType::NORMAL,
                       "", 1e9f, 0);
    if (CPlayer* owner = game.GetPlayer(caster->GetPlayerIndex())) {
        owner->AddGold(goldYield_);
    }
}

// ===== Bloodlust =====
CAbilityBloodlustCast::CAbilityBloodlustCast(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ablo"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityBloodlustCast::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    // No haste for the mechanical (ref unit-type immunity).
    if (targetUnit->GetUnitType()) {
        const auto& classes = targetUnit->GetUnitType()->GetClassifications();
        if (classes.find(CUnitClassification::MECHANICAL) != classes.end()) return;
    }
    targetUnit->AddBuff(std::make_unique<CBuffBloodlust>(
        duration_, speedBonus_, attackSpeedBonus_), game);
}

// ===== Purge =====
CAbilityPurge::CAbilityPurge(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aprg"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityPurge::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    // Summoned things burn first (checked pre-dispel: dispel strips the
    // timed-life marker that IsSummoned reads).
    if (targetUnit->IsSummoned()) {
        targetUnit->Damage(game, caster, false, false,
                           CAttackType::SPELLS, CDamageType::MAGIC,
                           "", summonDamage_, 0);
        if (targetUnit->IsDead()) return;
    }
    targetUnit->RemoveBuffsIf(game, true, true, false);
    targetUnit->AddBuff(std::make_unique<CBuffSlow>(
        slowDuration_, slowPercent_, caster ? caster->GetHandleId() : -1), game);
}

// ===== Lightning Shield =====
CAbilityLightningShield::CAbilityLightningShield(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Alsh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityLightningShield::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffLightningShield>(
        duration_, damagePerTick_, radius_), game);
}

// ===== Raise Dead =====
CAbilityRaiseDead::CAbilityRaiseDead(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Arai"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityRaiseDead::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    int raised = 0;
    for (auto* unit : game.GetUnits()) {
        if (raised >= maxTargets_) break;
        auto* corpse = dynamic_cast<CUnit*>(unit);
        if (!corpse || !corpse->IsDead()) continue;
        const float dx = corpse->GetX() - caster->GetX();
        const float dy = corpse->GetY() - caster->GetY();
        if (dx * dx + dy * dy > radius_ * radius_) continue;
        // Retail skeletons stand until killed (no timed life). Graceful
        // without unit data (ref summon-without-data): no corpse, no crash.
        CUnit* skeleton = game.CreateUnit(skeletonType_, caster->GetPlayerIndex(),
                                          corpse->GetX(), corpse->GetY(), 0);
        if (skeleton) {
            raised++;
            if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
                summonedCb(caster->GetHandleId(), skeleton->GetHandleId());
            }
        }
    }
}

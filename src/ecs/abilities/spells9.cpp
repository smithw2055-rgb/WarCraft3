#include "spells9.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_builder.h"
#include <cmath>

// ===== Orb of Frost =====
CAbilityOrbOfFrost::CAbilityOrbOfFrost(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIob"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Orb of Corruption =====
CAbilityOrbOfCorruption::CAbilityOrbOfCorruption(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIcb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Orb of Fire =====
CAbilityOrbOfFire::CAbilityOrbOfFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIfb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Wand of Illusion =====
CAbilityIllusion::CAbilityIllusion(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIil"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityIllusion::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    SummonTimedUnit(game, caster, targetUnit->GetTypeId(), War3ID("AIil"),
                    targetUnit->GetX() + 64.0f, targetUnit->GetY(),
                    targetUnit->GetFacing(), duration_);
}

// ===== Wand of Lightning Shield =====
CAbilityLightningShieldWand::CAbilityLightningShieldWand(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIls"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityLightningShieldWand::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffLightningShield>(
        duration_, damagePerTick_, radius_), game);
}

// ===== Scroll of the Beast =====
CAbilityBeastScroll::CAbilityBeastScroll(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIrr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityBeastScroll::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(std::make_unique<CBuffInnerFire>(
                duration_, bonusDamage_, 0), game);
        }
    }
}

// ===== Healing Wards =====
CAbilityHealingWard::CAbilityHealingWard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIhw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
}

void CAbilityHealingWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Graceful without unit data (ref summon-without-data).
    CUnit* ward = game.CreateUnit(wardType_, caster->GetPlayerIndex(), x, y, 0);
    if (!ward) return;
    ward->AddBuff(std::make_unique<CBuffHealingWard>(
        auraDuration_, healPerTick_, radius_), game);
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(caster->GetHandleId(), ward->GetHandleId());
    }
}

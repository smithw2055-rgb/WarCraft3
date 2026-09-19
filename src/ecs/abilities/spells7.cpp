#include "spells7.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Serpent Ward =====
CAbilitySerpentWard::CAbilitySerpentWard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOsw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilitySerpentWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Graceful without unit data (ref summon-without-data).
    CUnit* ward = game.CreateUnit(wardType_, caster->GetPlayerIndex(), x, y, 0);
    if (ward) {
        if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
            summonedCb(caster->GetHandleId(), ward->GetHandleId());
        }
    }
}

// ===== Polymorph =====
CAbilityPolymorph::CAbilityPolymorph(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aply"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityPolymorph::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffPolymorph>(duration_), game);
}

// ===== Inner Fire =====
CAbilityInnerFire::CAbilityInnerFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ainf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(35);
}

void CAbilityInnerFire::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffInnerFire>(
        duration_, bonusDamage_, bonusArmor_), game);
}

// ===== Heal =====
CAbilityHeal::CAbilityHeal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ahea"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(5.0f);
    SetManaCost(65);
}

void CAbilityHeal::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    float life = targetUnit->GetLife() + healAmount_;
    if (life > targetUnit->GetMaxLife()) life = targetUnit->GetMaxLife();
    targetUnit->SetLife(game, life);
}

// ===== Dispel Magic =====
CAbilityDispelMagic::CAbilityDispelMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Adis"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityDispelMagic::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        const float dx = unit->GetX() - x;
        const float dy = unit->GetY() - y;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->RemoveBuffsIf(game, true, true, false);
        }
    }
}

// ===== Rejuvenation =====
CAbilityRejuvenation::CAbilityRejuvenation(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Arej"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityRejuvenation::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffRejuvenation>(
        duration_, healPerTick_), game);
}

// ===== Roar =====
CAbilityRoar::CAbilityRoar(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aroa"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityRoar::DoEffect(CSimulation& game, CUnit* caster) {
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

// ===== Defend =====
CAbilityDefend::CAbilityDefend(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Adef"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(5.0f);
    SetManaCost(0);
}

void CAbilityDefend::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffDefend>(duration_), game);
}

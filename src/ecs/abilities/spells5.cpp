#include "spells5.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Flame Strike =====
CAbilityFlameStrike::CAbilityFlameStrike(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHfs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
    SetCooldown(9.0f);
    SetManaCost(75);
}

void CAbilityFlameStrike::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            const float dx = unit->GetX() - x;
            const float dy = unit->GetY() - y;
            if (dx * dx + dy * dy <= radius_ * radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::FIRE,
                             "", damagePerWave_, 0);
            }
        }
    }
}

// ===== Death and Decay =====
CAbilityDeathAndDecay::CAbilityDeathAndDecay(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUdd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
    SetCooldown(60.0f);
    SetManaCost(125);
}

void CAbilityDeathAndDecay::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            const float dx = unit->GetX() - x;
            const float dy = unit->GetY() - y;
            if (dx * dx + dy * dy <= radius_ * radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::DEATH,
                             "", damagePerWave_, 0);
            }
        }
    }
}

// ===== Impale =====
CAbilityImpale::CAbilityImpale(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUim"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(700.0f);
    SetCooldown(9.0f);
    SetManaCost(75);
}

void CAbilityImpale::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    const float dx = target->GetX() - caster->GetX();
    const float dy = target->GetY() - caster->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return;
    const float nx = dx / dist, ny = dy / dist;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        const float ux = unit->GetX() - caster->GetX();
        const float uy = unit->GetY() - caster->GetY();
        const float proj = ux * nx + uy * ny;
        if (proj < 0 || proj > range_) continue;
        const float perp = std::abs(ux * ny - uy * nx);
        if (perp <= width_ / 2) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::NORMAL,
                         "", damage_, 0);
            unit->AddBuff(std::make_unique<CBuffStun>(stunDuration_), game);
        }
    }
}

// ===== Fan of Knives =====
CAbilityFanOfKnives::CAbilityFanOfKnives(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEfk"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityFanOfKnives::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::NORMAL,
                         "", damage_, 0);
        }
    }
}

// ===== Rain of Fire =====
CAbilityRainOfFire::CAbilityRainOfFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANrf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityRainOfFire::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            const float dx = unit->GetX() - x;
            const float dy = unit->GetY() - y;
            if (dx * dx + dy * dy <= radius_ * radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::FIRE,
                             "", damagePerWave_, 0);
            }
        }
    }
}

// ===== Spiked Carapace =====
CAbilitySpikedCarapace::CAbilitySpikedCarapace(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUts"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilitySpikedCarapace::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->AddBonusArmor(armorBonus_);
}

void CAbilitySpikedCarapace::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->AddBonusArmor(-armorBonus_);
}

// ===== Dark Ritual =====
CAbilityDarkRitual::CAbilityDarkRitual(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUdr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(25);
}

void CAbilityDarkRitual::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead() || target == caster) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || targetUnit->IsHero()) return;
    if (targetUnit->GetPlayerIndex() != caster->GetPlayerIndex()) return;
    // The victim's life force becomes mana, capped by the caster's pool.
    float mana = caster->GetMana() + targetUnit->GetLife();
    if (mana > static_cast<float>(caster->GetMaxMana())) {
        mana = static_cast<float>(caster->GetMaxMana());
    }
    targetUnit->Damage(game, caster, false, false,
                       CAttackType::SPELLS, CDamageType::DEATH,
                       "", 1e9f, 0);
    caster->SetMana(mana);
}

// ===== Shadow Strike =====
CAbilityShadowStrike::CAbilityShadowStrike(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEsh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(65);
}

void CAbilityShadowStrike::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    target->Damage(game, caster, false, false,
                   CAttackType::SPELLS, CDamageType::POISON,
                   "", damage_, 0);
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffSlow>(
        slowDuration_, slowPercent_, caster ? caster->GetHandleId() : -1), game);
    targetUnit->AddBuff(std::make_unique<CBuffPoison>(
        poisonDuration_, poisonPerTick_, caster ? caster->GetHandleId() : -1), game);
}

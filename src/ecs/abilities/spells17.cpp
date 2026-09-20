#include "spells17.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Howl of Terror =====
CAbilityHowlOfTerror::CAbilityHowlOfTerror(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANht"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityHowlOfTerror::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(std::make_unique<CBuffHowl>(
                duration_, damageFactor_), game);
        }
    }
}

// ===== Breath of Fire =====
CAbilityBreathOfFire::CAbilityBreathOfFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANbf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(375.0f);
    SetCooldown(10.0f);
    SetManaCost(70);
}

void CAbilityBreathOfFire::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    const float tx = target->GetX(), ty = target->GetY();
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - tx;
        const float dy = unit->GetY() - ty;
        const bool isPrimary = (unit == target);
        if (!isPrimary && dx * dx + dy * dy > radius_ * radius_) continue;
        unit->Damage(game, caster, false, false,
                     CAttackType::SPELLS, CDamageType::FIRE,
                     "", damage_, 0);
        if (auto* burning = dynamic_cast<CUnit*>(unit)) {
            burning->AddBuff(std::make_unique<CBuffPoison>(
                burnDuration_, burnPerTick_, caster->GetHandleId()), game);
        }
    }
}

// ===== Drunken Haze =====
CAbilityDrunkenHaze::CAbilityDrunkenHaze(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANdh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(550.0f);
    SetCooldown(12.0f);
    SetManaCost(70);
}

void CAbilityDrunkenHaze::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - x;
        const float dy = unit->GetY() - y;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(std::make_unique<CBuffHaze>(
                duration_, missChance_), game);
            unit->AddBuff(std::make_unique<CBuffSlow>(
                duration_, slowPercent_, caster->GetHandleId()), game);
        }
    }
}

// ===== Mana Shield =====
CAbilityManaShield::CAbilityManaShield(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANms"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(25);
}

void CAbilityManaShield::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffManaShield>(duration_), game);
}

// ===== Locust Swarm =====
CAbilityLocustSwarm::CAbilityLocustSwarm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUls"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(150);
}

void CAbilityLocustSwarm::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
            const float dx = unit->GetX() - caster->GetX();
            const float dy = unit->GetY() - caster->GetY();
            if (dx * dx + dy * dy <= radius_ * radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::POISON,
                             "", damagePerWave_, 0);
            }
        }
    }
}

// ===== Cyclone =====
CAbilityCyclone::CAbilityCyclone(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Acyc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(5.0f);
    SetManaCost(150);
}

void CAbilityCyclone::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffCyclone>(duration_), game);
}

// ===== Vengeance =====
CAbilityVengeance::CAbilityVengeance(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Avng"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(2.0f);
    SetManaCost(25);
}

void CAbilityVengeance::OnDeath(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    // The fallen rises again as an avenging spirit for its side.
    CUnit* spirit = game.CreateUnit(spiritType_, unit->GetPlayerIndex(),
                                    unit->GetX(), unit->GetY(), 0);
    if (!spirit) return;
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(unit->GetHandleId(), spirit->GetHandleId());
    }
}

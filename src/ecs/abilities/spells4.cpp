#include "spells4.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Evasion =====
CAbilityEvasion::CAbilityEvasion(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEev"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityEvasion::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetEvasionChance(chance_);
}

void CAbilityEvasion::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetEvasionChance(0.0f);
}

// ===== Critical Strike =====
CAbilityCriticalStrike::CAbilityCriticalStrike(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOcr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityCriticalStrike::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetCriticalStrike(chance_, multiplier_);
}

void CAbilityCriticalStrike::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetCriticalStrike(0.0f, 1.0f);
}

// ===== Bash =====
CAbilityBash::CAbilityBash(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHbh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Slow =====
CAbilitySlow::CAbilitySlow(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aslo"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilitySlow::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffSlow>(
        duration_, slowPercent_, caster ? caster->GetHandleId() : -1), game);
}

// ===== Frost Armor =====
CAbilityFrostArmor::CAbilityFrostArmor(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUfa"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(5.0f);
    SetManaCost(40);
}

void CAbilityFrostArmor::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffArmor>(
        War3ID("BUfa"), duration_, armorBonus_, true), game);
}

// ===== Faerie Fire =====
CAbilityFaerieFire::CAbilityFaerieFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Afae"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(4.0f);
    SetManaCost(30);
}

void CAbilityFaerieFire::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffArmor>(
        War3ID("Bfae"), duration_, armorPenalty_, false), game);
}

// ===== Death Pact =====
CAbilityDeathPact::CAbilityDeathPact(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUdp"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(25);
}

void CAbilityDeathPact::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead() || target == caster) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || targetUnit->IsHero()) return;
    if (targetUnit->GetPlayerIndex() != caster->GetPlayerIndex()) return;
    const float heal = targetUnit->GetMaxLife();
    targetUnit->Damage(game, caster, false, false,
                       CAttackType::SPELLS, CDamageType::DEATH,
                       "", 1e9f, 0);
    float newLife = caster->GetLife() + heal;
    if (newLife > caster->GetMaxLife()) newLife = caster->GetMaxLife();
    caster->SetLife(game, newLife);
}

// ===== Animate Dead =====
CAbilityAnimateDead::CAbilityAnimateDead(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUan"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(30.0f);
    SetManaCost(75);
}

void CAbilityAnimateDead::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        auto* target = dynamic_cast<CUnit*>(unit);
        if (!target || !target->IsDead()) continue;
        const float dx = target->GetX() - x;
        const float dy = target->GetY() - y;
        if (dx * dx + dy * dy > radius_ * radius_) continue;
        target->Revive(game, target->GetX(), target->GetY());
        target->SetInvulnerable(true);
        if (target->GetPlayerIndex() != caster->GetPlayerIndex()) {
            if (CPlayer* oldOwner = game.GetPlayer(target->GetPlayerIndex())) {
                oldOwner->RemoveUnit(target);
            }
            target->SetPlayerIndex(caster->GetPlayerIndex());
            if (CPlayer* newOwner = game.GetPlayer(caster->GetPlayerIndex())) {
                newOwner->AddUnit(target);
            }
        }
        target->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("AUan"), duration_), game);
        if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
            summonedCb(caster->GetHandleId(), target->GetHandleId());
        }
    }
}

// ===== Doom =====
CAbilityDoom::CAbilityDoom(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANdo"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(650.0f);
    SetCooldown(120.0f);
    SetManaCost(150);
}

void CAbilityDoom::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffDoom>(
        duration_, damagePerSecond_, caster ? caster->GetHandleId() : -1), game);
}

// ===== Volcano =====
CAbilityVolcano::CAbilityVolcano(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANvc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(25.0f);
    SetManaCost(100);
}

void CAbilityVolcano::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        // Deterministic synced scatter (std::rand is banned in sim code).
        const float wx = x + game.SimRandRange(-radius_, radius_);
        const float wy = y + game.SimRandRange(-radius_, radius_);
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            const float dx = unit->GetX() - wx;
            const float dy = unit->GetY() - wy;
            if (dx * dx + dy * dy <= radius_ * radius_ / 4) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::FIRE,
                             "", damage_, 0);
            }
        }
    }
}

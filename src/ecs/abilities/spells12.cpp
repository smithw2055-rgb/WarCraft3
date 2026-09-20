#include "spells12.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/unit_type.h"
#include <cmath>

// ===== True Sight =====
CAbilityTrueSight::CAbilityTrueSight(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Atru"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityTrueSight::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetDetector(true);
}

void CAbilityTrueSight::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetDetector(false);
}

// ===== Sentry Ward =====
CAbilitySentryWard::CAbilitySentryWard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aeye"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilitySentryWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Graceful without unit data (ref summon-without-data).
    CUnit* ward = game.CreateUnit(wardType_, caster->GetPlayerIndex(), x, y, 0);
    if (!ward) return;
    auto detect = std::make_unique<CAbilityTrueSight>(ward->GetHandleId());
    detect->SetAlias(War3ID("Adet"));
    ward->AddAbility(game, std::move(detect));
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(caster->GetHandleId(), ward->GetHandleId());
    }
}

// ===== Flare =====
CAbilityFlare::CAbilityFlare(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Afla"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityFlare::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - x;
        const float dy = unit->GetY() - y;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(std::make_unique<CBuffArmor>(
                War3ID("Bfae"), duration_, 0, false), game);
        }
    }
}

// ===== Burrow =====
CAbilityBurrow::CAbilityBurrow(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Abur"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(5.0f);
}

void CAbilityBurrow::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffBurrow>(duration_), game);
}

// ===== Cannibalize =====
CAbilityCannibalize::CAbilityCannibalize(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Acan"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(300.0f);
    SetCooldown(5.0f);
}

void CAbilityCannibalize::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || !target->IsDead()) return;
    auto* corpse = dynamic_cast<CUnit*>(target);
    if (!corpse || corpse == caster) return;
    float life = caster->GetLife() + healAmount_;
    if (life > caster->GetMaxLife()) life = caster->GetMaxLife();
    caster->SetLife(game, life);
    game.RemoveUnit(corpse); // the corpse is consumed
}

// ===== Carrion Beetles =====
CAbilityCarrionBeetles::CAbilityCarrionBeetles(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUcb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityCarrionBeetles::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || !target->IsDead()) return;
    auto* corpse = dynamic_cast<CUnit*>(target);
    if (!corpse) return;
    // Retail beetles stand until killed (no timed life). Graceful
    // without unit data (ref summon-without-data).
    for (int i = 0; i < count_; i++) {
        CUnit* beetle = game.CreateUnit(beetleType_, caster->GetPlayerIndex(),
                                        corpse->GetX() + i * 64.0f, corpse->GetY(), 0);
        if (beetle) {
            if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
                summonedCb(caster->GetHandleId(), beetle->GetHandleId());
            }
        }
    }
}

// ===== Web =====
CAbilityWeb::CAbilityWeb(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aweb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityWeb::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || !targetUnit->GetUnitType()) return;
    // Web only grounds flyers (ref).
    if (targetUnit->GetUnitType()->GetMovementType() != PathingGridMovementType::FLY) return;
    targetUnit->AddBuff(std::make_unique<CBuffEnsnare>(duration_), game);
}

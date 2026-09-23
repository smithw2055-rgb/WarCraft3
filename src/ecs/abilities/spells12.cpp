#include "spells12.h"
#include "ecs/abilities/ability_builder.h"
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
    // Migrated onto a single AbilityBuilder create-and-grant (born with
    // True Sight; graceful without unit data like the code replaced).
    if (!caster) return;
    ABActionCreateUnitWithAbility ward(
        wardType_, x, y,
        [](CSimulation&, CUnit* newborn) -> std::unique_ptr<CAbility> {
            auto detect = std::make_unique<CAbilityTrueSight>(newborn->GetHandleId());
            detect->SetAlias(War3ID("Adet"));
            return detect;
        });
    ward.Run(game, caster, nullptr, true, x, y);
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
    // Migrated onto an AbilityBuilder iteration: enemies in radius gain the
    // Faerie Fire marker (same pattern as Dust of Appearance).
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() {
            return std::make_unique<CBuffArmor>(War3ID("Bfae"), duration, 0, false);
        }));
    ABActionForEachInRange flare(radius_, std::move(perUnit));
    flare.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    flare.Run(game, caster, nullptr, true, x, y);
}

// ===== Burrow =====
CAbilityBurrow::CAbilityBurrow(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Abur"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(5.0f);
}

void CAbilityBurrow::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget dig(
        [duration = duration_]() { return std::make_unique<CBuffBurrow>(duration); });
    dig.Run(game, caster, caster, false, 0, 0);
}

// ===== Cannibalize =====
CAbilityCannibalize::CAbilityCannibalize(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Acan"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(300.0f);
    SetCooldown(5.0f);
}

void CAbilityCannibalize::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder branch: a corpse (never the caster)
    // is consumed and its hunger heals the caster, clamped to max.
    if (!target || !caster || !target->IsDead()) return;
    auto* corpse = dynamic_cast<CUnit*>(target);
    if (!corpse || corpse == caster) return;
    const float heal = healAmount_;
    ABEffectChain feast;
    feast.Add(std::make_unique<ABActionHealCaster>(heal));
    feast.Add(std::make_unique<ABActionRemoveTarget>());
    ABActionIf cannibal(
        std::make_shared<ABCondFunc>(
            [caster](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && unit->IsDead() && unit != caster;
            }),
        std::move(feast));
    cannibal.Run(game, caster, target, false, 0, 0);
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
    // Migrated onto an AbilityBuilder branch: corpses rise beetles spaced
    // along +X (no timed life; graceful without unit data).
    if (!target || !caster || !target->IsDead()) return;
    ABEffectChain rise;
    rise.Add(std::make_unique<ABActionCreateUnitAtTarget>(beetleType_, count_, 64.0f));
    ABActionIf beetles(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* corpse = dynamic_cast<CUnit*>(unitTarget);
                return corpse != nullptr && corpse->IsDead();
            }),
        std::move(rise));
    beetles.Run(game, caster, target, false, 0, 0);
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
    // Migrated onto an AbilityBuilder branch: only flyers are snared
    // (ref). Same guard as the code replaced.
    ABEffectChain snare;
    snare.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() { return std::make_unique<CBuffEnsnare>(duration); }));
    ABActionIf web(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* target, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(target);
                if (!unit || unit->IsDead() || !unit->GetUnitType()) return false;
                return unit->GetUnitType()->GetMovementType() == PathingGridMovementType::FLY;
            }),
        std::move(snare));
    web.Run(game, caster, target, false, 0, 0);
}

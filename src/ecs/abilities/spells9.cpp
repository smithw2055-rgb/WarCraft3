#include "spells9.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
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
    // Migrated onto a single AbilityBuilder copy summon.
    ABActionSummonCopyOfTarget illusion(War3ID("AIil"), duration_);
    illusion.Run(game, caster, target, false, 0, 0);
}

// ===== Wand of Lightning Shield =====
CAbilityLightningShieldWand::CAbilityLightningShieldWand(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIls"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityLightningShieldWand::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget shield(
        [duration = duration_, tick = damagePerTick_, radius = radius_]() {
            return std::make_unique<CBuffLightningShield>(duration, tick, radius);
        });
    shield.Run(game, caster, target, false, 0, 0);
}

// ===== Scroll of the Beast =====
CAbilityBeastScroll::CAbilityBeastScroll(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIrr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityBeastScroll::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder iteration: same-player units around
    // the caster gain bonus damage (Roar pattern, armor-less Inner Fire).
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, damage = bonusDamage_]() {
            return std::make_unique<CBuffInnerFire>(duration, damage, 0);
        }));
    ABActionForEachInRange rally(radius_, std::move(perUnit));
    rally.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    rally.Run(game, caster, nullptr, false, 0, 0);
}

// ===== Healing Wards =====
CAbilityHealingWard::CAbilityHealingWard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIhw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
}

void CAbilityHealingWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto a single AbilityBuilder create-and-buff (the ward is
    // born already radiating; graceful without unit data).
    if (!caster) return;
    ABActionCreateUnitWithBuff ward(
        wardType_, x, y,
        {ABActionCreateUnitWithBuff::BuffFactory(
            [duration = auraDuration_, tick = healPerTick_, radius = radius_]() {
                return std::make_unique<CBuffHealingWard>(duration, tick, radius);
            })});
    ward.Run(game, caster, nullptr, true, x, y);
}

#include "spells6.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
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
    // Migrated onto an AbilityBuilder iteration: same-player units
    // (caster included) around the point are healed.
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionHealTarget>(healAmount_));
    ABActionForEachInRange spray(radius_, std::move(perUnit), true);
    spray.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    spray.Run(game, caster, nullptr, true, x, y);
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
    // Migrated onto an AbilityBuilder chain: armor curse plus poison.
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, penalty = armorPenalty_]() {
            return std::make_unique<CBuffArmor>(War3ID("Bacb"), duration, penalty, false);
        }));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, tick = poisonPerTick_, casterHandle]() {
            return std::make_unique<CBuffPoison>(duration, tick, casterHandle);
        }));
    chain.Run(game, caster, target, false, 0, 0);
}

// ===== Chemical Rage =====
CAbilityChemicalRage::CAbilityChemicalRage(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANcr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(20.0f);
    SetManaCost(50);
}

void CAbilityChemicalRage::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder buff attach on the caster itself.
    if (!caster) return;
    ABActionAddBuffTarget rage(
        [duration = duration_, speed = speedBonus_, attack = attackSpeedBonus_]() {
            return std::make_unique<CBuffBloodlust>(duration, speed, attack);
        });
    rage.Run(game, caster, caster, false, 0, 0);
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
    // Migrated onto an AbilityBuilder branch: an enemy non-hero is slain
    // and its gold goes to the caster's player.
    if (!target || !caster || target->IsDead()) return;
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return;
    ABEffectChain kill;
    kill.Add(std::make_unique<ABActionDamageTarget>(
        1e9f, CAttackType::SPELLS, CDamageType::NORMAL));
    kill.Add(std::make_unique<ABActionAddGoldToCaster>(goldYield_));
    ABActionIf transmute(
        std::make_shared<ABCondFunc>(
            [caster](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && !unit->IsHero() &&
                       unit->GetPlayerIndex() != caster->GetPlayerIndex();
            }),
        std::move(kill));
    transmute.Run(game, caster, target, false, 0, 0);
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
    // Migrated onto an AbilityBuilder branch: no haste for the mechanical
    // (ref unit-type immunity).
    ABEffectChain haste;
    haste.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, speed = speedBonus_, attack = attackSpeedBonus_]() {
            return std::make_unique<CBuffBloodlust>(duration, speed, attack);
        }));
    ABActionIf blood(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* target, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(target);
                if (!unit || unit->IsDead()) return false;
                auto* type = unit->GetUnitType();
                return type == nullptr ||
                       type->GetClassifications().count(CUnitClassification::MECHANICAL) == 0;
            }),
        std::move(haste));
    blood.Run(game, caster, target, false, 0, 0);
}

void CAbilityBloodlustCast::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
    speedBonus_ = def.DataFloatAt(level, 0, speedBonus_);
    attackSpeedBonus_ = def.DataFloatAt(level, 1, attackSpeedBonus_);
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
    // Migrated onto an AbilityBuilder chain: summoned victims burn first
    // (checked pre-dispel), then buffs are stripped and slow applied. A
    // burn kill leaves a corpse the later steps skip, matching the legacy
    // early-out.
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain burn;
    burn.Add(std::make_unique<ABActionDamageTarget>(summonDamage_, CAttackType::SPELLS,
                                                    CDamageType::MAGIC));
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionIf>(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* target, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(target);
                return unit != nullptr && unit->IsSummoned();
            }),
        std::move(burn)));
    chain.Add(std::make_unique<ABActionDispelTarget>(true, true));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [slowDuration = slowDuration_, slowPercent = slowPercent_, casterHandle]() {
            return std::make_unique<CBuffSlow>(slowDuration, slowPercent, casterHandle);
        }));
    chain.Run(game, caster, target, false, 0, 0);
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget shield(
        [duration = duration_, tick = damagePerTick_, radius = radius_]() {
            return std::make_unique<CBuffLightningShield>(duration, tick, radius);
        });
    shield.Run(game, caster, target, false, 0, 0);
}

// ===== Raise Dead =====
CAbilityRaiseDead::CAbilityRaiseDead(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Arai"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityRaiseDead::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder iteration: corpses around the caster
    // rise as skeletons (no timed life), capped at maxTargets. Graceful
    // without unit data, like the loop replaced.
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionCreateUnitAtTarget>(skeletonType_));
    ABActionForEachInRange raising(radius_, std::move(perUnit), false, maxTargets_);
    raising.SetLifeFilter(ABLifeFilter::DEAD);
    raising.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilityRaiseDead::ApplyEffectData(const CAbilityDefFull& def, int level) {
    maxTargets_ = def.DataIntAt(level, 0, maxTargets_);
}

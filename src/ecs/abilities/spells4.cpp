#include "spells4.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
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

void CAbilityEvasion::ApplyEffectData(const CAbilityDefFull& def, int level) {
    chance_ = def.DataFloatAt(level, 0, chance_);
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

void CAbilityCriticalStrike::ApplyEffectData(const CAbilityDefFull& def, int level) {
    chance_ = def.DataFloatAt(level, 0, chance_ * 100.0f) / 100.0f;
    multiplier_ = def.DataFloatAt(level, 1, multiplier_);
}

// ===== Bash =====
CAbilityBash::CAbilityBash(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHbh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityBash::ApplyEffectData(const CAbilityDefFull& def, int level) {
    chance_ = def.DataFloatAt(level, 0, chance_ * 100.0f) / 100.0f;
    duration_ = def.DurationAt(level);
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
    // Migrated onto a single AbilityBuilder buff attach.
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABActionAddBuffTarget slow(
        [duration = duration_, percent = slowPercent_, casterHandle]() {
            return std::make_unique<CBuffSlow>(duration, percent, casterHandle);
        });
    slow.Run(game, caster, target, false, 0, 0);
}

void CAbilitySlow::ApplyEffectData(const CAbilityDefFull& def, int level) {
    slowPercent_ = def.DataFloatAt(level, 0, slowPercent_);
    duration_ = def.DurationAt(level);
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget armor(
        [duration = duration_, bonus = armorBonus_]() {
            return std::make_unique<CBuffArmor>(War3ID("BUfa"), duration, bonus, true);
        });
    armor.Run(game, caster, target, false, 0, 0);
}

void CAbilityFrostArmor::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
    armorBonus_ = def.DataIntAt(level, 1, armorBonus_);
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget curse(
        [duration = duration_, penalty = armorPenalty_]() {
            return std::make_unique<CBuffArmor>(War3ID("Bfae"), duration, penalty, false);
        });
    curse.Run(game, caster, target, false, 0, 0);
}

void CAbilityFaerieFire::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
    armorPenalty_ = -def.DataIntAt(level, 0, -armorPenalty_);
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
    // Migrated onto an AbilityBuilder branch: a same-player non-hero
    // victim (never the caster) is slain and its max life feeds the
    // caster, clamped to max.
    if (!target || !caster || target->IsDead() || target == caster) return;
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return;
    const float heal = victim->GetMaxLife();
    ABEffectChain sacrifice;
    sacrifice.Add(std::make_unique<ABActionDamageTarget>(
        1e9f, CAttackType::SPELLS, CDamageType::DEATH));
    sacrifice.Add(std::make_unique<ABActionHealCaster>(heal));
    ABActionIf pact(
        std::make_shared<ABCondFunc>(
            [caster](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && !unit->IsHero() && unit != caster &&
                       unit->GetPlayerIndex() == caster->GetPlayerIndex();
            }),
        std::move(sacrifice));
    pact.Run(game, caster, target, false, 0, 0);
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
    // Migrated onto an AbilityBuilder iteration: every corpse in radius
    // gets the full raise ritual (revive, invulnerable, ownership,
    // timed life, summoned callback).
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAnimateDeadCorpse>(War3ID("AUan"), duration_));
    ABActionForEachInRange raising(radius_, std::move(perUnit));
    raising.SetLifeFilter(ABLifeFilter::DEAD);
    raising.Run(game, caster, nullptr, true, x, y);
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
    // Migrated onto an AbilityBuilder buff attach (CBuffDoom carries the
    // duration/dps/caster triple for the guard-on-death hook).
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABActionAddBuffTarget doom(
        [duration = duration_, dps = damagePerSecond_, casterHandle]() {
            return std::make_unique<CBuffDoom>(duration, dps, casterHandle);
        });
    doom.Run(game, caster, target, false, 0, 0);
}

void CAbilityDoom::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_);
    duration_ = def.DataFloatAt(level, 2, duration_);
}

// ===== Volcano =====
CAbilityVolcano::CAbilityVolcano(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANvc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(180.0f); // ref Cool1
    SetManaCost(200); // ref Cost1
}

void CAbilityVolcano::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto AbilityBuilder waves (same scattered pattern as
    // Starfall: deterministic synced RNG, damage radius_/2).
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        const float wx = x + game.SimRandRange(-radius_, radius_);
        const float wy = y + game.SimRandRange(-radius_, radius_);
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damage_, CAttackType::SPELLS, CDamageType::FIRE));
        ABActionForEachInRange eruption(radius_ / 2, std::move(perUnit));
        eruption.Run(game, caster, nullptr, true, wx, wy);
    }
}

void CAbilityVolcano::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 4, damage_);
    radius_ = def.AreaAt(level);
}

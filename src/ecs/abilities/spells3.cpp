#include "spells3.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Mass Teleport =====
CAbilityMassTeleport::CAbilityMassTeleport(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHmt"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(30.0f);
    SetManaCost(100);
}

void CAbilityMassTeleport::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto an AbilityBuilder iteration: same-player units around
    // the caster relocate to the spell destination (carried by the teleport
    // action itself, since iteration repoints the effect point).
    if (!caster) return;
    ABEffectChain group;
    group.Add(std::make_unique<ABActionTeleportTarget>(x, y));
    ABActionForEachInRange mass(radius_, std::move(group));
    mass.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    mass.Run(game, caster, nullptr, false, 0, 0);
    caster->SetPosition(x, y);
}

// ===== Summon Water Elemental =====
CAbilitySummonWaterElemental::CAbilitySummonWaterElemental(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHwe"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(75);
}

void CAbilitySummonWaterElemental::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder summon (spread-64 fan one row above
    // the caster; run point y+32 compensates the action's half-spread
    // lift, matching the legacy layout exactly).
    if (!caster) return;
    ABActionSummon elemental(elementalType_, War3ID("AHwe"), count_,
                             summonDuration_, 64.0f);
    elemental.Run(game, caster, nullptr, true, caster->GetX(), caster->GetY() + 32.0f);
}

// ===== Resurrect (Paladin Resurrection, AHre) =====
CAbilityResurrect::CAbilityResurrect(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHre"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(180.0f);
    SetManaCost(175);
}

void CAbilityResurrect::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto an AbilityBuilder iteration: up to maxTargets dead
    // same-player units in radius are revived in place.
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionReviveTarget>());
    ABActionForEachInRange resurrection(radius_, std::move(perUnit), false, maxTargets_);
    resurrection.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    resurrection.SetLifeFilter(ABLifeFilter::DEAD);
    resurrection.Run(game, caster, nullptr, true, x, y);
}

// ===== Blink =====
CAbilityBlink::CAbilityBlink(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEbl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(5.0f);
    SetManaCost(10);
}

void CAbilityBlink::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated relocation onto an AbilityBuilder teleport (range clamp
    // stays hand-written; no-op without terrain data, as before).
    if (!caster) return;
    const float dx = x - caster->GetX(), dy = y - caster->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > maxRange_ && dist > 0.0f) {
        x = caster->GetX() + dx / dist * maxRange_;
        y = caster->GetY() + dy / dist * maxRange_;
    }
    ABActionTeleportTarget blink;
    blink.Run(game, caster, caster, true, x, y);
}

void CAbilityBlink::ApplyEffectData(const CAbilityDefFull& def, int level) {
    maxRange_ = def.DataFloatAt(level, 0, maxRange_);
}

// ===== Mirror Image =====
CAbilityMirrorImage::CAbilityMirrorImage(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOmi"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(20.0f);
    SetManaCost(50);
}

void CAbilityMirrorImage::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto AbilityBuilder copy summons: one action per image,
    // fanned one row above the caster exactly like the legacy layout.
    if (!caster) return;
    ABEffectChain images;
    for (int i = 0; i < count_; i++) {
        const float ox = (i - count_ / 2) * 64.0f;
        images.Add(std::make_unique<ABActionSummonCopyOfTarget>(
            War3ID("AOmi"), duration_, ox, 64.0f));
    }
    images.Run(game, caster, caster, false, 0, 0);
}

void CAbilityMirrorImage::ApplyEffectData(const CAbilityDefFull& def, int level) {
    count_ = def.DataIntAt(level, 0, count_);
}

// ===== Shockwave =====
CAbilityShockwave::CAbilityShockwave(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOsh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(700.0f);
    SetCooldown(8.0f);
    SetManaCost(100);
}

void CAbilityShockwave::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder line iteration (damage corridor from
    // the caster toward the target). Same geometry as the loop replaced.
    if (!target || !caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                       CDamageType::NORMAL));
    ABActionForEachInLine shock(range_, width_, std::move(perUnit));
    shock.Run(game, caster, target, false, 0, 0);
}

void CAbilityShockwave::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
}

// ===== Starfall =====
CAbilityStarfall::CAbilityStarfall(int handleId)
    // Retail AEsf (AEst is the Owl Scout); corrected against the
    // ref mapping and the retail order list (starfall=852183/AEsf).
    // Defaults are the SLK level-1 row (Cost 200, Cool 180).
    : CAbilitySpellBase(handleId, War3ID("AEsf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(200);
}

void CAbilityStarfall::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto AbilityBuilder waves: each wave scatters (deterministic
    // synced RNG, std::rand stays banned) and damages its neighborhood.
    if (!caster) return;
    const float cx = caster->GetX(), cy = caster->GetY();
    for (int w = 0; w < waves_; w++) {
        const float wx = cx + game.SimRandRange(-radius_, radius_);
        const float wy = cy + game.SimRandRange(-radius_, radius_);
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damage_, CAttackType::SPELLS, CDamageType::MAGIC));
        ABActionForEachInRange star(radius_ / 2, std::move(perUnit));
        star.Run(game, caster, nullptr, true, wx, wy);
    }
}

// ===== Tranquility =====
CAbilityTranquility::CAbilityTranquility(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEtr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(60.0f);
    SetManaCost(125);
}

void CAbilityTranquility::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder iteration: same-player units around
    // the caster (caster included) are healed and gain Rejuvenation.
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionHealTarget>(healPerTick_ * duration_));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, tick = healPerTick_]() {
            return std::make_unique<CBuffRejuvenation>(duration, tick);
        }));
    ABActionForEachInRange calm(radius_, std::move(perUnit), true);
    calm.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    calm.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilityTranquility::ApplyEffectData(const CAbilityDefFull& def, int level) {
    healPerTick_ = def.DataFloatAt(level, 0, healPerTick_);
    radius_ = def.AreaAt(level);
}

// ===== Reincarnation =====
CAbilityReincarnation::CAbilityReincarnation(int handleId, War3ID code, int manaCost)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL), manaCost_(manaCost) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(0.0f);
    SetManaCost(0);
}

void CAbilityReincarnation::OnDeath(CSimulation& game, CUnit* unit) {
    if (!unit || reincarnateRemaining_ > 0.0f) return;
    if (unit->GetMana() < manaCost_) return;
    unit->SetMana(unit->GetMana() - manaCost_);
    reincarnateRemaining_ = reincarnateCooldown_;
    // Revive in place at full life; corpse state cleared by Revive().
    unit->Revive(game, unit->GetX(), unit->GetY());
}

void CAbilityReincarnation::OnTick(CSimulation& game, CUnit* unit) {
    (void)unit;
    if (reincarnateRemaining_ > 0.0f) {
        reincarnateRemaining_ -= game.GetSimStepTime();
        if (reincarnateRemaining_ < 0.0f) reincarnateRemaining_ = 0.0f;
    }
}

// ===== Charm =====
CAbilityCharm::CAbilityCharm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANch"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(7.0f);
    SetManaCost(70);
}

void CAbilityCharm::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder branch: low-level non-heroes change
    // ownership to the caster's player.
    if (!target || !caster || target->IsDead()) return;
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return;
    ABEffectChain convert;
    convert.Add(std::make_unique<ABActionTransferToCaster>());
    ABActionIf charm(
        std::make_shared<ABCondFunc>(
            [maxLevel = maxLevel_](CSimulation&, CUnit*, CWidget* unitTarget,
                                   bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                if (!unit || unit->IsDead() || unit->IsHero()) return false;
                const CUnitType* type = unit->GetUnitType();
                return type == nullptr || type->GetLevel() <= maxLevel;
            }),
        std::move(convert));
    charm.Run(game, caster, target, false, 0, 0);
}

void CAbilityCharm::ApplyEffectData(const CAbilityDefFull& def, int level) {
    maxLevel_ = def.DataIntAt(level, 0, maxLevel_);
}

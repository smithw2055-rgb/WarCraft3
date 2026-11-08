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
    if (!caster) return;
    auto makeFinish = [&]() {
        ABEffectChain group;
        group.Add(std::make_unique<ABActionTeleportTarget>(x, y));
        auto iter = std::make_unique<ABActionForEachInRange>(
            radius_, std::move(group), true);
        iter->SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
        ABEffectChain finish;
        finish.Add(std::move(iter));
        return finish;
    };
    if (castingDelay_ <= 0.0f) {
        ABEffectChain finish = makeFinish();
        finish.Run(game, caster, nullptr, false, 0, 0);
        return;
    }
    const int ticks =
        (int)std::ceil(castingDelay_ / (double)game.GetSimStepTime());
    ABActionBeginChannel channel(ticks, ABEffectChain{}, makeFinish());
    channel.Run(game, caster, nullptr, true, x, y);
}

void CAbilityMassTeleport::ApplyEffectData(const CAbilityDefFull& def, int level) {
    castingDelay_ = def.DataFloatAt(level, 1, castingDelay_);
}

// ===== Summon Water Elemental =====
CAbilitySummonWaterElemental::CAbilitySummonWaterElemental(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHwe"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(75);
}

void CAbilitySummonWaterElemental::DoEffect(CSimulation& game, CUnit* caster) {
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
    if (!target || !caster) return;
    CastLineDamageAndStun(game, caster, target, false, 0.0f, 0.0f,
                          range_, width_, damage_, 0.0f);
}

void CAbilityShockwave::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
}

// ===== Starfall =====
CAbilityStarfall::CAbilityStarfall(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEsf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(200);
}

void CAbilityStarfall::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    const float cx = caster->GetX(), cy = caster->GetY();
    for (int w = 0; w < waves_; w++) {
        const float wx = cx + game.SimRandRange(-radius_, radius_);
        const float wy = cy + game.SimRandRange(-radius_, radius_);
        CastPointDamageWaves(game, caster, wx, wy, 1, radius_ / 2.0f, damage_,
                             CAttackType::SPELLS, CDamageType::MAGIC);
    }
}

// ===== Tranquility =====
CAbilityTranquility::CAbilityTranquility(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEtq"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(60.0f);
    SetManaCost(125);
}

void CAbilityTranquility::DoEffect(CSimulation& game, CUnit* caster) {
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

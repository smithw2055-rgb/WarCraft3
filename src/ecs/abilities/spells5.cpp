#include "spells5.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
#include <algorithm>
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
    // Migrated onto AbilityBuilder waves (same instant multi-wave pattern
    // as Blizzard / Rain of Fire).
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damagePerWave_, CAttackType::SPELLS, CDamageType::FIRE));
        ABActionForEachInRange wave(radius_, std::move(perUnit));
        wave.Run(game, caster, nullptr, true, x, y);
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
    // Migrated onto AbilityBuilder waves; each wave corrupts every unit in
    // the area (friend and foe, caster exempt) for a max-life fraction.
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTargetMaxLifePct>(
            damageFraction_, CAttackType::SPELLS, CDamageType::DEATH));
        ABActionForEachInRange wave(radius_, std::move(perUnit));
        wave.Run(game, caster, nullptr, true, x, y);
    }
}

void CAbilityDeathAndDecay::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damageFraction_ = def.DataFloatAt(level, 0, damageFraction_);
    radius_ = def.AreaAt(level);
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
    // Migrated onto an AbilityBuilder line iteration (damage + stun
    // corridor). The dead-target early-out matches the loop replaced.
    if (!target || !caster || target->IsDead()) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                       CDamageType::NORMAL));
    perUnit.Add(std::make_unique<ABActionStunTarget>(stunDuration_));
    ABActionForEachInLine impale(range_, width_, std::move(perUnit));
    impale.Run(game, caster, target, false, 0, 0);
}

// ===== Fan of Knives =====
CAbilityFanOfKnives::CAbilityFanOfKnives(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEfk"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityFanOfKnives::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder iteration: only units owned by other
    // players take damage (same player-index rule as the loop replaced).
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                       CDamageType::NORMAL));
    ABActionForEachInRange fan(radius_, std::move(perUnit));
    fan.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    fan.Run(game, caster, nullptr, false, 0, 0);
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
    // Migrated onto AbilityBuilder waves (same instant multi-wave pattern
    // as Blizzard; the .j drop timing is a visual pass).
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damagePerWave_, CAttackType::SPELLS, CDamageType::FIRE));
        ABActionForEachInRange wave(radius_, std::move(perUnit));
        wave.Run(game, caster, nullptr, true, x, y);
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
    // Migrated onto AbilityBuilder pieces: a same-player non-hero victim
    // (never the caster) dies, and its life force becomes caster mana
    // (capped by the pool), computed up front like AbsorbMana.
    if (!target || !caster || target->IsDead() || target == caster) return;
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim || victim->IsHero()) return;
    if (victim->GetPlayerIndex() != caster->GetPlayerIndex()) return;
    const float mana = std::min(caster->GetMana() + victim->GetLife(),
                                static_cast<float>(caster->GetMaxMana()));
    ABActionDamageTarget slay(1e9f, CAttackType::SPELLS, CDamageType::DEATH);
    slay.Run(game, caster, target, false, 0, 0);
    ABActionSetManaTarget gift(mana);
    gift.Run(game, caster, caster, false, 0, 0);
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
    // Migrated onto an AbilityBuilder chain: poison hit plus slow and
    // poison over time.
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                     CDamageType::POISON));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = slowDuration_, percent = slowPercent_, casterHandle]() {
            return std::make_unique<CBuffSlow>(duration, percent, casterHandle);
        }));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = poisonDuration_, tick = poisonPerTick_, casterHandle]() {
            return std::make_unique<CBuffPoison>(duration, tick, casterHandle);
        }));
    chain.Run(game, caster, target, false, 0, 0);
}

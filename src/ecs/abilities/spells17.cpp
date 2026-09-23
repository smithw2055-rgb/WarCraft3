#include "spells17.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
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
    // Migrated onto an AbilityBuilder iteration: other-player units around
    // the caster gain the Howl debuff. The caster is excluded, matching the
    // hand-rolled loop this replaces.
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, factor = damageFactor_]() {
            return std::make_unique<CBuffHowl>(duration, factor);
        }));
    ABActionForEachInRange howl(radius_, std::move(perUnit));
    howl.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    howl.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilityHowlOfTerror::ApplyEffectData(const CAbilityDefFull& def, int level) {
    // DataA is the reduction amount; the engine stores the factor.
    damageFactor_ = 1.0f - def.DataFloatAt(level, 0, 1.0f - damageFactor_);
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
    // Migrated onto an AbilityBuilder pair (ref cone-approximated-as-splash):
    // the primary target always takes the full FIRE hit plus the burn, then
    // other-player units around it splash. The primary is excluded from the
    // splash (it is NOT exempt -- it is hit once via the direct chain), and
    // the caster stays out, both matching the hand-rolled loop.
    const int casterHandle = caster->GetHandleId();
    ABEffectChain direct;
    direct.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::FIRE));
    direct.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = burnDuration_, tick = burnPerTick_, casterHandle]() {
            return std::make_unique<CBuffPoison>(duration, tick, casterHandle);
        }));
    direct.Run(game, caster, target, false, 0, 0);
    ABEffectChain splash;
    splash.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::FIRE));
    splash.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = burnDuration_, tick = burnPerTick_, casterHandle]() {
            return std::make_unique<CBuffPoison>(duration, tick, casterHandle);
        }));
    ABActionForEachInRange breath(radius_, std::move(splash));
    breath.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    breath.SetExclude(target);
    breath.Run(game, caster, target, false, 0, 0);
}

void CAbilityBreathOfFire::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
    burnPerTick_ = def.DataFloatAt(level, 4, burnPerTick_);
    burnDuration_ = def.DurationAt(level);
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
    // Migrated onto an AbilityBuilder iteration: other-player units at the
    // impact point gain Haze plus Slow. The caster falls out via the
    // faction filter, matching the hand-rolled loop this replaces.
    const int casterHandle = caster->GetHandleId();
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, miss = missChance_]() {
            return std::make_unique<CBuffHaze>(duration, miss);
        }));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, slow = slowPercent_, casterHandle]() {
            return std::make_unique<CBuffSlow>(duration, slow, casterHandle);
        }));
    ABActionForEachInRange haze(radius_, std::move(perUnit));
    haze.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    haze.Run(game, caster, nullptr, true, x, y);
}

void CAbilityDrunkenHaze::ApplyEffectData(const CAbilityDefFull& def, int level) {
    missChance_ = def.DataFloatAt(level, 1, missChance_);
    slowPercent_ = def.DataFloatAt(level, 2, slowPercent_);
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget shield(
        [duration = duration_]() {
            return std::make_unique<CBuffManaShield>(duration);
        });
    shield.Run(game, caster, caster, false, 0, 0);
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
    // Migrated onto AbilityBuilder waves: each wave runs a per-unit POISON
    // damage chain over other-player units around the caster. Waves resolve
    // instantly; the total matches the old loop.
    for (int w = 0; w < waves_; w++) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damagePerWave_, CAttackType::SPELLS, CDamageType::POISON));
        ABActionForEachInRange swarm(radius_, std::move(perUnit));
        swarm.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
        swarm.Run(game, caster, nullptr, false, 0, 0);
    }
}

void CAbilityLocustSwarm::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerWave_ = def.DataFloatAt(level, 0, damagePerWave_);
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget storm(
        [duration = duration_]() { return std::make_unique<CBuffCyclone>(duration); });
    storm.Run(game, caster, target, false, 0, 0);
}

void CAbilityCyclone::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
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

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
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, factor = damageFactor_]() {
                           return std::make_unique<CBuffHowl>(duration, factor);
                       },
                       ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}

void CAbilityHowlOfTerror::ApplyEffectData(const CAbilityDefFull& def, int level) {
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
    const int casterHandle = caster->GetHandleId();
    auto makeHitAndBurn = [&]() {
        ABEffectChain chain;
        chain.Add(std::make_unique<ABActionDamageTarget>(
            damage_, CAttackType::SPELLS, CDamageType::FIRE));
        chain.Add(std::make_unique<ABActionAddBuffTarget>(
            [duration = burnDuration_, tick = burnPerTick_, casterHandle]() {
                return std::make_unique<CBuffPoison>(duration, tick, casterHandle);
            }));
        return chain;
    };
    ABEffectChain direct = makeHitAndBurn();
    direct.Run(game, caster, target, false, 0, 0);
    ABActionForEachInRange breath(radius_, makeHitAndBurn());
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
    CastBuffOnSelf(game, caster, [duration = duration_]() {
        return std::make_unique<CBuffManaShield>(duration);
    });
}

// ===== Locust Swarm =====
CAbilityLocustSwarm::CAbilityLocustSwarm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUls"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(150);
}

void CAbilityLocustSwarm::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaDamageWaves(game, caster, waves_, radius_, damagePerWave_,
                              CAttackType::SPELLS, CDamageType::POISON,
                              ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
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
    CastBuffOnTarget(game, caster, target, [duration = duration_]() {
        return std::make_unique<CBuffCyclone>(duration);
    });
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
    CUnit* spirit = game.CreateUnit(spiritType_, unit->GetPlayerIndex(),
                                    unit->GetX(), unit->GetY(), 0);
    if (!spirit) return;
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(unit->GetHandleId(), spirit->GetHandleId());
    }
}

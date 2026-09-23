#include "spells14.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_builder.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Pocket Factory =====
CAbilityPocketFactory::CAbilityPocketFactory(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsy"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(35.0f);
    SetManaCost(125);
}

void CAbilityPocketFactory::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto a single AbilityBuilder create-and-buff (TimedLife
    // plus Spawner; graceful without unit data like the code replaced).
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    ABActionCreateUnitWithBuff factory(
        factoryType_, x, y,
        {BF([life = factoryLife_]() {
             return std::make_unique<CBuffTimedLife>(War3ID("ANsy"), life);
         }),
         BF([life = factoryLife_, goblin = goblinType_, interval = spawnInterval_,
             boom = explodeDamage_, radius = explodeRadius_]() {
             return std::make_unique<CBuffSpawner>(life, goblin, interval, boom, radius);
         })});
    factory.Run(game, caster, nullptr, true, x, y);
}

void CAbilityPocketFactory::ApplyEffectData(const CAbilityDefFull& def, int level) {
    factoryType_ = def.UnitIdAt(level, factoryType_);
    const std::string goblin = def.DataAt(level, 1);
    if (!goblin.empty()) goblinType_ = War3ID::FromString(goblin);
}

// ===== Lava Spawn =====
CAbilityLavaSpawn::CAbilityLavaSpawn(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANlm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(32.0f);
    SetManaCost(150);
}

void CAbilityLavaSpawn::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder create-and-buff (TimedLife
    // plus Split down the tier chain).
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    ABActionCreateUnitWithBuff spawn(
        spawnType_, caster->GetX() + 64.0f, caster->GetY(),
        {BF([dur = summonDuration_]() {
             return std::make_unique<CBuffTimedLife>(War3ID("ANlm"), dur);
         }),
         BF([split = splitType_, n = splitCount_, child = childType_, cn = childCount_]() {
             return std::make_unique<CBuffSplit>(split, n, child, cn);
         })});
    spawn.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilityLavaSpawn::ApplyEffectData(const CAbilityDefFull& def, int level) {
    // The summoned tier walks the UnitID progression; each tier splits
    // into the next while one exists (ref nlv1 -> nlv2 -> nlv3).
    spawnType_ = def.UnitIdAt(level, spawnType_);
    splitType_ = def.UnitIdAt(level + 1, War3ID());
    if (splitType_ == War3ID() || splitType_ == spawnType_) {
        splitCount_ = 0;
    } else {
        splitCount_ = 2;
    }
    childType_ = def.UnitIdAt(level + 2, War3ID());
    if (childType_ == War3ID() || childType_ == splitType_) {
        childCount_ = 0;
    } else {
        childCount_ = 2;
    }
}

// ===== Soul Burn =====
CAbilitySoulBurn::CAbilitySoulBurn(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(9.0f);
    SetManaCost(75);
}

void CAbilitySoulBurn::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder chain: direct FIRE hit plus poison.
    if (!target || target->IsDead()) return;
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::FIRE));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = burnDuration_, tick = burnPerTick_, casterHandle]() {
            return std::make_unique<CBuffPoison>(duration, tick, casterHandle);
        }));
    chain.Run(game, caster, target, false, 0, 0);
}

// ===== Quilbeast =====
CAbilityQuilbeast::CAbilityQuilbeast(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsq"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
    SetManaCost(75);
}

void CAbilityQuilbeast::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder summon (ref UnitID progression): one
    // timed beast beside the caster. Spread 0 + explicit point preserves
    // the exact legacy spawn offset (caster + 64, 0).
    if (!caster) return;
    ABActionSummon quil(beastType_, War3ID("ANsq"), 1, summonDuration_, 0.0f);
    quil.Run(game, caster, nullptr, true, caster->GetX() + 64.0f, caster->GetY());
}

void CAbilityQuilbeast::ApplyEffectData(const CAbilityDefFull& def, int level) {
    beastType_ = def.UnitIdAt(level, beastType_);
}

// ===== Frost Breath =====
CAbilityFrostBreath::CAbilityFrostBreath(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Afsh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(300.0f);
    SetCooldown(8.0f);
    SetManaCost(100);
}

void CAbilityFrostBreath::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder iteration: enemy units (caster
    // excluded) around the target take COLD damage + slow. The primary
    // needs no SetExclude -- distance 0 keeps it in the splash, matching
    // the hand-rolled loop's isPrimary exemption.
    if (!target || !caster || target->IsDead()) return;
    const int casterHandle = caster->GetHandleId();
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::COLD));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = slowDuration_, percent = slowPercent_, casterHandle]() {
            return std::make_unique<CBuffSlow>(duration, percent, casterHandle);
        }));
    ABActionForEachInRange breath(radius_, std::move(perUnit));
    breath.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    breath.Run(game, caster, target, false, 0, 0);
}

// ===== Pulverize =====
CAbilityPulverize::CAbilityPulverize(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Awar"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Forked Lightning =====
CAbilityForkedLightning::CAbilityForkedLightning(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANfl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityForkedLightning::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder bounce (ref ChainLightning pattern in
    // spells2.cpp): each hop rebuilds its LIGHTNING hit with the
    // (1 - reduction) falloff baked in. Arcs between enemies per the repo
    // AoE convention in the loop replaced.
    if (!target || target->IsDead()) return;
    ABActionBounce forked(
        bounces_, bounceRange_,
        [damage = damage_, reduction = damageReduction_](int bounce) {
            ABEffectChain hit;
            hit.Add(std::make_unique<ABActionDamageTarget>(
                static_cast<float>(damage * std::pow(1.0f - reduction, bounce)),
                CAttackType::SPELLS, CDamageType::LIGHTNING));
            return hit;
        });
    forked.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    forked.Run(game, caster, target, false, 0, 0);
}

void CAbilityForkedLightning::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
    bounces_ = def.DataIntAt(level, 1, bounces_);
}

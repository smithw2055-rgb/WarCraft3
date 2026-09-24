#include "spells.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "data/abilitydata.h"
#include <cmath>
#include <iostream>

// ===== Blizzard =====
CAbilityBlizzard::CAbilityBlizzard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHbz"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(75);
}

void CAbilityBlizzard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // Migrated onto an AbilityBuilder wave: each wave runs a per-unit
    // damage chain over the target area. Waves resolve instantly (the .j
    // shard timing is a visual pass); the total matches the old loop.
    for (int wave = 0; wave < waves_; wave++) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damagePerWave_, CAttackType::SPELLS, CDamageType::COLD));
        ABActionForEachInRange shard(radius_, std::move(perUnit));
        shard.Run(game, caster, nullptr, true, x, y);
    }
}

void CAbilityBlizzard::ApplyEffectData(const CAbilityDefFull& def, int level) {
    waves_ = def.DataIntAt(level, 0, waves_);
    damagePerWave_ = def.DataFloatAt(level, 1, damagePerWave_);
}

// ===== Holy Light =====
CAbilityHolyLight::CAbilityHolyLight(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHhb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(5.0f);
    SetManaCost(65);
}

void CAbilityHolyLight::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder branch (ref HolyLight.j doEffect:
    // undead take half damage as a ranged DIVINE bolt, others are healed).
    ABEffectChain smite;
    smite.Add(std::make_unique<ABActionDamageTarget>(
        healAmount_ * 0.5f, CAttackType::NORMAL, CDamageType::DIVINE, true));
    ABEffectChain mend;
    mend.Add(std::make_unique<ABActionHealTarget>(healAmount_));
    ABActionIf holy(std::make_shared<ABCondTargetUndead>(), std::move(smite),
                    std::move(mend));
    holy.Run(game, caster, target, false, 0, 0);
}

void CAbilityHolyLight::ApplyEffectData(const CAbilityDefFull& def, int level) {
    healAmount_ = def.DataFloatAt(level, 0, healAmount_);
}

// ===== Divine Shield =====
CAbilityDivineShield::CAbilityDivineShield(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHds"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(35.0f);
    SetManaCost(25);
}

void CAbilityDivineShield::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget shield(
        [duration = duration_]() { return std::make_unique<CBuffDivineShield>(duration); });
    shield.Run(game, caster, caster, false, 0, 0);
}

void CAbilityDivineShield::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
}

// ===== Thunder Clap =====
CAbilityThunderClap::CAbilityThunderClap(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHtc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCastRange(0);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(90);
}

void CAbilityThunderClap::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder iteration (damage + slow around the
    // caster). Same coverage as the loop replaced.
    if (!caster) return;
    const int casterHandle = caster->GetHandleId();
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                       CDamageType::SONIC));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [slowDuration = slowDuration_, slowPercent = slowPercent_, casterHandle]() {
            return std::make_unique<CBuffSlow>(slowDuration, slowPercent, casterHandle);
        }));
    ABActionForEachInRange clap(radius_, std::move(perUnit));
    clap.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilityThunderClap::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
}

// ===== Storm Bolt =====
CAbilityStormBolt::CAbilityStormBolt(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHtb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(9.0f);
    SetManaCost(75);
}

void CAbilityStormBolt::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // StormBolt.j doEffect: a homing missile whose on-hit deals damage and
    // stuns. Nothing lands at cast time; flight time applies.
    ABActionMissileStun bolt(damage_, stunDuration_, 800.0f,
                             CAttackType::SPELLS, CDamageType::LIGHTNING);
    bolt.Run(game, caster, target, false, 0, 0);
}

void CAbilityStormBolt::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
}

// ===== War Stomp =====
CAbilityWarStomp::CAbilityWarStomp(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOws"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCastRange(0);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(90);
}

void CAbilityWarStomp::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder iteration (damage + stun per unit in
    // radius around the caster, caster excluded). Same coverage as the
    // hand-rolled loop it replaces.
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::SONIC));
    perUnit.Add(std::make_unique<ABActionStunTarget>(stunDuration_));
    ABActionForEachInRange stomp(radius_, std::move(perUnit));
    stomp.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilityWarStomp::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
}

// ===== Death Coil =====
CAbilityDeathCoil::CAbilityDeathCoil(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUdc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(75);
}

void CAbilityDeathCoil::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder branch: allied undead are healed,
    // everyone else takes DEATH damage. Same rule as the code replaced.
    ABEffectChain mend;
    mend.Add(std::make_unique<ABActionHealTarget>(healAmount_));
    ABEffectChain harm;
    harm.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                    CDamageType::DEATH));
    ABActionIf coil(std::make_shared<ABCondAll>(
                        std::vector<std::shared_ptr<ABCondition>>{
                            std::make_shared<ABCondTargetAllyOfCaster>(),
                            std::make_shared<ABCondTargetUndead>(),
                        }),
                    std::move(mend), std::move(harm));
    coil.Run(game, caster, target, false, 0, 0);
}

void CAbilityDeathCoil::ApplyEffectData(const CAbilityDefFull& def, int level) {
    healAmount_ = def.DataFloatAt(level, 0, healAmount_);
}

// ===== Frost Nova =====
CAbilityFrostNova::CAbilityFrostNova(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUfn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(8.0f);
    SetManaCost(125);
}

void CAbilityFrostNova::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder pair: direct COLD hit on the primary
    // target, then a splash (reduced damage + slow) around it. The primary
    // is excluded from the splash, and the caster is included when in
    // radius -- both match the hand-rolled loop this replaces.
    ABEffectChain direct;
    direct.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                      CDamageType::COLD));
    direct.Run(game, caster, target, false, 0, 0);
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain splash;
    splash.Add(std::make_unique<ABActionDamageTarget>(aoeDamage_, CAttackType::SPELLS,
                                                      CDamageType::COLD));
    splash.Add(std::make_unique<ABActionAddBuffTarget>(
        [slowDuration = slowDuration_, slowPercent = slowPercent_, casterHandle]() {
            return std::make_unique<CBuffSlow>(slowDuration, slowPercent, casterHandle);
        }));
    ABActionForEachInRange nova(radius_, std::move(splash), true);
    nova.SetExclude(target);
    nova.Run(game, caster, target, false, 0, 0);
}

void CAbilityFrostNova::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 1, damage_);
    aoeDamage_ = def.DataFloatAt(level, 0, aoeDamage_);
}

// ===== Mana Burn =====
CAbilityManaBurn::CAbilityManaBurn(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEmb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(7.0f);
    SetManaCost(50);
}

void CAbilityManaBurn::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto a single AbilityBuilder action (burn + feedback damage).
    ABActionManaBurn burn(manaBurned_, CAttackType::SPELLS, CDamageType::MIND);
    burn.Run(game, caster, target, false, 0, 0);
}

void CAbilityManaBurn::ApplyEffectData(const CAbilityDefFull& def, int level) {
    manaBurned_ = def.DataIntAt(level, 0, manaBurned_);
}

// ===== Entangling Roots =====
CAbilityEntanglingRoots::CAbilityEntanglingRoots(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEer"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityEntanglingRoots::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto an AbilityBuilder chain: pin (ensnare) plus burn-over-
    // duration (poison). No upfront nuke, matching the code replaced.
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() { return std::make_unique<CBuffEnsnare>(duration); }));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, dps = damagePerSecond_, casterHandle]() {
            return std::make_unique<CBuffPoison>(duration, dps, casterHandle);
        }));
    chain.Run(game, caster, target, false, 0, 0);
}

void CAbilityEntanglingRoots::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_);
    duration_ = def.DurationAt(level);
}

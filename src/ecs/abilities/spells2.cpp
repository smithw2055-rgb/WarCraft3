#include "spells2.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_builder.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Healing Wave =====
CAbilityHealingWave::CAbilityHealingWave(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOhw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(9.0f);
    SetManaCost(90);
}

void CAbilityHealingWave::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    ABActionBounce wave(
        bounces_, bounceRange_,
        [heal = healAmount_](int bounce) {
            ABEffectChain mend;
            mend.Add(std::make_unique<ABActionHealTarget>(
                static_cast<float>(heal * std::pow(0.75f, bounce))));
            return mend;
        });
    wave.Run(game, caster, target, false, 0, 0);
}

void CAbilityHealingWave::ApplyEffectData(const CAbilityDefFull& def, int level) {
    healAmount_ = def.DataFloatAt(level, 0, healAmount_);
    bounces_ = def.DataIntAt(level, 1, bounces_);
}

// ===== Hex =====
CAbilityHex::CAbilityHex(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOhx"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(7.0f);
    SetManaCost(70);
}

void CAbilityHex::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() {
        return std::make_unique<CBuffHex>(duration);
    });
}

void CAbilityHex::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
}

// ===== Chain Lightning =====
CAbilityChainLightning::CAbilityChainLightning(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOcl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(9.0f);
    SetManaCost(120);
}

void CAbilityChainLightning::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    ABActionBounce bolt(
        bounces_, bounceRange_,
        [damage = damage_, reduction = damageReduction_](int bounce) {
            ABEffectChain hit;
            hit.Add(std::make_unique<ABActionDamageTarget>(
                static_cast<float>(damage * std::pow(1.0f - reduction, bounce)),
                CAttackType::SPELLS, CDamageType::LIGHTNING));
            return hit;
        });
    bolt.Run(game, caster, target, false, 0, 0);
}

void CAbilityChainLightning::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
    bounces_ = def.DataIntAt(level, 1, bounces_);
    damageReduction_ = def.DataFloatAt(level, 2, damageReduction_);
}

// ===== Feral Spirit =====
CAbilityFeralSpirit::CAbilityFeralSpirit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOsf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(75);
}

void CAbilityFeralSpirit::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionSummon wolves(wolfType_, War3ID("AOsf"), count_, summonDuration_, 64.0f);
    wolves.Run(game, caster, nullptr, true, caster->GetX(), caster->GetY() + 32.0f);
}

void CAbilityFeralSpirit::ApplyEffectData(const CAbilityDefFull& def, int level) {
    count_ = def.DataIntAt(level, 1, count_);
}

// ===== Earthquake =====
CAbilityEarthquake::CAbilityEarthquake(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOeq"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(90.0f);
    SetManaCost(150);
}

void CAbilityEarthquake::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    ABEffectChain smash;
    smash.Add(std::make_unique<ABActionDamageTarget>(
        damagePerSecond_ * duration_, CAttackType::SIEGE, CDamageType::NORMAL));
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionIf>(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && !unit->IsDead() && unit->IsBuilding();
            }),
        std::move(smash)));
    ABActionForEachInRange quake(radius_, std::move(perUnit), true);
    quake.Run(game, caster, nullptr, true, x, y);
}

void CAbilityEarthquake::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerSecond_ = def.DataFloatAt(level, 1, damagePerSecond_);
    radius_ = def.AreaAt(level);
    duration_ = def.DurationAt(level);
}

// ===== Carrion Swarm =====
CAbilityCarrionSwarm::CAbilityCarrionSwarm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUcs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(700.0f);
    SetCooldown(10.0f);
    SetManaCost(110);
}

void CAbilityCarrionSwarm::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster) return;
    CastLineDamageAndStun(game, caster, target, false, 0.0f, 0.0f,
                          range_, width_, damage_, 0.0f,
                          CAttackType::SPELLS, CDamageType::POISON);
}

void CAbilityCarrionSwarm::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
}

// ===== Sleep =====
CAbilitySleep::CAbilitySleep(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUsl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(100);
}

void CAbilitySleep::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABEffectChain doze;
    doze.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() { return std::make_unique<CBuffSleep>(duration); }));
    ABActionIf sleep(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && !unit->IsDead() && unit->CanSleep();
            }),
        std::move(doze));
    sleep.Run(game, caster, target, false, 0, 0);
}

void CAbilitySleep::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
}

// ===== Inferno =====
CAbilityInferno::CAbilityInferno(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUin"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(900.0f);
    SetCooldown(180.0f);
    SetManaCost(175);
}

void CAbilityInferno::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionStunTarget>(stunDuration_));
    ABActionForEachInRange terror(stunRadius_, std::move(perUnit), true);
    terror.Run(game, caster, nullptr, true, x, y);
    ABActionSummon infernal(infernalType_, War3ID("AUin"), 1, summonDuration_, 0.0f);
    infernal.Run(game, caster, nullptr, true, x, y);
}

void CAbilityInferno::ApplyEffectData(const CAbilityDefFull& def, int level) {
    summonDuration_ = def.DataFloatAt(level, 1, summonDuration_);
}

// ===== Silence =====
CAbilitySilence::CAbilitySilence(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsi"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(900.0f);
    SetCooldown(15.0f);
    SetManaCost(75);
}

void CAbilitySilence::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastPointAreaBuff(game, caster, x, y, radius_, [duration = duration_]() {
        return std::make_unique<CBuffSilence>(duration);
    });
}

void CAbilitySilence::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
    radius_ = def.AreaAt(level);
}

// ===== Black Arrow =====
CAbilityBlackArrow::CAbilityBlackArrow(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANba"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(2.0f);
    SetManaCost(10);
}

void CAbilityBlackArrow::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster) return;
    ABActionDamageTarget shot(bonusDamage_, CAttackType::PIERCE, CDamageType::NORMAL,
                              true, true);
    shot.Run(game, caster, target, false, 0, 0);
}

void CAbilityBlackArrow::ApplyEffectData(const CAbilityDefFull& def, int level) {
    bonusDamage_ = def.DataFloatAt(level, 0, bonusDamage_);
}

// ===== Life Drain =====
CAbilityLifeDrain::CAbilityLifeDrain(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANdr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityLifeDrain::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    const int casterHandle = caster->GetHandleId();
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, tick = drainPerSecond_, casterHandle]() {
                         return std::make_unique<CBuffLifeDrain>(duration, tick, casterHandle);
                     });
}

void CAbilityLifeDrain::ApplyEffectData(const CAbilityDefFull& def, int level) {
    drainPerSecond_ = def.DataFloatAt(level, 0, drainPerSecond_);
    duration_ = def.DurationAt(level);
}

// ===== Finger of Death =====
CAbilityFingerOfDeath::CAbilityFingerOfDeath(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANfd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCooldown(15.0f);
    SetManaCost(0);
}

void CAbilityFingerOfDeath::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target) return;
    ABEffectChain lethal;
    lethal.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                      CDamageType::DEATH));
    ABEffectChain heroic;
    heroic.Add(std::make_unique<ABActionDamageTarget>(heroDamage_, CAttackType::SPELLS,
                                                      CDamageType::DEATH));
    ABActionIf finger(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && unit->IsHero();
            }),
        std::move(heroic), std::move(lethal));
    finger.Run(game, caster, target, false, 0, 0);
}

void CAbilityFingerOfDeath::ApplyEffectData(const CAbilityDefFull& def, int level) {
    heroDamage_ = def.DataFloatAt(level, 2, heroDamage_);
}

// ===== Dark Portal =====
CAbilityDarkPortal::CAbilityDarkPortal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANdp"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(100.0f);
    SetManaCost(0);
}

void CAbilityDarkPortal::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionSummon>(
        portalUnitType_, War3ID("ANdp"), summonCount_, summonDuration_));
    AbilityBuilderRegistry::Instance().Register(War3ID("ANdp"), std::move(chain));
    if (const ABEffectChain* found = AbilityBuilderRegistry::Instance().Find(War3ID("ANdp"))) {
        found->Run(game, caster, nullptr, true, x, y);
    }
}

void CAbilityDarkPortal::ApplyEffectData(const CAbilityDefFull& def, int level) {
    summonCount_ = def.DataIntAt(level, 1, summonCount_);
}

// ===== Rain of Chaos =====
CAbilityRainOfChaos::CAbilityRainOfChaos(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANrc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(120.0f);
    SetManaCost(0);
}

void CAbilityRainOfChaos::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    for (int w = 0; w < waves_; w++) {
        float wx = x + game.SimRandRange(-100.0f, 100.0f);
        float wy = y + game.SimRandRange(-100.0f, 100.0f);
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damage_, CAttackType::SPELLS, CDamageType::FIRE));
        ABActionForEachInRange chaos(radius_, std::move(perUnit), true);
        chaos.Run(game, caster, nullptr, true, wx, wy);
    }
}

void CAbilityRainOfChaos::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
}

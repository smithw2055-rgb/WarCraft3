// ============================================================================
// WarCraft 3 Domain Spell, Aura, Passive & Race Economy Ability Implementation
// Phase 30 (Track B): Consolidated from 30 legacy `src/ecs/abilities/spells*.cpp`
// and `auras.cpp` files into `src/game/war3/abilities/war3_spells.cpp`.
// ============================================================================
#include "game/war3/abilities/war3_spells.h"
#include "data/abilitydata.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/ability_factory.h"
#include "ecs/buff.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include <algorithm>
#include <cmath>
// === auras.cpp ===
CAbilityDevotionAura::CAbilityDevotionAura(int handleId)
    : CAbilityAura(handleId, War3ID("AHad"), 900.0f, true, true, false,
                   {{CAuraEffectType::ARMOR, 1.5f, false}}) {}
CAbilityEnduranceAura::CAbilityEnduranceAura(int handleId)
    : CAbilityAura(handleId, War3ID("AOae"), 900.0f, true, true, false,
                   {{CAuraEffectType::MOVE_SPEED, 0.10f, true},
                    {CAuraEffectType::ATTACK_SPEED, 0.10f, true}}) {}
CAbilityUnholyAura::CAbilityUnholyAura(int handleId)
    : CAbilityAura(handleId, War3ID("AUau"), 900.0f, true, true, false,
                   {{CAuraEffectType::MOVE_SPEED, 0.10f, true},
                    {CAuraEffectType::LIFE_REGEN, 0.5f, false}}) {}
CAbilityThornsAura::CAbilityThornsAura(int handleId)
    : CAbilityAura(handleId, War3ID("AEah"), 900.0f, true, true, false) {
    SetAuraBuff(War3ID("BEah"), []() {
        return std::make_unique<CBuffThorns>(2.0f, 0.10f, War3ID("BEah"));
    });
}
CAbilityBrillianceAura::CAbilityBrillianceAura(int handleId)
    : CAbilityAura(handleId, War3ID("AHab"), 900.0f, true, true, false,
                   {{CAuraEffectType::MANA_REGEN, 0.75f, false}}) {}
CAbilityTrueshotAura::CAbilityTrueshotAura(int handleId)
    : CAbilityAura(handleId, War3ID("AEar"), 900.0f, true, true, false,
                   {{CAuraEffectType::DAMAGE, 0.10f, true}}) {}
CAbilityVampiricAura::CAbilityVampiricAura(int handleId)
    : CAbilityAura(handleId, War3ID("AUav"), 900.0f, true, true, false) {
    SetAuraBuff(War3ID("BUav"), []() { return std::make_unique<CBuffLifesteal>(2.0f, 0.15f); });
}
CAbilityCommandAura::CAbilityCommandAura(int handleId)
    : CAbilityAura(handleId, War3ID("AOac"), 900.0f, true, false, false,
                   {{CAuraEffectType::DAMAGE, 0.10f, true}}) {}
CAbilityRegenerationAura::CAbilityRegenerationAura(int handleId)
    : CAbilityAura(handleId, War3ID("Aoar"), 900.0f, true, true, false,
                   {{CAuraEffectType::LIFE_REGEN, 1.0f, false}}) {}
CAbilityManaRegenerationAura::CAbilityManaRegenerationAura(int handleId)
    : CAbilityAura(handleId, War3ID("Aarm"), 900.0f, true, true, false,
                   {{CAuraEffectType::MANA_REGEN, 1.0f, false}}) {}
CAbilityDiseaseCloud::CAbilityDiseaseCloud(int handleId)
    : CAbilityAura(handleId, War3ID("Aapl"), 176.0f, false, false, true) {
    SetAuraBuff(War3ID("Bapl"), []() {
        return std::make_unique<CBuffDisease>(War3ID("Bapl"), 10.0f, 10.0f);
    });
}
// === spells.cpp ===
CAbilityBlizzard::CAbilityBlizzard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHbz"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(75);
}
void CAbilityBlizzard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) { CastPointDamageWaves(game, caster, x, y, waves_, radius_, damagePerWave_, CAttackType::SPELLS, CDamageType::COLD); }
void CAbilityBlizzard::ApplyEffectData(const CAbilityDefFull& def, int level) { waves_ = def.DataIntAt(level, 0, waves_); damagePerWave_ = def.DataFloatAt(level, 1, damagePerWave_); }
CAbilityHolyLight::CAbilityHolyLight(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHhb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(5.0f);
    SetManaCost(65);
}
void CAbilityHolyLight::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABEffectChain smite;
    smite.Add(std::make_unique<ABActionDamageTarget>(
        healAmount_ * 0.5f, CAttackType::NORMAL, CDamageType::DIVINE, true));
    ABEffectChain mend;
    mend.Add(std::make_unique<ABActionHealTarget>(healAmount_));
    ABActionIf holy(std::make_shared<ABCondTargetUndead>(), std::move(smite),
                    std::move(mend));
    holy.Run(game, caster, target, false, 0, 0);
}
void CAbilityHolyLight::ApplyEffectData(const CAbilityDefFull& def, int level) { healAmount_ = def.DataFloatAt(level, 0, healAmount_); }
CAbilityDivineShield::CAbilityDivineShield(int handleId) : CAbilitySpellBase(handleId, War3ID("AHds"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(35.0f); SetManaCost(25); }
void CAbilityDivineShield::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() { return std::make_unique<CBuffDivineShield>(duration); });
}
void CAbilityDivineShield::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); }
CAbilityThunderClap::CAbilityThunderClap(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHtc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCastRange(0);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(90);
}
void CAbilityThunderClap::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    const int casterHandle = caster->GetHandleId();
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                       CDamageType::SONIC));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [slowDuration = slowDuration_, slowPercent = slowPercent_, casterHandle]() { return std::make_unique<CBuffSlow>(slowDuration, slowPercent, casterHandle); }));
    ABActionForEachInRange clap(radius_, std::move(perUnit));
    clap.Run(game, caster, nullptr, false, 0, 0);
}
void CAbilityThunderClap::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); }
CAbilityStormBolt::CAbilityStormBolt(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHtb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(9.0f);
    SetManaCost(75);
}
void CAbilityStormBolt::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionMissileStun bolt(damage_, stunDuration_, 800.0f,
                             CAttackType::SPELLS, CDamageType::LIGHTNING);
    bolt.Run(game, caster, target, false, 0, 0);
}
void CAbilityStormBolt::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); }
CAbilityWarStomp::CAbilityWarStomp(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOws"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCastRange(0);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(90);
}
void CAbilityWarStomp::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::SONIC));
    perUnit.Add(std::make_unique<ABActionStunTarget>(stunDuration_));
    ABActionForEachInRange stomp(radius_, std::move(perUnit));
    stomp.Run(game, caster, nullptr, false, 0, 0);
}
void CAbilityWarStomp::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); }
CAbilityDeathCoil::CAbilityDeathCoil(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUdc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(75);
}
void CAbilityDeathCoil::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
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
void CAbilityDeathCoil::ApplyEffectData(const CAbilityDefFull& def, int level) { healAmount_ = def.DataFloatAt(level, 0, healAmount_); }
CAbilityFrostNova::CAbilityFrostNova(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUfn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(8.0f);
    SetManaCost(125);
}
void CAbilityFrostNova::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABEffectChain direct;
    direct.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                      CDamageType::COLD));
    direct.Run(game, caster, target, false, 0, 0);
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain splash;
    splash.Add(std::make_unique<ABActionDamageTarget>(aoeDamage_, CAttackType::SPELLS,
                                                      CDamageType::COLD));
    splash.Add(std::make_unique<ABActionAddBuffTarget>(
        [slowDuration = slowDuration_, slowPercent = slowPercent_, casterHandle]() { return std::make_unique<CBuffSlow>(slowDuration, slowPercent, casterHandle); }));
    ABActionForEachInRange nova(radius_, std::move(splash), true);
    nova.SetExclude(target);
    nova.Run(game, caster, target, false, 0, 0);
}
void CAbilityFrostNova::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 1, damage_); aoeDamage_ = def.DataFloatAt(level, 0, aoeDamage_); }
CAbilityManaBurn::CAbilityManaBurn(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEmb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(7.0f);
    SetManaCost(50);
}
void CAbilityManaBurn::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { ABActionManaBurn burn(manaBurned_, CAttackType::SPELLS, CDamageType::MIND); burn.Run(game, caster, target, false, 0, 0); }
void CAbilityManaBurn::ApplyEffectData(const CAbilityDefFull& def, int level) { manaBurned_ = def.DataIntAt(level, 0, manaBurned_); }
CAbilityEntanglingRoots::CAbilityEntanglingRoots(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEer"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(8.0f);
    SetManaCost(75);
}
void CAbilityEntanglingRoots::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() { return std::make_unique<CBuffEnsnare>(duration); }));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, dps = damagePerSecond_, casterHandle]() { return std::make_unique<CBuffPoison>(duration, dps, casterHandle); }));
    chain.Run(game, caster, target, false, 0, 0);
}
void CAbilityEntanglingRoots::ApplyEffectData(const CAbilityDefFull& def, int level) { damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_); duration_ = def.DurationAt(level); }
// === spells2.cpp ===
CAbilityHealingWave::CAbilityHealingWave(int handleId) : CAbilitySpellBase(handleId, War3ID("AOhw"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(9.0f); SetManaCost(90); }
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
void CAbilityHealingWave::ApplyEffectData(const CAbilityDefFull& def, int level) { healAmount_ = def.DataFloatAt(level, 0, healAmount_); bounces_ = def.DataIntAt(level, 1, bounces_); }
CAbilityHex::CAbilityHex(int handleId) : CAbilitySpellBase(handleId, War3ID("AOhx"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(7.0f); SetManaCost(70); }
void CAbilityHex::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() { return std::make_unique<CBuffHex>(duration); });
}
void CAbilityHex::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); }
CAbilityChainLightning::CAbilityChainLightning(int handleId) : CAbilitySpellBase(handleId, War3ID("AOcl"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(9.0f); SetManaCost(120); }
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
CAbilityFeralSpirit::CAbilityFeralSpirit(int handleId) : CAbilitySpellBase(handleId, War3ID("AOsf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(30.0f); SetManaCost(75); }
void CAbilityFeralSpirit::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionSummon wolves(wolfType_, War3ID("AOsf"), count_, summonDuration_, 64.0f);
    wolves.Run(game, caster, nullptr, true, caster->GetX(), caster->GetY() + 32.0f);
}
void CAbilityFeralSpirit::ApplyEffectData(const CAbilityDefFull& def, int level) { count_ = def.DataIntAt(level, 1, count_); }
CAbilityEarthquake::CAbilityEarthquake(int handleId) : CAbilitySpellBase(handleId, War3ID("AOeq"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(1000.0f); SetCooldown(90.0f); SetManaCost(150); }
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
void CAbilityEarthquake::ApplyEffectData(const CAbilityDefFull& def, int level) { damagePerSecond_ = def.DataFloatAt(level, 1, damagePerSecond_); radius_ = def.AreaAt(level); duration_ = def.DurationAt(level); }
CAbilityCarrionSwarm::CAbilityCarrionSwarm(int handleId) : CAbilitySpellBase(handleId, War3ID("AUcs"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(700.0f); SetCooldown(10.0f); SetManaCost(110); }
void CAbilityCarrionSwarm::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster) return;
    CastLineDamageAndStun(game, caster, target, false, 0.0f, 0.0f,
                          range_, width_, damage_, 0.0f,
                          CAttackType::SPELLS, CDamageType::POISON);
}
void CAbilityCarrionSwarm::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); }
CAbilitySleep::CAbilitySleep(int handleId) : CAbilitySpellBase(handleId, War3ID("AUsl"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(100); }
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
void CAbilitySleep::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); }
CAbilityInferno::CAbilityInferno(int handleId) : CAbilitySpellBase(handleId, War3ID("AUin"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(900.0f); SetCooldown(180.0f); SetManaCost(175); }
void CAbilityInferno::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionStunTarget>(stunDuration_));
    ABActionForEachInRange terror(stunRadius_, std::move(perUnit), true);
    terror.Run(game, caster, nullptr, true, x, y);
    ABActionSummon infernal(infernalType_, War3ID("AUin"), 1, summonDuration_, 0.0f);
    infernal.Run(game, caster, nullptr, true, x, y);
}
void CAbilityInferno::ApplyEffectData(const CAbilityDefFull& def, int level) { summonDuration_ = def.DataFloatAt(level, 1, summonDuration_); }
CAbilitySilence::CAbilitySilence(int handleId) : CAbilitySpellBase(handleId, War3ID("ANsi"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(900.0f); SetCooldown(15.0f); SetManaCost(75); }
void CAbilitySilence::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastPointAreaBuff(game, caster, x, y, radius_, [duration = duration_]() { return std::make_unique<CBuffSilence>(duration); });
}
void CAbilitySilence::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); radius_ = def.AreaAt(level); }
CAbilityBlackArrow::CAbilityBlackArrow(int handleId) : CAbilitySpellBase(handleId, War3ID("ANba"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(2.0f); SetManaCost(10); }
void CAbilityBlackArrow::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster) return;
    ABActionDamageTarget shot(bonusDamage_, CAttackType::PIERCE, CDamageType::NORMAL,
                              true, true);
    shot.Run(game, caster, target, false, 0, 0);
}
void CAbilityBlackArrow::ApplyEffectData(const CAbilityDefFull& def, int level) { bonusDamage_ = def.DataFloatAt(level, 0, bonusDamage_); }
CAbilityLifeDrain::CAbilityLifeDrain(int handleId) : CAbilitySpellBase(handleId, War3ID("ANdr"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(8.0f); SetManaCost(75); }
void CAbilityLifeDrain::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    const int casterHandle = caster->GetHandleId();
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, tick = drainPerSecond_, casterHandle]() { return std::make_unique<CBuffLifeDrain>(duration, tick, casterHandle); });
}
void CAbilityLifeDrain::ApplyEffectData(const CAbilityDefFull& def, int level) { drainPerSecond_ = def.DataFloatAt(level, 0, drainPerSecond_); duration_ = def.DurationAt(level); }
CAbilityFingerOfDeath::CAbilityFingerOfDeath(int handleId) : CAbilitySpellBase(handleId, War3ID("ANfd"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(800.0f); SetCooldown(15.0f); SetManaCost(0); }
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
void CAbilityFingerOfDeath::ApplyEffectData(const CAbilityDefFull& def, int level) { heroDamage_ = def.DataFloatAt(level, 2, heroDamage_); }
CAbilityDarkPortal::CAbilityDarkPortal(int handleId) : CAbilitySpellBase(handleId, War3ID("ANdp"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(800.0f); SetCooldown(100.0f); SetManaCost(0); }
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
void CAbilityDarkPortal::ApplyEffectData(const CAbilityDefFull& def, int level) { summonCount_ = def.DataIntAt(level, 1, summonCount_); }
CAbilityRainOfChaos::CAbilityRainOfChaos(int handleId) : CAbilitySpellBase(handleId, War3ID("ANrc"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(1000.0f); SetCooldown(120.0f); SetManaCost(0); }
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
void CAbilityRainOfChaos::ApplyEffectData(const CAbilityDefFull& def, int level) { radius_ = def.AreaAt(level); }
// === spells3.cpp ===
CAbilityMassTeleport::CAbilityMassTeleport(int handleId) : CAbilitySpellBase(handleId, War3ID("AHmt"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(1000.0f); SetCooldown(30.0f); SetManaCost(100); }
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
void CAbilityMassTeleport::ApplyEffectData(const CAbilityDefFull& def, int level) { castingDelay_ = def.DataFloatAt(level, 1, castingDelay_); }
CAbilitySummonWaterElemental::CAbilitySummonWaterElemental(int handleId) : CAbilitySpellBase(handleId, War3ID("AHwe"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(30.0f); SetManaCost(75); }
void CAbilitySummonWaterElemental::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionSummon elemental(elementalType_, War3ID("AHwe"), count_,
                             summonDuration_, 64.0f);
    elemental.Run(game, caster, nullptr, true, caster->GetX(), caster->GetY() + 32.0f);
}
CAbilityResurrect::CAbilityResurrect(int handleId) : CAbilitySpellBase(handleId, War3ID("AHre"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(800.0f); SetCooldown(180.0f); SetManaCost(175); }
void CAbilityResurrect::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionReviveTarget>());
    ABActionForEachInRange resurrection(radius_, std::move(perUnit), false, maxTargets_);
    resurrection.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    resurrection.SetLifeFilter(ABLifeFilter::DEAD);
    resurrection.Run(game, caster, nullptr, true, x, y);
}
CAbilityBlink::CAbilityBlink(int handleId) : CAbilitySpellBase(handleId, War3ID("AEbl"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(1000.0f); SetCooldown(5.0f); SetManaCost(10); }
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
void CAbilityBlink::ApplyEffectData(const CAbilityDefFull& def, int level) { maxRange_ = def.DataFloatAt(level, 0, maxRange_); }
CAbilityMirrorImage::CAbilityMirrorImage(int handleId) : CAbilitySpellBase(handleId, War3ID("AOmi"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(20.0f); SetManaCost(50); }
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
void CAbilityMirrorImage::ApplyEffectData(const CAbilityDefFull& def, int level) { count_ = def.DataIntAt(level, 0, count_); }
CAbilityShockwave::CAbilityShockwave(int handleId) : CAbilitySpellBase(handleId, War3ID("AOsh"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(700.0f); SetCooldown(8.0f); SetManaCost(100); }
void CAbilityShockwave::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { if (!target || !caster) return; CastLineDamageAndStun(game, caster, target, false, 0.0f, 0.0f, range_, width_, damage_, 0.0f); }
void CAbilityShockwave::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); }
CAbilityStarfall::CAbilityStarfall(int handleId) : CAbilitySpellBase(handleId, War3ID("AEsf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(180.0f); SetManaCost(200); }
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
CAbilityTranquility::CAbilityTranquility(int handleId) : CAbilitySpellBase(handleId, War3ID("AEtq"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(60.0f); SetManaCost(125); }
void CAbilityTranquility::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionHealTarget>(healPerTick_ * duration_));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, tick = healPerTick_]() { return std::make_unique<CBuffRejuvenation>(duration, tick); }));
    ABActionForEachInRange calm(radius_, std::move(perUnit), true);
    calm.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    calm.Run(game, caster, nullptr, false, 0, 0);
}
void CAbilityTranquility::ApplyEffectData(const CAbilityDefFull& def, int level) { healPerTick_ = def.DataFloatAt(level, 0, healPerTick_); radius_ = def.AreaAt(level); }
CAbilityReincarnation::CAbilityReincarnation(int handleId, War3ID code, int manaCost) : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL), manaCost_(manaCost) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(0.0f); SetManaCost(0); }
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
CAbilityCharm::CAbilityCharm(int handleId) : CAbilitySpellBase(handleId, War3ID("ANch"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(7.0f); SetManaCost(70); }
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
void CAbilityCharm::ApplyEffectData(const CAbilityDefFull& def, int level) { maxLevel_ = def.DataIntAt(level, 0, maxLevel_); }
// === spells4.cpp ===
CAbilityEvasion::CAbilityEvasion(int handleId) : CAbilitySpellBase(handleId, War3ID("AEev"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityEvasion::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->AddEvasionSource(War3ID("AEev"), chance_); }
void CAbilityEvasion::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->RemoveEvasionSource(War3ID("AEev")); }
void CAbilityEvasion::ApplyEffectData(const CAbilityDefFull& def, int level) { chance_ = def.DataFloatAt(level, 0, chance_); }
CAbilityCriticalStrike::CAbilityCriticalStrike(int handleId) : CAbilitySpellBase(handleId, War3ID("AOcr"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityCriticalStrike::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->AddCritSource(GetAlias(), chance_, multiplier_); }
void CAbilityCriticalStrike::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->RemoveCritSource(GetAlias()); }
void CAbilityCriticalStrike::ApplyEffectData(const CAbilityDefFull& def, int level) { chance_ = def.DataFloatAt(level, 0, chance_ * 100.0f) / 100.0f; multiplier_ = def.DataFloatAt(level, 1, multiplier_); }
CAbilityBash::CAbilityBash(int handleId) : CAbilitySpellBase(handleId, War3ID("AHbh"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityBash::ApplyEffectData(const CAbilityDefFull& def, int level) { chance_ = def.DataFloatAt(level, 0, chance_ * 100.0f) / 100.0f; duration_ = def.DurationAt(level); }
CAbilitySlow::CAbilitySlow(int handleId) : CAbilitySpellBase(handleId, War3ID("Aslo"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(50); }
void CAbilitySlow::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, percent = slowPercent_, casterHandle]() { return std::make_unique<CBuffSlow>(duration, percent, casterHandle); });
}
void CAbilitySlow::ApplyEffectData(const CAbilityDefFull& def, int level) { slowPercent_ = def.DataFloatAt(level, 0, slowPercent_); duration_ = def.DurationAt(level); }
CAbilityFrostArmor::CAbilityFrostArmor(int handleId) : CAbilitySpellBase(handleId, War3ID("AUfa"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(5.0f); SetManaCost(40); }
void CAbilityFrostArmor::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_, bonus = armorBonus_]() {
        return std::make_unique<CBuffArmor>(War3ID("BUfa"), duration, bonus, true);
    });
}
void CAbilityFrostArmor::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); armorBonus_ = def.DataIntAt(level, 1, armorBonus_); }
CAbilityFaerieFire::CAbilityFaerieFire(int handleId) : CAbilitySpellBase(handleId, War3ID("Afae"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(4.0f); SetManaCost(30); }
void CAbilityFaerieFire::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_, penalty = armorPenalty_]() {
        return std::make_unique<CBuffArmor>(War3ID("Bfae"), duration, penalty, false);
    });
}
void CAbilityFaerieFire::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); armorPenalty_ = -def.DataIntAt(level, 0, -armorPenalty_); }
CAbilityDeathPact::CAbilityDeathPact(int handleId) : CAbilitySpellBase(handleId, War3ID("AUdp"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(10.0f); SetManaCost(25); }
void CAbilityDeathPact::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
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
CAbilityAnimateDead::CAbilityAnimateDead(int handleId) : CAbilitySpellBase(handleId, War3ID("AUan"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(800.0f); SetCooldown(30.0f); SetManaCost(75); }
void CAbilityAnimateDead::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAnimateDeadCorpse>(War3ID("AUan"), duration_));
    ABActionForEachInRange raising(radius_, std::move(perUnit));
    raising.SetLifeFilter(ABLifeFilter::DEAD);
    raising.Run(game, caster, nullptr, true, x, y);
}
CAbilityDoom::CAbilityDoom(int handleId) : CAbilitySpellBase(handleId, War3ID("ANdo"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(650.0f); SetCooldown(120.0f); SetManaCost(150); }
void CAbilityDoom::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, dps = damagePerSecond_, casterHandle]() { return std::make_unique<CBuffDoom>(duration, dps, casterHandle); });
}
void CAbilityDoom::ApplyEffectData(const CAbilityDefFull& def, int level) { damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_); duration_ = def.DataFloatAt(level, 2, duration_); }
CAbilityVolcano::CAbilityVolcano(int handleId) : CAbilitySpellBase(handleId, War3ID("ANvc"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(800.0f); SetCooldown(180.0f); SetManaCost(200); }
void CAbilityVolcano::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        const float wx = x + game.SimRandRange(-radius_, radius_);
        const float wy = y + game.SimRandRange(-radius_, radius_);
        CastPointDamageWaves(game, caster, wx, wy, 1, radius_ / 2.0f, damage_,
                             CAttackType::SPELLS, CDamageType::FIRE);
    }
}
void CAbilityVolcano::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 4, damage_); radius_ = def.AreaAt(level); }
// === spells5.cpp ===
CAbilityFlameStrike::CAbilityFlameStrike(int handleId) : CAbilitySpellBase(handleId, War3ID("AHfs"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(700.0f); SetCooldown(9.0f); SetManaCost(75); }
void CAbilityFlameStrike::DoEffect(CSimulation& game, CUnit* caster, float x, float y) { CastPointDamageWaves(game, caster, x, y, waves_, radius_, damagePerWave_, CAttackType::SPELLS, CDamageType::FIRE); }
CAbilityDeathAndDecay::CAbilityDeathAndDecay(int handleId) : CAbilitySpellBase(handleId, War3ID("AUdd"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(700.0f); SetCooldown(60.0f); SetManaCost(125); }
void CAbilityDeathAndDecay::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (int w = 0; w < waves_; w++) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTargetMaxLifePct>(
            damageFraction_, CAttackType::SPELLS, CDamageType::DEATH));
        ABActionForEachInRange wave(radius_, std::move(perUnit));
        wave.Run(game, caster, nullptr, true, x, y);
    }
}
void CAbilityDeathAndDecay::ApplyEffectData(const CAbilityDefFull& def, int level) { damageFraction_ = def.DataFloatAt(level, 0, damageFraction_); radius_ = def.AreaAt(level); }
CAbilityImpale::CAbilityImpale(int handleId) : CAbilitySpellBase(handleId, War3ID("AUim"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(700.0f); SetCooldown(9.0f); SetManaCost(75); }
void CAbilityImpale::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    CastLineDamageAndStun(game, caster, target, false, 0.0f, 0.0f,
                          range_, width_, damage_, stunDuration_);
}
CAbilityFanOfKnives::CAbilityFanOfKnives(int handleId) : CAbilitySpellBase(handleId, War3ID("AEfk"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(8.0f); SetManaCost(75); }
void CAbilityFanOfKnives::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaDamageWaves(game, caster, 1, radius_, damage_,
                              CAttackType::SPELLS, CDamageType::NORMAL,
                              ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
CAbilityRainOfFire::CAbilityRainOfFire(int handleId) : CAbilitySpellBase(handleId, War3ID("ANrf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(700.0f); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityRainOfFire::DoEffect(CSimulation& game, CUnit* caster, float x, float y) { CastPointDamageWaves(game, caster, x, y, waves_, radius_, damagePerWave_, CAttackType::SPELLS, CDamageType::FIRE); }
CAbilitySpikedCarapace::CAbilitySpikedCarapace(int handleId) : CAbilitySpellBase(handleId, War3ID("AUts"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilitySpikedCarapace::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->AddBonusArmor(armorBonus_); }
void CAbilitySpikedCarapace::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->AddBonusArmor(-armorBonus_); }
CAbilityDarkRitual::CAbilityDarkRitual(int handleId) : CAbilitySpellBase(handleId, War3ID("AUdr"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(10.0f); SetManaCost(25); }
void CAbilityDarkRitual::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
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
CAbilityShadowStrike::CAbilityShadowStrike(int handleId) : CAbilitySpellBase(handleId, War3ID("AEsh"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(65); }
void CAbilityShadowStrike::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                     CDamageType::POISON));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = slowDuration_, percent = slowPercent_, casterHandle]() { return std::make_unique<CBuffSlow>(duration, percent, casterHandle); }));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = poisonDuration_, tick = poisonPerTick_, casterHandle]() { return std::make_unique<CBuffPoison>(duration, tick, casterHandle); }));
    chain.Run(game, caster, target, false, 0, 0);
}
// === spells6.cpp ===
CAbilityHealingSpray::CAbilityHealingSpray(int handleId) : CAbilitySpellBase(handleId, War3ID("ANhs"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(700.0f); SetCooldown(9.0f); SetManaCost(75); }
void CAbilityHealingSpray::DoEffect(CSimulation& game, CUnit* caster, float x, float y) { CastPointAreaHeal(game, caster, x, y, radius_, healAmount_); }
CAbilityAcidBomb::CAbilityAcidBomb(int handleId) : CAbilitySpellBase(handleId, War3ID("ANab"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityAcidBomb::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, penalty = armorPenalty_]() {
            return std::make_unique<CBuffArmor>(War3ID("Bacb"), duration, penalty, false);
        }));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, tick = poisonPerTick_, casterHandle]() { return std::make_unique<CBuffPoison>(duration, tick, casterHandle); }));
    chain.Run(game, caster, target, false, 0, 0);
}
CAbilityChemicalRage::CAbilityChemicalRage(int handleId) : CAbilitySpellBase(handleId, War3ID("ANcr"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(20.0f); SetManaCost(50); }
void CAbilityChemicalRage::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster,
                   [duration = duration_, speed = speedBonus_, attack = attackSpeedBonus_]() { return std::make_unique<CBuffBloodlust>(duration, speed, attack); });
}
CAbilityTransmute::CAbilityTransmute(int handleId) : CAbilitySpellBase(handleId, War3ID("ANtm"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(10.0f); SetManaCost(25); }
void CAbilityTransmute::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
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
CAbilityBloodlustCast::CAbilityBloodlustCast(int handleId) : CAbilitySpellBase(handleId, War3ID("Ablo"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilityBloodlustCast::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABEffectChain haste;
    haste.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, speed = speedBonus_, attack = attackSpeedBonus_]() { return std::make_unique<CBuffBloodlust>(duration, speed, attack); }));
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
CAbilityPurge::CAbilityPurge(int handleId) : CAbilitySpellBase(handleId, War3ID("Aprg"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(50); }
void CAbilityPurge::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
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
        [slowDuration = slowDuration_, slowPercent = slowPercent_, casterHandle]() { return std::make_unique<CBuffSlow>(slowDuration, slowPercent, casterHandle); }));
    chain.Run(game, caster, target, false, 0, 0);
}
CAbilityLightningShield::CAbilityLightningShield(int handleId) : CAbilitySpellBase(handleId, War3ID("Alsh"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(75); }
void CAbilityLightningShield::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, tick = damagePerTick_, radius = radius_]() { return std::make_unique<CBuffLightningShield>(duration, tick, radius); });
}
CAbilityRaiseDead::CAbilityRaiseDead(int handleId) : CAbilitySpellBase(handleId, War3ID("Arai"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(12.0f); SetManaCost(75); }
void CAbilityRaiseDead::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionCreateUnitAtTarget>(skeletonType_));
    ABActionForEachInRange raising(radius_, std::move(perUnit), false, maxTargets_);
    raising.SetLifeFilter(ABLifeFilter::DEAD);
    raising.Run(game, caster, nullptr, false, 0, 0);
}
void CAbilityRaiseDead::ApplyEffectData(const CAbilityDefFull& def, int level) { maxTargets_ = def.DataIntAt(level, 0, maxTargets_); }
// === spells7.cpp ===
CAbilitySerpentWard::CAbilitySerpentWard(int handleId) : CAbilitySpellBase(handleId, War3ID("AOsw"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(500.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilitySerpentWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) { if (!caster) return; ABActionCreateUnitAtPoint ward(wardType_, x, y, 0); ward.Run(game, caster, nullptr, true, x, y); }
CAbilityPolymorph::CAbilityPolymorph(int handleId) : CAbilitySpellBase(handleId, War3ID("Aply"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(50); }
void CAbilityPolymorph::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() { return std::make_unique<CBuffPolymorph>(duration); });
}
void CAbilityPolymorph::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); }
CAbilityInnerFire::CAbilityInnerFire(int handleId) : CAbilitySpellBase(handleId, War3ID("Ainf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(35); }
void CAbilityInnerFire::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, damage = bonusDamage_, armor = bonusArmor_]() { return std::make_unique<CBuffInnerFire>(duration, damage, armor); });
}
CAbilityHeal::CAbilityHeal(int handleId) : CAbilitySpellBase(handleId, War3ID("Ahea"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(5.0f); SetManaCost(65); }
void CAbilityHeal::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { ABActionHealTarget heal(healAmount_); heal.Run(game, caster, target, false, 0, 0); }
CAbilityDispelMagic::CAbilityDispelMagic(int handleId) : CAbilitySpellBase(handleId, War3ID("Adis"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(75); }
void CAbilityDispelMagic::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain burn;
    burn.Add(std::make_unique<ABActionDamageTarget>(summonDamage_, CAttackType::SPELLS,
                                                    CDamageType::MAGIC));
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionIf>(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* target, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(target);
                return unit != nullptr && unit->IsSummoned();
            }),
        std::move(burn)));
    perUnit.Add(std::make_unique<ABActionDispelTarget>(true, true));
    ABActionForEachInRange dispel(radius_, std::move(perUnit));
    dispel.Run(game, caster, nullptr, true, x, y);
}
void CAbilityDispelMagic::ApplyEffectData(const CAbilityDefFull& def, int level) { radius_ = def.AreaAt(level); summonDamage_ = def.DataFloatAt(level, 1, summonDamage_); }
CAbilityRejuvenation::CAbilityRejuvenation(int handleId) : CAbilitySpellBase(handleId, War3ID("Arej"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(50); }
void CAbilityRejuvenation::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_, tick = healPerTick_]() { return std::make_unique<CBuffRejuvenation>(duration, tick); });
}
CAbilityRoar::CAbilityRoar(int handleId) : CAbilitySpellBase(handleId, War3ID("Aroa"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(10.0f); SetManaCost(50); }
void CAbilityRoar::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, damage = bonusDamage_]() {
                           return std::make_unique<CBuffInnerFire>(duration, damage, 0, War3ID("Broa"));
                       });
}
void CAbilityRoar::ApplyEffectData(const CAbilityDefFull& def, int level) { radius_ = def.AreaAt(level); }
CAbilityDefend::CAbilityDefend(int handleId) : CAbilitySpellBase(handleId, War3ID("Adef"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(5.0f); SetManaCost(0); }
void CAbilityDefend::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() { return std::make_unique<CBuffDefend>(duration); });
}
// === spells8.cpp ===
CAbilityItemHealGroup::CAbilityItemHealGroup(int handleId) : CAbilitySpellBase(handleId, War3ID("AIha"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(700.0f); }
void CAbilityItemHealGroup::DoEffect(CSimulation& game, CUnit* caster, float x, float y) { CastPointAreaHeal(game, caster, x, y, radius_, healAmount_); }
CAbilityItemTownPortal::CAbilityItemTownPortal(int handleId) : CAbilitySpellBase(handleId, War3ID("AItp"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(99999.0f); }
void CAbilityItemTownPortal::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain group;
    group.Add(std::make_unique<ABActionTeleportTarget>(x, y));
    ABActionForEachInRange portal(radius_, std::move(group));
    portal.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    portal.Run(game, caster, nullptr, false, 0, 0);
    caster->SetPosition(x, y);
}
CAbilityItemInvulnerable::CAbilityItemInvulnerable(int handleId) : CAbilitySpellBase(handleId, War3ID("AIvl"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemInvulnerable::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() { return std::make_unique<CBuffDivineShield>(duration); });
}
CAbilityItemHealSelf::CAbilityItemHealSelf(int handleId) : CAbilitySpellBase(handleId, War3ID("AIh1"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemHealSelf::DoEffect(CSimulation& game, CUnit* caster) { if (!caster) return; ABActionHealTarget heal(healAmount_); heal.Run(game, caster, caster, false, 0, 0); }
CAbilityItemManaSelf::CAbilityItemManaSelf(int handleId) : CAbilitySpellBase(handleId, War3ID("AIm1"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemManaSelf::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    float mana = std::min(caster->GetMana() + manaAmount_, static_cast<float>(caster->GetMaxMana()));
    ABActionSetManaTarget setMana(mana);
    setMana.Run(game, caster, caster, false, 0, 0);
}
CAbilityItemClarity::CAbilityItemClarity(int handleId) : CAbilitySpellBase(handleId, War3ID("AIpl"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemClarity::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_, regen = regenBonus_]() { return std::make_unique<CBuffClarity>(duration, regen); });
}
CAbilityItemManaSteal::CAbilityItemManaSteal(int handleId) : CAbilitySpellBase(handleId, War3ID("Aste"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); }
void CAbilityItemManaSteal::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { ABActionManaSteal steal(stealAmount_); steal.Run(game, caster, target, false, 0, 0); }
CAbilityItemProtection::CAbilityItemProtection(int handleId) : CAbilitySpellBase(handleId, War3ID("AIda"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemProtection::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_, bonus = armorBonus_]() {
        return std::make_unique<CBuffArmor>(War3ID("AIda"), duration, bonus, true);
    });
}
// === spells9.cpp ===
CAbilityOrbOfFrost::CAbilityOrbOfFrost(int handleId) : CAbilitySpellBase(handleId, War3ID("AIob"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
namespace {
class OrbFrostEffect : public CAttackModifierEffect {
public:
    OrbFrostEffect(float duration, float slowPercent)
        : duration_(duration), slowPercent_(slowPercent) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override {
        (void)damageDealt;
        if (!victim) return;
        victim->AddBuff(std::make_unique<CBuffSlow>(duration_, slowPercent_,
                                                    attacker ? attacker->GetHandleId() : -1),
                        game);
    }
private:
    float duration_;
    float slowPercent_;
};
class OrbCorruptionEffect : public CAttackModifierEffect {
public:
    OrbCorruptionEffect(float duration, int armorPenalty)
        : duration_(duration), armorPenalty_(armorPenalty) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override {
        (void)attacker;
        (void)damageDealt;
        if (!victim) return;
        victim->AddBuff(std::make_unique<CBuffArmor>(War3ID("AIcb"), duration_, armorPenalty_, false),
                        game);
    }
private:
    float duration_;
    int armorPenalty_;
};
}
void CAbilityOrbOfFrost::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetAttackModifier(0, War3ID("AIob"), std::make_unique<OrbFrostEffect>(duration_, slowPercent_)); }
void CAbilityOrbOfFrost::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->ClearAttackModifier(0, War3ID("AIob")); }
CAbilityOrbOfCorruption::CAbilityOrbOfCorruption(int handleId) : CAbilitySpellBase(handleId, War3ID("AIcb"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityOrbOfCorruption::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetAttackModifier(0, War3ID("AIcb"), std::make_unique<OrbCorruptionEffect>(duration_, armorPenalty_)); }
void CAbilityOrbOfCorruption::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->ClearAttackModifier(0, War3ID("AIcb")); }
CAbilityOrbOfFire::CAbilityOrbOfFire(int handleId) : CAbilitySpellBase(handleId, War3ID("AIfb"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityOrbOfFire::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetAttackModifier(0, War3ID("AIfb"), std::make_unique<CAttackBonusDamageEffect>(bonusDamage_)); }
void CAbilityOrbOfFire::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->ClearAttackModifier(0, War3ID("AIfb")); }
CAbilityIllusion::CAbilityIllusion(int handleId) : CAbilitySpellBase(handleId, War3ID("AIil"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); }
void CAbilityIllusion::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { ABActionSummonCopyOfTarget illusion(War3ID("AIil"), duration_); illusion.Run(game, caster, target, false, 0, 0); }
CAbilityLightningShieldWand::CAbilityLightningShieldWand(int handleId) : CAbilitySpellBase(handleId, War3ID("AIls"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); }
void CAbilityLightningShieldWand::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, tick = damagePerTick_, radius = radius_]() { return std::make_unique<CBuffLightningShield>(duration, tick, radius); });
}
CAbilityBeastScroll::CAbilityBeastScroll(int handleId) : CAbilitySpellBase(handleId, War3ID("AIrr"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityBeastScroll::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, damage = bonusDamage_]() { return std::make_unique<CBuffInnerFire>(duration, damage, 0); });
}
CAbilityHealingWard::CAbilityHealingWard(int handleId) : CAbilitySpellBase(handleId, War3ID("AIhw"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(500.0f); }
void CAbilityHealingWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABActionCreateUnitWithBuff ward(
        wardType_, x, y,
        {ABActionCreateUnitWithBuff::BuffFactory(
            [duration = auraDuration_, tick = healPerTick_, radius = radius_]() { return std::make_unique<CBuffHealingWard>(duration, tick, radius); })});
    ward.Run(game, caster, nullptr, true, x, y);
}
// === spells10.cpp ===
CAbilityCleavingAttack::CAbilityCleavingAttack(int handleId) : CAbilitySpellBase(handleId, War3ID("ANca"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
CAbilitySearingArrows::CAbilitySearingArrows(int handleId) : CAbilitySpellBase(handleId, War3ID("AHfa"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilitySearingArrows::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetAttackModifier(0, War3ID("AHfa"), std::make_unique<CAttackBonusDamageEffect>(bonusDamage_)); }
void CAbilitySearingArrows::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->ClearAttackModifier(0, War3ID("AHfa")); }
CAbilityEnsnare::CAbilityEnsnare(int handleId) : CAbilitySpellBase(handleId, War3ID("Aens"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilityEnsnare::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() { return std::make_unique<CBuffEnsnare>(duration); });
}
CAbilityAncestralSpirit::CAbilityAncestralSpirit(int handleId) : CAbilitySpellBase(handleId, War3ID("Aast"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(12.0f); SetManaCost(75); }
void CAbilityAncestralSpirit::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { ABActionAncestralRevive spirit; spirit.Run(game, caster, target, false, 0, 0); }
CAbilityHealingWardUnit::CAbilityHealingWardUnit(int handleId) : CAbilitySpellBase(handleId, War3ID("Ahwd"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(500.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilityHealingWardUnit::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABActionCreateUnitWithBuff ward(
        wardType_, x, y,
        {ABActionCreateUnitWithBuff::BuffFactory(
            [duration = auraDuration_, tick = healPerTick_, radius = radius_]() { return std::make_unique<CBuffHealingWard>(duration, tick, radius); })});
    ward.Run(game, caster, nullptr, true, x, y);
}
CAbilityUnstableConcoction::CAbilityUnstableConcoction(int handleId) : CAbilitySpellBase(handleId, War3ID("Auco"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityUnstableConcoction::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    CastCasterAreaDamageWaves(game, caster, 1, radius_, damage_,
                              CAttackType::SPELLS, CDamageType::FIRE,
                              ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    ABActionDamageTarget selfHit(selfDamage_, CAttackType::SPELLS, CDamageType::FIRE);
    selfHit.Run(game, caster, caster, false, 0, 0);
}
CAbilityAbolishMagic::CAbilityAbolishMagic(int handleId) : CAbilitySpellBase(handleId, War3ID("Aadm"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(75); }
void CAbilityAbolishMagic::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDispelTarget>(true, true));
    ABActionForEachInRange strip(radius_, std::move(perUnit));
    strip.Run(game, caster, nullptr, true, x, y);
}
CAbilityClusterRockets::CAbilityClusterRockets(int handleId) : CAbilitySpellBase(handleId, War3ID("ANcs"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(800.0f); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityClusterRockets::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    CastPointDamageWaves(game, caster, x - 75.0f, y, 1, radius_, damage_,
                         CAttackType::SPELLS, CDamageType::FIRE,
                         ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    CastPointDamageWaves(game, caster, x + 75.0f, y, 1, radius_, damage_,
                         CAttackType::SPELLS, CDamageType::FIRE,
                         ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
// === spells11.cpp ===
CAbilityWindWalk::CAbilityWindWalk(int handleId) : CAbilitySpellBase(handleId, War3ID("AOwk"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityWindWalk::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_, backstab = backstabDamage_]() { return std::make_unique<CBuffWindWalk>(duration, backstab); });
}
CAbilityInvisibility::CAbilityInvisibility(int handleId) : CAbilitySpellBase(handleId, War3ID("Aivs"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilityInvisibility::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() { return std::make_unique<CBuffWindWalk>(duration, 0.0f); });
}
CAbilityPermanentInvisibility::CAbilityPermanentInvisibility(int handleId) : CAbilitySpellBase(handleId, War3ID("Apiv"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityPermanentInvisibility::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetInvisible(true); }
void CAbilityPermanentInvisibility::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetInvisible(false); }
CAbilityShadowmeld::CAbilityShadowmeld(int handleId) : CAbilitySpellBase(handleId, War3ID("Ashm"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityShadowmeld::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (!unit) return;
    lastX_ = unit->GetX();
    lastY_ = unit->GetY();
    tracking_ = true;
}
void CAbilityShadowmeld::OnRemove(CSimulation& game, CUnit* unit) { (void)game; tracking_ = false; if (unit) unit->SetInvisible(false); }
void CAbilityShadowmeld::OnTick(CSimulation& game, CUnit* unit) {
    (void)game;
    if (!unit || !tracking_) return;
    const float dx = unit->GetX() - lastX_;
    const float dy = unit->GetY() - lastY_;
    lastX_ = unit->GetX();
    lastY_ = unit->GetY();
    unit->SetInvisible(dx * dx + dy * dy < 1.0f);
}
CAbilityDustOfAppearance::CAbilityDustOfAppearance(int handleId) : CAbilitySpellBase(handleId, War3ID("AItb"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(600.0f); }
void CAbilityDustOfAppearance::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastPointAreaBuff(game, caster, x, y, radius_, [duration = duration_]() {
        return std::make_unique<CBuffArmor>(War3ID("Bfae"), duration, 0, false);
    }, ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
CAbilityBerserk::CAbilityBerserk(int handleId) : CAbilitySpellBase(handleId, War3ID("Absk"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(10.0f); SetManaCost(50); }
void CAbilityBerserk::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster,
                   [duration = duration_, attack = attackSpeedMul_, taken = damageTakenMul_]() { return std::make_unique<CBuffBerserk>(duration, attack, taken); });
}
CAbilitySummonBear::CAbilitySummonBear(int handleId) : CAbilitySpellBase(handleId, War3ID("ANsg"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(30.0f); SetManaCost(75); }
void CAbilitySummonBear::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionCreateUnitAtPoint bear(bearType_, caster->GetX() + 64.0f,
                                   caster->GetY(), 0);
    bear.Run(game, caster, nullptr, false, 0, 0);
}
void CAbilitySummonBear::ApplyEffectData(const CAbilityDefFull& def, int level) { bearType_ = def.UnitIdAt(level, bearType_); }
CAbilityBattleRoar::CAbilityBattleRoar(int handleId) : CAbilitySpellBase(handleId, War3ID("ANbr"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(10.0f); SetManaCost(50); }
void CAbilityBattleRoar::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, damage = bonusDamage_]() { return std::make_unique<CBuffInnerFire>(duration, damage, 0); });
}
void CAbilityBattleRoar::ApplyEffectData(const CAbilityDefFull& def, int level) { radius_ = def.AreaAt(level); }
// === spells12.cpp ===
CAbilityTrueSight::CAbilityTrueSight(int handleId) : CAbilitySpellBase(handleId, War3ID("Atru"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityTrueSight::OnAdd(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetDetector(true); }
void CAbilityTrueSight::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->SetDetector(false); }
CAbilitySentryWard::CAbilitySentryWard(int handleId) : CAbilitySpellBase(handleId, War3ID("Aeye"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(500.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilitySentryWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABActionCreateUnitWithAbility ward(
        wardType_, x, y,
        [](CSimulation&, CUnit* newborn) -> std::unique_ptr<CAbility> {
            auto detect = std::make_unique<CAbilityTrueSight>(newborn->GetHandleId());
            detect->SetAlias(War3ID("Adet"));
            return detect;
        });
    ward.Run(game, caster, nullptr, true, x, y);
}
CAbilityFlare::CAbilityFlare(int handleId) : CAbilitySpellBase(handleId, War3ID("Afla"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(1000.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilityFlare::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastPointAreaBuff(game, caster, x, y, radius_, [duration = duration_]() {
        return std::make_unique<CBuffArmor>(War3ID("Bfae"), duration, 0, false);
    }, ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
CAbilityBurrow::CAbilityBurrow(int handleId) : CAbilitySpellBase(handleId, War3ID("Abur"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(5.0f); }
void CAbilityBurrow::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() { return std::make_unique<CBuffBurrow>(duration); });
}
CAbilityCannibalize::CAbilityCannibalize(int handleId) : CAbilitySpellBase(handleId, War3ID("Acan"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(300.0f); SetCooldown(5.0f); }
void CAbilityCannibalize::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || !target->IsDead()) return;
    auto* corpse = dynamic_cast<CUnit*>(target);
    if (!corpse || corpse == caster) return;
    const float heal = healAmount_;
    ABEffectChain feast;
    feast.Add(std::make_unique<ABActionHealCaster>(heal));
    feast.Add(std::make_unique<ABActionRemoveTarget>());
    ABActionIf cannibal(
        std::make_shared<ABCondFunc>(
            [caster](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && unit->IsDead() && unit != caster;
            }),
        std::move(feast));
    cannibal.Run(game, caster, target, false, 0, 0);
}
CAbilityCarrionBeetles::CAbilityCarrionBeetles(int handleId) : CAbilitySpellBase(handleId, War3ID("AUcb"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(10.0f); SetManaCost(50); }
void CAbilityCarrionBeetles::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || !target->IsDead()) return;
    ABEffectChain rise;
    rise.Add(std::make_unique<ABActionCreateUnitAtTarget>(beetleType_, count_, 64.0f));
    ABActionIf beetles(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* corpse = dynamic_cast<CUnit*>(unitTarget);
                return corpse != nullptr && corpse->IsDead();
            }),
        std::move(rise));
    beetles.Run(game, caster, target, false, 0, 0);
}
CAbilityWeb::CAbilityWeb(int handleId) : CAbilitySpellBase(handleId, War3ID("Aweb"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilityWeb::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABEffectChain snare;
    snare.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() { return std::make_unique<CBuffEnsnare>(duration); }));
    ABActionIf web(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* target, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(target);
                if (!unit || unit->IsDead() || !unit->GetUnitType()) return false;
                return unit->GetUnitType()->GetMovementType() == PathingGridMovementType::FLY;
            }),
        std::move(snare));
    web.Run(game, caster, target, false, 0, 0);
}
// === spells13.cpp ===
namespace {
void CastTransformOnSelf(CSimulation& game, CUnit* caster,
                         War3ID buffId, float duration, War3ID formId) {
    CAbilitySpellBase::CastBuffOnSelf(game, caster, [buffId, duration, formId]() { return std::make_unique<CBuffTransform>(buffId, duration, formId); });
}
}
CAbilityMetamorphosis::CAbilityMetamorphosis(int handleId) : CAbilitySpellBase(handleId, War3ID("AEme"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(180.0f); SetManaCost(150); }
void CAbilityMetamorphosis::DoEffect(CSimulation& game, CUnit* caster) { CastTransformOnSelf(game, caster, War3ID("BEme"), duration_, War3ID("Edem")); }
CAbilityBearForm::CAbilityBearForm(int handleId) : CAbilitySpellBase(handleId, War3ID("Abrf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(25.0f); }
void CAbilityBearForm::DoEffect(CSimulation& game, CUnit* caster) { CastTransformOnSelf(game, caster, War3ID("Abrf"), duration_, War3ID("edcm")); }
CAbilityRavenForm::CAbilityRavenForm(int handleId) : CAbilitySpellBase(handleId, War3ID("Arav"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(25.0f); }
void CAbilityRavenForm::DoEffect(CSimulation& game, CUnit* caster) { CastTransformOnSelf(game, caster, War3ID("Arav"), duration_, War3ID("edot")); }
CAbilityRoboGoblin::CAbilityRoboGoblin(int handleId) : CAbilitySpellBase(handleId, War3ID("ANrg"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(1.0f); SetManaCost(25); }
void CAbilityRoboGoblin::DoEffect(CSimulation& game, CUnit* caster) { CastTransformOnSelf(game, caster, War3ID("ANrg"), duration_, War3ID("Ntin")); }
CAbilityStoneForm::CAbilityStoneForm(int handleId) : CAbilitySpellBase(handleId, War3ID("Astn"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(30.0f); }
void CAbilityStoneForm::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() { return std::make_unique<CBuffStoneForm>(duration); });
}
CAbilityAvatar::CAbilityAvatar(int handleId) : CAbilitySpellBase(handleId, War3ID("AHav"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(180.0f); SetManaCost(150); }
void CAbilityAvatar::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster,
                   [duration = duration_, damage = bonusDamage_, life = bonusLife_]() { return std::make_unique<CBuffAvatar>(duration, damage, life); });
}
CAbilitySiphonMana::CAbilitySiphonMana(int handleId) : CAbilitySpellBase(handleId, War3ID("AHdr"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(10); }
void CAbilitySiphonMana::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, tick = manaPerTick_, casterHandle]() { return std::make_unique<CBuffManaSiphon>(duration, tick, casterHandle); });
}
CAbilityBigBadVoodoo::CAbilityBigBadVoodoo(int handleId) : CAbilitySpellBase(handleId, War3ID("AOvd"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(180.0f); SetManaCost(200); }
void CAbilityBigBadVoodoo::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_, [duration = duration_]() { return std::make_unique<CBuffDivineShield>(duration); }, ABFactionFilter::SAME_PLAYER_AS_CASTER, true);
}
// === spells14.cpp ===
CAbilityPocketFactory::CAbilityPocketFactory(int handleId) : CAbilitySpellBase(handleId, War3ID("ANsy"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(500.0f); SetCooldown(35.0f); SetManaCost(125); }
void CAbilityPocketFactory::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastTimedSpawnerUnit(game, caster, x, y,
                         factoryType_, War3ID("ANsy"), factoryLife_,
                         goblinType_, spawnInterval_, explodeDamage_, explodeRadius_);
}
void CAbilityPocketFactory::ApplyEffectData(const CAbilityDefFull& def, int level) {
    factoryType_ = def.UnitIdAt(level, factoryType_);
    const std::string goblin = def.DataAt(level, 1);
    if (!goblin.empty()) goblinType_ = War3ID::FromString(goblin);
}
CAbilityLavaSpawn::CAbilityLavaSpawn(int handleId) : CAbilitySpellBase(handleId, War3ID("ANlm"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(32.0f); SetManaCost(150); }
void CAbilityLavaSpawn::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    ABActionCreateUnitWithBuff spawn(
        spawnType_, caster->GetX() + 64.0f, caster->GetY(),
        {BF([dur = summonDuration_]() {
             return std::make_unique<CBuffTimedLife>(War3ID("ANlm"), dur);
         }),
         BF([split = splitType_, n = splitCount_, child = childType_, cn = childCount_]() { return std::make_unique<CBuffSplit>(split, n, child, cn); })});
    spawn.Run(game, caster, nullptr, false, 0, 0);
}
void CAbilityLavaSpawn::ApplyEffectData(const CAbilityDefFull& def, int level) {
    spawnType_ = def.UnitIdAt(level, spawnType_);
    splitType_ = def.UnitIdAt(level + 1, War3ID());
    splitCount_ = (splitType_ == War3ID() || splitType_ == spawnType_) ? 0 : 2;
    childType_ = def.UnitIdAt(level + 2, War3ID());
    childCount_ = (childType_ == War3ID() || childType_ == splitType_) ? 0 : 2;
}
CAbilitySoulBurn::CAbilitySoulBurn(int handleId) : CAbilitySpellBase(handleId, War3ID("ANsb"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(9.0f); SetManaCost(75); }
void CAbilitySoulBurn::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABEffectChain chain;
    chain.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::FIRE));
    chain.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = burnDuration_, tick = burnPerTick_, casterHandle]() { return std::make_unique<CBuffPoison>(duration, tick, casterHandle); }));
    chain.Run(game, caster, target, false, 0, 0);
}
CAbilityQuilbeast::CAbilityQuilbeast(int handleId) : CAbilitySpellBase(handleId, War3ID("ANsq"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(25.0f); SetManaCost(75); }
void CAbilityQuilbeast::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionSummon quil(beastType_, War3ID("ANsq"), 1, summonDuration_, 0.0f);
    quil.Run(game, caster, nullptr, true, caster->GetX() + 64.0f, caster->GetY());
}
void CAbilityQuilbeast::ApplyEffectData(const CAbilityDefFull& def, int level) { beastType_ = def.UnitIdAt(level, beastType_); }
CAbilityFrostBreath::CAbilityFrostBreath(int handleId) : CAbilitySpellBase(handleId, War3ID("Afsh"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(300.0f); SetCooldown(8.0f); SetManaCost(100); }
void CAbilityFrostBreath::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    const int casterHandle = caster->GetHandleId();
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(
        damage_, CAttackType::SPELLS, CDamageType::COLD));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = slowDuration_, percent = slowPercent_, casterHandle]() { return std::make_unique<CBuffSlow>(duration, percent, casterHandle); }));
    ABActionForEachInRange breath(radius_, std::move(perUnit));
    breath.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    breath.Run(game, caster, target, false, 0, 0);
}
CAbilityPulverize::CAbilityPulverize(int handleId) : CAbilitySpellBase(handleId, War3ID("Awar"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
CAbilityForkedLightning::CAbilityForkedLightning(int handleId) : CAbilitySpellBase(handleId, War3ID("ANfl"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(75); }
void CAbilityForkedLightning::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
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
void CAbilityForkedLightning::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); bounces_ = def.DataIntAt(level, 1, bounces_); }
// === spells15.cpp ===
CAbilityPhoenix::CAbilityPhoenix(int handleId) : CAbilitySpellBase(handleId, War3ID("AHpx"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(180.0f); SetManaCost(175); }
void CAbilityPhoenix::ApplyEffectData(const CAbilityDefFull& def, int level) {
    const float duration = def.DurationAt(level);
    if (duration > 0.0f) {
        hatchDelay_ = duration;
        return;
    }
    const float data = def.DataFloatAt(level, 0, 0.0f);
    if (data > 0.0f) hatchDelay_ = data;
}
void CAbilityPhoenix::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    const float delay = hatchDelay_;
    ABActionCreateUnitWithBuff bird(
        phoenixType_, caster->GetX() + 64.0f, caster->GetY(),
        {BF([delay]() { return std::make_unique<CBuffPhoenixRebirth>(delay); })});
    bird.Run(game, caster, nullptr, false, 0, 0);
}
// === spells16.cpp ===
CAbilityDevour::CAbilityDevour(int handleId) : CAbilitySpellBase(handleId, War3ID("Adev"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(100.0f); }
void CAbilityDevour::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { ABActionDevourTarget devour(digestDuration_, digestPerTick_); devour.Run(game, caster, target, false, 0, 0); }
void CAbilityDevour::ApplyEffectData(const CAbilityDefFull& def, int level) { digestPerTick_ = def.DataFloatAt(level, 0, digestPerTick_); }
CAbilityDevourMagic::CAbilityDevourMagic(int handleId) : CAbilitySpellBase(handleId, War3ID("Advm"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(6.0f); SetManaCost(50); }
void CAbilityDevourMagic::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { ABActionDevourMagic devour(manaPerBuff_); devour.Run(game, caster, target, false, 0, 0); }
CAbilityColdArrows::CAbilityColdArrows(int handleId) : CAbilitySpellBase(handleId, War3ID("AHca"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
namespace {
class ColdArrowsEffect : public CAttackModifierEffect {
public:
    ColdArrowsEffect(float slowDuration, float slowPercent,
                     float poisonDuration, float poisonPerTick)
        : slowDuration_(slowDuration), slowPercent_(slowPercent),
          poisonDuration_(poisonDuration), poisonPerTick_(poisonPerTick) {}
    void OnAttackLanded(CSimulation& game, CUnit* attacker, CUnit* victim,
                        float damageDealt) override {
        (void)damageDealt;
        if (!victim) return;
        const int caster = attacker ? attacker->GetHandleId() : -1;
        victim->AddBuff(std::make_unique<CBuffSlow>(slowDuration_, slowPercent_, caster), game);
        victim->AddBuff(std::make_unique<CBuffPoison>(poisonDuration_, poisonPerTick_, caster), game);
    }
private:
    float slowDuration_;
    float slowPercent_;
    float poisonDuration_;
    float poisonPerTick_;
};
}
void CAbilityColdArrows::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetAttackModifier(0, GetAlias(),
        std::make_unique<ColdArrowsEffect>(slowDuration_, slowPercent_,
                                           poisonDuration_, poisonPerTick_));
}
void CAbilityColdArrows::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (unit) unit->ClearAttackModifier(0, GetAlias()); }
void CAbilityColdArrows::ApplyEffectData(const CAbilityDefFull& def, int level) {
    poisonPerTick_ = def.DataFloatAt(level, 0, poisonPerTick_);
    slowPercent_ = def.DataFloatAt(level, 1, slowPercent_);
    slowDuration_ = def.DurationAt(level);
    poisonDuration_ = def.DataFloatAt(level, 3, poisonDuration_);
}
CAbilityMonsoon::CAbilityMonsoon(int handleId) : CAbilitySpellBase(handleId, War3ID("ANmo"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(500.0f); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityMonsoon::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastPointDamageWaves(game, caster, x, y, waves_, radius_, damagePerWave_,
                         CAttackType::SPELLS, CDamageType::COLD,
                         ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
void CAbilityMonsoon::ApplyEffectData(const CAbilityDefFull& def, int level) { damagePerWave_ = def.DataFloatAt(level, 0, damagePerWave_); radius_ = def.AreaAt(level); }
CAbilityAbsorbMana::CAbilityAbsorbMana(int handleId) : CAbilitySpellBase(handleId, War3ID("Aabs"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); }
void CAbilityAbsorbMana::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || !targetUnit->IsSummoned()) return;
    const float mana = std::min(caster->GetMana() + manaGain_, static_cast<float>(caster->GetMaxMana()));
    ABActionKillTarget kill;
    kill.Run(game, caster, target, false, 0, 0);
    ABActionSetManaTarget refund(mana);
    refund.Run(game, caster, caster, false, 0, 0);
}
CAbilityUnholyFrenzy::CAbilityUnholyFrenzy(int handleId) : CAbilitySpellBase(handleId, War3ID("Auhf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityUnholyFrenzy::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_, attack = attackSpeedMul_]() { return std::make_unique<CBuffBerserk>(duration, attack, 1.0f); });
}
// === spells17.cpp ===
CAbilityHowlOfTerror::CAbilityHowlOfTerror(int handleId) : CAbilitySpellBase(handleId, War3ID("ANht"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(12.0f); SetManaCost(75); }
void CAbilityHowlOfTerror::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, factor = damageFactor_]() { return std::make_unique<CBuffHowl>(duration, factor); },
                       ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
void CAbilityHowlOfTerror::ApplyEffectData(const CAbilityDefFull& def, int level) { damageFactor_ = 1.0f - def.DataFloatAt(level, 0, 1.0f - damageFactor_); }
CAbilityBreathOfFire::CAbilityBreathOfFire(int handleId) : CAbilitySpellBase(handleId, War3ID("ANbf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(375.0f); SetCooldown(10.0f); SetManaCost(70); }
void CAbilityBreathOfFire::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    const int casterHandle = caster->GetHandleId();
    auto makeHitAndBurn = [&]() {
        ABEffectChain chain;
        chain.Add(std::make_unique<ABActionDamageTarget>(
            damage_, CAttackType::SPELLS, CDamageType::FIRE));
        chain.Add(std::make_unique<ABActionAddBuffTarget>(
            [duration = burnDuration_, tick = burnPerTick_, casterHandle]() { return std::make_unique<CBuffPoison>(duration, tick, casterHandle); }));
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
CAbilityDrunkenHaze::CAbilityDrunkenHaze(int handleId) : CAbilitySpellBase(handleId, War3ID("ANdh"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(550.0f); SetCooldown(12.0f); SetManaCost(70); }
void CAbilityDrunkenHaze::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    const int casterHandle = caster->GetHandleId();
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, miss = missChance_]() { return std::make_unique<CBuffHaze>(duration, miss); }));
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, slow = slowPercent_, casterHandle]() { return std::make_unique<CBuffSlow>(duration, slow, casterHandle); }));
    ABActionForEachInRange haze(radius_, std::move(perUnit));
    haze.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    haze.Run(game, caster, nullptr, true, x, y);
}
void CAbilityDrunkenHaze::ApplyEffectData(const CAbilityDefFull& def, int level) { missChance_ = def.DataFloatAt(level, 1, missChance_); slowPercent_ = def.DataFloatAt(level, 2, slowPercent_); }
CAbilityManaShield::CAbilityManaShield(int handleId) : CAbilitySpellBase(handleId, War3ID("ANms"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(10.0f); SetManaCost(25); }
void CAbilityManaShield::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() { return std::make_unique<CBuffManaShield>(duration); });
}
CAbilityLocustSwarm::CAbilityLocustSwarm(int handleId) : CAbilitySpellBase(handleId, War3ID("AUls"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(180.0f); SetManaCost(150); }
void CAbilityLocustSwarm::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaDamageWaves(game, caster, waves_, radius_, damagePerWave_,
                              CAttackType::SPELLS, CDamageType::POISON,
                              ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
void CAbilityLocustSwarm::ApplyEffectData(const CAbilityDefFull& def, int level) { damagePerWave_ = def.DataFloatAt(level, 0, damagePerWave_); }
CAbilityCyclone::CAbilityCyclone(int handleId) : CAbilitySpellBase(handleId, War3ID("Acyc"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(5.0f); SetManaCost(150); }
void CAbilityCyclone::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() { return std::make_unique<CBuffCyclone>(duration); });
}
void CAbilityCyclone::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); }
CAbilityVengeance::CAbilityVengeance(int handleId) : CAbilitySpellBase(handleId, War3ID("Avng"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(2.0f); SetManaCost(25); }
void CAbilityVengeance::OnDeath(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    CUnit* spirit = game.CreateUnit(spiritType_, unit->GetPlayerIndex(),
                                    unit->GetX(), unit->GetY(), 0);
    if (!spirit) return;
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(unit->GetHandleId(), spirit->GetHandleId());
    }
}
// === spells18.cpp ===
CAbilityWhirlwind::CAbilityWhirlwind(int handleId) : CAbilitySpellBase(handleId, War3ID("AOww"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(8.0f); SetManaCost(75); }
void CAbilityWhirlwind::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster,
                   [duration = duration_, dps = damagePerSecond_, radius = radius_]() { return std::make_unique<CBuffWhirlwind>(duration, dps, radius); });
}
void CAbilityWhirlwind::ApplyEffectData(const CAbilityDefFull& def, int level) { damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_); radius_ = def.AreaAt(level); duration_ = def.DurationAt(level); }
CAbilityImmolation::CAbilityImmolation(int handleId) : CAbilitySpellBase(handleId, War3ID("AEim"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(5.0f); SetManaCost(10); }
void CAbilityImmolation::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster,
                   [duration = duration_, tick = damagePerTick_, drain = manaDrain_, radius = radius_]() { return std::make_unique<CBuffImmolation>(duration, tick, drain, radius); });
}
void CAbilityImmolation::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerTick_ = def.DataFloatAt(level, 0, damagePerTick_);
    manaDrain_ = def.DataFloatAt(level, 1, manaDrain_);
    radius_ = def.AreaAt(level);
    duration_ = def.DurationAt(level);
}
CAbilityForceOfNature::CAbilityForceOfNature(int handleId) : CAbilitySpellBase(handleId, War3ID("AEfn"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(600.0f); SetCooldown(11.0f); SetManaCost(75); }
void CAbilityForceOfNature::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABActionSummonRing nature(treantType_, War3ID("AEfn"), count_, summonDuration_);
    nature.Run(game, caster, nullptr, true, x, y);
}
void CAbilityForceOfNature::ApplyEffectData(const CAbilityDefFull& def, int level) { count_ = def.DataIntAt(level, 0, count_); treantType_ = def.UnitIdAt(level, treantType_); summonDuration_ = def.DurationAt(level); }
CAbilityOwlScout::CAbilityOwlScout(int handleId) : CAbilitySpellBase(handleId, War3ID("AEst"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(40.0f); SetManaCost(25); }
void CAbilityOwlScout::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionSummon owl(owlType_, War3ID("AEst"), 1, summonDuration_, 0.0f);
    owl.Run(game, caster, nullptr, true, caster->GetX() + 64.0f, caster->GetY());
}
void CAbilityOwlScout::ApplyEffectData(const CAbilityDefFull& def, int level) { owlType_ = def.UnitIdAt(level, owlType_); summonDuration_ = def.DurationAt(level); }
CAbilitySummonHawk::CAbilitySummonHawk(int handleId) : CAbilitySpellBase(handleId, War3ID("ANsw"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(30.0f); SetManaCost(50); }
void CAbilitySummonHawk::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionSummon hawk(hawkType_, War3ID("ANsw"), 1, summonDuration_, 0.0f);
    hawk.Run(game, caster, nullptr, true, caster->GetX() + 64.0f, caster->GetY());
}
void CAbilitySummonHawk::ApplyEffectData(const CAbilityDefFull& def, int level) { hawkType_ = def.UnitIdAt(level, hawkType_); summonDuration_ = def.DurationAt(level); }
CAbilityFactory::CAbilityFactory(int handleId) : CAbilitySpellBase(handleId, War3ID("ANfy"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(500.0f); SetCooldown(60.0f); SetManaCost(150); }
void CAbilityFactory::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastTimedSpawnerUnit(game, caster, x, y,
                         factoryType_, War3ID("ANfy"), factoryLife_,
                         goblinType_, spawnInterval_, explodeDamage_, explodeRadius_);
}
void CAbilityFactory::ApplyEffectData(const CAbilityDefFull& def, int level) {
    factoryType_ = def.UnitIdAt(level, factoryType_);
    const std::string goblin = def.DataAt(level, 1);
    if (!goblin.empty()) goblinType_ = War3ID::FromString(goblin);
}
CAbilityKaboom::CAbilityKaboom(int handleId) : CAbilitySpellBase(handleId, War3ID("Asds"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_BOTH); SetCastRange(128.0f); SetCooldown(5.0f); SetManaCost(0); }
void CAbilityKaboom::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)target;
    if (!caster) return;
    armed_ = true;
    ABActionKillTarget boom;
    boom.Run(game, caster, caster, false, 0, 0);
}
void CAbilityKaboom::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    (void)x; (void)y;
    if (!caster) return;
    armed_ = true;
    ABActionKillTarget boom;
    boom.Run(game, caster, caster, false, 0, 0);
}
void CAbilityKaboom::OnDeath(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (armed_ || explodesOnDeath_) {
        armed_ = false;
        Explode(game, unit);
    }
}
void CAbilityKaboom::Explode(CSimulation& game, CUnit* caster) {
    const float radius = std::max(partialRadius_, fullRadius_);
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        const float dist2 = dx * dx + dy * dy;
        if (dist2 > radius * radius) continue;
        float damage = (dist2 <= fullRadius_ * fullRadius_)
            ? fullDamage_ : partialDamage_;
        if (unit->IsBuilding()) damage *= buildingFactor_;
        unit->Damage(game, caster, false, true,
                     CAttackType::SPELLS, CDamageType::DEMOLITION,
                     "", damage, 0);
    }
}
void CAbilityKaboom::ApplyEffectData(const CAbilityDefFull& def, int level) {
    fullRadius_ = def.DataFloatAt(level, 0, fullRadius_);
    fullDamage_ = def.DataFloatAt(level, 1, fullDamage_);
    partialRadius_ = def.DataFloatAt(level, 2, partialRadius_);
    partialDamage_ = def.DataFloatAt(level, 3, partialDamage_);
    buildingFactor_ = def.DataFloatAt(level, 4, buildingFactor_);
    explodesOnDeath_ = def.DataIntAt(level, 5, explodesOnDeath_ ? 1 : 0) != 0;
}
CAbilityPhoenixFire::CAbilityPhoenixFire(int handleId) : CAbilitySpellBase(handleId, War3ID("Apxf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(0.0f); SetManaCost(0); }
void CAbilityPhoenixFire::OnAdd(CSimulation& game, CUnit* unit) { if (!unit) return; unit->AddBuff(std::make_unique<CBuffPhoenixFire>( 99999.0f, damagePerSecond_, radius_, initialDamage_), game); }
void CAbilityPhoenixFire::OnRemove(CSimulation& game, CUnit* unit) { if (!unit) return; unit->RemoveBuff(War3ID("BPxf"), game); }
void CAbilityPhoenixFire::ApplyEffectData(const CAbilityDefFull& def, int level) {
    initialDamage_ = def.DataFloatAt(level, 0, initialDamage_);
    damagePerSecond_ = def.DataFloatAt(level, 1, damagePerSecond_);
    radius_ = def.AreaAt(level);
}
// === spells19.cpp ===
CAbilityBanish::CAbilityBanish(int handleId) : CAbilitySpellBase(handleId, War3ID("AHbn"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(65); }
void CAbilityBanish::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() {
        return std::make_unique<CBuffEthereal>(duration, War3ID("Bban"));
    });
}
void CAbilityBanish::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); }
CAbilityAntiMagicShell::CAbilityAntiMagicShell(int handleId) : CAbilitySpellBase(handleId, War3ID("Aam2"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(12.0f); SetManaCost(75); }
void CAbilityAntiMagicShell::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() {
        return std::make_unique<CBuffMagicImmune>(duration, War3ID("Bam2"));
    });
}
void CAbilityAntiMagicShell::ApplyEffectData(const CAbilityDefFull& def, int level) { duration_ = def.DurationAt(level); }
// === spells20.cpp ===
CAbilityPossession::CAbilityPossession(int handleId) : CAbilitySpellBase(handleId, War3ID("Apos"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(500.0f); SetCooldown(10.0f); SetManaCost(150); }
void CAbilityPossession::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return;
    const int beforePlayer = victim->GetPlayerIndex();
    ABEffectChain convert;
    convert.Add(std::make_unique<ABActionTransferToCaster>());
    ABActionIf possess(
        std::make_shared<ABCondFunc>(
            [maxLevel = maxLevel_](CSimulation&, CUnit*, CWidget* unitTarget,
                                   bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                if (!unit || unit->IsDead() || unit->IsHero()) return false;
                const CUnitType* type = unit->GetUnitType();
                return type == nullptr || type->GetLevel() <= maxLevel;
            }),
        std::move(convert));
    possess.Run(game, caster, target, false, 0, 0);
    if (beforePlayer != caster->GetPlayerIndex() &&
        victim->GetPlayerIndex() == caster->GetPlayerIndex()) {
        game.RemoveUnit(caster);
    }
}
void CAbilityPossession::ApplyEffectData(const CAbilityDefFull& def, int level) { maxLevel_ = def.DataIntAt(level, 0, maxLevel_); }
CAbilityStampede::CAbilityStampede(int handleId) : CAbilitySpellBase(handleId, War3ID("ANst"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(700.0f); SetCooldown(60.0f); SetManaCost(150); }
void CAbilityStampede::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastLineDamageAndStun(game, caster, nullptr, true, x, y,
                          range_, width_, damage_, slowDuration_,
                          CAttackType::SPELLS, CDamageType::NORMAL,
                          ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}
void CAbilityStampede::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); range_ = def.DataFloatAt(level, 1, range_); }
// === spells21.cpp ===
CAbilityStasisTrap::CAbilityStasisTrap(int handleId) : CAbilitySpellBase(handleId, War3ID("Asta"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(700.0f); SetCooldown(10.0f); SetManaCost(75); }
void CAbilityStasisTrap::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    const int casterPlayer = caster->GetPlayerIndex();
    CSimulation::SimHazard trap;
    trap.x = x;
    trap.y = y;
    trap.radius = radius_;
    trap.ticksLeft = (int)(lifetime_ / CSimulation::SIM_STEP_TIME);
    trap.tickPeriod = 5;
    trap.tick = [casterPlayer, stun = stunDuration_, radius = radius_](
                    CSimulation& sim, CSimulation::SimHazard& h) {
        bool tripped = false;
        for (CUnit* unit : sim.GetUnits()) {
            if (!unit || unit->IsDead() || IsFriendlyToPlayer(sim, casterPlayer, unit)) continue;
            const float dx = unit->GetX() - h.x;
            const float dy = unit->GetY() - h.y;
            if (dx * dx + dy * dy > radius * radius) continue;
            unit->AddBuff(std::make_unique<CBuffStun>(stun), sim);
            tripped = true;
        }
        return tripped;
    };
    game.AddHazard(std::move(trap));
}
void CAbilityStasisTrap::ApplyEffectData(const CAbilityDefFull& def, int level) { stunDuration_ = def.DurationAt(level); radius_ = def.AreaAt(level); }
CAbilityTornado::CAbilityTornado(int handleId) : CAbilitySpellBase(handleId, War3ID("ANto"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(800.0f); SetCooldown(20.0f); SetManaCost(150); }
void CAbilityTornado::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    const int casterHandle = caster->GetHandleId();
    const int casterPlayer = caster->GetPlayerIndex();
    for (int i = 0; i < count_; i++) {
        CSimulation::SimHazard twister;
        twister.x = x + game.SimRandRange(-64.0f, 64.0f);
        twister.y = y + game.SimRandRange(-64.0f, 64.0f);
        twister.radius = radius_;
        twister.ticksLeft = (int)(duration_ / CSimulation::SIM_STEP_TIME);
        twister.tickPeriod = 20;
        twister.speed = speed_;
        twister.tx = x + game.SimRandRange(-400.0f, 400.0f);
        twister.ty = y + game.SimRandRange(-400.0f, 400.0f);
        twister.tick = [casterHandle, casterPlayer, radius = radius_, damage = damage_](
                           CSimulation& sim, CSimulation::SimHazard& h) {
            const float dx = h.tx - h.x;
            const float dy = h.ty - h.y;
            if (dx * dx + dy * dy < 32.0f * 32.0f) {
                h.tx = h.x + sim.SimRandRange(-400.0f, 400.0f);
                h.ty = h.y + sim.SimRandRange(-400.0f, 400.0f);
            }
            CUnit* caster = sim.GetUnit(casterHandle);
            for (CUnit* unit : sim.GetUnits()) {
                if (!unit || unit->IsDead() || IsFriendlyToPlayer(sim, casterPlayer, unit)) continue;
                const float ux = unit->GetX() - h.x;
                const float uy = unit->GetY() - h.y;
                if (ux * ux + uy * uy > radius * radius) continue;
                sim.GetCombatSystem()->ApplyDamage(sim, caster, unit, false, false,
                                                  CAttackType::SPELLS, CDamageType::MAGIC,
                                                  "", damage, 0.0f, 0, 0);
                unit->AddBuff(std::make_unique<CBuffCyclone>(2.0f), sim);
            }
            return false;
        };
        game.AddHazard(std::move(twister));
    }
}
void CAbilityTornado::ApplyEffectData(const CAbilityDefFull& def, int level) { damage_ = def.DataFloatAt(level, 0, damage_); radius_ = def.AreaAt(level); duration_ = def.DurationAt(level); }
// === spells22.cpp ===
std::unique_ptr<CAbility> CreateSpells22Alias(int handleId, War3ID code) {
    if (code == War3ID("Afod") || code == War3ID("Acht") ||
        code == War3ID("ANin") || code == War3ID("Aams")) {
        return AbilityFactory::CreateAbility(handleId, code);
    }
    return nullptr;
}
// === spells23.cpp ===
CAbilityChainDispel::CAbilityChainDispel(int handleId) : CAbilitySpellBase(handleId, War3ID("Ache"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(12.0f); SetManaCost(75); }
void CAbilityChainDispel::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!caster || !target) return;
    ABActionBounce chain(
        bounces_, range_,
        [](int) {
            ABEffectChain perHit;
            ABEffectChain allyBranch;
            allyBranch.Add(std::make_unique<ABActionDispelTarget>(false, true));
            ABEffectChain foeBranch;
            foeBranch.Add(std::make_unique<ABActionDispelTarget>(true, false));
            perHit.Add(std::make_unique<ABActionIf>(
                std::make_shared<ABCondTargetAllyOfCaster>(), std::move(allyBranch),
                std::move(foeBranch)));
            return perHit;
        });
    chain.Run(game, caster, target, false, 0, 0);
}
void CAbilityChainDispel::ApplyEffectData(const CAbilityDefFull& def, int level) { bounces_ = def.DataIntAt(level, 0, bounces_); range_ = def.AreaAt(level); }
CAbilityTaunt::CAbilityTaunt(int handleId) : CAbilitySpellBase(handleId, War3ID("ANta"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(10.0f); SetManaCost(50); }
void CAbilityTaunt::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionTauntTarget>(caster));
    ABActionForEachInRange taunt(radius_, std::move(perUnit));
    taunt.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    taunt.Run(game, caster, nullptr, true, caster->GetX(), caster->GetY());
}
void CAbilityTaunt::ApplyEffectData(const CAbilityDefFull& def, int level) { radius_ = def.AreaAt(level); }
// === spells24.cpp ===
CAbilitySelfStatPassive::CAbilitySelfStatPassive(int handleId, War3ID code, CAuraEffectType type, float value, bool percent) : CAbility(handleId, code, CAbilityCategory::ITEM) , type_(type), value_(value), percent_(percent) { iconShowing_ = false; permanent_ = true; }
void CAbilitySelfStatPassive::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    std::vector<CBuffAuraStat::Bonus> bonuses{{type_, value_, percent_}};
    unit->AddBuff(std::make_unique<CBuffAuraStat>(GetCode(), std::move(bonuses)), game);
}
void CAbilitySelfStatPassive::OnRemove(CSimulation& game, CUnit* unit) { if (!unit) return; unit->RemoveBuff(GetCode(), game); }
CAbilityItemAttackSpeed::CAbilityItemAttackSpeed(int handleId) : CAbilitySelfStatPassive(handleId, War3ID("AIas"), CAuraEffectType::ATTACK_SPEED, 0.15f, true) {}
CAbilityItemManaRegen::CAbilityItemManaRegen(int handleId) : CAbilitySelfStatPassive(handleId, War3ID("AIrm"), CAuraEffectType::MANA_REGEN, 0.50f, true) {}
CAbilityItemMoveSpeed::CAbilityItemMoveSpeed(int handleId) : CAbilitySelfStatPassive(handleId, War3ID("AIms"), CAuraEffectType::MOVE_SPEED, 50.0f, false) {}
CAbilityItemVampirism::CAbilityItemVampirism(int handleId) : CAbility(handleId, War3ID("AIva"), CAbilityCategory::ITEM) { iconShowing_ = false; permanent_ = true; }
void CAbilityItemVampirism::OnAdd(CSimulation& game, CUnit* unit) { if (!unit) return; unit->AddBuff(std::make_unique<CBuffLifesteal>(1.0e9f, 0.15f), game); }
void CAbilityItemVampirism::OnRemove(CSimulation& game, CUnit* unit) { if (!unit) return; unit->RemoveBuff(War3ID("BUav"), game); }
// === spells25.cpp ===
namespace {
CAbility* FindHero(CUnit* unit) {
    return unit ? unit->GetAbility(War3ID("AHer")) : nullptr;
}
void KillUnit(CSimulation& game, CUnit* unit) {
    if (!unit || unit->IsDead()) return;
    unit->SetLife(game, 0);
}
void ApplyUnitStatSlotDelta(CUnit* unit, CAbilityItemStatBonus::StatSlot slot, int delta) {
    if (!unit || delta == 0) return;
    switch (slot) {
        case CAbilityItemStatBonus::StatSlot::DAMAGE:   unit->AddBonusDamage(delta); break;
        case CAbilityItemStatBonus::StatSlot::ARMOR:    unit->AddBonusArmor(delta); break;
        case CAbilityItemStatBonus::StatSlot::MAX_LIFE: unit->AddBonusMaxLife(static_cast<float>(delta)); break;
        case CAbilityItemStatBonus::StatSlot::MAX_MANA: unit->AddBonusMaxMana(delta); break;
        default: break;
    }
}
}
CAbilityCurse::CAbilityCurse(int handleId) : CAbilitySpellBase(handleId, War3ID("Acrs"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(8.0f); SetManaCost(50); }
void CAbilityCurse::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (!unit || unit->IsDead()) return;
    (void)caster;
    unit->AddBuff(std::make_unique<CBuffCurse>(duration_, missChance_), game);
}
void CAbilityCurse::ApplyEffectData(const CAbilityDefFull& def, int level) { missChance_ = def.DataFloatAt(level, 0, missChance_); duration_ = def.DurationAt(level); }
CAbilityMoonWell::CAbilityMoonWell(int handleId) : CAbilitySpellBase(handleId, War3ID("Ambt"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); }
void CAbilityMoonWell::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (!caster || !unit || unit->IsDead()) return;
    float mana = caster->GetMana();
    if (manaPerLife_ > 0.0f) {
        const float wanted = std::max(0.0f, unit->GetMaxLife() - unit->GetLife());
        if (wanted > 0.0f && mana > 0.0f) {
            const float gained = std::min(mana / manaPerLife_, wanted);
            unit->SetLife(game, unit->GetLife() + gained);
            mana -= gained * manaPerLife_;
        }
    }
    if (manaPerMana_ > 0.0f) {
        const float wanted = std::max(0.0f, (float)unit->GetMaxMana() - unit->GetMana());
        if (wanted > 0.0f && mana > 0.0f) {
            const float gained = std::min(mana / manaPerMana_, wanted);
            unit->SetMana(unit->GetMana() + gained);
            mana -= gained * manaPerMana_;
        }
    }
    caster->SetMana(mana);
}
void CAbilityMoonWell::ApplyEffectData(const CAbilityDefFull& def, int level) { manaPerMana_ = def.DataFloatAt(level, 0, manaPerMana_); manaPerLife_ = def.DataFloatAt(level, 1, manaPerLife_); }
CAbilityEatTree::CAbilityEatTree(int handleId) : CAbilitySpellBase(handleId, War3ID("Aeat"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(150.0f); }
void CAbilityEatTree::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* tree = dynamic_cast<CDestructable*>(target);
    if (!caster || !tree || tree->IsDead()) return;
    tree->Damage(game, caster, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                 "", tree->GetLife() + 1.0f, 0);
    const float life = std::min(caster->GetMaxLife(), caster->GetLife() + hitPointsGained_);
    caster->SetLife(game, life);
}
void CAbilityEatTree::ApplyEffectData(const CAbilityDefFull& def, int level) { hitPointsGained_ = def.DataFloatAt(level, 2, hitPointsGained_); }
CAbilitySacrifice::CAbilitySacrifice(int handleId) : CAbilitySpellBase(handleId, War3ID("Asac"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(100000.0f); }
void CAbilitySacrifice::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return;
    const float x = victim->GetX();
    const float y = victim->GetY();
    const int player = caster ? caster->GetPlayerIndex() : victim->GetPlayerIndex();
    KillUnit(game, victim);
    game.CreateUnit(War3ID("ushd"), player, x, y, 0);
}
CAbilityRequestSacrifice::CAbilityRequestSacrifice(int handleId) : CAbilitySpellBase(handleId, War3ID("Alam"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(100000.0f); }
void CAbilityRequestSacrifice::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) { (void)target; KillUnit(game, caster); }
CAbilityHide::CAbilityHide(int handleId) : CAbilitySpellBase(handleId, War3ID("Ahid"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityHide::DoEffect(CSimulation& game, CUnit* caster) { (void)game; if (caster) caster->SetInvisible(true); }
CAbilityEthereal::CAbilityEthereal(int handleId) : CAbility(handleId, War3ID("Aeth"), CAbilityCategory::BUFF) { iconShowing_ = false; permanent_ = true; }
void CAbilityEthereal::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit || applied_) return;
    savedCollision_ = unit->GetCollisionSize();
    unit->SetCollisionSize(0.0f);
    unit->AddBuff(std::make_unique<CBuffEthereal>(1.0e9f, War3ID("BEth")), game);
    applied_ = true;
}
void CAbilityEthereal::OnRemove(CSimulation& game, CUnit* unit) { if (!unit || !applied_) return; unit->SetCollisionSize(savedCollision_); unit->RemoveBuff(War3ID("BEth"), game); applied_ = false; }
CAbilityChestOfGold::CAbilityChestOfGold(int handleId) : CAbilitySpellBase(handleId, War3ID("AIgo"), CAbilityCategory::ITEM) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityChestOfGold::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (CPlayer* player = game.GetPlayer(caster->GetPlayerIndex())) {
        player->AddGold(gold_);
    }
}
void CAbilityChestOfGold::ApplyEffectData(const CAbilityDefFull& def, int level) { gold_ = def.DataIntAt(level, 0, gold_); }
CAbilityItemStatBonus::CAbilityItemStatBonus(int handleId, War3ID code, StatSlot slot, int defaultBonus) : CAbilitySpellBase(handleId, code, CAbilityCategory::ITEM) , slot_(slot) , bonus_(defaultBonus) { iconShowing_ = false; permanent_ = true; SetPassiveSpell(true); SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemStatBonus::OnAdd(CSimulation&, CUnit* unit) { if (!unit || slot_ == StatSlot::NONE) return; applied_ = bonus_; ApplyUnitStatSlotDelta(unit, slot_, applied_); }
void CAbilityItemStatBonus::OnRemove(CSimulation&, CUnit* unit) { if (!unit || slot_ == StatSlot::NONE) return; ApplyUnitStatSlotDelta(unit, slot_, -applied_); applied_ = 0; }
void CAbilityItemStatBonus::ApplyEffectData(const CAbilityDefFull& def, int level) {
    if (slot_ != StatSlot::NONE) {
        bonus_ = def.DataIntAt(level, 0, bonus_);
    }
}
CAbilityItemAttributeBonus::CAbilityItemAttributeBonus(int handleId) : CAbilityItemStatBonus(handleId, War3ID("AIab"), StatSlot::NONE, 0) {}
void CAbilityItemAttributeBonus::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    appliedAgi_ = agility_;
    appliedInt_ = intelligence_;
    appliedStr_ = strength_;
    if (CAbility* hero = FindHero(unit)) {
        hero->AddAgilityBonus(game, unit, appliedAgi_);
        hero->AddIntelligenceBonus(game, unit, appliedInt_);
        hero->AddStrengthBonus(game, unit, appliedStr_);
    }
}
void CAbilityItemAttributeBonus::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (CAbility* hero = FindHero(unit)) {
        hero->AddAgilityBonus(game, unit, -appliedAgi_);
        hero->AddIntelligenceBonus(game, unit, -appliedInt_);
        hero->AddStrengthBonus(game, unit, -appliedStr_);
    }
    appliedAgi_ = appliedInt_ = appliedStr_ = 0;
}
void CAbilityItemAttributeBonus::ApplyEffectData(const CAbilityDefFull& def, int level) {
    agility_ = def.DataIntAt(level, 0, agility_);
    intelligence_ = def.DataIntAt(level, 1, intelligence_);
    strength_ = def.DataIntAt(level, 2, strength_);
}
// === spells26.cpp ===
CAbilitySpellShieldPassive::CAbilitySpellShieldPassive(int handleId) : CAbility(handleId, War3ID("ANss"), CAbilityCategory::SPELL) { iconShowing_ = false; permanent_ = true; }
void CAbilitySpellShieldPassive::Arm(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (!unit->HasBuff(War3ID("BNss"))) {
        unit->AddBuff(std::make_unique<CBuffSpellShield>(1.0e9f), game);
    }
    armed_ = true;
    remaining_ = 0.0f;
}
void CAbilitySpellShieldPassive::OnAdd(CSimulation& game, CUnit* unit) { Arm(game, unit); }
void CAbilitySpellShieldPassive::OnRemove(CSimulation& game, CUnit* unit) { if (unit) unit->RemoveBuff(War3ID("BNss"), game); armed_ = false; remaining_ = 0.0f; }
void CAbilitySpellShieldPassive::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (remaining_ > 0.0f) {
        remaining_ -= game.GetSimStepTime();
        if (remaining_ <= 0.0f) Arm(game, unit);
        return;
    }
    if (armed_ && !unit->HasBuff(War3ID("BNss"))) {
        armed_ = false;
        remaining_ = cooldown_;
    }
}
CAbilitySpellShieldArea::CAbilitySpellShieldArea(int handleId) : CAbilitySpellBase(handleId, War3ID("ANse"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_POINT); SetCastRange(600.0f); SetManaCost(50); }
void CAbilitySpellShieldArea::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    ForEachPassiveAllyInRange(game, caster, x, y, radius_, true,
                              [&game, duration = duration_](CUnit* other) {
                                  other->AddBuff(std::make_unique<CBuffSpellShield>(duration), game);
                              });
}
void CAbilitySpellShieldArea::ApplyEffectData(const CAbilityDefFull& def, int level) { radius_ = def.AreaAt(level); duration_ = def.DurationAt(level); }
CAbilityItemPermanentStats::CAbilityItemPermanentStats(int handleId, War3ID code, int agility, int intelligence, int strength) : CAbilitySpellBase(handleId, code, CAbilityCategory::ITEM) , agility_(agility), intelligence_(intelligence), strength_(strength) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemPermanentStats::DoEffect(CSimulation& game, CUnit* caster) {
    CAbility* hero = FindHero(caster);
    if (!hero) return;
    hero->SetAgilityBase(hero->GetAgility().base + agility_);
    hero->SetIntelligenceBase(hero->GetIntelligence().base + intelligence_);
    hero->SetStrengthBase(hero->GetStrength().base + strength_);
    hero->AddStrengthBonus(game, caster, 0);
}
void CAbilityItemPermanentStats::ApplyEffectData(const CAbilityDefFull& def, int level) {
    agility_ = def.DataIntAt(level, 0, agility_);
    intelligence_ = def.DataIntAt(level, 1, intelligence_);
    strength_ = def.DataIntAt(level, 2, strength_);
}
CAbilityItemPermanentLife::CAbilityItemPermanentLife(int handleId) : CAbilitySpellBase(handleId, War3ID("AImi"), CAbilityCategory::ITEM) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemPermanentLife::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster || life_ == 0) return;
    const float before = caster->GetMaxLife();
    const float ratio = before > 0.0f ? caster->GetLife() / before : 1.0f;
    caster->AddBonusMaxLife((float)life_);
    caster->SetLife(game, ratio * caster->GetMaxLife());
}
void CAbilityItemPermanentLife::ApplyEffectData(const CAbilityDefFull& def, int level) { life_ = def.DataIntAt(level, 0, life_); }
CAbilityItemExperience::CAbilityItemExperience(int handleId) : CAbilitySpellBase(handleId, War3ID("AIem"), CAbilityCategory::ITEM) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemExperience::DoEffect(CSimulation& game, CUnit* caster) {
    if (CAbility* hero = FindHero(caster)) {
        hero->AddXP(game, caster, xp_);
    }
}
void CAbilityItemExperience::ApplyEffectData(const CAbilityDefFull& def, int level) { xp_ = def.DataIntAt(level, 0, xp_); }
CAbilityItemLevelGain::CAbilityItemLevelGain(int handleId) : CAbilitySpellBase(handleId, War3ID("AIlm"), CAbilityCategory::ITEM) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemLevelGain::DoEffect(CSimulation& game, CUnit* caster) {
    if (CAbility* hero = FindHero(caster)) {
        hero->SetHeroLevel(game, caster, hero->GetHeroLevel() + levels_);
    }
}
void CAbilityItemLevelGain::ApplyEffectData(const CAbilityDefFull& def, int level) { levels_ = def.DataIntAt(level, 0, levels_); }
CAbilityItemFigurine::CAbilityItemFigurine(int handleId) : CAbilitySpellBase(handleId, War3ID("AIfs"), CAbilityCategory::ITEM) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemFigurine::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster || count_ <= 0) return;
    for (int i = 0; i < count_; i++) {
        CUnit* summoned = game.CreateUnit(summonId_, caster->GetPlayerIndex(),
                                          caster->GetX() + 64.0f, caster->GetY(),
                                          caster->GetFacing());
        if (!summoned) continue;
        summoned->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("BFig"), duration_), game);
    }
}
void CAbilityItemFigurine::ApplyEffectData(const CAbilityDefFull& def, int level) {
    count_ = def.DataIntAt(level, 0, count_);
    const std::string id = def.DataAt(level, 2);
    if (id.size() == 4) summonId_ = War3ID(id.c_str());
    duration_ = def.DurationAt(level);
}
CAbilityPermanentInvulnerable::CAbilityPermanentInvulnerable(int handleId) : CAbility(handleId, War3ID("Avul"), CAbilityCategory::BUFF) { iconShowing_ = false; permanent_ = true; }
void CAbilityPermanentInvulnerable::OnAdd(CSimulation&, CUnit* unit) { if (unit) unit->SetInvulnerable(true); }
void CAbilityPermanentInvulnerable::OnRemove(CSimulation&, CUnit* unit) { if (unit) unit->SetInvulnerable(false); }
CAbilityChangeTimeOfDay::CAbilityChangeTimeOfDay(int handleId) : CAbilitySpellBase(handleId, War3ID("AIct"), CAbilityCategory::ITEM) { SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityChangeTimeOfDay::DoEffect(CSimulation& game, CUnit* caster) { (void)caster; const float clockHours = (float)hour_ + (float)minute_ / 60.0f; game.SetGameTimeOfDay(clockHours); }
void CAbilityChangeTimeOfDay::ApplyEffectData(const CAbilityDefFull& def, int level) { hour_ = def.DataIntAt(level, 0, hour_); minute_ = def.DataIntAt(level, 1, minute_); }
CAbilityToggleForm::CAbilityToggleForm(int handleId, War3ID code) : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); formId_ = code; }
void CAbilityToggleForm::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (caster->GetBuff(GetCode())) {
        caster->RemoveBuff(GetCode(), game);
        return;
    }
    caster->AddBuff(std::make_unique<CBuffTransform>(GetCode(), 1.0e9f, formId_), game);
}
void CAbilityToggleForm::ApplyEffectData(const CAbilityDefFull& def, int level) { formId_ = def.UnitIdAt(level, formId_); }
CAbilityChaos::CAbilityChaos(int handleId) : CAbilitySpellBase(handleId, War3ID("Acha"), CAbilityCategory::SPELL) { iconShowing_ = false; permanent_ = true; SetPassiveSpell(true); SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityChaos::OnAdd(CSimulation& game, CUnit* unit) { if (!unit || unit->GetBuff(GetCode())) return; unit->AddBuff(std::make_unique<CBuffTransform>(GetCode(), 1.0e9f, formId_), game); }
void CAbilityChaos::OnRemove(CSimulation& game, CUnit* unit) { if (unit) unit->RemoveBuff(GetCode(), game); }
void CAbilityChaos::ApplyEffectData(const CAbilityDefFull& def, int level) { formId_ = def.UnitIdAt(level, formId_); }
// === spells27.cpp ===
namespace {
float ClampedFactor(float penalty) {
    float factor = 1.0f - penalty;
    if (factor < 0.05f) factor = 0.05f;
    if (factor > 1.0f) factor = 1.0f;
    return factor;
}
}
CAbilityStatePassive::CAbilityStatePassive(int handleId, War3ID code, CPassiveState state) : CAbility(handleId, code, CAbilityCategory::SPELL), state_(state) { iconShowing_ = false; permanent_ = true; }
void CAbilityStatePassive::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit || applied_) return;
    applied_ = true;
    switch (state_) {
    case CPassiveState::Ethereal:
        unit->AddBuff(std::make_unique<CBuffEthereal>(1.0e9f, War3ID("Betl")), game);
        break;
    case CPassiveState::MagicImmune:
        unit->AddBuff(std::make_unique<CBuffMagicImmune>(1.0e9f, War3ID("Bmim")), game);
        break;
    case CPassiveState::Resistant:
        unit->AddBuff(std::make_unique<CBuffResistant>(), game);
        break;
    case CPassiveState::Ghost:
        savedCollision_ = unit->GetCollisionSize();
        unit->SetInvisible(true);
        unit->SetCollisionSize(0.0f);
        break;
    }
}
void CAbilityStatePassive::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit || !applied_) return;
    applied_ = false;
    switch (state_) {
    case CPassiveState::Ethereal:
        unit->RemoveBuff(War3ID("Betl"), game);
        break;
    case CPassiveState::MagicImmune:
        unit->RemoveBuff(War3ID("Bmim"), game);
        break;
    case CPassiveState::Resistant:
        unit->RemoveBuff(War3ID("Brsk"), game);
        break;
    case CPassiveState::Ghost:
        unit->SetInvisible(false);
        unit->SetCollisionSize(savedCollision_);
        break;
    }
}
CAbilityItemSpellResist::CAbilityItemSpellResist(int handleId) : CAbilitySpellBase(handleId, War3ID("AIsr"), CAbilityCategory::ITEM) { iconShowing_ = false; permanent_ = true; SetPassiveSpell(true); SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemSpellResist::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    appliedAttack_ = (int)std::lround(attackBonus_);
    if (appliedAttack_ != 0) unit->AddBonusDamage(appliedAttack_);
    float taken = std::max(0.0f, 1.0f - spellReduction_);
    unit->AddBuff(std::make_unique<CBuffDamageMod>(War3ID("BIsr"), taken, 1.0f, 1.0f, 0.0f), game);
}
void CAbilityItemSpellResist::OnRemove(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    if (appliedAttack_ != 0) unit->AddBonusDamage(-appliedAttack_);
    appliedAttack_ = 0;
    unit->RemoveBuff(War3ID("BIsr"), game);
}
void CAbilityItemSpellResist::ApplyEffectData(const CAbilityDefFull& def, int level) { attackBonus_ = def.DataFloatAt(level, 0, attackBonus_); spellReduction_ = def.DataFloatAt(level, 1, spellReduction_); }
CAbilityItemMixedDefense::CAbilityItemMixedDefense(int handleId) : CAbilitySpellBase(handleId, War3ID("AIdd"), CAbilityCategory::ITEM) { iconShowing_ = false; permanent_ = true; SetPassiveSpell(true); SetTargetType(CSpellTargetType::NO_TARGET); }
void CAbilityItemMixedDefense::OnAdd(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    unit->AddBuff(std::make_unique<CBuffDamageMod>(
        War3ID("BIdd"), spellTaken_, spellTaken_, pierceTaken_, dealtAdd_), game);
}
void CAbilityItemMixedDefense::OnRemove(CSimulation& game, CUnit* unit) { if (unit) unit->RemoveBuff(War3ID("BIdd"), game); }
void CAbilityItemMixedDefense::ApplyEffectData(const CAbilityDefFull& def, int level) {
    pierceTaken_ = def.DataFloatAt(level, 0, pierceTaken_);
    dealtAdd_ = def.DataFloatAt(level, 1, dealtAdd_);
    spellTaken_ = def.DataFloatAt(level, 4, spellTaken_);
}
CAbilityMagicDefense::CAbilityMagicDefense(int handleId) : CAbilitySpellBase(handleId, War3ID("Amdf"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetCooldown(0.0f); SetManaCost(0); }
void CAbilityMagicDefense::TurnOff(CSimulation& game, CUnit* unit) {
    if (!unit || !active_) return;
    unit->RemoveBuff(War3ID("Bmdf"), game);
    unit->ClearNonStackingAttackSpeed(War3ID("Bmdf"));
    active_ = false;
    appliedFactor_ = 1.0f;
}
void CAbilityMagicDefense::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (active_) {
        TurnOff(game, caster);
        return;
    }
    caster->AddBuff(std::make_unique<CBuffMagicImmune>(1.0e9f, War3ID("Bmdf")), game);
    appliedFactor_ = ClampedFactor(speedPenalty_);
    caster->SetNonStackingAttackSpeed(War3ID("Bmdf"), appliedFactor_ - 1.0f);
    active_ = true;
}
void CAbilityMagicDefense::OnRemove(CSimulation& game, CUnit* unit) { TurnOff(game, unit); }
void CAbilityMagicDefense::ApplyEffectData(const CAbilityDefFull& def, int level) { speedPenalty_ = def.DataFloatAt(level, 3, speedPenalty_); }
CAbilityMagicLeash::CAbilityMagicLeash(int handleId) : CAbilitySpellBase(handleId, War3ID("Amls"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(600.0f); SetCooldown(12.0f); SetManaCost(50); }
void CAbilityMagicLeash::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (!unit || unit->IsDead()) return;
    if (!unit->CheckSpellEffectReaction(game, caster)) return;
    unit->AddBuff(std::make_unique<CBuffStun>(duration_), game);
    unit->AddBuff(std::make_unique<CBuffPoison>(duration_, damagePerSecond_), game);
    unit->Damage(game, caster, false, false, CAttackType::SPELLS, CDamageType::MAGIC,
                 "", damagePerSecond_, 0);
}
void CAbilityMagicLeash::ApplyEffectData(const CAbilityDefFull& def, int level) { damagePerSecond_ = def.DataFloatAt(level, 0, damagePerSecond_); duration_ = def.DurationAt(level); }
CAbilityReplenish::CAbilityReplenish(int handleId, War3ID code, bool restoreLife) : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL) , restoreLife_(restoreLife) { SetTargetType(CSpellTargetType::NO_TARGET); SetManaCost(5); SetCooldown(1.0f); }
void CAbilityReplenish::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster || amount_ <= 0.0f) return;
    ForEachPassiveAllyInRange(game, caster, caster->GetX(), caster->GetY(), radius_, true,
                              [&game, restoreLife = restoreLife_, amount = amount_](CUnit* other) {
                                  if (restoreLife) {
                                      const float missing = other->GetMaxLife() - other->GetLife();
                                      if (missing > 0.0f) {
                                          other->SetLife(game, other->GetLife() + std::min(amount, missing));
                                      }
                                  } else {
                                      const float missing = static_cast<float>(other->GetMaxMana()) - other->GetMana();
                                      if (missing > 0.0f) {
                                          other->SetMana(other->GetMana() + std::min(amount, missing));
                                      }
                                  }
                              });
}
void CAbilityReplenish::ApplyEffectData(const CAbilityDefFull& def, int level) { amount_ = def.DataFloatAt(level, 0, amount_); radius_ = def.AreaAt(level); }
CAbilitySphere::CAbilitySphere(int handleId) : CAbility(handleId, War3ID("Asph"), CAbilityCategory::SPELL) { iconShowing_ = false; permanent_ = true; }
// === spells28.cpp ===
namespace {
void MorphIfKnownImpl(CSimulation& game, CUnit* unit, War3ID formId) {
    if (!unit) return;
    CUnitData* data = game.GetUnitData();
    CUnitType* form = data ? data->GetUnitType(formId) : nullptr;
    if (form) unit->MorphTo(form, formId);
}
CAbility* FindCargo(CUnit* unit) {
    if (!unit) return nullptr;
    if (CAbility* burrow = unit->GetAbility(War3ID("Abun"))) {
        return burrow;
    }
    return unit->GetAbility(War3ID("Acar"));
}
void SetHarvestDisabled(CUnit* unit, bool disabled) {
    if (!unit) return;
    if (auto* harvest = unit->GetAbility(War3ID("Ahar"))) {
        harvest->SetDisabled(disabled, CAbilityDisableType::TRANSFORMATION);
    }
}
void SendBackToWork(CSimulation& game, CUnit* unit) {
    CAbility* harvest = unit ? unit->GetAbility(War3ID("Ahar")) : nullptr;
    if (!harvest || harvest->GetLastTargetHandle() < 0) return;
    CWidget* target = game.GetWidget(harvest->GetLastTargetHandle());
    if (!target || target->IsDead()) return;
    unit->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, target));
}
void ReturnCarriedResources(CSimulation& game, CUnit* unit) {
    CAbility* harvest = unit ? unit->GetAbility(War3ID("Ahar")) : nullptr;
    if (!harvest || harvest->GetCarriedAmount() <= 0) return;
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        if (harvest->GetCarriedResourceType() == CResourceType::GOLD) {
            player->AddGold(harvest->GetCarriedAmount());
        } else if (harvest->GetCarriedResourceType() == CResourceType::LUMBER) {
            player->AddLumber(harvest->GetCarriedAmount());
        }
    }
    harvest->ClearCarried();
}
class CBuffMilitia : public CBuffCloneable<CBuffMilitia> {
public:
    CBuffMilitia(float duration, War3ID formId)
        : CBuffCloneable(War3ID("Bmil"), duration, true), formId_(formId) {}
    void OnApply(CSimulation& game, CUnit* target) override {
        if (!target) return;
        SetHarvestDisabled(target, true);
        MorphIfKnownImpl(game, target, formId_);
    }
    void OnRemove(CSimulation& game, CUnit* target) override {
        if (!target) return;
        target->Unmorph();
        SetHarvestDisabled(target, false);
        if (CAbility* militia = target->GetAbility(War3ID("Amil"))) {
            militia->SetConverted(false);
        }
        SendBackToWork(game, target);
    }
private:
    War3ID formId_;
};
}
CAbilityPairing::CAbilityPairing(int handleId, War3ID code, CPairMode mode)
    : CAbilitySpellBase(handleId, code, CAbilityCategory::SPELL), mode_(mode) {
    SetManaCost(0);
    SetCooldown(0.0f);
    partnerCode_ = code;
    resultId_ = code;
    switch (mode) {
    case CPairMode::MilitiaCall:
        partnerCode_ = War3ID("Amil");
        radius_ = 512.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    case CPairMode::Militia:
        partnerCode_ = War3ID("Amic");
        resultId_ = War3ID("hmil");
        radius_ = 512.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    case CPairMode::Mount:
        partnerCode_ = War3ID("Acoh");
        resultId_ = War3ID("ehpr");
        radius_ = 200.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    case CPairMode::LoadPartner:
        partnerCode_ = War3ID("Acoa");
        resultId_ = War3ID("ehpr");
        radius_ = 200.0f;
        SetTargetType(CSpellTargetType::TARGET_UNIT);
        SetCastRange(200.0f);
        break;
    case CPairMode::Couple:
        partnerCode_ = War3ID("Acoi");
        resultId_ = War3ID("ecop");
        radius_ = 200.0f;
        SetTargetType(CSpellTargetType::NO_TARGET);
        break;
    }
}
void CAbilityPairing::MorphIfKnown(CSimulation& game, CUnit* unit, War3ID formId) { MorphIfKnownImpl(game, unit, formId); }
CUnit* CAbilityPairing::FindPartner(CSimulation& game, CUnit* self) const {
    if (!self) return nullptr;
    CUnit* best = nullptr;
    float bestDist = radius_ * radius_;
    for (CUnit* other : game.GetUnits()) {
        if (!other || other == self || other->IsDead() || other->IsLoadedAboard()) continue;
        if (other->GetPlayerIndex() != self->GetPlayerIndex()) continue;
        if (!other->GetAbility(partnerCode_)) continue;
        const float dx = other->GetX() - self->GetX();
        const float dy = other->GetY() - self->GetY();
        const float dist = dx * dx + dy * dy;
        if (dist > bestDist) continue;
        bestDist = dist;
        best = other;
    }
    return best;
}
bool CAbilityPairing::Convert(CSimulation& game, CUnit* self) {
    if (!self || converted_) return converted_;
    if (mode_ == CPairMode::Militia) {
        if (!FindPartner(game, self)) return false;
        ReturnCarriedResources(game, self);
        SetHarvestDisabled(self, true);
        if (militiaDuration_ > 0.0f) {
            self->AddBuff(std::make_unique<CBuffMilitia>(militiaDuration_, resultId_), game);
        } else {
            MorphIfKnown(game, self, resultId_);
        }
        converted_ = true;
        return true;
    }
    return false;
}
void CAbilityPairing::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    if (mode_ == CPairMode::Militia && converted_) {
        if (caster->GetBuff(War3ID("Bmil"))) {
            caster->RemoveBuff(War3ID("Bmil"), game);
        } else {
            caster->Unmorph();
            SetHarvestDisabled(caster, false);
            converted_ = false;
            SendBackToWork(game, caster);
        }
        return;
    }
    if (mode_ == CPairMode::MilitiaCall) {
        for (CUnit* other : game.GetUnits()) {
            if (!other || other->IsDead()) continue;
            if (other->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
            const float dx = other->GetX() - caster->GetX();
            const float dy = other->GetY() - caster->GetY();
            if (dx * dx + dy * dy > radius_ * radius_) continue;
            if (CAbility* militia = other->GetAbility(War3ID("Amil"))) {
                militia->Convert(game, other);
            }
        }
        return;
    }
    if (mode_ == CPairMode::Militia) {
        Convert(game, caster);
        return;
    }
    CUnit* partner = FindPartner(game, caster);
    if (!partner) return;
    if (mode_ == CPairMode::Mount || mode_ == CPairMode::LoadPartner) {
        CUnit* mount = (mode_ == CPairMode::Mount) ? partner : caster;
        CUnit* rider = (mode_ == CPairMode::Mount) ? caster : partner;
        if (mount->LoadUnit(rider)) {
            MorphIfKnown(game, mount, resultId_);
            converted_ = true;
        }
        return;
    }
    if (mode_ == CPairMode::Couple) {
        MorphIfKnown(game, caster, resultId_);
        partner->SetLife(game, 0);
        converted_ = true;
    }
}
void CAbilityPairing::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* unit = dynamic_cast<CUnit*>(target);
    if (mode_ == CPairMode::LoadPartner && caster && unit && unit->GetAbility(partnerCode_)) {
        if (caster->LoadUnit(unit)) {
            MorphIfKnown(game, caster, resultId_);
            converted_ = true;
        }
        return;
    }
    DoEffect(game, caster);
}
void CAbilityPairing::ApplyEffectData(const CAbilityDefFull& def, int level) { (void)def; (void)level; }
CAbilityBlight::CAbilityBlight(int handleId) : CAbility(handleId, War3ID("Abli"), CAbilityCategory::BLIGHT) { iconShowing_ = false; permanent_ = true; }
void CAbilityBlight::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || !creates_) return;
    accum_ += game.GetSimStepTime();
    if (accum_ < interval_) return;
    accum_ = 0.0f;
    if (current_ >= area_) return;
    current_ = std::min(area_, current_ + expansion_);
    game.SetBlightCircle(unit->GetX(), unit->GetY(), current_, true);
}
CAbilityBlightedGoldMine::CAbilityBlightedGoldMine(int handleId) : CAbility(handleId, War3ID("Abgm"), CAbilityCategory::GOLD_MINE) { iconShowing_ = false; permanent_ = true; }
bool CAbilityBlightedGoldMine::AddMiner() {
    if (miners_ >= maxMiners_) return false;
    miners_++;
    return true;
}
void CAbilityBlightedGoldMine::RemoveMiner() {
    if (miners_ > 0) miners_--;
}
void CAbilityBlightedGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || miners_ <= 0 || gold_ <= 0) return;
    accum_ += game.GetSimStepTime();
    const float interval = interval_ * ((float)maxMiners_ / (float)miners_);
    if (accum_ < interval) return;
    accum_ -= interval;
    const int gain = std::min(gold_, goldPerInterval_);
    gold_ -= gain;
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        player->AddGold(gain);
    }
}
CAbilityAcolyteHarvest::CAbilityAcolyteHarvest(int handleId) : CAbilitySpellBase(handleId, War3ID("Aaha"), CAbilityCategory::HARVEST) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(100.0f); SetManaCost(0); }
bool CAbilityAcolyteHarvest::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId,
                                           CWidget* target) const {
    if (!CAbilitySpellBase::CheckCanTarget(game, unit, orderId, target)) return false;
    auto* mineUnit = dynamic_cast<CUnit*>(target);
    if (!mineUnit || !unit) return false;
    if (mineUnit->GetPlayerIndex() != unit->GetPlayerIndex()) return false;
    return mineUnit->GetAbility(War3ID("Abgm")) != nullptr;
}
void CAbilityAcolyteHarvest::Unbind(CSimulation& game) {
    if (mineUnitHandle_ < 0) return;
    if (CUnit* mineUnit = game.GetUnit(mineUnitHandle_)) {
        if (CAbility* mine = mineUnit->GetAbility(War3ID("Abgm"))) {
            mine->RemoveMiner();
        }
    }
    mineUnitHandle_ = -1;
}
void CAbilityAcolyteHarvest::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* mineUnit = dynamic_cast<CUnit*>(target);
    if (!caster || !mineUnit) return;
    CAbility* mine = mineUnit->GetAbility(War3ID("Abgm"));
    if (!mine) return;
    if (mineUnitHandle_ == mineUnit->GetHandleId()) return;
    Unbind(game);
    if (!mine->AddMiner()) return;
    mineUnitHandle_ = mineUnit->GetHandleId();
}
void CAbilityAcolyteHarvest::OnDeath(CSimulation& game, CUnit* unit) { (void)unit; Unbind(game); }
void CAbilityAcolyteHarvest::OnRemove(CSimulation& game, CUnit* unit) { (void)unit; Unbind(game); }
CAbilityWispHarvest::CAbilityWispHarvest(int handleId) : CAbilitySpellBase(handleId, War3ID("Awha"), CAbilityCategory::HARVEST) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(100.0f); SetManaCost(0); }
void CAbilityWispHarvest::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)game;
    auto* tree = dynamic_cast<CDestructable*>(target);
    if (!caster || !tree || tree->IsDead()) return;
    treeHandle_ = tree->GetHandleId();
    accum_ = interval_;
    caster->SetPosition(tree->GetX(), tree->GetY());
}
void CAbilityWispHarvest::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit) return;
    CDestructable* tree =
        treeHandle_ >= 0 ? game.GetDestructable(treeHandle_) : nullptr;
    if (!tree || tree->IsDead()) {
        treeHandle_ = -1;
        return;
    }
    accum_ += game.GetSimStepTime();
    if (accum_ < interval_) return;
    accum_ -= interval_;
    tree->Damage(game, unit, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                  "", damagePerInterval_, 0);
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        player->AddLumber(lumberPerInterval_);
    }
    if (tree->IsDead()) treeHandle_ = -1;
}
CAbilityHarvestLumber::CAbilityHarvestLumber(int handleId) : CAbilitySpellBase(handleId, War3ID("Ahrl"), CAbilityCategory::HARVEST) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(128.0f); SetManaCost(0); }
void CAbilityHarvestLumber::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* tree = dynamic_cast<CDestructable*>(target);
    if (!caster || !tree || tree->IsDead()) return;
    tree->Damage(game, caster, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                 "", damageToTree_, 0);
    if (CAbility* harvest = caster->GetAbility(War3ID("Ahar"))) {
        harvest->SetCarried(CResourceType::LUMBER, capacity_);
        return;
    }
    if (CPlayer* player = game.GetPlayer(caster->GetPlayerIndex())) {
        player->AddLumber(capacity_);
    }
}
CAbilityCargoHold::CAbilityCargoHold(int handleId, War3ID code, bool burrow, int capacity) : CAbility(handleId, code, CAbilityCategory::CARGO_HOLD), burrow_(burrow), capacity_(capacity) { iconShowing_ = false; permanent_ = true; }
void CAbilityCargoHold::OnAdd(CSimulation&, CUnit* unit) { if (burrow_ && unit) unit->SetAttacksDisabled(true); }
void CAbilityCargoHold::OnRemove(CSimulation&, CUnit* unit) { UnloadAll(unit); if (burrow_ && unit) unit->SetAttacksDisabled(false); }
bool CAbilityCargoHold::TryLoad(CUnit* holder, CUnit* passenger) {
    if (!holder || !passenger) return false;
    if ((int)holder->GetCargoUnits().size() >= capacity_) return false;
    if (!holder->LoadUnit(passenger)) return false;
    if (burrow_) {
        holder->SetAttacksDisabled(false);
        holder->MultiplyAttackSpeed(2.0f);
        speedSteps_++;
    }
    return true;
}
void CAbilityCargoHold::UnloadAll(CUnit* holder) {
    if (!holder) return;
    std::vector<CUnit*> cargo = holder->GetCargoUnits();
    for (CUnit* passenger : cargo) {
        holder->UnloadUnitAt(passenger, holder->GetX(), holder->GetY());
        if (burrow_ && speedSteps_ > 0) {
            holder->DivideAttackSpeed(2.0f);
            speedSteps_--;
        }
    }
    if (burrow_ && holder->GetCargoUnits().empty()) {
        holder->SetAttacksDisabled(true);
    }
}
CAbilityDropInstant::CAbilityDropInstant(int handleId, War3ID code) : CAbilitySpellBase(handleId, code, CAbilityCategory::CARGO_HOLD) { SetTargetType(CSpellTargetType::NO_TARGET); SetManaCost(0); }
void CAbilityDropInstant::DoEffect(CSimulation&, CUnit* caster) {
    if (!caster) return;
    if (CAbility* cargo = FindCargo(caster)) {
        cargo->UnloadAll(caster);
        return;
    }
    std::vector<CUnit*> aboard = caster->GetCargoUnits();
    for (CUnit* passenger : aboard) {
        caster->UnloadUnitAt(passenger, caster->GetX(), caster->GetY());
    }
}
CAbilityWayGate::CAbilityWayGate(int handleId) : CAbility(handleId, War3ID("Awrp"), CAbilityCategory::MOVEMENT) { iconShowing_ = false; permanent_ = true; }
void CAbilityWayGate::SetDestination(CUnit* gate, float x, float y) {
    destX_ = x;
    destY_ = y;
    enabled_ = true;
    if (!gate) return;
    gate->SetWaygateDestination(x, y);
    gate->SetWaygateActive(true);
}
bool CAbilityWayGate::Inside(const CUnit* gate, const CUnit* target) const {
    if (!gate || !target) return false;
    return std::fabs(target->GetX() - gate->GetX()) <= width_ * 0.5f
        && std::fabs(target->GetY() - gate->GetY()) <= height_ * 0.5f;
}
bool CAbilityWayGate::TryTeleport(CUnit* gate, CUnit* target) const {
    if (!enabled_ || !gate || !target || target == gate) return false;
    if (target->IsDead() || target->IsBuilding() || target->IsPaused()) return false;
    if (target->IsLoadedAboard()) return false;
    if (!Inside(gate, target)) return false;
    target->SetPosition(destX_, destY_);
    return true;
}
bool CAbilityWayGate::CheckCanTarget(CSimulation&, CUnit* unit, int orderId, CWidget* target) const {
    if (!enabled_ || orderId != OrderIds::SMART) return false;
    auto* unitTarget = dynamic_cast<CUnit*>(target);
    if (!unitTarget || unitTarget->IsDead() || unitTarget->IsBuilding()) return false;
    return Inside(unit, unitTarget);
}
bool CAbilityWayGate::CheckBeforeQueue(CSimulation&, CUnit* caster, int orderId, CWidget* target) { if (orderId != OrderIds::SMART) return true; TryTeleport(caster, dynamic_cast<CUnit*>(target)); return false; }
CBehavior* CAbilityWayGate::Begin(CSimulation&, CUnit* caster, int, CWidget* target) { TryTeleport(caster, dynamic_cast<CUnit*>(target)); return nullptr; }
CAbilityShopPurchase::CAbilityShopPurchase(int handleId) : CAbility(handleId, War3ID("Apit"), CAbilityCategory::SELL_ITEMS) { iconShowing_ = false; permanent_ = true; }
CAbilityNeutralBuilding::CAbilityNeutralBuilding(int handleId, War3ID code, bool onlyAllies) : CAbility(handleId, code, CAbilityCategory::NEUTRAL_BUILDING), onlyAllies_(onlyAllies) { iconShowing_ = false; permanent_ = true; }
void CAbilityNeutralBuilding::OnTick(CSimulation& game, CUnit* unit) {
    accum_ += game.GetSimStepTime();
    if (primed_ && accum_ < interval_) return;
    primed_ = true;
    accum_ = 0.0f;
    Rescan(game, unit);
}
bool CAbilityNeutralBuilding::CanSelect(const CSimulation& game, const CUnit* shop,
                                         const CUnit* other) const {
    if (!shop || !other || other == shop || other->IsDead() || other->IsBuilding()) return false;
    if (other->IsLoadedAboard()) return false;
    if (onlyAllies_) {
        const CPlayer* shopPlayer = game.GetPlayer(shop->GetPlayerIndex());
        const CPlayer* otherPlayer = game.GetPlayer(other->GetPlayerIndex());
        if (!shopPlayer || !otherPlayer ||
            !shopPlayer->HasAlliance(*otherPlayer, CAllianceType::ALLIED)) {
            return false;
        }
    }
    const float dx = other->GetX() - shop->GetX();
    const float dy = other->GetY() - shop->GetY();
    return dx * dx + dy * dy <= radius_ * radius_;
}
void CAbilityNeutralBuilding::Rescan(CSimulation& game, CUnit* shop) {
    if (!shop) return;
    bool search = false;
    for (int i = 0; i < kPlayerSlots; i++) {
        if (!CanSelect(game, shop, selected_[i])) {
            selected_[i] = nullptr;
            search = true;
        }
    }
    if (!search) return;
    for (CUnit* other : game.GetUnits()) {
        if (!other) continue;
        const int player = other->GetPlayerIndex();
        if (player < 0 || player >= kPlayerSlots) continue;
        if (selected_[player] != nullptr) continue;
        if (CanSelect(game, shop, other)) selected_[player] = other;
    }
}
CUnit* CAbilityNeutralBuilding::GetActiveUnit() const {
    for (CUnit* unit : selected_) {
        if (unit) return unit;
    }
    return nullptr;
}
CUnit* CAbilityNeutralBuilding::GetSelectedUnit(int playerIndex) const {
    if (playerIndex < 0 || playerIndex >= kPlayerSlots) return nullptr;
    return selected_[playerIndex];
}
bool CAbilityNeutralBuilding::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId,
                                             CWidget* target) const {
    if (orderId != OrderIds::NEUTRALINTERACT) {
        if (orderId < 852462 || orderId > 852465) return false;
        auto* hero = dynamic_cast<CUnit*>(target);
        return hero && !hero->IsDead() && hero->IsHero();
    }
    return CanSelect(game, unit, dynamic_cast<CUnit*>(target));
}
CBehavior* CAbilityNeutralBuilding::Begin(CSimulation& game, CUnit* caster, int orderId,
                                          CWidget* target) {
    auto* unitTarget = dynamic_cast<CUnit*>(target);
    if (orderId >= 852462 && orderId <= 852465 && unitTarget) {
        HireFromTavern(game, caster, unitTarget);
        return nullptr;
    }
    if (unitTarget && CanSelect(game, caster, unitTarget)) {
        const int player = unitTarget->GetPlayerIndex();
        if (player >= 0 && player < kPlayerSlots) {
            selected_[player] = unitTarget;
            shopper_[player] = unitTarget;
        }
    }
    return nullptr;
}
bool CAbilityNeutralBuilding::HireFromTavern(CSimulation& game, CUnit* tavern, CUnit* hero) {
    if (!tavern || !hero || hero->IsDead() || !hero->IsHero()) return false;
    int buyer = -1;
    for (int i = 0; i < kPlayerSlots; i++) {
        CUnit* shopper = shopper_[i];
        if (!shopper || shopper->IsDead() || shopper->GetPlayerIndex() != i) continue;
        if (!CanSelect(game, tavern, shopper)) continue;
        if (buyer >= 0) return false;
        buyer = i;
    }
    CPlayer* player = buyer >= 0 ? game.GetPlayer(buyer) : nullptr;
    if (!player) return false;
    if (hero->GetPlayerIndex() == buyer) return false;
    if (!CanSelect(game, tavern, hero)) return false;
    int aliveHeroes = 0;
    for (CUnit* unit : game.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetPlayerIndex() == buyer &&
            unit->IsHero()) {
            aliveHeroes++;
        }
    }
    if (aliveHeroes >= 3) return false;
    const CUnitType* heroType =
        game.GetUnitData() ? game.GetUnitData()->GetUnitType(hero->GetTypeId()) : nullptr;
    if (!heroType) return false;
    for (const auto& req : heroType->GetRequirementsTier(aliveHeroes)) {
        if (player->GetEffectiveTechLevel(game, req.id) < (req.level > 0 ? req.level : 1)) {
            return false;
        }
    }
    if (heroType->GetFoodUsed() > 0 &&
        player->GetFoodUsed() + heroType->GetFoodUsed() > player->GetFoodCap()) {
        return false;
    }
    const auto& gameplay = game.GetGameplayConstants();
    const int level = hero->GetHeroLevel() > 0 ? hero->GetHeroLevel() : 1;
    const int goldCost = gameplay.GetHeroAwakenGoldCost(heroType->GetGoldCost(), level);
    const int lumberCost = gameplay.GetHeroAwakenLumberCost(heroType->GetLumberCost(), level);
    if (player->GetGold() < goldCost || player->GetLumber() < lumberCost) return false;
    player->AddGold(-goldCost);
    player->AddLumber(-lumberCost);
    if (CPlayer* from = game.GetPlayer(hero->GetPlayerIndex())) from->RemoveUnit(hero);
    hero->SetPlayerIndex(buyer);
    player->AddUnit(hero);
    hero->SetPosition(tavern->GetX() + 128.0f, tavern->GetY());
    hero->SetLife(game, hero->GetMaxLife() * gameplay.heroAwakenLifeFactor);
    hero->SetMana(hero->GetMaxMana() * gameplay.heroAwakenManaFactor);
    return true;
}
// === spells29.cpp ===
namespace {
constexpr int kEntangleInstant = 852148;
void SetMineWorkDisabled(CUnit* unit, bool disabled) {
    if (!unit) return;
    const War3ID codes[] = {War3ID("Aegm"), War3ID("Aenc"), War3ID("Agl2")};
    for (War3ID code : codes) {
        if (CAbility* ability = unit->GetAbility(code)) {
            ability->SetDisabled(disabled, CAbilityDisableType::CONSTRUCTION);
        }
    }
}
}
CAbilityDrunkenBrawler::CAbilityDrunkenBrawler(int handleId) : CAbilitySpellBase(handleId, War3ID("ANdb"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetPassiveSpell(true); SetAlias(War3ID("ANdb")); }
void CAbilityDrunkenBrawler::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (!unit) return;
    unit->AddCritSource(GetAlias(), critChance_, critMultiplier_);
    unit->AddEvasionSource(GetAlias(), dodge_);
}
void CAbilityDrunkenBrawler::OnRemove(CSimulation& game, CUnit* unit) { (void)game; if (!unit) return; unit->RemoveCritSource(GetAlias()); unit->RemoveEvasionSource(GetAlias()); }
void CAbilityDrunkenBrawler::ApplyEffectData(const CAbilityDefFull& def, int level) {
    critChance_ = def.DataFloatAt(level, 0, critChance_ * 100.0f) / 100.0f;
    critMultiplier_ = def.DataFloatAt(level, 1, critMultiplier_);
    dodge_ = def.DataFloatAt(level, 3, dodge_);
}
CAbilityEntangleGoldMine::CAbilityEntangleGoldMine(int handleId) : CAbilitySpellBase(handleId, War3ID("Aent"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::TARGET_UNIT); SetCastRange(200.0f); SetManaCost(0); }
bool CAbilityEntangleGoldMine::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId,
                                             CWidget* target) const {
    if (replacement_ && !replacement_->IsDead()) return false;
    if (!CAbilitySpellBase::CheckCanTarget(game, unit, orderId, target)) return false;
    auto* mine = dynamic_cast<CUnit*>(target);
    if (!mine) return false;
    return mine->GetAbility(War3ID("Agld")) != nullptr;
}
void CAbilityEntangleGoldMine::Release(CSimulation& game, bool killReplacement) {
    if (replacement_ && killReplacement && !replacement_->IsDead()) {
        replacement_->SetLife(game, 0);
    }
    if (original_) {
        original_->SetHidden(false);
        original_->SetPaused(false);
    }
    replacement_ = nullptr;
    original_ = nullptr;
    constructing_ = false;
}
CBehavior* CAbilityEntangleGoldMine::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) { instant_ = (orderId == kEntangleInstant); return CAbilitySpellBase::Begin(game, caster, orderId, target); }
void CAbilityEntangleGoldMine::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    auto* mineUnit = dynamic_cast<CUnit*>(target);
    if (!caster || !mineUnit || mineUnit->IsDead()) return;
    CAbility* gold = mineUnit->GetAbility(War3ID("Agld"));
    if (!gold || !gold->IsResourceMine()) return;
    if (replacement_ && !replacement_->IsDead()) return;
    CUnit* created = game.CreateUnit(resultId_, caster->GetPlayerIndex(),
                                     mineUnit->GetX(), mineUnit->GetY(), 0);
    if (!created) return;
    mineUnit->SetHidden(true);
    mineUnit->SetPaused(true);
    original_ = mineUnit;
    replacement_ = created;
    CAbility* overlay = created->GetAbility(War3ID("Agl2"));
    if (!overlay) {
        auto made = AbilityFactory::CreateAbility(game.AllocateHandleId(), War3ID("Agl2"));
        overlay = made.get();
        if (made) created->AddAbility(game, std::move(made));
    }
    if (overlay) {
        overlay->BindParentMineUnit(mineUnit);
    }
    auto mine = std::make_unique<CAbilityEntangledMine>(game.AllocateHandleId());
    mine->SetParent(mineUnit);
    created->AddAbility(game, std::move(mine));
    if (!created->GetAbility(War3ID("Aenc"))) {
        if (auto cargo = AbilityFactory::CreateAbility(game.AllocateHandleId(), War3ID("Aenc"))) {
            created->AddAbility(game, std::move(cargo));
        }
    }
    if (!instant_) {
        created->SetConstructing(true);
        created->SetConstructionProgress(0);
        created->SetLife(game, created->GetMaxLife() * 0.1f);
        SetMineWorkDisabled(created, true);
        constructing_ = true;
    }
}
void CAbilityEntangleGoldMine::AdvanceConstruction(CSimulation& game) {
    if (!constructing_ || !replacement_ || replacement_->IsDead()) return;
    float buildTime = 30.0f;
    if (const CUnitType* type = replacement_->GetUnitType()) {
        if (type->GetBuildTime() > 0) buildTime = static_cast<float>(type->GetBuildTime());
    }
    float progress = std::min(buildTime, replacement_->GetConstructionProgress() + game.GetSimStepTime());
    replacement_->SetConstructionProgress(progress);
    const float fraction = 0.1f + 0.9f * (progress / buildTime);
    replacement_->SetLife(game, replacement_->GetMaxLife() * fraction);
    if (progress >= buildTime) {
        replacement_->SetConstructing(false);
        replacement_->SetLife(game, replacement_->GetMaxLife());
        SetMineWorkDisabled(replacement_, false);
        constructing_ = false;
    }
}
void CAbilityEntangleGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    (void)unit;
    if (replacement_ && replacement_->IsDead()) {
        Release(game, false);
        return;
    }
    AdvanceConstruction(game);
}
void CAbilityEntangleGoldMine::OnRemove(CSimulation& game, CUnit* unit) { (void)unit; Release(game, true); }
CAbilityEntangledMine::CAbilityEntangledMine(int handleId) : CAbility(handleId, War3ID("Aegm"), CAbilityCategory::GOLD_MINE) { iconShowing_ = false; permanent_ = true; }
void CAbilityEntangledMine::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || unit->IsDead() || !parent_) return;
    accum_ += game.GetSimStepTime();
    if (accum_ < interval_) return;
    accum_ -= interval_;
    if (CAbility* cargo = unit->GetAbility(War3ID("Aenc"))) {
        const int cap = cargo->GetCargoCapacity();
        if (cap <= 0) return;
        index_ = (index_ + 1) % cap;
        if (index_ >= (int)unit->GetCargoUnits().size()) return;
    }
    CAbility* gold = parent_->GetAbility(War3ID("Agld"));
    if (!gold || gold->GetGold() <= 0) {
        unit->SetLife(game, 0);
        return;
    }
    const int gain = std::min(gold->GetGold(), goldPerInterval_);
    gold->SetGold(gold->GetGold() - gain);
    if (CPlayer* player = game.GetPlayer(unit->GetPlayerIndex())) {
        player->AddGold(gain);
    }
    if (gold->GetGold() <= 0) unit->SetLife(game, 0);
}
CAbilityChannelTest::CAbilityChannelTest(int handleId) : CAbilitySpellBase(handleId, War3ID("ANcl"), CAbilityCategory::SPELL) { SetTargetType(CSpellTargetType::NO_TARGET); SetManaCost(0); }
void CAbilityChannelTest::DoEffect(CSimulation&, CUnit* caster) { (void)caster; channeling_ = true; remaining_ = artDuration_; }
void CAbilityChannelTest::OnTick(CSimulation& game, CUnit* unit) {
    (void)unit;
    if (!channeling_) return;
    remaining_ -= game.GetSimStepTime();
    if (remaining_ <= 0.0f) {
        remaining_ = 0.0f;
        channeling_ = false;
    }
}

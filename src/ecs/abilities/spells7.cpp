#include "spells7.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Serpent Ward =====
CAbilitySerpentWard::CAbilitySerpentWard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOsw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilitySerpentWard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABActionCreateUnitAtPoint ward(wardType_, x, y, 0);
    ward.Run(game, caster, nullptr, true, x, y);
}

// ===== Polymorph =====
CAbilityPolymorph::CAbilityPolymorph(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aply"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityPolymorph::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() {
        return std::make_unique<CBuffPolymorph>(duration);
    });
}

void CAbilityPolymorph::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
}

// ===== Inner Fire =====
CAbilityInnerFire::CAbilityInnerFire(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ainf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(35);
}

void CAbilityInnerFire::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, damage = bonusDamage_, armor = bonusArmor_]() {
                         return std::make_unique<CBuffInnerFire>(duration, damage, armor);
                     });
}

// ===== Heal =====
CAbilityHeal::CAbilityHeal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ahea"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(5.0f);
    SetManaCost(65);
}

void CAbilityHeal::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionHealTarget heal(healAmount_);
    heal.Run(game, caster, target, false, 0, 0);
}

// ===== Dispel Magic =====
CAbilityDispelMagic::CAbilityDispelMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Adis"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

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

void CAbilityDispelMagic::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
    summonDamage_ = def.DataFloatAt(level, 1, summonDamage_);
}

// ===== Rejuvenation =====
CAbilityRejuvenation::CAbilityRejuvenation(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Arej"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityRejuvenation::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_, tick = healPerTick_]() {
        return std::make_unique<CBuffRejuvenation>(duration, tick);
    });
}

// ===== Roar =====
CAbilityRoar::CAbilityRoar(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aroa"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityRoar::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, damage = bonusDamage_]() {
                           return std::make_unique<CBuffInnerFire>(duration, damage, 0, War3ID("Broa"));
                       });
}

void CAbilityRoar::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
}

// ===== Defend =====
CAbilityDefend::CAbilityDefend(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Adef"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(5.0f);
    SetManaCost(0);
}

void CAbilityDefend::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() {
        return std::make_unique<CBuffDefend>(duration);
    });
}

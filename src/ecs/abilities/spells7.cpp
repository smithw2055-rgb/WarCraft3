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
    // Migrated onto a single AbilityBuilder exact-point summon (no timed
    // life; graceful without unit data like the code replaced).
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget morph(
        [duration = duration_]() { return std::make_unique<CBuffPolymorph>(duration); });
    morph.Run(game, caster, target, false, 0, 0);
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget fire(
        [duration = duration_, damage = bonusDamage_, armor = bonusArmor_]() {
            return std::make_unique<CBuffInnerFire>(duration, damage, armor);
        });
    fire.Run(game, caster, target, false, 0, 0);
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
    // Migrated onto a single AbilityBuilder heal.
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
    // Migrated onto an AbilityBuilder iteration: per unit, summoned victims
    // burn first (the summon check reads the timed-life marker pre-dispel),
    // then buffs are stripped.
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
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget rejuv(
        [duration = duration_, tick = healPerTick_]() {
            return std::make_unique<CBuffRejuvenation>(duration, tick);
        });
    rejuv.Run(game, caster, target, false, 0, 0);
}

// ===== Roar =====
CAbilityRoar::CAbilityRoar(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aroa"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityRoar::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto an AbilityBuilder iteration: same-player units around
    // the caster gain bonus damage (no armor, unlike Inner Fire proper).
    // Own buff id (Broa, ref stackingKey = buffId) so Roar sums with Inner
    // Fire but re-casts refresh instead of stacking.
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, damage = bonusDamage_]() {
            return std::make_unique<CBuffInnerFire>(duration, damage, 0, War3ID("Broa"));
        }));
    ABActionForEachInRange roar(radius_, std::move(perUnit));
    roar.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    roar.Run(game, caster, nullptr, false, 0, 0);
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
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget defend(
        [duration = duration_]() { return std::make_unique<CBuffDefend>(duration); });
    defend.Run(game, caster, caster, false, 0, 0);
}

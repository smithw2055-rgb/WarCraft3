#include "spells16.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/unit_type.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Devour =====
CAbilityDevour::CAbilityDevour(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Adev"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(100.0f);
}

void CAbilityDevour::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // NOT migrated: swallowing uses the transport/cargo system (LoadUnit)
    // plus a Digest DoT, neither of which a target-triple action chain can
    // address. The only chain-shaped step (digest buff) cannot run without
    // the cargo hold existing first.
    if (!target || !caster || target->IsDead() || target == caster) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || targetUnit->IsHero()) return;
    if (targetUnit->GetPlayerIndex() == caster->GetPlayerIndex()) return;
    if (!targetUnit->GetUnitType()) return;
    // Ground meat only (ref targs); flyers cannot be swallowed.
    if (targetUnit->GetUnitType()->GetMovementType() == PathingGridMovementType::FLY) return;
    if (!caster->LoadUnit(targetUnit)) return;
    targetUnit->AddBuff(std::make_unique<CBuffDigest>(
        digestDuration_, digestPerTick_), game);
}

void CAbilityDevour::ApplyEffectData(const CAbilityDefFull& def, int level) {
    digestPerTick_ = def.DataFloatAt(level, 0, digestPerTick_);
}

// ===== Devour Magic =====
CAbilityDevourMagic::CAbilityDevourMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Advm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(50);
}

void CAbilityDevourMagic::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto a single AbilityBuilder devour (dispel + per-buff
    // mana refund).
    ABActionDevourMagic devour(manaPerBuff_);
    devour.Run(game, caster, target, false, 0, 0);
}

// ===== Cold Arrows =====
CAbilityColdArrows::CAbilityColdArrows(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHca"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityColdArrows::ApplyEffectData(const CAbilityDefFull& def, int level) {
    poisonPerTick_ = def.DataFloatAt(level, 0, poisonPerTick_);
    slowPercent_ = def.DataFloatAt(level, 1, slowPercent_);
    slowDuration_ = def.DurationAt(level);
    poisonDuration_ = def.DataFloatAt(level, 3, poisonDuration_);
}

// ===== Monsoon =====
CAbilityMonsoon::CAbilityMonsoon(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANmo"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityMonsoon::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Migrated onto AbilityBuilder waves (ref Blizzard wave pattern): each
    // wave runs a per-unit COLD damage chain over other-player units in
    // radius. Waves resolve instantly; the total matches the old loop.
    for (int w = 0; w < waves_; w++) {
        ABEffectChain perUnit;
        perUnit.Add(std::make_unique<ABActionDamageTarget>(
            damagePerWave_, CAttackType::SPELLS, CDamageType::COLD));
        ABActionForEachInRange wave(radius_, std::move(perUnit));
        wave.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
        wave.Run(game, caster, nullptr, true, x, y);
    }
}

void CAbilityMonsoon::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damagePerWave_ = def.DataFloatAt(level, 0, damagePerWave_);
    radius_ = def.AreaAt(level);
}

// ===== Absorb Mana =====
CAbilityAbsorbMana::CAbilityAbsorbMana(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aabs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
}

void CAbilityAbsorbMana::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    // Migrated onto AbilityBuilder conditionals: summoned victims burn via
    // KillTarget, and the caster is refunded manaGain_ (clamped to max, ref
    // the original arithmetic). The refund runs against the caster, so it
    // is a second conditional keyed on the captured victim.
    float mana = caster->GetMana() + manaGain_;
    if (mana > (float)caster->GetMaxMana()) mana = (float)caster->GetMaxMana();
    ABEffectChain burn;
    burn.Add(std::make_unique<ABActionKillTarget>());
    ABActionIf killIf(
        std::make_shared<ABCondFunc>(
            [](CSimulation&, CUnit*, CWidget* unitTarget, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                return unit != nullptr && unit->IsSummoned();
            }),
        std::move(burn));
    killIf.Run(game, caster, target, false, 0, 0);
    ABEffectChain refund;
    refund.Add(std::make_unique<ABActionSetManaTarget>(mana));
    ABActionIf refundIf(
        std::make_shared<ABCondFunc>(
            [target](CSimulation&, CUnit*, CWidget*, bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(target);
                return unit != nullptr && unit->IsSummoned();
            }),
        std::move(refund));
    refundIf.Run(game, caster, caster, false, 0, 0);
}

// ===== Unholy Frenzy =====
CAbilityUnholyFrenzy::CAbilityUnholyFrenzy(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Auhf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityUnholyFrenzy::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    // Migrated onto a single AbilityBuilder buff attach (Berserk without
    // the weak-skin half: damage-taken multiplier 1.0).
    ABActionAddBuffTarget frenzy(
        [duration = duration_, attack = attackSpeedMul_]() {
            return std::make_unique<CBuffBerserk>(duration, attack, 1.0f);
        });
    frenzy.Run(game, caster, target, false, 0, 0);
}

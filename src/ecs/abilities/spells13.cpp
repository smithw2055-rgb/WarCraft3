#include "spells13.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Metamorphosis =====
CAbilityMetamorphosis::CAbilityMetamorphosis(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEme"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(150);
}

void CAbilityMetamorphosis::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach (the type swap
    // lives in CBuffTransform itself).
    if (!caster) return;
    ABActionAddBuffTarget morph(
        [duration = duration_]() {
            return std::make_unique<CBuffTransform>(
                War3ID("BEme"), duration, War3ID("Edem"));
        });
    morph.Run(game, caster, caster, false, 0, 0);
}

// ===== Bear Form =====
CAbilityBearForm::CAbilityBearForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Abrf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
}

void CAbilityBearForm::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget morph(
        [duration = duration_]() {
            return std::make_unique<CBuffTransform>(
                War3ID("Abrf"), duration, War3ID("edcm"));
        });
    morph.Run(game, caster, caster, false, 0, 0);
}

// ===== Raven Form =====
CAbilityRavenForm::CAbilityRavenForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Arav"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
}

void CAbilityRavenForm::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget morph(
        [duration = duration_]() {
            return std::make_unique<CBuffTransform>(
                War3ID("Arav"), duration, War3ID("edot"));
        });
    morph.Run(game, caster, caster, false, 0, 0);
}

// ===== Robo-Goblin =====
CAbilityRoboGoblin::CAbilityRoboGoblin(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANrg"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(1.0f);
    SetManaCost(25);
}

void CAbilityRoboGoblin::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget morph(
        [duration = duration_]() {
            return std::make_unique<CBuffTransform>(
                War3ID("ANrg"), duration, War3ID("Ntin"));
        });
    morph.Run(game, caster, caster, false, 0, 0);
}

// ===== Stone Form =====
CAbilityStoneForm::CAbilityStoneForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Astn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
}

void CAbilityStoneForm::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder buff attach.
    if (!caster) return;
    ABActionAddBuffTarget statue(
        [duration = duration_]() { return std::make_unique<CBuffStoneForm>(duration); });
    statue.Run(game, caster, caster, false, 0, 0);
}

// ===== Avatar =====
CAbilityAvatar::CAbilityAvatar(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHav"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(150);
}

void CAbilityAvatar::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget avatar(
        [duration = duration_, damage = bonusDamage_, life = bonusLife_]() {
            return std::make_unique<CBuffAvatar>(duration, damage, life);
        });
    avatar.Run(game, caster, caster, false, 0, 0);
}

// ===== Siphon Mana =====
CAbilitySiphonMana::CAbilitySiphonMana(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHdr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(10);
}

void CAbilitySiphonMana::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto a single AbilityBuilder buff attach (the caster handle
    // rides in the factory; dead/non-unit targets are skipped by the action).
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    ABActionAddBuffTarget siphon(
        [duration = duration_, tick = manaPerTick_, casterHandle]() {
            return std::make_unique<CBuffManaSiphon>(duration, tick, casterHandle);
        });
    siphon.Run(game, caster, target, false, 0, 0);
}

// ===== Big Bad Voodoo =====
CAbilityBigBadVoodoo::CAbilityBigBadVoodoo(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOvd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(200);
}

void CAbilityBigBadVoodoo::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Migrated onto an AbilityBuilder iteration: same-player units around
    // the caster gain Divine Shield. The caster is included (distance 0 is
    // inside the radius), matching the hand-rolled loop this replaces.
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() {
            return std::make_unique<CBuffDivineShield>(duration);
        }));
    ABActionForEachInRange voodoo(radius_, std::move(perUnit), true);
    voodoo.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    voodoo.Run(game, caster, nullptr, false, 0, 0);
}

#include "spells10.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Cleaving Attack =====
CAbilityCleavingAttack::CAbilityCleavingAttack(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANca"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Searing Arrows =====
CAbilitySearingArrows::CAbilitySearingArrows(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHfa"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilitySearingArrows::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetAttackModifier(0, War3ID("AHfa"),
        std::make_unique<CAttackBonusDamageEffect>(bonusDamage_));
}

void CAbilitySearingArrows::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->ClearAttackModifier(0, War3ID("AHfa"));
}

// ===== Ensnare =====
CAbilityEnsnare::CAbilityEnsnare(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aens"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityEnsnare::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    CastBuffOnTarget(game, caster, target, [duration = duration_]() {
        return std::make_unique<CBuffEnsnare>(duration);
    });
}

// ===== Ancestral Spirit =====
CAbilityAncestralSpirit::CAbilityAncestralSpirit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aast"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityAncestralSpirit::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionAncestralRevive spirit;
    spirit.Run(game, caster, target, false, 0, 0);
}

// ===== Healing Ward (unit) =====
CAbilityHealingWardUnit::CAbilityHealingWardUnit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ahwd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityHealingWardUnit::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABActionCreateUnitWithBuff ward(
        wardType_, x, y,
        {ABActionCreateUnitWithBuff::BuffFactory(
            [duration = auraDuration_, tick = healPerTick_, radius = radius_]() {
                return std::make_unique<CBuffHealingWard>(duration, tick, radius);
            })});
    ward.Run(game, caster, nullptr, true, x, y);
}

// ===== Unstable Concoction =====
CAbilityUnstableConcoction::CAbilityUnstableConcoction(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Auco"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityUnstableConcoction::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    CastCasterAreaDamageWaves(game, caster, 1, radius_, damage_,
                              CAttackType::SPELLS, CDamageType::FIRE,
                              ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    ABActionDamageTarget selfHit(selfDamage_, CAttackType::SPELLS, CDamageType::FIRE);
    selfHit.Run(game, caster, caster, false, 0, 0);
}

// ===== Abolish Magic =====
CAbilityAbolishMagic::CAbilityAbolishMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aadm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityAbolishMagic::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDispelTarget>(true, true));
    ABActionForEachInRange strip(radius_, std::move(perUnit));
    strip.Run(game, caster, nullptr, true, x, y);
}

// ===== Cluster Rockets =====
CAbilityClusterRockets::CAbilityClusterRockets(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANcs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityClusterRockets::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    CastPointDamageWaves(game, caster, x - 75.0f, y, 1, radius_, damage_,
                         CAttackType::SPELLS, CDamageType::FIRE,
                         ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    CastPointDamageWaves(game, caster, x + 75.0f, y, 1, radius_, damage_,
                         CAttackType::SPELLS, CDamageType::FIRE,
                         ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}

#include "spells13.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

namespace {

void CastTransformOnSelf(CSimulation& game, CUnit* caster,
                         War3ID buffId, float duration, War3ID formId) {
    CAbilitySpellBase::CastBuffOnSelf(game, caster, [buffId, duration, formId]() {
        return std::make_unique<CBuffTransform>(buffId, duration, formId);
    });
}

} // namespace

// ===== Metamorphosis =====
CAbilityMetamorphosis::CAbilityMetamorphosis(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEme"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(150);
}

void CAbilityMetamorphosis::DoEffect(CSimulation& game, CUnit* caster) {
    CastTransformOnSelf(game, caster, War3ID("BEme"), duration_, War3ID("Edem"));
}

// ===== Bear Form =====
CAbilityBearForm::CAbilityBearForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Abrf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
}

void CAbilityBearForm::DoEffect(CSimulation& game, CUnit* caster) {
    CastTransformOnSelf(game, caster, War3ID("Abrf"), duration_, War3ID("edcm"));
}

// ===== Raven Form =====
CAbilityRavenForm::CAbilityRavenForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Arav"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
}

void CAbilityRavenForm::DoEffect(CSimulation& game, CUnit* caster) {
    CastTransformOnSelf(game, caster, War3ID("Arav"), duration_, War3ID("edot"));
}

// ===== Robo-Goblin =====
CAbilityRoboGoblin::CAbilityRoboGoblin(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANrg"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(1.0f);
    SetManaCost(25);
}

void CAbilityRoboGoblin::DoEffect(CSimulation& game, CUnit* caster) {
    CastTransformOnSelf(game, caster, War3ID("ANrg"), duration_, War3ID("Ntin"));
}

// ===== Stone Form =====
CAbilityStoneForm::CAbilityStoneForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Astn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
}

void CAbilityStoneForm::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() {
        return std::make_unique<CBuffStoneForm>(duration);
    });
}

// ===== Avatar =====
CAbilityAvatar::CAbilityAvatar(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHav"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(150);
}

void CAbilityAvatar::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster,
                   [duration = duration_, damage = bonusDamage_, life = bonusLife_]() {
                       return std::make_unique<CBuffAvatar>(duration, damage, life);
                   });
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
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    CastBuffOnTarget(game, caster, target,
                     [duration = duration_, tick = manaPerTick_, casterHandle]() {
                         return std::make_unique<CBuffManaSiphon>(duration, tick, casterHandle);
                     });
}

// ===== Big Bad Voodoo =====
CAbilityBigBadVoodoo::CAbilityBigBadVoodoo(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOvd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(200);
}

void CAbilityBigBadVoodoo::DoEffect(CSimulation& game, CUnit* caster) {
    CastCasterAreaBuff(game, caster, radius_, [duration = duration_]() {
        return std::make_unique<CBuffDivineShield>(duration);
    }, ABFactionFilter::SAME_PLAYER_AS_CASTER, true);
}

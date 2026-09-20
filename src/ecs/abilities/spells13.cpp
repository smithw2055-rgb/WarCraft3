#include "spells13.h"
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
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffTransform>(
        War3ID("BEme"), duration_, War3ID("Edem")), game);
}

// ===== Bear Form =====
CAbilityBearForm::CAbilityBearForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Abrf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
}

void CAbilityBearForm::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffTransform>(
        War3ID("Abrf"), duration_, War3ID("edcm")), game);
}

// ===== Raven Form =====
CAbilityRavenForm::CAbilityRavenForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Arav"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
}

void CAbilityRavenForm::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffTransform>(
        War3ID("Arav"), duration_, War3ID("edot")), game);
}

// ===== Robo-Goblin =====
CAbilityRoboGoblin::CAbilityRoboGoblin(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANrg"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(1.0f);
    SetManaCost(25);
}

void CAbilityRoboGoblin::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffTransform>(
        War3ID("ANrg"), duration_, War3ID("Ntin")), game);
}

// ===== Stone Form =====
CAbilityStoneForm::CAbilityStoneForm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Astn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
}

void CAbilityStoneForm::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffStoneForm>(duration_), game);
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
    caster->AddBuff(std::make_unique<CBuffAvatar>(
        duration_, bonusDamage_, bonusLife_), game);
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
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffManaSiphon>(
        duration_, manaPerTick_, caster ? caster->GetHandleId() : -1), game);
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
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(std::make_unique<CBuffDivineShield>(duration_), game);
        }
    }
}

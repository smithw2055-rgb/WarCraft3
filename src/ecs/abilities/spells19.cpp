#include "spells19.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_builder.h"
#include "data/abilitydata.h"

// ===== Banish =====
CAbilityBanish::CAbilityBanish(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHbn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(65);
}

void CAbilityBanish::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionAddBuffTarget banish(
        [duration = duration_]() {
            return std::make_unique<CBuffEthereal>(duration, War3ID("Bban"));
        });
    banish.Run(game, caster, target, false, 0, 0);
}

void CAbilityBanish::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
}

// ===== Anti-Magic Shell =====
CAbilityAntiMagicShell::CAbilityAntiMagicShell(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aam2"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityAntiMagicShell::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionAddBuffTarget shell(
        [duration = duration_]() {
            return std::make_unique<CBuffMagicImmune>(duration, War3ID("Bam2"));
        });
    shell.Run(game, caster, target, false, 0, 0);
}

void CAbilityAntiMagicShell::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
}

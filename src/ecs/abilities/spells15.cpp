#include "spells15.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"

// ===== Phoenix =====
CAbilityPhoenix::CAbilityPhoenix(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHpx"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(180.0f);
    SetManaCost(175);
}

void CAbilityPhoenix::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder create-and-buff (the rebirth
    // egg hatches later; graceful without unit data like the code
    // replaced).
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    ABActionCreateUnitWithBuff bird(
        phoenixType_, caster->GetX() + 64.0f, caster->GetY(),
        {BF([]() { return std::make_unique<CBuffPhoenixRebirth>(); })});
    bird.Run(game, caster, nullptr, false, 0, 0);
}

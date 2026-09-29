#include "spells15.h"
#include "data/abilitydata.h"
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
    // Migrated onto a single AbilityBuilder create-and-buff (the rebirth
    // egg hatches after hatchDelay_; graceful without unit data).
    if (!caster) return;
    using BF = ABActionCreateUnitWithBuff::BuffFactory;
    const float delay = hatchDelay_;
    ABActionCreateUnitWithBuff bird(
        phoenixType_, caster->GetX() + 64.0f, caster->GetY(),
        {BF([delay]() { return std::make_unique<CBuffPhoenixRebirth>(delay); })});
    bird.Run(game, caster, nullptr, false, 0, 0);
}

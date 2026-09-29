#include "spells23.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_builder.h"
#include "data/abilitydata.h"

// ===== Chain Dispel =====
CAbilityChainDispel::CAbilityChainDispel(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ache"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityChainDispel::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Bounce a faction-sensitive dispel: allies shed debuffs, enemies lose
    // buffs (ref chain dispel jumping cleanse).
    if (!caster || !target) return;
    ABActionBounce chain(
        bounces_, range_,
        [](int) {
            ABEffectChain perHit;
            ABEffectChain allyBranch;
            allyBranch.Add(std::make_unique<ABActionDispelTarget>(false, true));
            ABEffectChain foeBranch;
            foeBranch.Add(std::make_unique<ABActionDispelTarget>(true, false));
            perHit.Add(std::make_unique<ABActionIf>(
                std::make_shared<ABCondTargetAllyOfCaster>(), std::move(allyBranch),
                std::move(foeBranch)));
            return perHit;
        });
    chain.Run(game, caster, target, false, 0, 0);
}

void CAbilityChainDispel::ApplyEffectData(const CAbilityDefFull& def, int level) {
    bounces_ = def.DataIntAt(level, 0, bounces_);
    range_ = def.AreaAt(level);
}

// ===== Taunt =====
CAbilityTaunt::CAbilityTaunt(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANta"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityTaunt::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionTauntTarget>(caster));
    ABActionForEachInRange taunt(radius_, std::move(perUnit));
    taunt.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    taunt.Run(game, caster, nullptr, true, caster->GetX(), caster->GetY());
}

void CAbilityTaunt::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
}

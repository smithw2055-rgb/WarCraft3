#include "spells20.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_builder.h"
#include "data/abilitydata.h"

// ===== Possession =====
CAbilityPossession::CAbilityPossession(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Apos"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(10.0f);
    SetManaCost(150);
}

void CAbilityPossession::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Charm's ownership branch (non-hero victims only), then the retail
    // sacrifice: the banshee is removed once the victim converts.
    if (!target || !caster || target->IsDead()) return;
    auto* victim = dynamic_cast<CUnit*>(target);
    if (!victim) return;
    const int beforePlayer = victim->GetPlayerIndex();
    ABEffectChain convert;
    convert.Add(std::make_unique<ABActionTransferToCaster>());
    ABActionIf possess(
        std::make_shared<ABCondFunc>(
            [maxLevel = maxLevel_](CSimulation&, CUnit*, CWidget* unitTarget,
                                   bool, float, float) {
                auto* unit = dynamic_cast<CUnit*>(unitTarget);
                if (!unit || unit->IsDead() || unit->IsHero()) return false;
                const CUnitType* type = unit->GetUnitType();
                return type == nullptr || type->GetLevel() <= maxLevel;
            }),
        std::move(convert));
    possess.Run(game, caster, target, false, 0, 0);
    // Sacrifice only on success: the victim changed sides to our player.
    if (beforePlayer != caster->GetPlayerIndex() &&
        victim->GetPlayerIndex() == caster->GetPlayerIndex()) {
        game.RemoveUnit(caster);
    }
}

void CAbilityPossession::ApplyEffectData(const CAbilityDefFull& def, int level) {
    maxLevel_ = def.DataIntAt(level, 0, maxLevel_);
}

// ===== Stampede =====
CAbilityStampede::CAbilityStampede(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANst"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
    SetCooldown(60.0f);
    SetManaCost(150);
}

void CAbilityStampede::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionDamageTarget>(damage_, CAttackType::SPELLS,
                                                       CDamageType::NORMAL));
    perUnit.Add(std::make_unique<ABActionStunTarget>(slowDuration_));
    ABActionForEachInLine stampede(range_, width_, std::move(perUnit));
    stampede.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    stampede.Run(game, caster, nullptr, true, x, y);
}

void CAbilityStampede::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
    range_ = def.DataFloatAt(level, 1, range_);
}

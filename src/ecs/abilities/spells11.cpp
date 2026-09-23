#include "spells11.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "data/abilitydata.h"
#include <cmath>

// ===== Wind Walk =====
CAbilityWindWalk::CAbilityWindWalk(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOwk"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityWindWalk::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget walk(
        [duration = duration_, backstab = backstabDamage_]() {
            return std::make_unique<CBuffWindWalk>(duration, backstab);
        });
    walk.Run(game, caster, caster, false, 0, 0);
}

// ===== Invisibility =====
CAbilityInvisibility::CAbilityInvisibility(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aivs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityInvisibility::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget hide(
        [duration = duration_]() { return std::make_unique<CBuffWindWalk>(duration, 0.0f); });
    hide.Run(game, caster, target, false, 0, 0);
}

// ===== Permanent Invisibility =====
CAbilityPermanentInvisibility::CAbilityPermanentInvisibility(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Apiv"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityPermanentInvisibility::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetInvisible(true);
}

void CAbilityPermanentInvisibility::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    if (unit) unit->SetInvisible(false);
}

// ===== Shadowmeld =====
CAbilityShadowmeld::CAbilityShadowmeld(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ashm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityShadowmeld::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    if (!unit) return;
    lastX_ = unit->GetX();
    lastY_ = unit->GetY();
    tracking_ = true;
}

void CAbilityShadowmeld::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    tracking_ = false;
    if (unit) unit->SetInvisible(false);
}

void CAbilityShadowmeld::OnTick(CSimulation& game, CUnit* unit) {
    (void)game;
    if (!unit || !tracking_) return;
    const float dx = unit->GetX() - lastX_;
    const float dy = unit->GetY() - lastY_;
    lastX_ = unit->GetX();
    lastY_ = unit->GetY();
    unit->SetInvisible(dx * dx + dy * dy < 1.0f);
}

// ===== Dust of Appearance =====
CAbilityDustOfAppearance::CAbilityDustOfAppearance(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AItb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
}

void CAbilityDustOfAppearance::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Migrated onto an AbilityBuilder iteration: enemies in radius gain the
    // Faerie Fire marker. Same coverage as the loop replaced.
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_]() {
            return std::make_unique<CBuffArmor>(War3ID("Bfae"), duration, 0, false);
        }));
    ABActionForEachInRange dust(radius_, std::move(perUnit));
    dust.SetFactionFilter(ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
    dust.Run(game, caster, nullptr, true, x, y);
}

// ===== Berserk =====
CAbilityBerserk::CAbilityBerserk(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Absk"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityBerserk::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Migrated onto a single AbilityBuilder buff attach.
    ABActionAddBuffTarget rage(
        [duration = duration_, attack = attackSpeedMul_, taken = damageTakenMul_]() {
            return std::make_unique<CBuffBerserk>(duration, attack, taken);
        });
    rage.Run(game, caster, caster, false, 0, 0);
}

// ===== Summon Bear =====
CAbilitySummonBear::CAbilitySummonBear(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsg"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(75);
}

void CAbilitySummonBear::DoEffect(CSimulation& game, CUnit* caster) {
    // Migrated onto a single AbilityBuilder exact-point summon (no timed
    // life; graceful without unit data like the code replaced).
    if (!caster) return;
    ABActionCreateUnitAtPoint bear(bearType_, caster->GetX() + 64.0f,
                                   caster->GetY(), 0);
    bear.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilitySummonBear::ApplyEffectData(const CAbilityDefFull& def, int level) {
    bearType_ = def.UnitIdAt(level, bearType_);
}

// ===== Battle Roar =====
CAbilityBattleRoar::CAbilityBattleRoar(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANbr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityBattleRoar::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    // Migrated onto an AbilityBuilder iteration: same-player units around
    // the caster gain Inner Fire bonus damage. Caster excluded via
    // includeCaster=false, matching the loop replaced.
    ABEffectChain perUnit;
    perUnit.Add(std::make_unique<ABActionAddBuffTarget>(
        [duration = duration_, damage = bonusDamage_]() {
            return std::make_unique<CBuffInnerFire>(duration, damage, 0);
        }));
    ABActionForEachInRange roar(radius_, std::move(perUnit));
    roar.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    roar.Run(game, caster, nullptr, false, 0, 0);
}

void CAbilityBattleRoar::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
}

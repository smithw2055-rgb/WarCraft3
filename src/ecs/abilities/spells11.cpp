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
    CastBuffOnSelf(game, caster, [duration = duration_, backstab = backstabDamage_]() {
        return std::make_unique<CBuffWindWalk>(duration, backstab);
    });
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
    CastBuffOnTarget(game, caster, target, [duration = duration_]() {
        return std::make_unique<CBuffWindWalk>(duration, 0.0f);
    });
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
    CastPointAreaBuff(game, caster, x, y, radius_, [duration = duration_]() {
        return std::make_unique<CBuffArmor>(War3ID("Bfae"), duration, 0, false);
    }, ABFactionFilter::OTHER_PLAYER_THAN_CASTER);
}

// ===== Berserk =====
CAbilityBerserk::CAbilityBerserk(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Absk"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(50);
}

void CAbilityBerserk::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster,
                   [duration = duration_, attack = attackSpeedMul_, taken = damageTakenMul_]() {
                       return std::make_unique<CBuffBerserk>(duration, attack, taken);
                   });
}

// ===== Summon Bear =====
CAbilitySummonBear::CAbilitySummonBear(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsg"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(75);
}

void CAbilitySummonBear::DoEffect(CSimulation& game, CUnit* caster) {
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
    CastCasterAreaBuff(game, caster, radius_,
                       [duration = duration_, damage = bonusDamage_]() {
                           return std::make_unique<CBuffInnerFire>(duration, damage, 0);
                       });
}

void CAbilityBattleRoar::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
}

#include "spells11.h"
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
    caster->AddBuff(std::make_unique<CBuffWindWalk>(duration_, backstabDamage_), game);
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
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffWindWalk>(duration_, 0.0f), game);
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
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - x;
        const float dy = unit->GetY() - y;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(std::make_unique<CBuffArmor>(
                War3ID("Bfae"), duration_, 0, false), game);
        }
    }
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
    caster->AddBuff(std::make_unique<CBuffBerserk>(
        duration_, attackSpeedMul_, damageTakenMul_), game);
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
    // Spirit bears stand until killed (ref, no timed life). Graceful
    // without unit data (ref summon-without-data).
    CUnit* bear = game.CreateUnit(bearType_, caster->GetPlayerIndex(),
                                  caster->GetX() + 64.0f, caster->GetY(), 0);
    if (!bear) return;
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(caster->GetHandleId(), bear->GetHandleId());
    }
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
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->AddBuff(std::make_unique<CBuffInnerFire>(
                duration_, bonusDamage_, 0), game);
        }
    }
}

void CAbilityBattleRoar::ApplyEffectData(const CAbilityDefFull& def, int level) {
    radius_ = def.AreaAt(level);
}

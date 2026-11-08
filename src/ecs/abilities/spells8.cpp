#include "spells8.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Scroll of Healing =====
CAbilityItemHealGroup::CAbilityItemHealGroup(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIha"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
}

void CAbilityItemHealGroup::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    CastPointAreaHeal(game, caster, x, y, radius_, healAmount_);
}

// ===== Scroll of Town Portal =====
CAbilityItemTownPortal::CAbilityItemTownPortal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AItp"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(99999.0f);
}

void CAbilityItemTownPortal::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    ABEffectChain group;
    group.Add(std::make_unique<ABActionTeleportTarget>(x, y));
    ABActionForEachInRange portal(radius_, std::move(group));
    portal.SetFactionFilter(ABFactionFilter::SAME_PLAYER_AS_CASTER);
    portal.Run(game, caster, nullptr, false, 0, 0);
    caster->SetPosition(x, y);
}

// ===== Potion of Lesser Invulnerability =====
CAbilityItemInvulnerable::CAbilityItemInvulnerable(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIvl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemInvulnerable::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_]() {
        return std::make_unique<CBuffDivineShield>(duration);
    });
}

// ===== Potion of Healing =====
CAbilityItemHealSelf::CAbilityItemHealSelf(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIh1"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemHealSelf::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    ABActionHealTarget heal(healAmount_);
    heal.Run(game, caster, caster, false, 0, 0);
}

// ===== Potion of Mana =====
CAbilityItemManaSelf::CAbilityItemManaSelf(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIm1"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemManaSelf::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    float mana = std::min(caster->GetMana() + manaAmount_, static_cast<float>(caster->GetMaxMana()));
    ABActionSetManaTarget setMana(mana);
    setMana.Run(game, caster, caster, false, 0, 0);
}

// ===== Lesser Clarity Potion =====
CAbilityItemClarity::CAbilityItemClarity(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIpl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemClarity::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_, regen = regenBonus_]() {
        return std::make_unique<CBuffClarity>(duration, regen);
    });
}

// ===== Wand of Mana Stealing =====
CAbilityItemManaSteal::CAbilityItemManaSteal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aste"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityItemManaSteal::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    ABActionManaSteal steal(stealAmount_);
    steal.Run(game, caster, target, false, 0, 0);
}

// ===== Scroll of Protection =====
CAbilityItemProtection::CAbilityItemProtection(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIda"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemProtection::DoEffect(CSimulation& game, CUnit* caster) {
    CastBuffOnSelf(game, caster, [duration = duration_, bonus = armorBonus_]() {
        return std::make_unique<CBuffArmor>(War3ID("AIda"), duration, bonus, true);
    });
}

#include "spells8.h"
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
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - x;
        const float dy = unit->GetY() - y;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            float life = unit->GetLife() + healAmount_;
            if (life > unit->GetMaxLife()) life = unit->GetMaxLife();
            unit->SetLife(game, life);
        }
    }
}

// ===== Scroll of Town Portal =====
CAbilityItemTownPortal::CAbilityItemTownPortal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AItp"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(99999.0f);
}

void CAbilityItemTownPortal::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    const float cx = caster->GetX(), cy = caster->GetY();
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - cx;
        const float dy = unit->GetY() - cy;
        if (dx * dx + dy * dy > radius_ * radius_) continue;
        unit->SetPosition(x, y);
    }
    caster->SetPosition(x, y);
}

// ===== Potion of Lesser Invulnerability =====
CAbilityItemInvulnerable::CAbilityItemInvulnerable(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIvl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemInvulnerable::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffDivineShield>(duration_), game);
}

// ===== Potion of Healing =====
CAbilityItemHealSelf::CAbilityItemHealSelf(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIh1"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemHealSelf::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    float life = caster->GetLife() + healAmount_;
    if (life > caster->GetMaxLife()) life = caster->GetMaxLife();
    caster->SetLife(game, life);
}

// ===== Potion of Mana =====
CAbilityItemManaSelf::CAbilityItemManaSelf(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIm1"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemManaSelf::DoEffect(CSimulation& game, CUnit* caster) {
    (void)game;
    if (!caster) return;
    float mana = caster->GetMana() + manaAmount_;
    if (mana > static_cast<float>(caster->GetMaxMana())) {
        mana = static_cast<float>(caster->GetMaxMana());
    }
    caster->SetMana(mana);
}

// ===== Lesser Clarity Potion =====
CAbilityItemClarity::CAbilityItemClarity(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIpl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemClarity::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffClarity>(duration_, regenBonus_), game);
}

// ===== Wand of Mana Stealing =====
CAbilityItemManaSteal::CAbilityItemManaSteal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aste"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
}

void CAbilityItemManaSteal::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)game;
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    float stolen = stealAmount_;
    if (stolen > targetUnit->GetMana()) stolen = targetUnit->GetMana();
    targetUnit->SetMana(targetUnit->GetMana() - stolen);
    float mana = caster->GetMana() + stolen;
    if (mana > static_cast<float>(caster->GetMaxMana())) {
        mana = static_cast<float>(caster->GetMaxMana());
    }
    caster->SetMana(mana);
}

// ===== Scroll of Protection =====
CAbilityItemProtection::CAbilityItemProtection(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AIda"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

void CAbilityItemProtection::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffArmor>(
        War3ID("AIda"), duration_, armorBonus_, true), game);
}

#include "spells.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "data/abilitydata.h"
#include <cmath>
#include <iostream>

// ===== Blizzard =====
CAbilityBlizzard::CAbilityBlizzard(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHbz"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(75);
}

void CAbilityBlizzard::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead()) continue;
        float dx = unit->GetX() - x;
        float dy = unit->GetY() - y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= radius_) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::COLD,
                         "", damagePerWave_ * waves_, 0);
        }
    }
}

// ===== Holy Light =====
CAbilityHolyLight::CAbilityHolyLight(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHhb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(5.0f);
    SetManaCost(65);
}

void CAbilityHolyLight::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        float newLife = targetUnit->GetLife() + healAmount_;
        if (newLife > targetUnit->GetMaxLife()) newLife = targetUnit->GetMaxLife();
        targetUnit->SetLife(game, newLife);
    }
}

void CAbilityHolyLight::ApplyEffectData(const CAbilityDefFull& def, int level) {
    healAmount_ = def.DataFloatAt(level, 0, healAmount_);
}

// ===== Divine Shield =====
CAbilityDivineShield::CAbilityDivineShield(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHds"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(35.0f);
    SetManaCost(25);
}

void CAbilityDivineShield::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    caster->AddBuff(std::make_unique<CBuffDivineShield>(duration_), game);
}

// ===== Thunder Clap =====
CAbilityThunderClap::CAbilityThunderClap(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHtc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCastRange(0);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(90);
}

void CAbilityThunderClap::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead() || unit == caster) continue;
        float dx = unit->GetX() - caster->GetX();
        float dy = unit->GetY() - caster->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= radius_) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::SONIC,
                         "", damage_, 0);
            unit->AddBuff(std::make_unique<CBuffSlow>(slowDuration_, slowPercent_, caster->GetHandleId()), game);
        }
    }
}

// ===== Storm Bolt =====
CAbilityStormBolt::CAbilityStormBolt(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHtb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(9.0f);
    SetManaCost(75);
}

void CAbilityStormBolt::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    target->Damage(game, caster, false, false,
                   CAttackType::SPELLS, CDamageType::LIGHTNING,
                   "", damage_, 0);
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        targetUnit->AddBuff(std::make_unique<CBuffStun>(stunDuration_), game);
    }
}

void CAbilityStormBolt::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
}

// ===== War Stomp =====
CAbilityWarStomp::CAbilityWarStomp(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOws"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCastRange(0);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(90);
}

void CAbilityWarStomp::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead() || unit == caster) continue;
        float dx = unit->GetX() - caster->GetX();
        float dy = unit->GetY() - caster->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= radius_) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::SONIC,
                         "", damage_, 0);
            unit->AddBuff(std::make_unique<CBuffStun>(stunDuration_), game);
        }
    }
}

// ===== Death Coil =====
CAbilityDeathCoil::CAbilityDeathCoil(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUdc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(6.0f);
    SetManaCost(75);
}

void CAbilityDeathCoil::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;

    auto* player = game.GetPlayer(caster->GetPlayerIndex());
    auto* targetPlayer = game.GetPlayer(targetUnit->GetPlayerIndex());
    bool isAlly = false;
    if (player && targetPlayer) {
        isAlly = player->HasAlliance(*targetPlayer, CAllianceType::PASSIVE);
    }

    if (isAlly && targetUnit->GetUnitType()->GetClassifications().count(CUnitClassification::UNDEAD)) {
        float newLife = targetUnit->GetLife() + healAmount_;
        if (newLife > targetUnit->GetMaxLife()) newLife = targetUnit->GetMaxLife();
        targetUnit->SetLife(game, newLife);
    } else {
        targetUnit->Damage(game, caster, false, false,
                           CAttackType::SPELLS, CDamageType::DEATH,
                           "", damage_, 0);
    }
}

void CAbilityDeathCoil::ApplyEffectData(const CAbilityDefFull& def, int level) {
    healAmount_ = def.DataFloatAt(level, 0, healAmount_);
}

// ===== Frost Nova =====
CAbilityFrostNova::CAbilityFrostNova(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUfn"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(8.0f);
    SetManaCost(125);
}

void CAbilityFrostNova::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;

    target->Damage(game, caster, false, false,
                   CAttackType::SPELLS, CDamageType::COLD,
                   "", damage_, 0);

    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead() || unit == target) continue;
        float dx = unit->GetX() - target->GetX();
        float dy = unit->GetY() - target->GetY();
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= radius_) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::COLD,
                         "", aoeDamage_, 0);
            unit->AddBuff(std::make_unique<CBuffSlow>(slowDuration_, slowPercent_, caster->GetHandleId()), game);
        }
    }
}

void CAbilityFrostNova::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 1, damage_);
    aoeDamage_ = def.DataFloatAt(level, 0, aoeDamage_);
}

// ===== Mana Burn =====
CAbilityManaBurn::CAbilityManaBurn(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEmb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCastPoint(0.3f);
    SetCooldown(7.0f);
    SetManaCost(50);
}

void CAbilityManaBurn::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;

    int manaToBurn = std::min(manaBurned_, (int)targetUnit->GetMana());
    targetUnit->SetMana(targetUnit->GetMana() - manaToBurn);
    targetUnit->Damage(game, caster, false, false,
                       CAttackType::SPELLS, CDamageType::MIND,
                       "", (float)manaToBurn, 0);
}

// ===== Entangling Roots =====
CAbilityEntanglingRoots::CAbilityEntanglingRoots(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEer"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCastPoint(0.3f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityEntanglingRoots::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    float totalDamage = damagePerSecond_ * duration_;
    target->Damage(game, caster, false, false,
                   CAttackType::SPELLS, CDamageType::POISON,
                   "", totalDamage, 0);
    // Apply periodic damage as a buff
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        targetUnit->AddBuff(std::make_unique<CBuffPoison>(duration_, damagePerSecond_, caster->GetHandleId()), game);
    }
}

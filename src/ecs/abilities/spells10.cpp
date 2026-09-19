#include "spells10.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include <cmath>

// ===== Cleaving Attack =====
CAbilityCleavingAttack::CAbilityCleavingAttack(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANca"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Searing Arrows =====
CAbilitySearingArrows::CAbilitySearingArrows(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHfa"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Ensnare =====
CAbilityEnsnare::CAbilityEnsnare(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aens"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityEnsnare::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffEnsnare>(duration_), game);
}

// ===== Ancestral Spirit =====
CAbilityAncestralSpirit::CAbilityAncestralSpirit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aast"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(500.0f);
    SetCooldown(12.0f);
    SetManaCost(75);
}

void CAbilityAncestralSpirit::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || !target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    const War3ID typeId = targetUnit->GetTypeId();
    if (typeId != War3ID("otau") && typeId != War3ID("Otch")) return;
    targetUnit->Revive(game, targetUnit->GetX(), targetUnit->GetY());
    if (targetUnit->GetPlayerIndex() != caster->GetPlayerIndex()) {
        if (CPlayer* oldOwner = game.GetPlayer(targetUnit->GetPlayerIndex())) {
            oldOwner->RemoveUnit(targetUnit);
        }
        targetUnit->SetPlayerIndex(caster->GetPlayerIndex());
        if (CPlayer* newOwner = game.GetPlayer(caster->GetPlayerIndex())) {
            newOwner->AddUnit(targetUnit);
        }
    }
}

// ===== Healing Ward (unit) =====
CAbilityHealingWardUnit::CAbilityHealingWardUnit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Ahwd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(8.0f);
    SetManaCost(50);
}

void CAbilityHealingWardUnit::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Graceful without unit data (ref summon-without-data).
    CUnit* ward = game.CreateUnit(wardType_, caster->GetPlayerIndex(), x, y, 0);
    if (!ward) return;
    ward->AddBuff(std::make_unique<CBuffHealingWard>(
        auraDuration_, healPerTick_, radius_), game);
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(caster->GetHandleId(), ward->GetHandleId());
    }
}

// ===== Unstable Concoction =====
CAbilityUnstableConcoction::CAbilityUnstableConcoction(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Auco"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityUnstableConcoction::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - caster->GetX();
        const float dy = unit->GetY() - caster->GetY();
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::FIRE,
                         "", damage_, 0);
        }
    }
    caster->Damage(game, caster, false, false,
                   CAttackType::SPELLS, CDamageType::FIRE,
                   "", selfDamage_, 0);
}

// ===== Abolish Magic =====
CAbilityAbolishMagic::CAbilityAbolishMagic(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Aadm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityAbolishMagic::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        const float dx = unit->GetX() - x;
        const float dy = unit->GetY() - y;
        if (dx * dx + dy * dy <= radius_ * radius_) {
            unit->RemoveBuffsIf(game, true, true, false);
        }
    }
}

// ===== Cluster Rockets =====
CAbilityClusterRockets::CAbilityClusterRockets(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANcs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(10.0f);
    SetManaCost(75);
}

void CAbilityClusterRockets::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Two clusters straddling the target point (ref twin-rocket spread).
    const float offsets[2][2] = {{-75.0f, 0.0f}, {75.0f, 0.0f}};
    for (const auto& off : offsets) {
        const float wx = x + off[0], wy = y + off[1];
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
            const float dx = unit->GetX() - wx;
            const float dy = unit->GetY() - wy;
            if (dx * dx + dy * dy <= radius_ * radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::FIRE,
                             "", damage_, 0);
            }
        }
    }
}

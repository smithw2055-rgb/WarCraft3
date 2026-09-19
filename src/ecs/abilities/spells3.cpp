#include "spells3.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/player.h"
#include <cmath>

static float Dist2(float ax, float ay, float bx, float by) {
    const float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}

// ===== Mass Teleport =====
CAbilityMassTeleport::CAbilityMassTeleport(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHmt"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(30.0f);
    SetManaCost(100);
}

void CAbilityMassTeleport::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    const float cx = caster->GetX(), cy = caster->GetY();
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        if (Dist2(unit->GetX(), unit->GetY(), cx, cy) > radius_ * radius_) continue;
        unit->SetPosition(x, y);
    }
    caster->SetPosition(x, y);
}

// ===== Summon Water Elemental =====
CAbilitySummonWaterElemental::CAbilitySummonWaterElemental(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHwe"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(75);
}

void CAbilitySummonWaterElemental::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (int i = 0; i < count_; i++) {
        const float x = caster->GetX() + (i - count_ / 2) * 64.0f;
        const float y = caster->GetY() + 64.0f;
        SummonTimedUnit(game, caster, elementalType_, War3ID("AHwe"),
                        x, y, caster->GetFacing(), summonDuration_);
    }
}

// ===== Resurrect =====
CAbilityResurrect::CAbilityResurrect(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHrs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(800.0f);
    SetCooldown(180.0f);
    SetManaCost(175);
}

void CAbilityResurrect::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    int revived = 0;
    for (auto* unit : game.GetUnits()) {
        if (revived >= maxTargets_) break;
        if (!unit || !unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        if (Dist2(unit->GetX(), unit->GetY(), x, y) > radius_ * radius_) continue;
        auto* revivedUnit = dynamic_cast<CUnit*>(unit);
        if (!revivedUnit) continue;
        revivedUnit->Revive(game, unit->GetX(), unit->GetY());
        revived++;
    }
}

// ===== Blink =====
CAbilityBlink::CAbilityBlink(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEbl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(5.0f);
    SetManaCost(10);
}

void CAbilityBlink::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    (void)game;
    if (!caster) return;
    const float dx = x - caster->GetX(), dy = y - caster->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > maxRange_ && dist > 0.0f) {
        x = caster->GetX() + dx / dist * maxRange_;
        y = caster->GetY() + dy / dist * maxRange_;
    }
    caster->SetPosition(x, y);
}

// ===== Mirror Image =====
CAbilityMirrorImage::CAbilityMirrorImage(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOmi"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(20.0f);
    SetManaCost(50);
}

void CAbilityMirrorImage::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (int i = 0; i < count_; i++) {
        const float x = caster->GetX() + (i - count_ / 2) * 64.0f;
        const float y = caster->GetY() + 64.0f;
        SummonTimedUnit(game, caster, caster->GetTypeId(), War3ID("AOmi"),
                        x, y, caster->GetFacing(), duration_);
    }
}

// ===== Shockwave =====
CAbilityShockwave::CAbilityShockwave(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOsh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(700.0f);
    SetCooldown(8.0f);
    SetManaCost(100);
}

void CAbilityShockwave::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster) return;
    const float dx = target->GetX() - caster->GetX();
    const float dy = target->GetY() - caster->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return;
    const float nx = dx / dist, ny = dy / dist;
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        const float ux = unit->GetX() - caster->GetX();
        const float uy = unit->GetY() - caster->GetY();
        const float proj = ux * nx + uy * ny;
        if (proj < 0 || proj > range_) continue;
        const float perp = std::abs(ux * ny - uy * nx);
        if (perp <= width_ / 2) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SPELLS, CDamageType::NORMAL,
                         "", damage_, 0);
        }
    }
}

// ===== Starfall =====
CAbilityStarfall::CAbilityStarfall(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEst"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(60.0f);
    SetManaCost(100);
}

void CAbilityStarfall::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    const float cx = caster->GetX(), cy = caster->GetY();
    for (int w = 0; w < waves_; w++) {
        // Deterministic synced scatter (std::rand is banned in sim code).
        const float wx = cx + game.SimRandRange(-radius_, radius_);
        const float wy = cy + game.SimRandRange(-radius_, radius_);
        for (auto* unit : game.GetUnits()) {
            if (!unit || unit->IsDead() || unit == caster) continue;
            if (Dist2(unit->GetX(), unit->GetY(), wx, wy) <= radius_ * radius_ / 4) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::MAGIC,
                             "", damage_, 0);
            }
        }
    }
}

// ===== Tranquility =====
CAbilityTranquility::CAbilityTranquility(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AEtr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(60.0f);
    SetManaCost(125);
}

void CAbilityTranquility::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    const float cx = caster->GetX(), cy = caster->GetY();
    for (auto* unit : game.GetUnits()) {
        auto* target = dynamic_cast<CUnit*>(unit);
        if (!target || target->IsDead()) continue;
        if (target->GetPlayerIndex() != caster->GetPlayerIndex()) continue;
        if (Dist2(target->GetX(), target->GetY(), cx, cy) > radius_ * radius_) continue;
        const float totalHeal = healPerTick_ * duration_;
        float newLife = target->GetLife() + totalHeal;
        if (newLife > target->GetMaxLife()) newLife = target->GetMaxLife();
        target->SetLife(game, newLife);
        target->AddBuff(std::make_unique<CBuffRejuvenation>(duration_, healPerTick_), game);
    }
}

// ===== Reincarnation =====
CAbilityReincarnation::CAbilityReincarnation(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOre"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(0.0f);
    SetManaCost(0);
}

void CAbilityReincarnation::OnDeath(CSimulation& game, CUnit* unit) {
    if (!unit || reincarnateRemaining_ > 0.0f) return;
    if (unit->GetMana() < manaCost_) return;
    unit->SetMana(unit->GetMana() - manaCost_);
    reincarnateRemaining_ = reincarnateCooldown_;
    // Revive in place at full life; corpse state cleared by Revive().
    unit->Revive(game, unit->GetX(), unit->GetY());
}

void CAbilityReincarnation::OnTick(CSimulation& game, CUnit* unit) {
    (void)unit;
    if (reincarnateRemaining_ > 0.0f) {
        reincarnateRemaining_ -= game.GetSimStepTime();
        if (reincarnateRemaining_ < 0.0f) reincarnateRemaining_ = 0.0f;
    }
}

// ===== Charm =====
CAbilityCharm::CAbilityCharm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANch"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(7.0f);
    SetManaCost(70);
}

void CAbilityCharm::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit || targetUnit->IsHero()) return;
    const CUnitType* type = targetUnit->GetUnitType();
    if (type && type->GetLevel() > maxLevel_) return;
    CPlayer* oldOwner = game.GetPlayer(targetUnit->GetPlayerIndex());
    CPlayer* newOwner = game.GetPlayer(caster->GetPlayerIndex());
    if (oldOwner) oldOwner->RemoveUnit(targetUnit);
    targetUnit->SetPlayerIndex(caster->GetPlayerIndex());
    if (newOwner) newOwner->AddUnit(targetUnit);
}

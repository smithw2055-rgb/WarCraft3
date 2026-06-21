#include "spells2.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include <cmath>
#include <iostream>

// ===== Serenity =====
CAbilitySerenity::CAbilitySerenity(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AHre"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(5.0f);
    SetManaCost(75);
}

void CAbilitySerenity::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead()) continue;
        float dx = unit->GetX() - caster->GetX();
        float dy = unit->GetY() - caster->GetY();
        if (dx*dx + dy*dy <= radius_*radius_) {
            float newMana = unit->GetMana() + manaRestore_;
            if (newMana > unit->GetMaxMana()) newMana = (float)unit->GetMaxMana();
            unit->SetMana(newMana);
        }
    }
}

// ===== Healing Wave =====
CAbilityHealingWave::CAbilityHealingWave(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOhw"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(9.0f);
    SetManaCost(90);
}

void CAbilityHealingWave::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    CWidget* current = target;
    float heal = healAmount_;
    for (int i = 0; i < bounces_ && current; i++) {
        auto* unit = dynamic_cast<CUnit*>(current);
        if (unit && !unit->IsDead()) {
            float newLife = unit->GetLife() + heal;
            if (newLife > unit->GetMaxLife()) newLife = unit->GetMaxLife();
            unit->SetLife(game, newLife);
            heal *= 0.75f; // reduce per bounce
        }
        // Find next bounce target
        CUnit* next = nullptr;
        float bestDist = bounceRange_;
        for (auto* other : game.GetUnits()) {
            if (other->IsDead() || other == current) continue;
            float dx = other->GetX() - current->GetX();
            float dy = other->GetY() - current->GetY();
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist < bestDist) {
                bestDist = dist;
                next = other;
            }
        }
        current = next;
    }
}

// ===== Hex =====
CAbilityHex::CAbilityHex(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOhx"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(7.0f);
    SetManaCost(70);
}

void CAbilityHex::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        // Apply stun as hex
        targetUnit->AddBuff(std::make_unique<CBuffStun>(duration_), game);
    }
}

// ===== Chain Lightning =====
CAbilityChainLightning::CAbilityChainLightning(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOcl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(9.0f);
    SetManaCost(120);
}

void CAbilityChainLightning::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    CWidget* current = target;
    float dmg = damage_;
    for (int i = 0; i < bounces_ && current; i++) {
        current->Damage(game, caster, false, false,
                        CAttackType::NORMAL, CDamageType::NORMAL,
                        "", dmg, 0);
        dmg *= (1.0f - damageReduction_);
        CUnit* next = nullptr;
        float bestDist = bounceRange_;
        for (auto* other : game.GetUnits()) {
            if (other->IsDead() || other == current) continue;
            float dx = other->GetX() - current->GetX();
            float dy = other->GetY() - current->GetY();
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist < bestDist) { bestDist = dist; next = other; }
        }
        current = next;
    }
}

// ===== Feral Spirit =====
CAbilityFeralSpirit::CAbilityFeralSpirit(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOsf"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(30.0f);
    SetManaCost(100);
}

void CAbilityFeralSpirit::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    for (int i = 0; i < count_; i++) {
        float x = caster->GetX() + (i - count_/2) * 64.0f;
        float y = caster->GetY() + 64.0f;
        auto* wolf = game.CreateUnit(wolfType_, caster->GetPlayerIndex(), x, y, 0);
        if (wolf) {
            // TODO: set summon duration, mark as summoned
        }
    }
}

// ===== Earthquake =====
CAbilityEarthquake::CAbilityEarthquake(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AOeq"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(1000.0f);
    SetCooldown(90.0f);
    SetManaCost(150);
}

void CAbilityEarthquake::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    // AOE damage to buildings
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead() || !unit->IsBuilding()) continue;
        float dx = unit->GetX() - x;
        float dy = unit->GetY() - y;
        if (dx*dx + dy*dy <= radius_*radius_) {
            unit->Damage(game, caster, false, false,
                         CAttackType::SIEGE, CDamageType::NORMAL,
                         "", damagePerSecond_ * duration_, 0);
        }
    }
}

// ===== Carrion Swarm =====
CAbilityCarrionSwarm::CAbilityCarrionSwarm(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUcs"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(700.0f);
    SetCooldown(10.0f);
    SetManaCost(110);
}

void CAbilityCarrionSwarm::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster) return;
    float dx = target->GetX() - caster->GetX();
    float dy = target->GetY() - caster->GetY();
    float dist = std::sqrt(dx*dx + dy*dy);
    if (dist < 1.0f) return;
    float nx = dx/dist, ny = dy/dist;
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead() || unit == caster) continue;
        float ux = unit->GetX() - caster->GetX();
        float uy = unit->GetY() - caster->GetY();
        float proj = ux*nx + uy*ny;
        if (proj < 0 || proj > range_) continue;
        float perp = std::abs(ux*ny - uy*nx);
        if (perp <= width_/2) {
            unit->Damage(game, caster, false, false,
                         CAttackType::NORMAL, CDamageType::NORMAL,
                         "", damage_, 0);
        }
    }
}

// ===== Sleep =====
CAbilitySleep::CAbilitySleep(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUsl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(6.0f);
    SetManaCost(100);
}

void CAbilitySleep::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    (void)caster;
    if (!target || target->IsDead()) return;
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        targetUnit->AddBuff(std::make_unique<CBuffStun>(duration_), game);
    }
}

// ===== Inferno =====
CAbilityInferno::CAbilityInferno(int handleId)
    : CAbilitySpellBase(handleId, War3ID("AUin"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(900.0f);
    SetCooldown(180.0f);
    SetManaCost(175);
}

void CAbilityInferno::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Stun nearby enemies
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead()) continue;
        float dx = unit->GetX() - x;
        float dy = unit->GetY() - y;
        if (dx*dx + dy*dy <= stunRadius_*stunRadius_) {
            unit->AddBuff(std::make_unique<CBuffStun>(stunDuration_), game);
        }
    }
    // Summon infernal
    auto* infernal = game.CreateUnit(infernalType_, caster->GetPlayerIndex(), x, y, 0);
    (void)infernal;
}

// ===== Silence =====
CAbilitySilence::CAbilitySilence(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsi"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(700.0f);
    SetCooldown(15.0f);
    SetManaCost(75);
}

void CAbilitySilence::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    (void)caster;
    for (auto* unit : game.GetUnits()) {
        if (unit->IsDead()) continue;
        float dx = unit->GetX() - x;
        float dy = unit->GetY() - y;
        if (dx*dx + dy*dy <= radius_*radius_) {
            // Apply silence as a stun (simplified)
            unit->AddBuff(std::make_unique<CBuffStun>(duration_), game);
        }
    }
}

// ===== Black Arrow =====
CAbilityBlackArrow::CAbilityBlackArrow(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANba"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(2.0f);
    SetManaCost(10);
}

void CAbilityBlackArrow::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster) return;
    target->Damage(game, caster, true, true,
                   CAttackType::PIERCE, CDamageType::NORMAL,
                   "", bonusDamage_, 0);
}

// ===== Life Drain =====
CAbilityLifeDrain::CAbilityLifeDrain(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANdr"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityLifeDrain::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    float totalDrain = drainPerSecond_ * duration_;
    target->Damage(game, caster, false, false,
                   CAttackType::NORMAL, CDamageType::NORMAL,
                   "", totalDrain, 0);
    float newLife = caster->GetLife() + totalDrain;
    if (newLife > caster->GetMaxLife()) newLife = caster->GetMaxLife();
    caster->SetLife(game, newLife);
}

// ===== Finger of Death =====
CAbilityFingerOfDeath::CAbilityFingerOfDeath(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANfd"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(800.0f);
    SetCooldown(0);
    SetManaCost(0);
}

void CAbilityFingerOfDeath::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target) return;
    target->Damage(game, caster, false, false,
                   CAttackType::NORMAL, CDamageType::NORMAL,
                   "", damage_, 0);
}

// ===== Dark Portal =====
CAbilityDarkPortal::CAbilityDarkPortal(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANdp"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(900.0f);
    SetCooldown(180.0f);
    SetManaCost(0);
}

void CAbilityDarkPortal::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    (void)game; (void)caster; (void)x; (void)y;
    // TODO: summon portal units
}

// ===== Rain of Chaos =====
CAbilityRainOfChaos::CAbilityRainOfChaos(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANrc"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(900.0f);
    SetCooldown(120.0f);
    SetManaCost(0);
}

void CAbilityRainOfChaos::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    for (int w = 0; w < waves_; w++) {
        float wx = x + (std::rand() % 200 - 100);
        float wy = y + (std::rand() % 200 - 100);
        for (auto* unit : game.GetUnits()) {
            if (unit->IsDead()) continue;
            float dx = unit->GetX() - wx;
            float dy = unit->GetY() - wy;
            if (dx*dx + dy*dy <= radius_*radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::NORMAL, CDamageType::NORMAL,
                             "", damage_, 0);
            }
        }
    }
}

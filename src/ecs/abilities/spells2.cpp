#include "spells2.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/abilities/ability_builder.h"
#include "data/abilitydata.h"
#include <cmath>
#include <iostream>

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

void CAbilityHealingWave::ApplyEffectData(const CAbilityDefFull& def, int level) {
    healAmount_ = def.DataFloatAt(level, 0, healAmount_);
    bounces_ = def.DataIntAt(level, 1, bounces_);
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
        // Hex: cannot attack or cast, but can move (canonical buff BOhx).
        targetUnit->AddBuff(std::make_unique<CBuffHex>(duration_), game);
    }
}

void CAbilityHex::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
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
                        CAttackType::SPELLS, CDamageType::LIGHTNING,
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

void CAbilityChainLightning::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
    bounces_ = def.DataIntAt(level, 1, bounces_);
    damageReduction_ = def.DataFloatAt(level, 2, damageReduction_);
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
        // Timed-life summon (ref CBuffTimedLife): wolves dissipate instead
        // of living forever; summoned callback fires for JASS GetSummonedUnit.
        SummonTimedUnit(game, caster, wolfType_, War3ID("AOsf"),
                        x, y, caster->GetFacing(), summonDuration_);
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
                         CAttackType::SPELLS, CDamageType::POISON,
                         "", damage_, 0);
        }
    }
}

void CAbilityCarrionSwarm::ApplyEffectData(const CAbilityDefFull& def, int level) {
    damage_ = def.DataFloatAt(level, 0, damage_);
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
    if (targetUnit && targetUnit->CanSleep()) {
        // Sleep: idles the target; any damage wakes it (see CUnit::Damage).
        targetUnit->AddBuff(std::make_unique<CBuffSleep>(duration_), game);
    }
}

void CAbilitySleep::ApplyEffectData(const CAbilityDefFull& def, int level) {
    duration_ = def.DurationAt(level);
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
    // Summon infernal with timed life (ref CBuffTimedLife).
    SummonTimedUnit(game, caster, infernalType_, War3ID("AUin"),
                    x, y, caster->GetFacing(), summonDuration_);
}

void CAbilityInferno::ApplyEffectData(const CAbilityDefFull& def, int level) {
    summonDuration_ = def.DataFloatAt(level, 1, summonDuration_);
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
        if (unit->IsDead() || unit == caster) continue;
        float dx = unit->GetX() - x;
        float dy = unit->GetY() - y;
        if (dx*dx + dy*dy <= radius_*radius_) {
            // Silence blocks casting only; the target keeps moving/attacking.
            unit->AddBuff(std::make_unique<CBuffSilence>(duration_), game);
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
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    // Channelled drain over time (ref): periodic damage + heal to caster.
    targetUnit->AddBuff(std::make_unique<CBuffLifeDrain>(
        duration_, drainPerSecond_ * 1.0f, caster->GetHandleId()), game);
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
                   CAttackType::SPELLS, CDamageType::DEATH,
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
    if (!caster) return;
    // First spell on the AbilityBuilder path: register the summon chain once,
    // then run it like the ref .j doEffect (summon + fired summoned events).
    if (!chainRegistered_) {
        ABEffectChain chain;
        chain.Add(std::make_unique<ABActionSummon>(
            portalUnitType_, War3ID("ANdp"), summonCount_, summonDuration_));
        AbilityBuilderRegistry::Instance().Register(War3ID("ANdp"), std::move(chain));
        chainRegistered_ = true;
    }
    if (const ABEffectChain* chain = AbilityBuilderRegistry::Instance().Find(War3ID("ANdp"))) {
        chain->Run(game, caster, nullptr, true, x, y);
    }
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
        // Deterministic synced scatter (std::rand is banned in sim code).
        float wx = x + game.SimRandRange(-100.0f, 100.0f);
        float wy = y + game.SimRandRange(-100.0f, 100.0f);
        for (auto* unit : game.GetUnits()) {
            if (unit->IsDead()) continue;
            float dx = unit->GetX() - wx;
            float dy = unit->GetY() - wy;
            if (dx*dx + dy*dy <= radius_*radius_) {
                unit->Damage(game, caster, false, false,
                             CAttackType::SPELLS, CDamageType::FIRE,
                             "", damage_, 0);
            }
        }
    }
}

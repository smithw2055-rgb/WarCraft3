#include "spells14.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/buffs.h"
#include "ecs/player.h"
#include "ecs/abilities/ability_builder.h"
#include <cmath>

// ===== Pocket Factory =====
CAbilityPocketFactory::CAbilityPocketFactory(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsy"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_POINT);
    SetCastRange(500.0f);
    SetCooldown(125.0f);
    SetManaCost(150);
}

void CAbilityPocketFactory::DoEffect(CSimulation& game, CUnit* caster, float x, float y) {
    if (!caster) return;
    // Graceful without unit data (ref summon-without-data).
    CUnit* factory = game.CreateUnit(War3ID("nfac"), caster->GetPlayerIndex(), x, y, 0);
    if (!factory) return;
    factory->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("ANsy"), factoryLife_), game);
    factory->AddBuff(std::make_unique<CBuffSpawner>(
        factoryLife_, War3ID("ncgb"), spawnInterval_,
        explodeDamage_, explodeRadius_), game);
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(caster->GetHandleId(), factory->GetHandleId());
    }
}

// ===== Lava Spawn =====
CAbilityLavaSpawn::CAbilityLavaSpawn(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANlm"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(32.0f);
    SetManaCost(150);
}

void CAbilityLavaSpawn::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    const float x = caster->GetX() + 64.0f;
    const float y = caster->GetY();
    CUnit* spawn = game.CreateUnit(War3ID("nlv1"), caster->GetPlayerIndex(), x, y, 0);
    if (!spawn) return;
    spawn->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("ANlm"), summonDuration_), game);
    spawn->AddBuff(std::make_unique<CBuffSplit>(
        War3ID("nlv2"), 2, War3ID("nlv3"), 2), game);
    if (const auto& summonedCb = game.GetUnitSummonedCallback()) {
        summonedCb(caster->GetHandleId(), spawn->GetHandleId());
    }
}

// ===== Soul Burn =====
CAbilitySoulBurn::CAbilitySoulBurn(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsb"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(9.0f);
    SetManaCost(75);
}

void CAbilitySoulBurn::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || target->IsDead()) return;
    target->Damage(game, caster, false, false,
                   CAttackType::SPELLS, CDamageType::FIRE,
                   "", damage_, 0);
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return;
    targetUnit->AddBuff(std::make_unique<CBuffPoison>(
        burnDuration_, burnPerTick_, caster ? caster->GetHandleId() : -1), game);
}

// ===== Quilbeast =====
CAbilityQuilbeast::CAbilityQuilbeast(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANsq"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
    SetCooldown(25.0f);
    SetManaCost(75);
}

void CAbilityQuilbeast::DoEffect(CSimulation& game, CUnit* caster) {
    if (!caster) return;
    SummonTimedUnit(game, caster, beastType_, War3ID("ANsq"),
                    caster->GetX() + 64.0f, caster->GetY(),
                    caster->GetFacing(), summonDuration_);
}

// ===== Frost Breath =====
CAbilityFrostBreath::CAbilityFrostBreath(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Afsh"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(300.0f);
    SetCooldown(8.0f);
    SetManaCost(100);
}

void CAbilityFrostBreath::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
    if (!target || !caster || target->IsDead()) return;
    const float tx = target->GetX(), ty = target->GetY();
    for (auto* unit : game.GetUnits()) {
        if (!unit || unit->IsDead() || unit == caster) continue;
        if (unit->GetPlayerIndex() == caster->GetPlayerIndex()) continue;
        const float dx = unit->GetX() - tx;
        const float dy = unit->GetY() - ty;
        const bool isPrimary = (unit == target);
        if (!isPrimary && dx * dx + dy * dy > radius_ * radius_) continue;
        unit->Damage(game, caster, false, false,
                     CAttackType::SPELLS, CDamageType::COLD,
                     "", damage_, 0);
        if (auto* frozen = dynamic_cast<CUnit*>(unit)) {
            frozen->AddBuff(std::make_unique<CBuffSlow>(
                slowDuration_, slowPercent_, caster->GetHandleId()), game);
        }
    }
}

// ===== Pulverize =====
CAbilityPulverize::CAbilityPulverize(int handleId)
    : CAbilitySpellBase(handleId, War3ID("Awar"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::NO_TARGET);
}

// ===== Forked Lightning =====
CAbilityForkedLightning::CAbilityForkedLightning(int handleId)
    : CAbilitySpellBase(handleId, War3ID("ANfl"), CAbilityCategory::SPELL) {
    SetTargetType(CSpellTargetType::TARGET_UNIT);
    SetCastRange(600.0f);
    SetCooldown(8.0f);
    SetManaCost(75);
}

void CAbilityForkedLightning::DoEffect(CSimulation& game, CUnit* caster, CWidget* target) {
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
            const float dx = other->GetX() - current->GetX();
            const float dy = other->GetY() - current->GetY();
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) { bestDist = dist; next = other; }
        }
        current = next;
    }
}

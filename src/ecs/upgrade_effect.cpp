#include "upgrade_effect.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/unit_attack.h"

// ===== CUpgradeEffect base =====
CUpgradeEffect::CUpgradeEffect(CUpgradeEffectType type, War3ID targetUnitType)
    : type_(type), targetUnitType_(targetUnitType) {}

void CUpgradeEffect::ApplyToPlayer(CSimulation& game, int playerIndex, int level) {
    auto* player = game.GetPlayer(playerIndex);
    if (!player) return;
    for (auto* unit : player->GetUnits()) {
        if (unit->IsDead()) continue;
        if (targetUnitType_.value == 0 || unit->GetTypeId() == targetUnitType_) {
            Apply(game, unit, level);
        }
    }
}

void CUpgradeEffect::UnapplyFromPlayer(CSimulation& game, int playerIndex, int level) {
    auto* player = game.GetPlayer(playerIndex);
    if (!player) return;
    for (auto* unit : player->GetUnits()) {
        if (unit->IsDead()) continue;
        if (targetUnitType_.value == 0 || unit->GetTypeId() == targetUnitType_) {
            Unapply(game, unit, level);
        }
    }
}

// ===== CUpgradeEffectAttackDamage =====
CUpgradeEffectAttackDamage::CUpgradeEffectAttackDamage(War3ID target, int baseValue, int perLevel)
    : CUpgradeEffect(CUpgradeEffectType::ATTACK_DAMAGE, target)
    , baseValue_(baseValue), perLevel_(perLevel) {}

void CUpgradeEffectAttackDamage::Apply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddBonusDamage(baseValue_ + perLevel_ * (level - 1));
}

void CUpgradeEffectAttackDamage::Unapply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddBonusDamage(-(baseValue_ + perLevel_ * (level - 1)));
}

// ===== CUpgradeEffectDefense =====
CUpgradeEffectDefense::CUpgradeEffectDefense(War3ID target, int baseValue, int perLevel)
    : CUpgradeEffect(CUpgradeEffectType::DEFENSE_BONUS, target)
    , baseValue_(baseValue), perLevel_(perLevel) {}

void CUpgradeEffectDefense::Apply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddBonusArmor(baseValue_ + perLevel_ * (level - 1));
}

void CUpgradeEffectDefense::Unapply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddBonusArmor(-(baseValue_ + perLevel_ * (level - 1)));
}

// ===== CUpgradeEffectHitPoints =====
CUpgradeEffectHitPoints::CUpgradeEffectHitPoints(War3ID target, int baseValue, int perLevel)
    : CUpgradeEffect(CUpgradeEffectType::HIT_POINTS, target)
    , baseValue_(baseValue), perLevel_(perLevel) {}

void CUpgradeEffectHitPoints::Apply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddBonusMaxLife((float)(baseValue_ + perLevel_ * (level - 1)));
}

void CUpgradeEffectHitPoints::Unapply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddBonusMaxLife(-(float)(baseValue_ + perLevel_ * (level - 1)));
}

// ===== CUpgradeEffectMoveSpeed =====
CUpgradeEffectMoveSpeed::CUpgradeEffectMoveSpeed(War3ID target, int baseValue, int perLevel)
    : CUpgradeEffect(CUpgradeEffectType::MOVE_SPEED, target)
    , baseValue_(baseValue), perLevel_(perLevel) {}

void CUpgradeEffectMoveSpeed::Apply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    int bonus = baseValue_ + perLevel_ * (level - 1);
    unit->SetSpeed(unit->GetSpeed() + bonus);
}

void CUpgradeEffectMoveSpeed::Unapply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    int bonus = baseValue_ + perLevel_ * (level - 1);
    unit->SetSpeed(unit->GetSpeed() - bonus);
}

// ===== CUpgradeEffectAttackRange =====
CUpgradeEffectAttackRange::CUpgradeEffectAttackRange(War3ID target, int baseValue, int perLevel)
    : CUpgradeEffect(CUpgradeEffectType::ATTACK_RANGE, target)
    , baseValue_(baseValue), perLevel_(perLevel) {}

void CUpgradeEffectAttackRange::Apply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddAttackRangeBonusAll((float)(baseValue_ + perLevel_ * (level - 1)));
}

void CUpgradeEffectAttackRange::Unapply(CSimulation& game, CUnit* unit, int level) {
    (void)game;
    if (!unit) return;
    unit->AddAttackRangeBonusAll(-(float)(baseValue_ + perLevel_ * (level - 1)));
}

#include "ability_attack.h"
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_attack.h"
#include "ecs/behaviors/behavior_attack_move.h"
#include "ecs/behaviors/behavior_move.h"
#include <cmath>

CAbilityAttack::CAbilityAttack(int handleId)
    : CAbility(handleId, War3ID("Aatk"), CAbilityCategory::ATTACK) {
    physical_ = true;
    iconShowing_ = false; // attack is always available, no icon
}

void CAbilityAttack::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game;
    // Create attack behaviors and store on unit
    auto attackBehavior = std::make_unique<CBehaviorAttack>();
    attackBehavior->SetUnit(unit);
    unit->SetAttackBehavior(std::move(attackBehavior));

    auto attackMoveBehavior = std::make_unique<CBehaviorAttackMove>();
    attackMoveBehavior->SetUnit(unit);
    unit->SetAttackMoveBehavior(std::move(attackMoveBehavior));
}

void CAbilityAttack::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game;
    unit->SetAttackBehavior(nullptr);
    unit->SetAttackMoveBehavior(nullptr);
}

bool CAbilityAttack::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    if (IsDisabled()) return false;
    if ((orderId != OrderIds::SMART) && (orderId != OrderIds::ATTACK)) return false;
    if (!target || target->IsDead()) return false;
    if (target == unit) return false; // no self-attack

    // Burrowed units cannot attack (ref crypt fiend).
    if (unit->GetBuff(War3ID("Bbur"))) return false;

    // Invisible units cannot be ordered against (ref invisibility).
    // Detectors (shades, sentry wards) see through stealth in range.
    if (auto* targetUnit = dynamic_cast<CUnit*>(target)) {
        if (targetUnit->IsInvisibleTo(unit)) return false;
    }

    auto* targetUnit = dynamic_cast<CUnit*>(target);
    auto* targetDest = dynamic_cast<CDestructable*>(target);

    // On SMART order, right-clicking destructables (trees) or gold mines never attacks
    if (orderId == OrderIds::SMART) {
        if (targetDest) return false;
        if (targetUnit) {
            if (targetUnit->GetAbility(War3ID("Agld")) || targetUnit->GetTypeId() == War3ID("ngol")) {
                return false;
            }
        }
    }

    // Check if target is an enemy (not allied)
    if (targetUnit) {
        if (targetUnit->GetAbility(War3ID("Agld")) || targetUnit->GetTypeId() == War3ID("ngol")) {
            return false;
        }
        auto* player = game.GetPlayer(unit->GetPlayerIndex());
        auto* targetPlayer = game.GetPlayer(targetUnit->GetPlayerIndex());
        if (player && targetPlayer) {
            // Check if allied or passive
            if (player->HasAlliance(*targetPlayer, CAllianceType::PASSIVE)) {
                return false;
            }
            // Neutral passive player (usually 15 or >= 24) cannot be targeted with right click
            if (targetPlayer->GetId() >= 15 && orderId == OrderIds::SMART) {
                return false;
            }
        }
    } else if (targetDest) {
        // Normal unit attacks don't target trees unless force-ordered with attack
        if (orderId != OrderIds::ATTACK) return false;
    }

    // Check if unit has any attacks available
    auto* unitType = unit->GetUnitType();
    if (unitType && !unitType->GetAttacks().empty()) {
        return true;
    }
    return false;
}

bool CAbilityAttack::CheckCanTargetPoint(CSimulation& game, CUnit* unit, int orderId, float x, float y) const {
    (void)game; (void)x; (void)y;
    if (IsDisabled()) return false;
    // Attack-move or attack-ground
    return orderId == OrderIds::ATTACK || orderId == OrderIds::ATTACK_GROUND;
}

bool CAbilityAttack::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit; (void)orderId;
    return false; // attack always needs a target
}

CBehavior* CAbilityAttack::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)orderId;
    if (!target) return nullptr;

    // Use the attack behavior
    auto* attackBehavior = caster->GetAttackBehavior();
    if (attackBehavior) {
        attackBehavior->SetTarget(target);
        return attackBehavior;
    }

    // Fallback: move to target if no attack behavior
    auto* moveBehavior = caster->GetMoveBehavior();
    if (moveBehavior) {
        moveBehavior->SetTarget(target->GetX(), target->GetY());
        return moveBehavior;
    }
    return nullptr;
}

CBehavior* CAbilityAttack::Begin(CSimulation& game, CUnit* caster, int orderId, float x, float y) {
    (void)game;
    if (orderId == OrderIds::ATTACK || orderId == OrderIds::MOVE_ATTACK) {
        // Attack-move
        auto* attackMoveBehavior = caster->GetAttackMoveBehavior();
        if (attackMoveBehavior) {
            attackMoveBehavior->SetDestination(x, y);
            return attackMoveBehavior;
        }
    }
    return nullptr;
}

CBehavior* CAbilityAttack::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)caster; (void)orderId;
    return nullptr;
}

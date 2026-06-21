#include "ability_harvest.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/behaviors/behavior_return_resources.h"
#include "ecs/player.h"
#include <iostream>

CAbilityHarvest::CAbilityHarvest(int handleId)
    : CAbility(handleId, War3ID("Ahar"), CAbilityCategory::HARVEST) {
    iconShowing_ = true;
}

void CAbilityHarvest::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    // Harvest doesn't create persistent behaviors on add
    // Behaviors are created per-order in Begin()
}

void CAbilityHarvest::OnRemove(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    ClearCarried();
}

void CAbilityHarvest::OnTick(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    // Passive effects could go here
}

bool CAbilityHarvest::CheckCanTarget(CSimulation& game, CUnit* unit, int orderId, CWidget* target) const {
    (void)orderId;
    if (IsDisabled()) return false;
    if (!target || target->IsDead()) return false;

    // Can target units (gold mines, town halls) or destructables (trees)
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        // Check if it's a gold mine (has gold to mine)
        auto* targetGoldMine = targetUnit->GetAbility(War3ID("Agld"));
        if (targetGoldMine) {
            return true; // can harvest gold from mine
        }

        // Check if it's a town hall (can return resources)
        auto* targetReturn = targetUnit->GetAbility(War3ID("Aret"));
        if (targetReturn && targetUnit->GetPlayerIndex() == unit->GetPlayerIndex()) {
            return true; // can return resources
        }
    }

    // TODO: Check if target is a destructable (tree)
    return false;
}

bool CAbilityHarvest::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit;
    if (IsDisabled()) return false;
    return orderId == OrderIds::RETURN_RESOURCES && carriedAmount_ > 0;
}

CBehavior* CAbilityHarvest::Begin(CSimulation& game, CUnit* caster, int orderId, CWidget* target) {
    (void)orderId;
    if (!target) return nullptr;

    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (!targetUnit) return nullptr;

    // Check if target is a gold mine
    auto* goldMine = targetUnit->GetAbility(War3ID("Agld"));
    if (goldMine) {
        // Create harvest behavior for gold mining
        auto* behavior = new CBehaviorHarvest();
        behavior->SetUnit(caster);
        behavior->SetTarget(target);
        behavior->SetResourceType(CResourceType::GOLD);
        behavior->SetCapacity(goldCapacity_);
        return behavior;
    }

    // Check if target is a town hall (return resources)
    auto* returnAbility = targetUnit->GetAbility(War3ID("Aret"));
    if (returnAbility && targetUnit->GetPlayerIndex() == caster->GetPlayerIndex()) {
        // Create return resources behavior
        auto* behavior = new CBehaviorReturnResources();
        behavior->SetUnit(caster);
        behavior->SetTarget(target);
        behavior->SetHarvestAbility(this);
        return behavior;
    }

    return nullptr;
}

CBehavior* CAbilityHarvest::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game;
    if (orderId == OrderIds::RETURN_RESOURCES && carriedAmount_ > 0) {
        // Find nearest town hall
        auto* behavior = new CBehaviorReturnResources();
        behavior->SetUnit(caster);
        behavior->SetHarvestAbility(this);
        behavior->FindNearestDropoff(game);
        return behavior;
    }
    return nullptr;
}

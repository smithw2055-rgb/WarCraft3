#include "engine/simulation_module.h"
#include "ability_harvest.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/player.h"
#include "data/destructabledata.h"
#include <iostream>

namespace {

bool CanReturnToTarget(CSimulation& game, const CAbilityHarvest* harvestAbility, CUnit* worker, CUnit* targetUnit) {
    if (!harvestAbility || !worker || !targetUnit) {
        return false;
    }
    const CResourceType carriedType = harvestAbility->GetCarriedResourceType();
    if (harvestAbility->GetCarriedAmount() <= 0 || carriedType == CResourceType::NONE) {
        return false;
    }
    if (targetUnit->GetPlayerIndex() != worker->GetPlayerIndex()) {
        return false;
    }

    if (CAbility* returnAbility = targetUnit->GetAbility(War3ID("Aret"))) {
        if (returnAbility->AcceptsResourceDropoff(carriedType)) return true;
    }
    if (auto* mod = game.GetSimulationModule()) {
        if (mod->CanUnitAcceptResourceDropoff(*targetUnit, static_cast<int>(carriedType))) return true;
    }
    return false;
}

}

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
    if (IsDisabled()) return false;
    if ((orderId != OrderIds::SMART) && (orderId != OrderIds::HARVEST) &&
        (orderId != OrderIds::RETURN_RESOURCES)) {
        return false;
    }
    if (!target || target->IsDead()) return false;

    // Can target units (gold mines, town halls) or destructables (trees)
    auto* targetUnit = ((target) ? (target)->AsUnit() : nullptr);
    if (targetUnit) {
        if (CAbility* mine = targetUnit->GetGoldMineAbility()) {
            return goldCapacity_ > 0 && mine->GetGold() > 0;
        }

        if (CanReturnToTarget(game, this, unit, targetUnit)) {
            return true;
        }
    }

    auto* targetDestructable = ((target) ? (target)->AsDestructable() : nullptr);
    if (targetDestructable) {
        if (lumberCapacity_ <= 0) {
            return false;
        }
        auto* destructableData = game.GetDestructableData();
        if (!destructableData) {
            return false;
        }
        const auto* destructableType = destructableData->GetDestructable(targetDestructable->GetTypeId());
        return destructableType && destructableType->isTree;
    }

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

    auto* targetUnit = ((target) ? (target)->AsUnit() : nullptr);
    if (targetUnit) {
        CAbility* mine = targetUnit->GetGoldMineAbility();
        if (mine && goldCapacity_ > 0 && mine->GetGold() > 0) {
            if (carriedAmount_ > 0 && carriedType_ != CResourceType::NONE) {
                lastTargetHandle_ = target->GetHandleId();
                return CBehavior::CreateReturnResources(game, caster, this);
            }

            lastTargetHandle_ = target->GetHandleId();
            return CBehavior::CreateHarvest(caster, target, static_cast<int>(CResourceType::GOLD), goldCapacity_);
        }

        if (CanReturnToTarget(game, this, caster, targetUnit)) {
            return CBehavior::CreateReturnResources(game, caster, this, target);
        }
    }

    auto* targetDestructable = ((target) ? (target)->AsDestructable() : nullptr);
    if (targetDestructable) {
        auto* destructableData = game.GetDestructableData();
        if (destructableData) {
            const auto* destructableType = destructableData->GetDestructable(targetDestructable->GetTypeId());
            if (destructableType && destructableType->isTree && lumberCapacity_ > 0) {
                if (carriedAmount_ > 0 && carriedType_ != CResourceType::NONE) {
                    lastTargetHandle_ = target->GetHandleId();
                    return CBehavior::CreateReturnResources(game, caster, this);
                }

                lastTargetHandle_ = target->GetHandleId();
                return CBehavior::CreateHarvest(caster, target, static_cast<int>(CResourceType::LUMBER), lumberCapacity_);
            }
        }
    }

    return nullptr;
}

CBehavior* CAbilityHarvest::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game;
    if (orderId == OrderIds::RETURN_RESOURCES && carriedAmount_ > 0) {
        return CBehavior::CreateReturnResources(game, caster, this);
    }
    return nullptr;
}

#include "ability_harvest.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/ability_return_resources.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/behaviors/behavior_return_resources.h"
#include "ecs/player.h"
#include "data/destructabledata.h"
#include <iostream>

namespace {

bool CanReturnToTarget(const CAbilityHarvest* harvestAbility, CUnit* worker, CUnit* targetUnit) {
    if (!harvestAbility || !worker || !targetUnit) {
        return false;
    }
    if (harvestAbility->GetCarriedAmount() <= 0 || harvestAbility->GetCarriedResourceType() == CResourceType::NONE) {
        return false;
    }
    if (targetUnit->GetPlayerIndex() != worker->GetPlayerIndex()) {
        return false;
    }

    auto* returnAbility = dynamic_cast<CAbilityReturnResources*>(targetUnit->GetAbility(War3ID("Aret")));
    if (!returnAbility) {
        return false;
    }

    switch (harvestAbility->GetCarriedResourceType()) {
        case CResourceType::GOLD:
            return returnAbility->AcceptsGold();
        case CResourceType::LUMBER:
            return returnAbility->AcceptsLumber();
        default:
            return false;
    }
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
    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        CMineAccess mine = MineAccessOf(targetUnit);
        if (mine.ok()) {
            return goldCapacity_ > 0 && mine.Gold() > 0;
        }

        if (CanReturnToTarget(this, unit, targetUnit)) {
            return true;
        }
    }

    auto* targetDestructable = dynamic_cast<CDestructable*>(target);
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

    auto* targetUnit = dynamic_cast<CUnit*>(target);
    if (targetUnit) {
        CMineAccess mine = MineAccessOf(targetUnit);
        if (mine.ok() && goldCapacity_ > 0 && mine.Gold() > 0) {
            if (carriedAmount_ > 0 && carriedType_ != CResourceType::NONE) {
                lastTargetHandle_ = target->GetHandleId();
                auto* behavior = new CBehaviorReturnResources();
                behavior->SetUnit(caster);
                behavior->SetHarvestAbility(this);
                behavior->FindNearestDropoff(game);
                return behavior;
            }

            auto* behavior = new CBehaviorHarvest();
            behavior->SetUnit(caster);
            behavior->SetTarget(target);
            behavior->SetResourceType(CResourceType::GOLD);
            behavior->SetCapacity(goldCapacity_);
            lastTargetHandle_ = target->GetHandleId();
            return behavior;
        }

        if (CanReturnToTarget(this, caster, targetUnit)) {
            auto* behavior = new CBehaviorReturnResources();
            behavior->SetUnit(caster);
            behavior->SetTarget(target);
            behavior->SetHarvestAbility(this);
            return behavior;
        }
    }

    auto* targetDestructable = dynamic_cast<CDestructable*>(target);
    if (targetDestructable) {
        auto* destructableData = game.GetDestructableData();
        if (destructableData) {
            const auto* destructableType = destructableData->GetDestructable(targetDestructable->GetTypeId());
            if (destructableType && destructableType->isTree && lumberCapacity_ > 0) {
                if (carriedAmount_ > 0 && carriedType_ != CResourceType::NONE) {
                    lastTargetHandle_ = target->GetHandleId();
                    auto* behavior = new CBehaviorReturnResources();
                    behavior->SetUnit(caster);
                    behavior->SetHarvestAbility(this);
                    behavior->FindNearestDropoff(game);
                    return behavior;
                }

                auto* behavior = new CBehaviorHarvest();
                behavior->SetUnit(caster);
                behavior->SetTarget(target);
                behavior->SetResourceType(CResourceType::LUMBER);
                behavior->SetCapacity(lumberCapacity_);
                lastTargetHandle_ = target->GetHandleId();
                return behavior;
            }
        }
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

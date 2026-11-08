#include "behavior_return_resources.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/orders/order.h"
#include "engine/simulation_module.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

CAbility* ResolveHarvestAbility(CUnit* unit, const CAbility* preferred) {
    if (!unit) {
        return nullptr;
    }
    if (preferred) {
        for (CAbility* ab : unit->GetAbilities()) {
            if (ab == preferred) {
                return ab;
            }
        }
    }
    if (CAbility* ahar = unit->GetAbility(War3ID("Ahar"))) {
        return ahar;
    }
    for (CAbility* ab : unit->GetAbilities()) {
        if (ab && ab->IsCarryingResources()) {
            return ab;
        }
    }
    return nullptr;
}

bool HasActiveResourceMine(const CUnit* unit) {
    if (!unit) {
        return false;
    }
    if (const CAbility* overlay = unit->GetAbility(War3ID("Agl2"))) {
        if (overlay->IsResourceMine() && overlay->GetGold() > 0) return true;
    }
    if (const CAbility* base = unit->GetAbility(War3ID("Agld"))) {
        if (base->IsResourceMine() && base->GetGold() > 0) return true;
    }
    for (const auto& ab : unit->GetAbilities()) {
        if (ab && ab->IsResourceMine() && ab->GetGold() > 0) {
            return true;
        }
    }
    return false;
}

bool CanAcceptReturnedResource(CSimulation& sim, const CUnit* targetUnit, CResourceType resourceType) {
    if (!targetUnit) {
        return false;
    }
    if (const CAbility* returnAbility = targetUnit->GetAbility(War3ID("Aret"))) {
        if (returnAbility->AcceptsResourceDropoff(resourceType)) {
            return true;
        }
    }
    for (const auto& ab : targetUnit->GetAbilities()) {
        if (ab && ab->AcceptsResourceDropoff(resourceType)) {
            return true;
        }
    }
    if (auto* mod = sim.GetSimulationModule()) {
        if (mod->CanUnitAcceptResourceDropoff(*targetUnit, static_cast<int>(resourceType))) {
            return true;
        }
    }
    return false;
}

CUnit* FindNearestMine(CSimulation& simulation, const CUnit* worker) {
    if (!worker) {
        return nullptr;
    }
    if (auto* mod = simulation.GetSimulationModule()) {
        if (CWidget* customMine = mod->OnFindNearestHarvestTarget(
                simulation, *worker, static_cast<int>(CResourceType::GOLD), worker->GetX(), worker->GetY())) {
            if (auto* mineUnit = dynamic_cast<CUnit*>(customMine)) {
                return mineUnit;
            }
        }
    }

    CUnit* nearest = nullptr;
    float minDistSq = std::numeric_limits<float>::max();
    for (auto* otherUnit : simulation.GetUnits()) {
        if (!otherUnit || otherUnit->IsDead() || otherUnit == worker) {
            continue;
        }
        if (!HasActiveResourceMine(otherUnit)) {
            continue;
        }
        const float dx = otherUnit->GetX() - worker->GetX();
        const float dy = otherUnit->GetY() - worker->GetY();
        const float distSq = dx * dx + dy * dy;
        if (distSq < minDistSq) {
            minDistSq = distSq;
            nearest = otherUnit;
        }
    }
    return nearest;
}

} // namespace

CBehaviorReturnResources::CBehaviorReturnResources() {}

CBehavior* CBehavior::CreateReturnResources(CSimulation& sim, CUnit* caster,
                                            const CAbility* harvestAbility,
                                            CWidget* explicitTarget) {
    auto* behavior = new CBehaviorReturnResources();
    behavior->SetUnit(caster);
    behavior->SetHarvestAbility(harvestAbility);
    if (explicitTarget) {
        behavior->SetTarget(explicitTarget);
    } else {
        behavior->FindNearestDropoff(sim);
    }
    return behavior;
}

void CBehaviorReturnResources::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

void CBehaviorReturnResources::FindNearestDropoff(CSimulation& simulation) {
    if (!unit_) return;

    CAbility* harvestAbility = ResolveHarvestAbility(unit_, harvestAbility_);
    const CResourceType resourceType = harvestAbility ? harvestAbility->GetCarriedResourceType() : CResourceType::NONE;
    if (resourceType == CResourceType::NONE) {
        target_ = nullptr;
        return;
    }

    if (auto* mod = simulation.GetSimulationModule()) {
        if (CUnit* customDropoff = mod->OnFindNearestResourceDropoff(
                simulation, *unit_, static_cast<int>(resourceType))) {
            target_ = customDropoff;
            return;
        }
    }

    float minDist = std::numeric_limits<float>::max();
    CUnit* nearest = nullptr;

    for (auto* otherUnit : simulation.GetUnits()) {
        if (!otherUnit || otherUnit->IsDead() || otherUnit == unit_) continue;
        if (otherUnit->GetPlayerIndex() != unit_->GetPlayerIndex()) continue;
        if (otherUnit->IsConstructing()) continue;
        if (!CanAcceptReturnedResource(simulation, otherUnit, resourceType)) continue;

        const float dx = otherUnit->GetX() - unit_->GetX();
        const float dy = otherUnit->GetY() - unit_->GetY();
        const float dist = dx * dx + dy * dy;
        if (dist < minDist) {
            minDist = dist;
            nearest = otherUnit;
        }
    }

    target_ = nearest;
}

void CBehaviorReturnResources::Begin(CSimulation& simulation) {
    movingToDropoff_ = true;
    depositing_ = false;
    depositTimer_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    propelling_ = false;

    if (!target_ || target_->IsDead()) {
        FindNearestDropoff(simulation);
    }
}

void CBehaviorReturnResources::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
}

void CBehaviorReturnResources::DepositCarriedResources(CSimulation& simulation) {
    if (!unit_) return;
    auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
    CAbility* harvestAbility = ResolveHarvestAbility(unit_, harvestAbility_);
    if (!player || !harvestAbility) return;

    const int amount = harvestAbility->GetCarriedAmount();
    const CResourceType type = harvestAbility->GetCarriedResourceType();

    if (type == CResourceType::GOLD) {
        player->AddGold(player->ApplyUpkeepGoldTax(amount, &simulation));
    } else if (type == CResourceType::LUMBER) {
        player->AddLumber(amount);
    }

    CWidget* nextTarget = simulation.GetWidget(harvestAbility->GetLastTargetHandle());
    if (auto* originUnit = dynamic_cast<CUnit*>(nextTarget)) {
        if (type == CResourceType::GOLD && !HasActiveResourceMine(originUnit)) {
            nextTarget = nullptr;
        }
    }
    if ((!nextTarget || nextTarget->IsDead()) && type == CResourceType::GOLD) {
        nextTarget = FindNearestMine(simulation, unit_);
    } else if ((!nextTarget || nextTarget->IsDead()) && type == CResourceType::LUMBER) {
        CWidget* origin = simulation.GetWidget(harvestAbility->GetLastTargetHandle());
        if (!origin) {
            origin = unit_;
        }
        nextTarget = nullptr;
        if (!simulation.IsPureEngineMode()) {
            if (auto* mod = simulation.GetSimulationModule()) {
                nextTarget = mod->OnFindNearestHarvestTarget(
                    simulation, *unit_, static_cast<int>(CResourceType::LUMBER), origin->GetX(), origin->GetY());
            }
        }
        if (!nextTarget) {
            nextTarget = MoveIntoRange::FindNearestLivingTree(simulation, origin->GetX(), origin->GetY());
        }
    }

    harvestAbility->ClearCarried();
    if (nextTarget && !nextTarget->IsDead()) {
        harvestAbility->SetLastTargetHandle(nextTarget->GetHandleId());
        unit_->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, nextTarget));
    }
}

bool CBehaviorReturnResources::Update(CSimulation& simulation) {
    if (!unit_) return true;

    if (!target_ || target_->IsDead()) {
        FindNearestDropoff(simulation);
        if (!target_) {
            propelling_ = false;
            return true;
        }
    }

    if (movingToDropoff_) {
        const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), target_->GetX(), target_->GetY());
        float arrival = 128.0f;
        if (auto* targetUnit = dynamic_cast<CUnit*>(target_)) {
            arrival = std::max(220.0f, targetUnit->GetCollisionSize() + 96.0f);
        }

        if (dist < arrival) {
            movingToDropoff_ = false;
            depositing_ = false;
            propelling_ = false;
            unit_->TurnTowardPoint(target_->GetX(), target_->GetY());
            DepositCarriedResources(simulation);
            return true;
        }

        MoveIntoRange::StepWorkerTowardTarget(
            simulation, unit_, target_->GetX(), target_->GetY(), dist,
            waypoints_, pathRecalcCooldown_, propelling_,
            false, false, true);
        return false;
    }

    if (depositing_) {
        propelling_ = false;
        depositTimer_ += simulation.GetSimStepTime();

        if (depositTimer_ >= depositDuration_) {
            DepositCarriedResources(simulation);
            return true;
        }
        return false;
    }

    propelling_ = false;
    return true;
}

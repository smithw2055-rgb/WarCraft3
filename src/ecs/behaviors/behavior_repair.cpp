#include "behavior_repair.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/ability.h"
#include <algorithm>
#include <cmath>

CBehaviorRepair::CBehaviorRepair() {}

CBehavior* CBehavior::CreateRepair(CUnit* caster, const CAbility* repairAbility,
                                   CWidget* target) {
    auto* behavior = new CBehaviorRepair();
    behavior->SetUnit(caster);
    behavior->SetRepairAbility(repairAbility);
    behavior->SetTarget(target);
    return behavior;
}

void CBehaviorRepair::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

void CBehaviorRepair::Begin(CSimulation& simulation) {
    (void)simulation;
    movingToTarget_ = true;
    repairing_ = false;
    propelling_ = false;
    totalGoldCharged_ = 0.0f;
    totalLumberCharged_ = 0.0f;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    RefreshRepairRates();
}

void CBehaviorRepair::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    movingToTarget_ = false;
    repairing_ = false;
}

void CBehaviorRepair::RefreshRepairRates() {
    goldPerUpdate_ = 0.0f;
    lumberPerUpdate_ = 0.0f;
    hpPerUpdate_ = 0.0f;

    auto* targetUnit = ((target_) ? (target_)->AsUnit() : nullptr);
    if (!repairAbility_ || !targetUnit) {
        return;
    }

    const int repairTime = std::max(1, targetUnit->GetUnitType()->GetRepairTime());
    const float timeRatio = repairAbility_->GetRepairRate();
    const float normalizedStep = (repairTime > 0) ? (1.0f / static_cast<float>(repairTime)) : 0.0f;
    goldPerUpdate_ = targetUnit->GetUnitType()->GetGoldRepairCost() * normalizedStep * timeRatio;
    lumberPerUpdate_ = targetUnit->GetUnitType()->GetLumberRepairCost() * normalizedStep * timeRatio;
    hpPerUpdate_ = targetUnit->GetMaxLife() * normalizedStep * timeRatio;
}

bool CBehaviorRepair::RecalculatePath(CSimulation& simulation) {
    if (!target_) return false;
    return MoveIntoRange::RecalculatePrunedWaypoints(
        simulation, unit_, target_->GetX(), target_->GetY(), waypoints_);
}

bool CBehaviorRepair::MoveTowardWaypoint(CSimulation& simulation) {
    if (!unit_ || !target_) return true;
    const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), target_->GetX(), target_->GetY());
    MoveIntoRange::StepWorkerTowardTarget(
        simulation, unit_, target_->GetX(), target_->GetY(), dist,
        waypoints_, pathRecalcCooldown_, propelling_, true, true, false);
    return false;
}

bool CBehaviorRepair::ChargeRepairResources(CSimulation& simulation) {
    auto* player = simulation.GetPlayer(unit_->GetPlayerIndex());
    if (!player) {
        return false;
    }

    const int goldDelta = static_cast<int>(totalGoldCharged_ + goldPerUpdate_) - static_cast<int>(totalGoldCharged_);
    const int lumberDelta = static_cast<int>(totalLumberCharged_ + lumberPerUpdate_) - static_cast<int>(totalLumberCharged_);

    if (player->GetGold() < goldDelta || player->GetLumber() < lumberDelta) {
        return false;
    }

    totalGoldCharged_ += goldPerUpdate_;
    totalLumberCharged_ += lumberPerUpdate_;
    if (goldDelta != 0) {
        player->AddGold(-goldDelta);
    }
    if (lumberDelta != 0) {
        player->AddLumber(-lumberDelta);
    }
    return true;
}

bool CBehaviorRepair::Update(CSimulation& simulation) {
    if (!unit_ || !target_) {
        return true;
    }

    auto* targetUnit = ((target_) ? (target_)->AsUnit() : nullptr);
    if (!targetUnit || targetUnit->IsDead() || targetUnit->GetLife() >= targetUnit->GetMaxLife()) {
        propelling_ = false;
        movingToTarget_ = false;
        repairing_ = false;
        return true;
    }

    const float repairRange = repairAbility_ ? repairAbility_->GetRepairRange() : 128.0f;
    const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), target_->GetX(), target_->GetY());
    const float arrival = std::max(repairRange, targetUnit->GetCollisionSize() + 64.0f);

    if (movingToTarget_) {
        if (dist <= arrival) {
            movingToTarget_ = false;
            repairing_ = true;
            propelling_ = false;
            unit_->TurnTowardPoint(target_->GetX(), target_->GetY());
            return false;
        }
        return MoveTowardWaypoint(simulation);
    }

    propelling_ = false;
    repairing_ = true;
    unit_->TurnTowardPoint(target_->GetX(), target_->GetY());

    if (!ChargeRepairResources(simulation)) {
        return true;
    }

    const float repairedLife = std::min(targetUnit->GetMaxLife(), targetUnit->GetLife() + hpPerUpdate_);
    targetUnit->SetLife(simulation, repairedLife);
    return repairedLife >= targetUnit->GetMaxLife();
}

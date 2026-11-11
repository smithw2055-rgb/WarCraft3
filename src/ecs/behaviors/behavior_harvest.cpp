#include "behavior_harvest.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/unit.h"
#include "ecs/player.h"
#include "ecs/orders/order.h"
#include "engine/simulation_module.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

// Resolves the active mine ability on a unit (overlayed mine Agl2 takes precedence over Agld)
CAbility* ResolveMineAbility(CUnit* unit) {
    if (!unit) return nullptr;
    if (CAbility* overlay = unit->GetAbility(War3ID("Agl2"))) {
        if (overlay->IsResourceMine()) return overlay;
    }
    if (CAbility* base = unit->GetAbility(War3ID("Agld"))) {
        if (base->IsResourceMine()) return base;
    }
    for (CAbility* ab : unit->GetAbilities()) {
        if (ab && ab->IsResourceMine()) {
            return ab;
        }
    }
    return nullptr;
}

} // namespace

CBehaviorHarvest::CBehaviorHarvest() {}

CBehavior* CBehavior::CreateHarvest(CUnit* caster, CWidget* target,
                                    int resourceTypeOrd, int capacity) {
    auto* behavior = new CBehaviorHarvest();
    behavior->SetUnit(caster);
    behavior->SetTarget(target);
    behavior->SetResourceType(static_cast<CResourceType>(resourceTypeOrd));
    behavior->SetCapacity(capacity);
    return behavior;
}

void CBehaviorHarvest::SetTarget(CWidget* target) {
    target_ = target;
    targetHandle_ = target ? target->GetHandleId() : -1;
    waypoints_.clear();
    if (target_ && unit_) {
        if (CAbility* harvestAbility = unit_->GetAbility(War3ID("Ahar"))) {
            harvestAbility->SetLastTargetHandle(target_->GetHandleId());
        }
    }
}

void CBehaviorHarvest::Begin(CSimulation& simulation) {
    (void)simulation;
    movingToResource_ = true;
    gathering_ = false;
    returning_ = false;
    inMine_ = false;
    gatherTimer_ = 0;
    gatheredAmount_ = 0;
    popoutFromMineTurnTick_ = 0;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
    propelling_ = false;
}

void CBehaviorHarvest::End(CSimulation& simulation) {
    if (resourceType_ == CResourceType::GOLD && inMine_) {
        if (auto* targetUnit = (targetHandle_ >= 0 && simulation.GetUnit(targetHandle_)) ? simulation.GetUnit(targetHandle_) : ((target_) ? (target_)->AsUnit() : nullptr)) {
            if (CAbility* mine = ResolveMineAbility(targetUnit)) {
                mine->RemoveMiner(this);
            }
        }
        if (unit_) {
            unit_->SetHidden(false);
            unit_->SetInvulnerable(false);
        }
    }
    (void)simulation;
    waypoints_.clear();
    propelling_ = false;
    inMine_ = false;
    popoutFromMineTurnTick_ = 0;
}

bool CBehaviorHarvest::Update(CSimulation& simulation) {
    if (!unit_) return true;

    const bool shouldReturnWithPayload = gatheredAmount_ > 0
        && (resourceType_ == CResourceType::GOLD || gatheredAmount_ >= capacity_);
    if (shouldReturnWithPayload) {
        propelling_ = false;
        if (CAbility* harvestAbility = unit_->GetAbility(War3ID("Ahar"))) {
            harvestAbility->SetCarried(resourceType_, gatheredAmount_);
        }
        unit_->IssueOrder(std::make_unique<COrderNoTarget>(OrderIds::RETURN_RESOURCES));
        return true;
    }

    if (inMine_) {
        propelling_ = false;
        return false;
    }

    if (movingToResource_) {
        if (!target_ || target_->IsDead()) {
            if (resourceType_ == CResourceType::LUMBER) {
                FindNearestTree(simulation);
            }
            if (!target_) {
                propelling_ = false;
                return true;
            }
        }

        const float dist = MoveIntoRange::Distance2D(unit_->GetX(), unit_->GetY(), target_->GetX(), target_->GetY());

        float arrival = 128.0f;
        if (auto* targetUnit = ((target_) ? (target_)->AsUnit() : nullptr)) {
            arrival = std::max(220.0f, targetUnit->GetCollisionSize() + 96.0f);
            if (resourceType_ == CResourceType::GOLD || ResolveMineAbility(targetUnit) || targetUnit->GetTypeId() == War3ID("ngol")) {
                arrival = 256.0f;
            }
        }
        if (dist < arrival) {
            movingToResource_ = false;
            propelling_ = false;
            unit_->TurnTowardPoint(target_->GetX(), target_->GetY());

            if (resourceType_ == CResourceType::GOLD) {
                if (auto* targetUnit = ((target_) ? (target_)->AsUnit() : nullptr)) {
                    if (CAbility* goldMine = ResolveMineAbility(targetUnit)) {
                        if (goldMine->GetGold() <= 0) {
                            return true;
                        }
                        if (goldMine->CanAddMiner()) {
                            goldMine->AddMiner(this);
                            unit_->SetHidden(true);
                            unit_->SetInvulnerable(true);
                            inMine_ = true;
                            gathering_ = false;
                            popoutFromMineTurnTick_ = simulation.GetGameTurnTick() + std::max(1, static_cast<int>(
                                std::ceil(goldMine->GetMiningDuration() / simulation.GetSimStepTime())));
                            return false;
                        }
                    }
                }
            }

            gathering_ = true;
            gatherTimer_ = 0;
            return false;
        }

        MoveIntoRange::StepWorkerTowardTarget(
            simulation, unit_, target_->GetX(), target_->GetY(), dist,
            waypoints_, pathRecalcCooldown_, propelling_,
            resourceType_ != CResourceType::GOLD, false, true);
        return false;
    }

    if (gathering_) {
        propelling_ = false;
        if (target_) {
            unit_->TurnTowardPoint(target_->GetX(), target_->GetY());
        }
        gatherTimer_ += simulation.GetSimStepTime();

        if (gatherTimer_ >= gatherDuration_) {
            gatherTimer_ = 0;

            if (resourceType_ == CResourceType::GOLD) {
                if (auto* targetUnit = ((target_) ? (target_)->AsUnit() : nullptr)) {
                    if (CAbility* goldMine = ResolveMineAbility(targetUnit)) {
                        if (goldMine->GetGold() <= 0) {
                            return true;
                        }
                        if (goldMine->CanAddMiner()) {
                            goldMine->AddMiner(this);
                            unit_->SetHidden(true);
                            unit_->SetInvulnerable(true);
                            inMine_ = true;
                            gathering_ = false;
                            popoutFromMineTurnTick_ = simulation.GetGameTurnTick() + std::max(1, static_cast<int>(
                                std::ceil(goldMine->GetMiningDuration() / simulation.GetSimStepTime())));
                        }
                    }
                }
            } else if (resourceType_ == CResourceType::LUMBER) {
                constexpr int chopAmount = 2;
                gatheredAmount_ = std::min(gatheredAmount_ + chopAmount, capacity_);
                if (auto* tree = ((target_) ? (target_)->AsDestructable() : nullptr)) {
                    tree->Damage(simulation, unit_, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", (float)chopAmount);
                    if (tree->IsDead() && gatheredAmount_ < capacity_) {
                        FindNearestTree(simulation);
                        if (target_) {
                            movingToResource_ = true;
                            gathering_ = false;
                            waypoints_.clear();
                            return false;
                        }
                        if (CAbility* harvestAbility = unit_->GetAbility(War3ID("Ahar"))) {
                            if (gatheredAmount_ > 0) {
                                harvestAbility->SetCarried(resourceType_, gatheredAmount_);
                                unit_->IssueOrder(std::make_unique<COrderNoTarget>(
                                    OrderIds::RETURN_RESOURCES));
                            }
                        }
                        propelling_ = false;
                        return true;
                    }
                }
            }

            if (gatheredAmount_ >= capacity_) {
                gathering_ = false;
                return false;
            }
        }
        return false;
    }

    propelling_ = false;
    return true;
}

void CBehaviorHarvest::PopoutFromMine(int goldMined) {
    inMine_ = false;
    gathering_ = false;
    movingToResource_ = false;
    propelling_ = false;
    popoutFromMineTurnTick_ = 0;

    if (unit_) {
        unit_->SetHidden(false);
        unit_->SetInvulnerable(false);
    }

    gatheredAmount_ = std::max(0, goldMined);
}

void CBehaviorHarvest::FindNearestTree(CSimulation& simulation) {
    float searchX = unit_ ? unit_->GetX() : 0.0f;
    float searchY = unit_ ? unit_->GetY() : 0.0f;
    if (target_) {
        searchX = target_->GetX();
        searchY = target_->GetY();
    }

    if (unit_ && !simulation.IsPureEngineMode()) {
        if (auto* mod = simulation.GetSimulationModule()) {
            if (CWidget* customTree = mod->OnFindNearestHarvestTarget(
                    simulation, *unit_, static_cast<int>(CResourceType::LUMBER), searchX, searchY)) {
                SetTarget(customTree);
                return;
            }
        }
    }

    SetTarget(MoveIntoRange::FindNearestLivingTree(simulation, searchX, searchY));
}

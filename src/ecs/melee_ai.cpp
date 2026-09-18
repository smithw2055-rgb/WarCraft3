#include "melee_ai.h"
#include "csimulation.h"
#include "unit.h"
#include "player.h"
#include "unit_type.h"
#include "data/unitdata.h"
#include "destructable.h"
#include "data/destructabledata.h"
#include "orders/order.h"
#include "behaviors/behavior.h"
#include "pathing/pathfinding.h"
#include <cmath>
#include <algorithm>

namespace {

bool HasAbility(const CUnitType* type, War3ID ability) {
    if (!type) return false;
    const auto& list = type->GetAbilityList();
    return std::find(list.begin(), list.end(), ability) != list.end();
}

float Dist2(float x1, float y1, float x2, float y2) {
    const float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

} // namespace

MeleeAIController::MeleeAIController(CSimulation* sim, int playerIndex)
    : sim_(sim), playerIndex_(playerIndex) {}

void MeleeAIController::SetDifficulty(int difficulty) {
    if (difficulty < 0) difficulty = 0;
    if (difficulty > 2) difficulty = 2;
    difficulty_ = static_cast<AIDifficulty>(difficulty);
}

void MeleeAIController::Command(int command, int data) {
    lastCommand_ = command;
    lastCommandData_ = data;
}

void MeleeAIController::Update() {
    if (!enabled_ || !sim_) return;
    if (sim_->IsPaused()) return;
    if (++tickCounter_ < 20) return; // think roughly once per second
    tickCounter_ = 0;
    Think();
}

bool MeleeAIController::IsWorkerType(const CUnitType* type) {
    return HasAbility(type, War3ID("Ahar"));
}

bool MeleeAIController::IsCombatType(const CUnitType* type) {
    if (!type || type->IsBuilding()) return false;
    for (const auto& attack : type->GetAttacks()) {
        if (attack.attackType != CAttackType::NONE) return true;
    }
    return false;
}

bool MeleeAIController::IsEnemyPlayer(int index) const {
    if (!sim_ || index == playerIndex_ || index < 0) return false;
    CPlayer* me = sim_->GetPlayer(playerIndex_);
    CPlayer* other = sim_->GetPlayer(index);
    if (!me || !other) return false;
    const CMapControl control = other->GetController();
    if (control != CMapControl::USER && control != CMapControl::COMPUTER) return false;
    return !me->HasAlliance(*other, CAllianceType::ALLIED);
}

void MeleeAIController::DiscoverPlan() {
    plan_ = AIRacePlan{};
    if (!sim_ || !sim_->GetUnitData()) return;
    CUnitData* unitData = sim_->GetUnitData();

    // Town hall: first owned building that trains units.
    std::string race;
    const CUnitType* townHallType = nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        const CUnitType* type = unit->GetUnitType();
        if (!type || !type->IsBuilding() || type->GetUnitsTrained().empty()) continue;
        townHallType = type;
        race = type->GetUnitRace().name;
        plan_.townHall = unit->GetTypeId();
        break;
    }
    if (!townHallType || race.empty()) return;

    // Worker: trained unit with a harvest ability, else cheapest trained.
    War3ID worker;
    int bestWorkerCost = INT32_MAX;
    for (War3ID id : townHallType->GetUnitsTrained()) {
        const CUnitType* type = unitData->GetUnitType(id);
        if (!type) continue;
        if (IsWorkerType(type)) {
            worker = id;
            break;
        }
        const int cost = type->GetGoldCost() + type->GetLumberCost();
        if (cost < bestWorkerCost) {
            bestWorkerCost = cost;
            worker = id;
        }
    }
    if (worker.value == 0) return;
    plan_.worker = worker;

    // Farm: cheapest same-race building producing food. Barracks: same-race
    // building training combat units. Army: cheapest combat unit it trains.
    int bestFarmCost = INT32_MAX;
    for (const auto& [id, type] : unitData->GetAllTypes()) {
        if (!type || !type->IsBuilding()) continue;
        if (type->GetUnitRace().name != race) continue;
        if (type->GetFoodMade() > 0) {
            const int cost = type->GetGoldCost() + type->GetLumberCost();
            if (cost < bestFarmCost) {
                bestFarmCost = cost;
                plan_.farm = id;
            }
        }
        if (id == plan_.townHall) continue;
        for (War3ID trainedId : type->GetUnitsTrained()) {
            const CUnitType* trained = unitData->GetUnitType(trainedId);
            if (IsCombatType(trained)) {
                plan_.barracks = id;
                break;
            }
        }
        if (plan_.barracks.value != 0) break;
    }
    if (plan_.barracks.value != 0) {
        const CUnitType* barracksType = unitData->GetUnitType(plan_.barracks);
        int bestArmyCost = INT32_MAX;
        if (barracksType) {
            for (War3ID id : barracksType->GetUnitsTrained()) {
                const CUnitType* type = unitData->GetUnitType(id);
                if (!IsCombatType(type)) continue;
                const int cost = type->GetGoldCost() + type->GetLumberCost();
                if (cost < bestArmyCost) {
                    bestArmyCost = cost;
                    plan_.armyUnit = id;
                }
            }
        }
    }
    plan_.valid = true;
}

CUnit* MeleeAIController::FindTownHall() const {
    if (!sim_) return nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (plan_.townHall.value != 0) {
            if (unit->GetTypeId() == plan_.townHall) return unit;
        } else if (unit->GetUnitType() && unit->GetUnitType()->IsBuilding() &&
                   !unit->GetUnitType()->GetUnitsTrained().empty()) {
            return unit;
        }
    }
    return nullptr;
}

bool MeleeAIController::FindBuildSpot(float nearX, float nearY, float& outX, float& outY) const {
    if (!sim_) return false;
    static const float kRadii[] = {256.0f, 384.0f, 512.0f, 704.0f};
    for (float radius : kRadii) {
        for (int step = 0; step < 8; step++) {
            const float angle = step * 3.14159265f / 4.0f;
            const float x = nearX + std::cos(angle) * radius;
            const float y = nearY + std::sin(angle) * radius;
            if (sim_->GetPathfinding() && sim_->GetPathfinding()->HasPathingMap()) {
                if (!sim_->GetPathfinding()->IsWorldPointPathableForUnit(
                        x, y, 0.0f, PathingGridMovementType::FOOT, nullptr)) {
                    continue;
                }
            }
            outX = x;
            outY = y;
            return true;
        }
    }
    return false;
}

CUnit* MeleeAIController::FindIdleWorker() const {
    if (!sim_ || plan_.worker.value == 0) return nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() != plan_.worker) continue;
        if (unit->HasQueuedOrders()) continue;
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() != CBehaviorCategory::STOP) continue;
        return unit;
    }
    return nullptr;
}

CUnit* MeleeAIController::FindNearestGoldMine(float x, float y) const {
    if (!sim_) return nullptr;
    CUnit* best = nullptr;
    float bestDist = 0.0f;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        const CUnitType* type = unit->GetUnitType();
        if (!type) continue;
        const auto& classes = type->GetClassifications();
        if (classes.find(CUnitClassification::GOLD_MINE) == classes.end()) continue;
        const float d = Dist2(x, y, unit->GetX(), unit->GetY());
        if (!best || d < bestDist) {
            best = unit;
            bestDist = d;
        }
    }
    return best;
}

CDestructable* MeleeAIController::FindNearestTree(float x, float y) const {
    if (!sim_ || !sim_->GetDestructableData()) return nullptr;
    CDestructable* best = nullptr;
    float bestDist = 0.0f;
    for (CDestructable* dest : sim_->GetDestructables()) {
        if (!dest || dest->IsDead()) continue;
        const CDestructableType* type = sim_->GetDestructableData()->GetDestructable(dest->GetTypeId());
        if (!type || !type->isTree) continue;
        const float d = Dist2(x, y, dest->GetX(), dest->GetY());
        if (!best || d < bestDist) {
            best = dest;
            bestDist = d;
        }
    }
    return best;
}

CUnit* MeleeAIController::FindIdleProduction(const War3ID& buildingType) const {
    if (!sim_ || buildingType.value == 0) return nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() == buildingType) return unit;
    }
    return nullptr;
}

int MeleeAIController::CountWorkers() const {
    if (!sim_ || plan_.worker.value == 0) return 0;
    int count = 0;
    for (CUnit* unit : sim_->GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetPlayerIndex() == playerIndex_ &&
            unit->GetTypeId() == plan_.worker) {
            count++;
        }
    }
    return count;
}

int MeleeAIController::CountArmy() const {
    if (!sim_ || plan_.armyUnit.value == 0) return 0;
    int count = 0;
    for (CUnit* unit : sim_->GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetPlayerIndex() == playerIndex_ &&
            unit->GetTypeId() == plan_.armyUnit) {
            count++;
        }
    }
    return count;
}

int MeleeAIController::CountBuildings(const War3ID& type) const {
    if (!sim_ || type.value == 0) return 0;
    int count = 0;
    for (CUnit* unit : sim_->GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetPlayerIndex() == playerIndex_ &&
            unit->GetTypeId() == type) {
            count++;
        }
    }
    return count;
}

bool MeleeAIController::FindAttackTarget(float& outX, float& outY) const {
    if (!sim_) return false;
    CUnit* home = FindTownHall();
    const float hx = home ? home->GetX() : 0.0f;
    const float hy = home ? home->GetY() : 0.0f;
    // Nearest enemy building first, then nearest enemy unit.
    CUnit* best = nullptr;
    float bestDist = 0.0f;
    for (int pass = 0; pass < 2 && !best; pass++) {
        for (CUnit* unit : sim_->GetUnits()) {
            if (!unit || unit->IsDead() || !IsEnemyPlayer(unit->GetPlayerIndex())) continue;
            const bool building = unit->GetUnitType() && unit->GetUnitType()->IsBuilding();
            if ((pass == 0) != building) continue;
            const float d = Dist2(hx, hy, unit->GetX(), unit->GetY());
            if (!best || d < bestDist) {
                best = unit;
                bestDist = d;
            }
        }
    }
    if (!best) return false;
    outX = best->GetX();
    outY = best->GetY();
    return true;
}

bool MeleeAIController::TryTrainWorkers() {
    if (!sim_ || plan_.worker.value == 0) return false;
    const int target = difficulty_ == AIDifficulty::EASY ? 4
        : difficulty_ == AIDifficulty::INSANE ? 8 : 6;
    if (CountWorkers() >= target) return false;
    CUnit* hall = FindTownHall();
    if (!hall) return false;
    const CUnitType* workerType = sim_->GetUnitData()->GetUnitType(plan_.worker);
    if (!workerType) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    if (player->GetGold() < workerType->GetGoldCost() ||
        player->GetLumber() < workerType->GetLumberCost()) {
        return false;
    }
    if (workerType->GetFoodUsed() > 0 &&
        player->GetFoodUsed() + workerType->GetFoodUsed() > player->GetFoodCap()) {
        return false;
    }
    hall->IssueOrder(std::make_unique<COrderNoTarget>(static_cast<int>(plan_.worker.value)));
    return true;
}

bool MeleeAIController::TryBuildSupply() {
    if (!sim_ || plan_.farm.value == 0) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    if (player->GetFoodUsed() < player->GetFoodCap() - 2) return false;
    const CUnitType* farmType = sim_->GetUnitData()->GetUnitType(plan_.farm);
    if (!farmType) return false;
    if (player->GetGold() < farmType->GetGoldCost() ||
        player->GetLumber() < farmType->GetLumberCost()) {
        return false;
    }
    CUnit* hall = FindTownHall();
    if (!hall) return false;
    CUnit* worker = FindIdleWorker();
    if (!worker) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindBuildSpot(hall->GetX(), hall->GetY(), x, y)) return false;
    worker->IssueOrder(
        std::make_unique<COrderTargetPoint>(static_cast<int>(plan_.farm.value), x, y));
    return true;
}

bool MeleeAIController::TryBuildBarracks() {
    if (!sim_ || plan_.barracks.value == 0 || CountBuildings(plan_.barracks) > 0) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    const CUnitType* barracksType = sim_->GetUnitData()->GetUnitType(plan_.barracks);
    if (!barracksType) return false;
    if (player->GetGold() < barracksType->GetGoldCost() + 150) return false;
    if (player->GetLumber() < barracksType->GetLumberCost()) return false;
    CUnit* hall = FindTownHall();
    if (!hall) return false;
    CUnit* worker = FindIdleWorker();
    if (!worker) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindBuildSpot(hall->GetX(), hall->GetY(), x, y)) return false;
    worker->IssueOrder(
        std::make_unique<COrderTargetPoint>(static_cast<int>(plan_.barracks.value), x, y));
    return true;
}

bool MeleeAIController::TryTrainArmy() {
    if (!sim_ || plan_.armyUnit.value == 0 || CountArmy() >= 12) return false;
    CUnit* barracks = FindIdleProduction(plan_.barracks);
    if (!barracks) return false;
    const CUnitType* armyType = sim_->GetUnitData()->GetUnitType(plan_.armyUnit);
    if (!armyType) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    if (player->GetGold() < armyType->GetGoldCost() ||
        player->GetLumber() < armyType->GetLumberCost()) {
        return false;
    }
    if (armyType->GetFoodUsed() > 0 &&
        player->GetFoodUsed() + armyType->GetFoodUsed() > player->GetFoodCap()) {
        return false;
    }
    barracks->IssueOrder(std::make_unique<COrderNoTarget>(static_cast<int>(plan_.armyUnit.value)));
    return true;
}

bool MeleeAIController::TryAttackWave(bool force) {
    if (!sim_) return false;
    const int waveSize = difficulty_ == AIDifficulty::EASY ? 3
        : difficulty_ == AIDifficulty::INSANE ? 9 : 6;
    const int army = CountArmy();
    if (army < (force ? 2 : waveSize)) return false;
    if (!force && sim_->GetGameTurnTick() - lastWaveTick_ < 600) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindAttackTarget(x, y)) return false;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (plan_.armyUnit.value != 0 && unit->GetTypeId() != plan_.armyUnit) continue;
        unit->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::ATTACK, x, y));
    }
    lastWaveTick_ = sim_->GetGameTurnTick();
    waveNumber_++;
    return true;
}

void MeleeAIController::AssignIdleWorkers() {
    if (!sim_) return;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return;
    CUnit* worker = nullptr;
    int lumberCrew = 0;
    // Count workers already on lumber duty is impractical without order
    // introspection; instead dedicate by threshold each think.
    while ((worker = FindIdleWorker()) != nullptr) {
        const bool needLumber = player->GetLumber() < 150 && lumberCrew < 2;
        if (needLumber) {
            CDestructable* tree = FindNearestTree(worker->GetX(), worker->GetY());
            if (tree) {
                worker->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, tree));
                lumberCrew++;
                continue;
            }
        }
        CUnit* mine = FindNearestGoldMine(worker->GetX(), worker->GetY());
        if (mine) {
            worker->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, mine));
            continue;
        }
        CDestructable* tree = FindNearestTree(worker->GetX(), worker->GetY());
        if (tree) {
            worker->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, tree));
            continue;
        }
        break; // nothing to harvest; stay idle
    }
}

void MeleeAIController::Think() {
    thinkCount_++;
    if (!sim_ || !sim_->GetUnitData()) return;
    if (!plan_.valid) DiscoverPlan();
    if (!plan_.valid) {
        // No base: throw the army at the enemy as a last resort.
        TryAttackWave(true);
        return;
    }
    // Point defense: engage nearby threats with the standing army.
    CUnit* hall = FindTownHall();
    if (hall && plan_.armyUnit.value != 0) {
        CUnit* threat = nullptr;
        float bestDist = 1500.0f * 1500.0f;
        for (CUnit* unit : sim_->GetUnits()) {
            if (!unit || unit->IsDead() || !IsEnemyPlayer(unit->GetPlayerIndex())) continue;
            if (unit->GetUnitType() && unit->GetUnitType()->IsBuilding()) continue;
            const float d = Dist2(hall->GetX(), hall->GetY(), unit->GetX(), unit->GetY());
            if (d < bestDist) {
                bestDist = d;
                threat = unit;
            }
        }
        if (threat) {
            for (CUnit* unit : sim_->GetUnits()) {
                if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
                if (unit->GetTypeId() != plan_.armyUnit) continue;
                unit->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::ATTACK, threat));
            }
        }
    }
    AssignIdleWorkers();
    TryTrainWorkers();
    TryBuildSupply();
    TryBuildBarracks();
    TryTrainArmy();
    const bool forced = lastCommand_ == 1;
    if (TryAttackWave(forced) && forced) lastCommand_ = -1;
}

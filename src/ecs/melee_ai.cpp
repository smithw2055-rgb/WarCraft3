#include "melee_ai.h"
#include "csimulation.h"
#include "unit.h"
#include "player.h"
#include "unit_type.h"
#include "data/unitdata.h"
#include "destructable.h"
#include "data/destructabledata.h"
#include "abilities/ability_gold_mine.h"
#include "abilities/ability_build.h"
#include "abilities/ability_queue.h"
#include "data/upgradedata.h"
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

void MeleeAIController::IssueAiOrder(CUnit* unit, std::unique_ptr<COrder> order) {
    if (!unit || !order) return;
    if (sim_) {
        sim_->IssueUnitOrder(unit->GetHandleId(), std::move(order));
    } else {
        unit->IssueOrder(std::move(order));
    }
}

void MeleeAIController::SetDifficulty(int difficulty) {
    if (difficulty < 0) difficulty = 0;
    if (difficulty > 2) difficulty = 2;
    difficulty_ = static_cast<AIDifficulty>(difficulty);
}

void MeleeAIController::Command(int command, int data) {
    lastCommand_ = command;
    lastCommandData_ = data;
}

int MeleeAIController::GetThinkIntervalTicks() const {
    if (difficulty_ == AIDifficulty::EASY) return 40;
    if (difficulty_ == AIDifficulty::INSANE) return 10;
    return 20;
}

int MeleeAIController::GetWaveCooldownTicks() const {
    if (difficulty_ == AIDifficulty::EASY) return 1200;
    if (difficulty_ == AIDifficulty::INSANE) return 300;
    return 600;
}

void MeleeAIController::Update() {
    if (!enabled_ || !sim_) return;
    if (sim_->IsPaused()) return;
    if (++tickCounter_ < GetThinkIntervalTicks()) return; // ~1/sec on NORMAL
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

bool MeleeAIController::IsTowerType(const CUnitType* type) {
    if (!type || !type->IsBuilding()) return false;
    // Armed buildings that train nothing: guard towers, not production.
    if (!type->GetUnitsTrained().empty()) return false;
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
    // Altar: same-race building training hero units (ref altar revive).
    // Hero: cheapest hero it trains. Skipped silently on hero-less data.
    for (const auto& [id, type] : unitData->GetAllTypes()) {
        if (!type || !type->IsBuilding()) continue;
        if (type->GetUnitRace().name != race) continue;
        if (id == plan_.townHall) continue;
        int bestHeroCost = INT32_MAX;
        War3ID bestHero;
        for (War3ID trainedId : type->GetUnitsTrained()) {
            const CUnitType* trained = unitData->GetUnitType(trainedId);
            if (!trained || !trained->IsHero()) continue;
            const int cost = trained->GetGoldCost() + trained->GetLumberCost();
            if (cost < bestHeroCost) {
                bestHeroCost = cost;
                bestHero = trainedId;
            }
        }
        if (bestHero.value != 0) {
            plan_.altar = id;
            plan_.hero = bestHero;
            break;
        }
    }
    // Tower: cheapest armed same-race building that trains nothing (guard
    // towers; orc burrows match too and double as farms, which is fine).
    int bestTowerCost = INT32_MAX;
    for (const auto& [id, type] : unitData->GetAllTypes()) {
        if (!IsTowerType(type.get())) continue;
        if (type->GetUnitRace().name != race) continue;
        const int cost = type->GetGoldCost() + type->GetLumberCost();
        if (cost < bestTowerCost) {
            bestTowerCost = cost;
            plan_.tower = id;
        }
    }
    plan_.valid = true;
}

CUnit* MeleeAIController::FindTownHall() const {
    if (!sim_) return nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsConstructing() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (plan_.townHall.value != 0) {
            if (unit->GetTypeId() == plan_.townHall) return unit;
        } else if (unit->GetUnitType() && unit->GetUnitType()->IsBuilding() &&
                   !unit->GetUnitType()->GetUnitsTrained().empty()) {
            return unit;
        }
    }
    return nullptr;
}

bool MeleeAIController::FindBuildSpot(float nearX, float nearY, const War3ID& buildingType,
                                       float& outX, float& outY) const {
    if (!sim_) return false;
    // Validate the full footprint (ref CAbilityBuild::IsBuildLocationObstructed),
    // not just the walkable center: otherwise Begin() rejects the order at
    // dispatch time and construction never starts.
    static const float kRadii[] = {256.0f, 384.0f, 512.0f, 704.0f, 896.0f, 1088.0f};
    for (float radius : kRadii) {
        for (int step = 0; step < 12; step++) {
            const float angle = step * 3.14159265f / 6.0f;
            float x = nearX + std::cos(angle) * radius;
            float y = nearY + std::sin(angle) * radius;
            const CUnitType* buildType =
                (sim_ && sim_->GetUnitData()) ? sim_->GetUnitData()->GetUnitType(buildingType) : nullptr;
            CAbilityBuild::RoundBuildPositionForType(buildType, x, y);
            if (sim_->GetPathfinding() && sim_->GetPathfinding()->HasPathingMap()) {
                if (!sim_->GetPathfinding()->IsWorldPointPathableForUnit(
                        x, y, 0.0f, PathingGridMovementType::FOOT, nullptr)) {
                    continue;
                }
            }
            if (!CAbilityBuild::IsBuildLocationObstructed(*sim_, x, y, playerIndex_, buildingType)) {
                outX = x;
                outY = y;
                return true;
            }
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

CUnit* MeleeAIController::FindWorkerForBuild() const {
    if (CUnit* idle = FindIdleWorker()) return idle;
    if (!sim_ || plan_.worker.value == 0) return nullptr;
    // Pull the nearest harvester to the town hall; never interrupt a worker
    // that is already constructing (BUILD behavior).
    CUnit* hall = FindTownHall();
    const float hx = hall ? hall->GetX() : 0.0f;
    const float hy = hall ? hall->GetY() : 0.0f;
    CUnit* best = nullptr;
    float bestDist = 0.0f;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() != plan_.worker) continue;
        // Never yank a worker riding inside a gold mine (hidden) or aboard
        // a transport: clearing its orders would strand it hidden forever.
        if (unit->IsHidden() || unit->IsLoadedAboard()) continue;
        // NOTE: no HasQueuedOrders filter here — harvesters perpetually
        // carry chained orders, so requiring an empty queue would starve
        // construction forever. The caller clears the queue when
        // commandeering (carried resources ride along on the ability).
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() == CBehaviorCategory::BUILD) continue;
        const float d = Dist2(hx, hy, unit->GetX(), unit->GetY());
        if (!best || d < bestDist) {
            best = unit;
            bestDist = d;
        }
    }
    return best;
}

CUnit* MeleeAIController::FindNearestGoldMine(float x, float y) const {
    if (!sim_) return nullptr;
    // Ability-based (ref MineAccessOf): unit classifications are not loaded
    // from data, so the GOLD_MINE tag can never match. A mine must also have
    // gold left and be visible (haunted backing mines are hidden).
    CUnit* best = nullptr;
    float bestDist = 0.0f;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        CMineAccess mine = MineAccessOf(unit);
        if (!mine.ok() || mine.Gold() <= 0) continue;
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
    CUnit* bestAvailable = nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsConstructing() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() == buildingType) {
            auto* queue = dynamic_cast<CAbilityQueue*>(unit->GetAbility(War3ID("Aque")));
            if (!queue || queue->GetQueueCount() == 0) {
                return unit; // completely idle!
            }
            if (queue->GetQueueCount() < CAbilityQueue::MAX_QUEUE && !bestAvailable) {
                bestAvailable = unit; // has room in queue
            }
        }
    }
    return bestAvailable;
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
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() == type) count++;
    }
    return count;
}

std::vector<CUnit*> MeleeAIController::FindOwnedHalls() const {
    std::vector<CUnit*> halls;
    if (!sim_ || plan_.townHall.value == 0) return halls;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsConstructing() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() == plan_.townHall) halls.push_back(unit);
    }
    return halls;
}

CUnit* MeleeAIController::FindRichMine(float x, float y, const CUnit* exclude) const {
    if (!sim_) return nullptr;
    CUnit* best = nullptr;
    float bestDist = 0.0f;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden() || unit == exclude) continue;
        CMineAccess mine = MineAccessOf(unit);
        if (!mine.ok() || mine.Gold() <= 0) continue;
        const float d = Dist2(x, y, unit->GetX(), unit->GetY());
        if (!best || d < bestDist) {
            best = unit;
            bestDist = d;
        }
    }
    return best;
}

int MeleeAIController::CountHeroes() const {
    if (!sim_) return 0;
    int count = 0;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        const CUnitType* type = unit->GetUnitType();
        if (type && type->IsHero()) count++;
    }
    return count;
}

CUnit* MeleeAIController::FindDeadHero() const {
    if (!sim_) return nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || !unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        const CUnitType* type = unit->GetUnitType();
        if (type && type->IsHero()) return unit;
    }
    return nullptr;
}

CUnit* MeleeAIController::FindAltar() const {
    if (!sim_ || plan_.altar.value == 0) return nullptr;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsConstructing() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() == plan_.altar) return unit;
    }
    return nullptr;
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
    const std::vector<CUnit*> halls = FindOwnedHalls();
    if (halls.empty()) return false;
    // ~5 workers per base: the expo staffs itself once it lands.
    const int perBase = difficulty_ == AIDifficulty::EASY ? 4
        : difficulty_ == AIDifficulty::INSANE ? 6 : 5;
    const int target = perBase + 3 * ((int)halls.size() - 1);
    if (CountWorkers() >= target) return false;
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
    // First hall with queue room trains (expo halls join automatically).
    for (CUnit* hall : halls) {
        if (!hall) continue;
        bool room = true;
        for (CAbility* ab : hall->GetAbilities()) {
            if (auto* queue = dynamic_cast<CAbilityQueue*>(ab)) {
                if (queue->GetQueueCount() >= CAbilityQueue::MAX_QUEUE) room = false;
                break;
            }
        }
        if (!room) continue;
        IssueAiOrder(hall, std::make_unique<COrderNoTarget>(static_cast<int>(plan_.worker.value)));
        return true;
    }
    return false;
}

bool MeleeAIController::TryBuildSupply() {
    if (!sim_ || plan_.farm.value == 0) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    if (player->GetFoodUsed() < player->GetFoodCap() - 2) return false;
    // One construction at a time: a builder is already on duty.
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() == CBehaviorCategory::BUILD) return false;
    }
    const CUnitType* farmType = sim_->GetUnitData()->GetUnitType(plan_.farm);
    if (!farmType) return false;
    if (player->GetGold() < farmType->GetGoldCost() ||
        player->GetLumber() < farmType->GetLumberCost()) {
        return false;
    }
    CUnit* hall = FindTownHall();
    if (!hall) return false;
    CUnit* worker = FindWorkerForBuild();
    if (!worker) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindBuildSpot(hall->GetX(), hall->GetY(), plan_.farm, x, y)) return false;
    // Commandeer promptly: clear any chained harvest orders first (carried
    // resources stay on the ability and resume afterwards).
    worker->ClearOrders();
    IssueAiOrder(worker,
        std::make_unique<COrderTargetPoint>(static_cast<int>(plan_.farm.value), x, y));
    return true;
}

bool MeleeAIController::TryBuildBarracks() {
    if (!sim_ || plan_.barracks.value == 0 || CountBuildings(plan_.barracks) > 0) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    // One construction at a time (ref TryBuildSupply).
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() == CBehaviorCategory::BUILD) return false;
    }
    const CUnitType* barracksType = sim_->GetUnitData()->GetUnitType(plan_.barracks);
    if (!barracksType) return false;
    if (player->GetGold() < barracksType->GetGoldCost() + 150) return false;
    if (player->GetLumber() < barracksType->GetLumberCost()) return false;
    CUnit* hall = FindTownHall();
    if (!hall) return false;
    CUnit* worker = FindWorkerForBuild();
    if (!worker) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindBuildSpot(hall->GetX(), hall->GetY(), plan_.barracks, x, y)) return false;
    worker->ClearOrders();
    IssueAiOrder(worker,
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
    // High upkeep (60% gold tax): only train when rich, so the AI does not
    // tax-spiral. Level comes from food-derived thresholds (ref upkeep).
    if (player->GetUpkeepLevel() >= 2 &&
        player->GetGold() < armyType->GetGoldCost() + 300) {
        return false;
    }
    if (armyType->GetFoodUsed() > 0 &&
        player->GetFoodUsed() + armyType->GetFoodUsed() > player->GetFoodCap()) {
        return false;
    }
    IssueAiOrder(barracks, std::make_unique<COrderNoTarget>(static_cast<int>(plan_.armyUnit.value)));
    return true;
}

bool MeleeAIController::TryBuildAltar() {
    if (!sim_ || plan_.altar.value == 0 || CountBuildings(plan_.altar) > 0) return false;
    // Altar after barracks: heroes need an army to protect them.
    if (plan_.barracks.value != 0 && CountBuildings(plan_.barracks) == 0) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    // One construction at a time (ref TryBuildSupply).
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() == CBehaviorCategory::BUILD) return false;
    }
    const CUnitType* altarType = sim_->GetUnitData()->GetUnitType(plan_.altar);
    if (!altarType) return false;
    if (player->GetGold() < altarType->GetGoldCost() + 100) return false;
    if (player->GetLumber() < altarType->GetLumberCost()) return false;
    CUnit* hall = FindTownHall();
    if (!hall) return false;
    CUnit* worker = FindWorkerForBuild();
    if (!worker) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindBuildSpot(hall->GetX(), hall->GetY(), plan_.altar, x, y)) return false;
    worker->ClearOrders();
    IssueAiOrder(worker,
        std::make_unique<COrderTargetPoint>(static_cast<int>(plan_.altar.value), x, y));
    return true;
}

bool MeleeAIController::TryBuildTowers() {
    if (!sim_ || plan_.tower.value == 0) return false;
    // Cap scales with difficulty; towers wait until the base exists
    // (barracks up) so defense never starves the opening.
    const int cap = difficulty_ == AIDifficulty::EASY ? 1
        : difficulty_ == AIDifficulty::INSANE ? 4 : 2;
    if (CountBuildings(plan_.tower) >= cap) return false;
    if (plan_.barracks.value != 0 && CountBuildings(plan_.barracks) == 0) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    // One construction at a time (ref TryBuildSupply).
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() == CBehaviorCategory::BUILD) return false;
    }
    const CUnitType* towerType = sim_->GetUnitData()->GetUnitType(plan_.tower);
    if (!towerType) return false;
    if (player->GetGold() < towerType->GetGoldCost() + 150) return false;
    if (player->GetLumber() < towerType->GetLumberCost()) return false;
    CUnit* hall = FindTownHall();
    if (!hall) return false;
    CUnit* worker = FindWorkerForBuild();
    if (!worker) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindBuildSpot(hall->GetX(), hall->GetY(), plan_.tower, x, y)) return false;
    worker->ClearOrders();
    IssueAiOrder(worker,
        std::make_unique<COrderTargetPoint>(static_cast<int>(plan_.tower.value), x, y));
    return true;
}

bool MeleeAIController::TryExpand() {
    if (!sim_ || plan_.townHall.value == 0) return false;
    CUnit* mainHall = FindTownHall();
    if (!mainHall) return false;
    // One expo at a time; the second hall joins worker training by itself.
    if ((int)FindOwnedHalls().size() >= 2) return false;
    // The main mine still has reserves: no reason to walk across the map.
    CUnit* mainMine = FindRichMine(mainHall->GetX(), mainHall->GetY(), nullptr);
    if (!mainMine) return false;
    if (MineAccessOf(mainMine).Gold() > 3000) return false;
    // Richest unowned mine: nearest with real gold that is not the main one.
    CUnit* expoMine = FindRichMine(mainHall->GetX(), mainHall->GetY(), mainMine);
    if (!expoMine || MineAccessOf(expoMine).Gold() < 1500) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    const CUnitType* hallType = sim_->GetUnitData()->GetUnitType(plan_.townHall);
    if (!hallType) return false;
    if (player->GetGold() < hallType->GetGoldCost() + 200) return false;
    if (player->GetLumber() < hallType->GetLumberCost()) return false;
    // One construction at a time (ref TryBuildSupply).
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() == CBehaviorCategory::BUILD) return false;
    }
    CUnit* worker = FindWorkerForBuild();
    if (!worker) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindBuildSpot(expoMine->GetX(), expoMine->GetY(), plan_.townHall, x, y)) return false;
    worker->ClearOrders();
    IssueAiOrder(worker,
        std::make_unique<COrderTargetPoint>(static_cast<int>(plan_.townHall.value), x, y));
    // Walk three harvesters over so the expo mines from landing: without a
    // transfer the new hall idles (AssignIdleWorkers only claims idle ones).
    int moved = 0;
    for (CUnit* unit : sim_->GetUnits()) {
        if (moved >= 3) break;
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (unit->GetTypeId() != plan_.worker) continue;
        if (unit == worker || unit->IsHidden() || unit->IsLoadedAboard()) continue;
        CBehavior* behavior = unit->GetCurrentBehavior();
        if (behavior && behavior->GetCategory() == CBehaviorCategory::BUILD) continue;
        unit->ClearOrders();
        IssueAiOrder(unit, std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, expoMine));
        moved++;
    }
    return true;
}

int MeleeAIController::MaxHeroes() const {
    // Retail hero limit is 3 per tavern tier in melee; the AI paces by
    // difficulty so EASY fields a single hero.
    if (difficulty_ == AIDifficulty::EASY) return 1;
    if (difficulty_ == AIDifficulty::INSANE) return 3;
    return 2;
}

bool MeleeAIController::TryTrainHero() {
    if (!sim_ || plan_.hero.value == 0) return false;
    if (CountHeroes() >= MaxHeroes()) return false;
    if (FindDeadHero()) return false; // revive first; see TryReviveHeroes
    // Economy first: no hero before the workforce is saturated and an
    // escort exists, so early waves and worker targets are never starved.
    // (Saturation matches TryTrainWorkers: per-base crew plus expo top-up.)
    const int perBase = difficulty_ == AIDifficulty::EASY ? 4
        : difficulty_ == AIDifficulty::INSANE ? 6 : 5;
    const int workerTarget = perBase + 3 * ((int)FindOwnedHalls().size() - 1);
    if (CountWorkers() < workerTarget || CountArmy() < 2) return false;
    CUnit* altar = FindAltar();
    if (!altar) return false;
    const CUnitType* heroType = sim_->GetUnitData()->GetUnitType(plan_.hero);
    if (!heroType) return false;
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!player) return false;
    if (player->GetGold() < heroType->GetGoldCost() ||
        player->GetLumber() < heroType->GetLumberCost()) {
        return false;
    }
    if (heroType->GetFoodUsed() > 0 &&
        player->GetFoodUsed() + heroType->GetFoodUsed() > player->GetFoodCap()) {
        return false;
    }
    IssueAiOrder(altar, std::make_unique<COrderNoTarget>(static_cast<int>(plan_.hero.value)));
    return true;
}

bool MeleeAIController::TryReviveHeroes() {
    if (!sim_) return false;
    CUnit* dead = FindDeadHero();
    if (!dead) return false;
    CUnit* altar = FindAltar();
    if (!altar) return false;
    // The revive order id is the dead hero's handle (ref CAbilityReviveHero);
    // affordability is enforced by the ability itself (soft fail).
    if (!altar->GetAbility(War3ID("Arev"))) return false;
    IssueAiOrder(altar, std::make_unique<COrderNoTarget>(dead->GetHandleId()));
    return true;
}

bool MeleeAIController::TryResearchUpgrades() {
    if (!sim_) return false;
    CUpgradeData* upgrades = sim_->GetUpgradeData();
    CPlayer* player = sim_->GetPlayer(playerIndex_);
    if (!upgrades || !player) return false;
    // Economy buffer by difficulty: research never starves the army.
    const int reserve = difficulty_ == AIDifficulty::EASY ? 400
        : difficulty_ == AIDifficulty::INSANE ? 0 : 150;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsConstructing() || unit->GetPlayerIndex() != playerIndex_) continue;
        for (CAbility* ability : unit->GetAbilities()) {
            auto* queue = dynamic_cast<CAbilityQueue*>(ability);
            // Idle production only: research shares the train FIFO, so a
            // busy building waits for a later think instead of queueing
            // behind units.
            if (!queue || queue->GetQueueCount() > 0) continue;
            for (const War3ID& id : queue->GetResearches()) {
                const CUpgradeType* type = upgrades->GetUpgrade(id);
                if (!type) continue;
                const int level = player->GetTechLevel(id);
                if (level >= type->maxLevel) continue;
                const int gold = level >= 0 && level < (int)type->goldPerLevel.size()
                    ? type->goldPerLevel[level] : type->goldCost;
                const int lumber = level >= 0 && level < (int)type->lumberPerLevel.size()
                    ? type->lumberPerLevel[level] : type->lumberCost;
                if (player->GetGold() < gold + reserve || player->GetLumber() < lumber) continue;
                IssueAiOrder(unit, std::make_unique<COrderNoTarget>(static_cast<int>(id.value)));
                return true; // one research per think
            }
        }
    }
    return false;
}

bool MeleeAIController::TryAttackWave(bool force) {
    if (!sim_) return false;
    const int waveSize = difficulty_ == AIDifficulty::EASY ? 3
        : difficulty_ == AIDifficulty::INSANE ? 9 : 6;
    const int army = CountArmy();
    if (army < (force ? 2 : waveSize)) return false;
    if (!force && sim_->GetGameTurnTick() - lastWaveTick_ < GetWaveCooldownTicks()) return false;
    float x = 0.0f, y = 0.0f;
    if (!FindAttackTarget(x, y)) return false;
    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        if (plan_.armyUnit.value != 0 && unit->GetTypeId() != plan_.armyUnit) continue;
        IssueAiOrder(unit, std::make_unique<COrderTargetPoint>(OrderIds::ATTACK, x, y));
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
                IssueAiOrder(worker, std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, tree));
                lumberCrew++;
                continue;
            }
        }
        CUnit* mine = FindNearestGoldMine(worker->GetX(), worker->GetY());
        if (mine) {
            IssueAiOrder(worker, std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, mine));
            continue;
        }
        CDestructable* tree = FindNearestTree(worker->GetX(), worker->GetY());
        if (tree) {
            IssueAiOrder(worker, std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, tree));
            continue;
        }
        break; // nothing to harvest; stay idle
    }
}

TacticalThreatMap::TacticalThreatMap(int gridW, int gridH,
                                     float minX, float minY,
                                     float maxX, float maxY)
    : gridW_(std::max(1, gridW))
    , gridH_(std::max(1, gridH))
    , minX_(minX)
    , minY_(minY)
    , maxX_(maxX)
    , maxY_(maxY)
    , cells_((size_t)gridW_ * gridH_) {}

int TacticalThreatMap::WorldToCol(float x) const {
    const float spanX = (maxX_ > minX_) ? (maxX_ - minX_) : 1.0f;
    return std::clamp((int)((x - minX_) / spanX * gridW_), 0, gridW_ - 1);
}

int TacticalThreatMap::WorldToRow(float y) const {
    const float spanY = (maxY_ > minY_) ? (maxY_ - minY_) : 1.0f;
    return std::clamp((int)((y - minY_) / spanY * gridH_), 0, gridH_ - 1);
}

void TacticalThreatMap::Rebuild(const CSimulation& sim, int aiPlayerIndex) {
    std::fill(cells_.begin(), cells_.end(), TacticalThreatCell{});
    activeThreatCells_ = 0;

    const CPlayer* selfPlayer = sim.GetPlayer(aiPlayerIndex);
    for (const CUnit* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden()) continue;
        const int owner = unit->GetPlayerIndex();
        if (owner < 0) continue;

        bool isAlly = (owner == aiPlayerIndex);
        if (!isAlly && selfPlayer) {
            if (const CPlayer* other = sim.GetPlayer(owner)) {
                isAlly = selfPlayer->HasAlliance(*other, CAllianceType::PASSIVE);
            }
        }

        const int col = WorldToCol(unit->GetX());
        const int row = WorldToRow(unit->GetY());
        TacticalThreatCell& cell = cells_[(size_t)row * gridW_ + col];

        float unitPower = 15.0f;
        if (const CUnitType* ut = unit->GetUnitType()) {
            if (!ut->GetAttacks().empty()) {
                const auto& atk = ut->GetAttacks()[0];
                const float avgDmg = (float)atk.damageBase + 0.5f * (float)(atk.damageDice * (atk.damageSidesPerDie + 1));
                const float cd = std::max(0.25f, atk.cooldown);
                unitPower = (avgDmg / cd) * 2.0f + (float)unit->GetTotalDefense() * 1.5f;
            } else if (ut->IsBuilding()) {
                unitPower = 5.0f;
            }
        }
        if (unit->IsHero()) {
            unitPower *= 1.75f;
        }

        if (isAlly) {
            cell.friendlyPower += unitPower;
            cell.friendlyCount++;
        } else {
            cell.enemyThreat += unitPower;
            cell.enemyCount++;
            const float maxHp = std::max(1.0f, unit->GetMaxLife());
            const float missingHpRatio = std::clamp(1.0f - (unit->GetLife() / maxHp), 0.0f, 1.0f);
            const float valueWeight = (unit->IsHero() ? 50.0f : 20.0f) * (1.0f + missingHpRatio * 1.5f);
            cell.highValueScore = std::max(cell.highValueScore, valueWeight);
        }
    }

    for (const auto& cell : cells_) {
        if (cell.enemyThreat > 0.0f || cell.friendlyPower > 0.0f) {
            ++activeThreatCells_;
        }
    }
}

const TacticalThreatCell& TacticalThreatMap::GetCellAtWorld(float x, float y) const {
    if (cells_.empty()) return emptyCell_;
    const int col = WorldToCol(x);
    const int row = WorldToRow(y);
    return cells_[(size_t)row * gridW_ + col];
}

float TacticalThreatMap::SampleThreatAt(float x, float y) const {
    return GetCellAtWorld(x, y).enemyThreat;
}

float TacticalThreatMap::SampleFriendlyPowerAt(float x, float y) const {
    return GetCellAtWorld(x, y).friendlyPower;
}

float TacticalThreatMap::ScoreTargetUtility(const CUnit* attacker, const CUnit* candidateTarget) const {
    if (!attacker || !candidateTarget || candidateTarget->IsDead() || candidateTarget->IsHidden() ||
        candidateTarget->IsInvulnerable()) {
        return -1.0f;
    }

    const float dx = candidateTarget->GetX() - attacker->GetX();
    const float dy = candidateTarget->GetY() - attacker->GetY();
    const float dist = std::sqrt(dx * dx + dy * dy);

    // 1. Low-health execution utility (up to +120 pts for nearly dead targets)
    const float maxHp = std::max(1.0f, candidateTarget->GetMaxLife());
    const float hpRatio = std::clamp(candidateTarget->GetLife() / maxHp, 0.0f, 1.0f);
    const float executionBonus = (1.0f - hpRatio) * 120.0f;

    // 2. Target tactical role value (Hero > Combat Unit > Worker > Passive Building)
    float roleBonus = 25.0f;
    if (candidateTarget->IsHero()) {
        roleBonus = 85.0f;
    } else if (const CUnitType* ut = candidateTarget->GetUnitType()) {
        if (!ut->GetAttacks().empty()) {
            roleBonus = 50.0f;
        } else if (ut->IsBuilding()) {
            roleBonus = 15.0f;
        }
    }

    // 3. Distance penalty (prefer in-range or nearby targets)
    const float atkRange = std::max(100.0f, attacker->GetAttackRange(0));
    const float distPenalty = (dist <= atkRange) ? (dist * 0.02f) : (20.0f + (dist - atkRange) * 0.08f);

    return std::max(0.01f, roleBonus + executionBonus - distPenalty);
}

bool TacticalThreatMap::ComputeRetreatVector(float unitX, float unitY, float& outDirX, float& outDirY) const {
    if (cells_.empty()) {
        outDirX = 0.0f;
        outDirY = 0.0f;
        return false;
    }
    const int centerCol = WorldToCol(unitX);
    const int centerRow = WorldToRow(unitY);
    const float centerThreat = cells_[(size_t)centerRow * gridW_ + centerCol].enemyThreat;

    float gradX = 0.0f;
    float gradY = 0.0f;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            const int r = std::clamp(centerRow + dr, 0, gridH_ - 1);
            const int c = std::clamp(centerCol + dc, 0, gridW_ - 1);
            const TacticalThreatCell& neighbor = cells_[(size_t)r * gridW_ + c];
            // Push away from higher enemyThreat and toward higher friendlyPower
            const float safetyDelta = (centerThreat - neighbor.enemyThreat) + 0.35f * neighbor.friendlyPower;
            gradX += (float)dc * safetyDelta;
            gradY += (float)dr * safetyDelta;
        }
    }

    const float len = std::sqrt(gradX * gradX + gradY * gradY);
    if (len > 1e-4f) {
        outDirX = gradX / len;
        outDirY = gradY / len;
        return true;
    }
    // Fallback deterministic retreat direction (-X) if threat is uniform
    if (centerThreat > 0.0f) {
        outDirX = -1.0f;
        outDirY = 0.0f;
        return true;
    }
    outDirX = 0.0f;
    outDirY = 0.0f;
    return false;
}

void MeleeAIController::RebuildThreatMap() {
    if (!sim_) return;
    threatMap_.Rebuild(*sim_, playerIndex_);
}

CUnit* MeleeAIController::EvaluateBestFocusTarget(const CUnit* attacker, float searchRadius) const {
    if (!sim_ || !attacker) return nullptr;
    const float r2 = searchRadius * searchRadius;
    CUnit* bestTarget = nullptr;
    float bestScore = -1.0f;

    for (CUnit* cand : sim_->GetUnits()) {
        if (!cand || cand->IsDead() || cand->IsHidden() || !IsEnemyPlayer(cand->GetPlayerIndex()) ||
            cand->IsInvisibleTo(attacker)) continue;
        const float d2 = Dist2(attacker->GetX(), attacker->GetY(), cand->GetX(), cand->GetY());
        if (d2 > r2) continue;
        const float score = threatMap_.ScoreTargetUtility(attacker, cand);
        if (score > bestScore) {
            bestScore = score;
            bestTarget = cand;
        }
    }
    return bestTarget;
}

int MeleeAIController::ExecuteTacticalMicro(float retreatHpRatioThreshold, float retreatStepDist) {
    if (!sim_) return 0;
    RebuildThreatMap();
    int microActions = 0;

    for (CUnit* unit : sim_->GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != playerIndex_) continue;
        const CUnitType* ut = unit->GetUnitType();
        if (!ut || ut->IsBuilding() || IsWorkerType(ut)) continue;

        const float maxHp = std::max(1.0f, unit->GetMaxLife());
        const float hpRatio = unit->GetLife() / maxHp;
        const float localThreat = threatMap_.SampleThreatAt(unit->GetX(), unit->GetY());

        // 1. Wounded unit under active threat -> kite/retreat along safety gradient
        if (hpRatio <= retreatHpRatioThreshold && localThreat > 0.0f) {
            float dirX = 0.0f, dirY = 0.0f;
            if (threatMap_.ComputeRetreatVector(unit->GetX(), unit->GetY(), dirX, dirY)) {
                const float retreatX = unit->GetX() + dirX * retreatStepDist;
                const float retreatY = unit->GetY() + dirY * retreatStepDist;
                IssueAiOrder(unit, std::make_unique<COrderTargetPoint>(OrderIds::MOVE, retreatX, retreatY));
                ++kitingRetreatCount_;
                ++microActions;
                continue;
            }
        }

        // 2. Healthy combat unit -> focus-fire highest-utility enemy target in range
        if (CUnit* focusTarget = EvaluateBestFocusTarget(unit, 900.0f)) {
            IssueAiOrder(unit, std::make_unique<COrderTargetWidget>(OrderIds::ATTACK, focusTarget));
            ++focusFireOrderCount_;
            ++microActions;
        }
    }
    return microActions;
}

void MeleeAIController::Think() {
    thinkCount_++;
    if (!sim_ || !sim_->GetUnitData()) return;
    RebuildThreatMap();
    if (!plan_.valid) DiscoverPlan();
    // Research is building-driven, not plan-driven: a surviving lab keeps
    // working even when the base plan collapsed (or never formed).
    TryResearchUpgrades();
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
                IssueAiOrder(unit, std::make_unique<COrderTargetWidget>(OrderIds::ATTACK, threat));
            }
        }
    }
    TryTrainWorkers();
    // Construction before harvest assignment: AssignIdleWorkers claims every
    // idle worker for harvest, so builders must be dispatched first (a
    // worker with an active BUILD behavior is not idle and keeps building).
    TryBuildSupply();
    TryBuildBarracks();
    TryBuildAltar();
    TryBuildTowers();
    TryExpand();
    TryReviveHeroes();
    TryTrainHero();
    TryTrainArmy();
    AssignIdleWorkers();
    const bool forced = lastCommand_ == 1;
    if (TryAttackWave(forced) && forced) lastCommand_ = -1;
}

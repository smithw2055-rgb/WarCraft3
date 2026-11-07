#include "ability_queue.h"
#include "ability_rally.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/tech_tree.h"
#include "data/unitdata.h"
#include "data/upgradedata.h"
#include <iostream>
#include <algorithm>

namespace {

int GetUpgradeGoldCost(const CUpgradeType& upgrade, int currentLevel) {
    if (currentLevel >= 0 && currentLevel < (int)upgrade.goldPerLevel.size()) {
        return upgrade.goldPerLevel[currentLevel];
    }
    return upgrade.goldCost;
}

int GetUpgradeLumberCost(const CUpgradeType& upgrade, int currentLevel) {
    if (currentLevel >= 0 && currentLevel < (int)upgrade.lumberPerLevel.size()) {
        return upgrade.lumberPerLevel[currentLevel];
    }
    return upgrade.lumberCost;
}

int GetUpgradeResearchTime(const CUpgradeType& upgrade, int currentLevel) {
    if (currentLevel >= 0 && currentLevel < (int)upgrade.timePerLevel.size()) {
        return upgrade.timePerLevel[currentLevel];
    }
    return upgrade.researchTime;
}

}

CAbilityQueue::CAbilityQueue(int handleId)
    : CAbility(handleId, War3ID("Aque"), CAbilityCategory::QUEUE) {
    iconShowing_ = false;
}

void CAbilityQueue::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    for (int i = 0; i < MAX_QUEUE; i++) {
        queueTypes_[i] = CQueueItemType::NONE;
    }
}

void CAbilityQueue::OnRemove(CSimulation& game, CUnit* unit) {
    CancelAll(game, unit);
}

void CAbilityQueue::OnTick(CSimulation& game, CUnit* unit) {
    if (queueTypes_[0] == CQueueItemType::NONE) return;

    float dt = game.GetSimStepTime();
    progress_ += dt;

    if (progress_ >= buildTime_) {
        ProcessCompletion(game, unit);
    }
}

void CAbilityQueue::OnDeath(CSimulation& game, CUnit* unit) {
    CancelAll(game, unit);
}

bool CAbilityQueue::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game;
    (void)unit;
    if (IsDisabled()) return false;

    War3ID rawcode(static_cast<uint32_t>(orderId));
    return unitsTrained_.count(rawcode) > 0
        || researchesAvailable_.count(rawcode) > 0
        || orderId == OrderIds::CANCEL_QUEUE;
}

bool CAbilityQueue::CheckBeforeQueueNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    War3ID rawcode(static_cast<uint32_t>(orderId));

    // Check if it's a unit training order
    if (unitsTrained_.count(rawcode) > 0) {
        return QueueUnit(game, caster, rawcode);
    }

    // Check if it's a research order
    if (researchesAvailable_.count(rawcode) > 0) {
        return QueueResearch(game, caster, rawcode);
    }

    // Cancel order
    if (orderId == OrderIds::CANCEL_QUEUE) {
        CancelQueueItem(game, caster, 0);
        return false;
    }

    return true;
}

CBehavior* CAbilityQueue::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    (void)game; (void)caster; (void)orderId;
    return nullptr; // Queue doesn't create behaviors
}

bool CAbilityQueue::QueueUnit(CSimulation& game, CUnit* building, War3ID unitTypeId) {
    // Find empty slot
    int slot = -1;
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (queueTypes_[i] == CQueueItemType::NONE) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false; // queue full

    // Check if player can afford it
    auto* player = game.GetPlayer(building->GetPlayerIndex());
    auto* unitData = game.GetUnitData();
    if (!player || !unitData) return false;

    auto* trainType = unitData->GetUnitType(unitTypeId);
    if (!trainType) return false;

    // Techtree gate (ref queue checks): flat requirements plus, for
    // heroes, the tier list at the current hero count (tier = heroes
    // owned, 1-based; e.g. Keep for the 2nd hero, Castle for the 3rd).
    if (!TechTree::MeetsRequirements(game, building->GetPlayerIndex(), unitTypeId)) {
        return false;
    }
    // Availability cap (ref isTechtreeAllowedByMax first): w3i bans and
    // SetPlayerTechMaxAllowed caps refuse training like requirements do.
    if (!player->IsTechtreeAllowedByMax(unitTypeId)) {
        return false;
    }
    if (trainType->IsHero()) {
        int heroCount = 0;
        for (CUnit* unit : game.GetUnits()) {
            if (unit && !unit->IsDead() &&
                unit->GetPlayerIndex() == building->GetPlayerIndex() && unit->IsHero()) {
                heroCount++;
            }
        }
        for (const auto& req : trainType->GetRequirementsTier(heroCount)) {
            if (player->GetEffectiveTechLevel(game, req.id) < (req.level > 0 ? req.level : 1)) {
                return false;
            }
        }
    }

    // Ref queueTrainingUnit: hero tokens pay INSTEAD of gold/lumber.
    const bool tokenHero = trainType->IsHero() && player->GetHeroTokens() > 0;
    if (!tokenHero && player->GetGold() < trainType->GetGoldCost()) {
        std::cout << "Not enough gold to train!" << std::endl;
        return false;
    }
    if (!tokenHero && player->GetLumber() < trainType->GetLumberCost()) {
        std::cout << "Not enough lumber to train!" << std::endl;
        return false;
    }
    if (trainType->GetFoodUsed() > 0 &&
        player->GetFoodUsed() + trainType->GetFoodUsed() > player->GetFoodCap()) {
        std::cout << "Not enough food to train!" << std::endl;
        return false;
    }

    // Ref queueTrainingUnit: hero tokens pay INSTEAD of gold/lumber.
    if (tokenHero) {
        player->SetHeroTokens(player->GetHeroTokens() - 1);
    } else {
        // Deduct resources
        player->AddGold(-trainType->GetGoldCost());
        player->AddLumber(-trainType->GetLumberCost());
    }

    // Add to queue
    queue_[slot] = unitTypeId;
    queueTypes_[slot] = CQueueItemType::UNIT;

    // If this is the first item, start progress
    if (slot == 0) {
        progress_ = 0;
        buildTime_ = CalculateBuildTime(game, building, unitTypeId, CQueueItemType::UNIT);
    }

    std::cout << "Queued unit: " << unitTypeId.ToString() << " in slot " << slot << std::endl;
    if (const auto& cb = game.GetQueueEventCallback()) {
        cb(building->GetHandleId(), (int)unitTypeId.value, 0, true);
    }
    return false; // don't queue the order
}

bool CAbilityQueue::QueueResearch(CSimulation& game, CUnit* building, War3ID upgradeId) {
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (queueTypes_[i] == CQueueItemType::RESEARCH && queue_[i] == upgradeId) {
            std::cout << "Research already queued: " << upgradeId.ToString() << std::endl;
            return false;
        }
    }

    // Find empty slot
    int slot = -1;
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (queueTypes_[i] == CQueueItemType::NONE) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;

    // Check if player can afford it
    auto* player = game.GetPlayer(building->GetPlayerIndex());
    auto* upgradeData = game.GetUpgradeData();
    if (!player || !upgradeData) return false;

    const CUpgradeType* upgrade = upgradeData->GetUpgrade(upgradeId);
    if (!upgrade) return false;

    // Availability cap (ref isTechtreeAllowedByMax first): w3i bans and
    // SetPlayerTechMaxAllowed caps refuse research before costs are taken.
    if (!player->IsTechtreeAllowedByMax(upgradeId)) {
        std::cout << "Research blocked by tech cap: " << upgradeId.ToString() << std::endl;
        return false;
    }

    const int currentLevel = player->GetTechLevel(upgradeId);
    if (currentLevel >= upgrade->maxLevel) {
        std::cout << "Research max level reached: " << upgradeId.ToString() << std::endl;
        return false;
    }

    const int goldCost = GetUpgradeGoldCost(*upgrade, currentLevel);
    const int lumberCost = GetUpgradeLumberCost(*upgrade, currentLevel);
    if (player->GetGold() < goldCost) {
        std::cout << "Not enough gold to research!" << std::endl;
        return false;
    }
    if (player->GetLumber() < lumberCost) {
        std::cout << "Not enough lumber to research!" << std::endl;
        return false;
    }

    player->AddGold(-goldCost);
    player->AddLumber(-lumberCost);
    queue_[slot] = upgradeId;
    queueTypes_[slot] = CQueueItemType::RESEARCH;

    if (slot == 0) {
        progress_ = 0;
        buildTime_ = CalculateBuildTime(game, building, upgradeId, CQueueItemType::RESEARCH);
    }

    std::cout << "Queued research: " << upgradeId.ToString() << std::endl;
    if (const auto& cb = game.GetQueueEventCallback()) {
        cb(building->GetHandleId(), (int)upgradeId.value, 1, true);
    }
    return false;
}

bool CAbilityQueue::QueueReviveHero(CSimulation& game, CUnit* building, CUnit* hero) {
    if (!hero || !hero->IsDead()) return false;

    // Find empty slot
    int slot = -1;
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (queueTypes_[i] == CQueueItemType::NONE) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;

    // Calculate revive cost
    auto* player = game.GetPlayer(building->GetPlayerIndex());
    auto* unitData = game.GetUnitData();
    if (!player || !unitData) return false;

    auto* heroType = unitData->GetUnitType(hero->GetTypeId());
    if (!heroType) return false;

    int heroLevel = hero->GetHeroLevel();
    // Ref CAbilityReviveHero cost fns (lumber keys off the GOLD cost).
    const auto& gameplay = game.GetGameplayConstants();
    int reviveGold = gameplay.GetHeroReviveGoldCost(heroType->GetGoldCost(), heroLevel);
    int reviveLumber = gameplay.GetHeroReviveLumberCost(heroType->GetGoldCost(), heroLevel);

    if (player->GetGold() < reviveGold) {
        std::cout << "Not enough gold to revive hero!" << std::endl;
        return false;
    }
    if (player->GetLumber() < reviveLumber) {
        std::cout << "Not enough lumber to revive hero!" << std::endl;
        return false;
    }

    player->AddGold(-reviveGold);
    player->AddLumber(-reviveLumber);

    // Store hero handleId as the queue item
    War3ID heroHandle(static_cast<uint32_t>(hero->GetHandleId()));
    queue_[slot] = heroHandle;
    queueTypes_[slot] = CQueueItemType::HERO_REVIVE;

    if (slot == 0) {
        progress_ = 0;
        buildTime_ = (float)gameplay.GetHeroReviveTime(heroType->GetBuildTime(), heroLevel);
        if (buildTime_ < 5.0f) buildTime_ = 5.0f;
    }

    std::cout << "Queued hero revive: " << heroType->GetName() << std::endl;
    if (const auto& cb = game.GetQueueEventCallback()) {
        cb(building->GetHandleId(), hero->GetHandleId(), 3, true);
    }
    return false;
}

void CAbilityQueue::CancelQueueItem(CSimulation& game, CUnit* building, int slot) {
    if (slot < 0 || slot >= MAX_QUEUE) return;
    if (queueTypes_[slot] == CQueueItemType::NONE) return;
    // Capture for the cancel event before the queue shifts.
    const War3ID removedId = queue_[slot];
    const CQueueItemType removedKind = queueTypes_[slot];

    // Refund resources
    auto* player = game.GetPlayer(building->GetPlayerIndex());
    auto* unitData = game.GetUnitData();
    auto* upgradeData = game.GetUpgradeData();
    if (player && unitData) {
        War3ID typeId = queue_[slot];
        if (queueTypes_[slot] == CQueueItemType::UNIT) {
            auto* trainType = unitData->GetUnitType(typeId);
            if (trainType) {
                player->AddGold(trainType->GetGoldCost());
                player->AddLumber(trainType->GetLumberCost());
            }
        } else if (queueTypes_[slot] == CQueueItemType::RESEARCH && upgradeData) {
            const CUpgradeType* upgrade = upgradeData->GetUpgrade(typeId);
            if (upgrade) {
                const int currentLevel = player->GetTechLevel(typeId);
                player->AddGold(GetUpgradeGoldCost(*upgrade, currentLevel));
                player->AddLumber(GetUpgradeLumberCost(*upgrade, currentLevel));
            }
        }
        // Refund hero revive with the same level-scaled formula used when the
        // revive was queued (see QueueReviveHero). The queue stores the hero
        // handle id, so look the hero up for its current level/type costs.
        if (queueTypes_[slot] == CQueueItemType::HERO_REVIVE) {
            if (CUnit* hero = game.GetUnit((int)typeId.value)) {
                if (auto* heroType = unitData->GetUnitType(hero->GetTypeId())) {
                    const auto& gameplay = game.GetGameplayConstants();
                    const int heroLevel = hero->GetHeroLevel();
                    player->AddGold(gameplay.GetHeroReviveGoldCost(
                        heroType->GetGoldCost(), heroLevel));
                    player->AddLumber(gameplay.GetHeroReviveLumberCost(
                        heroType->GetGoldCost(), heroLevel));
                }
            }
        }
    }

    // Shift queue left
    for (int i = slot; i < MAX_QUEUE - 1; i++) {
        queue_[i] = queue_[i + 1];
        queueTypes_[i] = queueTypes_[i + 1];
    }
    queue_[MAX_QUEUE - 1] = War3ID();
    queueTypes_[MAX_QUEUE - 1] = CQueueItemType::NONE;

    // Reset progress if first item changed
    if (slot == 0) {
        progress_ = 0;
        if (queueTypes_[0] != CQueueItemType::NONE) {
            buildTime_ = CalculateBuildTime(game, building, queue_[0], queueTypes_[0]);
        } else {
            buildTime_ = 0;
        }
    }

    // Train/research/revive cancellations observe the removed entry.
    if (removedKind == CQueueItemType::UNIT || removedKind == CQueueItemType::RESEARCH ||
        removedKind == CQueueItemType::HERO_REVIVE) {
        if (const auto& cb = game.GetQueueEventCallback()) {
            int kind = 0;
            if (removedKind == CQueueItemType::RESEARCH) kind = 1;
            else if (removedKind == CQueueItemType::HERO_REVIVE) kind = 3;
            cb(building->GetHandleId(), (int)removedId.value, kind, false);
        }
    }
}

void CAbilityQueue::CancelAll(CSimulation& game, CUnit* building) {
    for (int i = MAX_QUEUE - 1; i >= 0; i--) {
        if (queueTypes_[i] != CQueueItemType::NONE) {
            CancelQueueItem(game, building, i);
        }
    }
}

War3ID CAbilityQueue::GetQueuedItem(int slot) const {
    if (slot < 0 || slot >= MAX_QUEUE) return War3ID();
    return queue_[slot];
}

CQueueItemType CAbilityQueue::GetQueuedItemType(int slot) const {
    if (slot < 0 || slot >= MAX_QUEUE) return CQueueItemType::NONE;
    return queueTypes_[slot];
}

int CAbilityQueue::GetQueueCount() const {
    int count = 0;
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (queueTypes_[i] != CQueueItemType::NONE) count++;
    }
    return count;
}

void CAbilityQueue::ProcessCompletion(CSimulation& game, CUnit* building) {
    if (queueTypes_[0] == CQueueItemType::NONE) return;

    War3ID completedId = queue_[0];
    CQueueItemType completedType = queueTypes_[0];

    switch (completedType) {
        case CQueueItemType::UNIT: {
            // Spawn the trained unit
            auto* unitData = game.GetUnitData();
            if (unitData) {
                auto* trainType = unitData->GetUnitType(completedId);
                if (trainType) {
                    float spawnX = building->GetX();
                    float spawnY = building->GetY();
                    float offset = building->GetCollisionSize() + 64.0f;
                    spawnX += offset; // spawn to the right

                    auto* newUnit = game.CreateUnit(completedId, building->GetPlayerIndex(),
                                                     spawnX, spawnY, 0);
                    if (newUnit) {
                        auto* rallyAbility = dynamic_cast<CAbilityRally*>(building->GetAbility(War3ID("ARal")));
                        if (rallyAbility) {
                            rallyAbility->ApplyToTrainedUnit(game, building, newUnit);
                        }
                        if (const auto& trainedCb = game.GetUnitTrainedCallback()) {
                            trainedCb(building->GetHandleId(), newUnit->GetHandleId());
                        }
                        std::cout << "Trained unit: " << completedId.ToString() << std::endl;
                    }
                }
            }
            break;
        }
        case CQueueItemType::RESEARCH: {
            // Apply research
            auto* player = game.GetPlayer(building->GetPlayerIndex());
            if (player) {
                player->AddTechUnlocked(game, completedId);
                if (const auto& researchCb = game.GetResearchFinishedCallback()) {
                    researchCb(building->GetHandleId(), (int)completedId.value);
                }
                std::cout << "Research completed: " << completedId.ToString() << std::endl;
            }
            break;
        }
        case CQueueItemType::HERO_REVIVE: {
            // Revive the hero
            int heroHandle = static_cast<int>(completedId.value);
            auto* hero = game.GetUnit(heroHandle);
            if (hero) {
                hero->SetHidden(false);
                hero->SetLife(game, hero->GetMaxLife() * 0.5f);
                hero->SetMana((float)hero->GetMaxMana() * 0.5f);
                hero->SetPosition(building->GetX() + building->GetCollisionSize() + 64.0f,
                                  building->GetY());
                if (const auto& revivedCb = game.GetHeroRevivedCallback()) {
                    revivedCb(building->GetHandleId(), hero->GetHandleId());
                }
                std::cout << "Hero revived!" << std::endl;
            }
            break;
        }
        default:
            break;
    }

    // Shift queue
    ShiftQueue(game, building);
}

void CAbilityQueue::ShiftQueue(CSimulation& game, CUnit* building) {
    for (int i = 0; i < MAX_QUEUE - 1; i++) {
        queue_[i] = queue_[i + 1];
        queueTypes_[i] = queueTypes_[i + 1];
    }
    queue_[MAX_QUEUE - 1] = War3ID();
    queueTypes_[MAX_QUEUE - 1] = CQueueItemType::NONE;

    progress_ = 0;
    if (queueTypes_[0] != CQueueItemType::NONE) {
        buildTime_ = CalculateBuildTime(game, building, queue_[0], queueTypes_[0]);
    } else {
        buildTime_ = 0;
    }
}

float CAbilityQueue::CalculateBuildTime(CSimulation& game, CUnit* building, War3ID typeId, CQueueItemType type) const {
    if (type == CQueueItemType::UNIT) {
        auto* unitData = game.GetUnitData();
        if (unitData) {
            if (auto* unitType = unitData->GetUnitType(typeId)) {
                return std::max(0.0f, (float)unitType->GetBuildTime());
            }
        }
    } else if (type == CQueueItemType::RESEARCH) {
        auto* upgradeData = game.GetUpgradeData();
        if (upgradeData) {
            if (const CUpgradeType* upgrade = upgradeData->GetUpgrade(typeId)) {
                int currentLevel = 0;
                if (building) {
                    if (auto* player = game.GetPlayer(building->GetPlayerIndex())) {
                        currentLevel = player->GetTechLevel(typeId);
                    }
                }
                return std::max(0.0f, (float)GetUpgradeResearchTime(*upgrade, currentLevel));
            }
        }
    } else if (type == CQueueItemType::HERO_REVIVE) {
        int heroHandle = static_cast<int>(typeId.value);
        if (auto* hero = game.GetUnit(heroHandle)) {
            if (auto* unitData = game.GetUnitData()) {
                if (auto* heroType = unitData->GetUnitType(hero->GetTypeId())) {
                    const float reviveTime = (float)game.GetGameplayConstants().GetHeroReviveTime(
                        heroType->GetBuildTime(), hero->GetHeroLevel());
                    return std::max(5.0f, reviveTime);
                }
            }
        }
    }
    return 0.0f;
}

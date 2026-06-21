#include "ability_queue.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "data/unitdata.h"
#include "data/upgradedata.h"
#include <iostream>
#include <algorithm>

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

    if (player->GetGold() < trainType->GetGoldCost()) {
        std::cout << "Not enough gold to train!" << std::endl;
        return false;
    }
    if (player->GetLumber() < trainType->GetLumberCost()) {
        std::cout << "Not enough lumber to train!" << std::endl;
        return false;
    }

    // Deduct resources
    player->AddGold(-trainType->GetGoldCost());
    player->AddLumber(-trainType->GetLumberCost());

    // Add to queue
    queue_[slot] = unitTypeId;
    queueTypes_[slot] = CQueueItemType::UNIT;

    // If this is the first item, start progress
    if (slot == 0) {
        progress_ = 0;
        buildTime_ = CalculateBuildTime(game, unitTypeId, CQueueItemType::UNIT);
    }

    std::cout << "Queued unit: " << unitTypeId.ToString() << " in slot " << slot << std::endl;
    return false; // don't queue the order
}

bool CAbilityQueue::QueueResearch(CSimulation& game, CUnit* building, War3ID upgradeId) {
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
    if (!player) return false;

    // TODO: Get upgrade data and check costs
    // For now, just queue it
    queue_[slot] = upgradeId;
    queueTypes_[slot] = CQueueItemType::RESEARCH;

    if (slot == 0) {
        progress_ = 0;
        buildTime_ = CalculateBuildTime(game, upgradeId, CQueueItemType::RESEARCH);
    }

    std::cout << "Queued research: " << upgradeId.ToString() << std::endl;
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
    int reviveGold = (int)(heroType->GetGoldCost() * (0.5f + 0.1f * heroLevel));
    int reviveLumber = (int)(heroType->GetLumberCost() * (0.5f + 0.1f * heroLevel));

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
        buildTime_ = (float)heroType->GetBuildTime() * heroLevel * 0.5f;
        if (buildTime_ < 5.0f) buildTime_ = 5.0f;
    }

    std::cout << "Queued hero revive: " << heroType->GetName() << std::endl;
    return false;
}

void CAbilityQueue::CancelQueueItem(CSimulation& game, CUnit* building, int slot) {
    if (slot < 0 || slot >= MAX_QUEUE) return;
    if (queueTypes_[slot] == CQueueItemType::NONE) return;

    // Refund resources
    auto* player = game.GetPlayer(building->GetPlayerIndex());
    auto* unitData = game.GetUnitData();
    if (player && unitData) {
        War3ID typeId = queue_[slot];
        if (queueTypes_[slot] == CQueueItemType::UNIT) {
            auto* trainType = unitData->GetUnitType(typeId);
            if (trainType) {
                player->AddGold(trainType->GetGoldCost());
                player->AddLumber(trainType->GetLumberCost());
            }
        }
        // TODO: refund research and hero revive
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
            buildTime_ = CalculateBuildTime(game, queue_[0], queueTypes_[0]);
        } else {
            buildTime_ = 0;
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
                        // Set rally point
                        auto* rallyAbility = building->GetAbility(War3ID("ARal"));
                        if (rallyAbility) {
                            // TODO: move new unit to rally point
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
                std::cout << "Hero revived!" << std::endl;
            }
            break;
        }
        default:
            break;
    }

    // Shift queue
    ShiftQueue();
}

void CAbilityQueue::ShiftQueue() {
    for (int i = 0; i < MAX_QUEUE - 1; i++) {
        queue_[i] = queue_[i + 1];
        queueTypes_[i] = queueTypes_[i + 1];
    }
    queue_[MAX_QUEUE - 1] = War3ID();
    queueTypes_[MAX_QUEUE - 1] = CQueueItemType::NONE;

    progress_ = 0;
    if (queueTypes_[0] != CQueueItemType::NONE) {
        // TODO: recalculate build time for new front item
        buildTime_ = 10.0f; // placeholder
    } else {
        buildTime_ = 0;
    }
}

float CAbilityQueue::CalculateBuildTime(CSimulation& game, War3ID typeId, CQueueItemType type) const {
    (void)game;
    (void)typeId;
    (void)type;
    // TODO: look up actual build time from data
    return 10.0f; // placeholder
}

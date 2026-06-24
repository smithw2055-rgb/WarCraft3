#include "ability_gold_mine.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include <iostream>
#include <algorithm>

CAbilityGoldMine::CAbilityGoldMine(int handleId)
    : CAbility(handleId, War3ID("Agld"), CAbilityCategory::GOLD_MINE) {
    iconShowing_ = false;
}

void CAbilityGoldMine::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbilityGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    for (auto it = activeMiners_.begin(); it != activeMiners_.end();) {
        CBehaviorHarvest* miner = *it;
        if (!miner) {
            it = activeMiners_.erase(it);
            continue;
        }
        if (game.GetGameTurnTick() >= miner->GetPopoutFromMineTurnTick()) {
            int goldMined = 0;
            if (gold_ > 0) {
                goldMined = std::min(gold_, miner->GetGoldCapacity());
                gold_ -= goldMined;
            }
            miner->PopoutFromMine(goldMined);
            it = activeMiners_.erase(it);
        }
        else {
            ++it;
        }
    }

    if (unit && gold_ <= 0 && unit->GetLife() > 0.0f) {
        unit->SetLife(game, 0.0f);
    }
}

void CAbilityGoldMine::AddMiner(CBehaviorHarvest* miner) {
    if (!miner) {
        return;
    }
    if (std::find(activeMiners_.begin(), activeMiners_.end(), miner) == activeMiners_.end()) {
        activeMiners_.push_back(miner);
    }
}

void CAbilityGoldMine::RemoveMiner(CBehaviorHarvest* miner) {
    activeMiners_.erase(std::remove(activeMiners_.begin(), activeMiners_.end(), miner), activeMiners_.end());
}

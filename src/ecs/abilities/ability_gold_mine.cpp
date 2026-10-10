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
    if (GetActiveMinerCount() >= miningCapacity_) {
        return; // full: entry is refused at CanAdd() sites; enforce here too
    }
    if (std::find(activeMiners_.begin(), activeMiners_.end(), miner) == activeMiners_.end()) {
        activeMiners_.push_back(miner);
    }
}

void CAbilityGoldMine::RemoveMiner(CBehaviorHarvest* miner) {
    activeMiners_.erase(std::remove(activeMiners_.begin(), activeMiners_.end(), miner), activeMiners_.end());
}

CAbilityOverlayedGoldMine::CAbilityOverlayedGoldMine(int handleId)
    : CAbility(handleId, War3ID("Agl2"), CAbilityCategory::GOLD_MINE) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityOverlayedGoldMine::SetParentMine(CUnit* parentUnit, CAbilityGoldMine* parent) {
    parentUnit_ = parentUnit;
    parent_ = parent;
}

int CAbilityOverlayedGoldMine::GetGold() const {
    return parent_ ? parent_->GetGold() : 0;
}

void CAbilityOverlayedGoldMine::SetGold(int v) {
    if (parent_) parent_->SetGold(v);
}

void CAbilityOverlayedGoldMine::AddMiner(CBehaviorHarvest* miner) {
    if (!miner) return;
    if (GetActiveMinerCount() >= miningCapacity_) return; // full (retail 5)
    if (std::find(activeMiners_.begin(), activeMiners_.end(), miner) == activeMiners_.end()) {
        activeMiners_.push_back(miner);
    }
}

void CAbilityOverlayedGoldMine::RemoveMiner(CBehaviorHarvest* miner) {
    activeMiners_.erase(std::remove(activeMiners_.begin(), activeMiners_.end(), miner),
                        activeMiners_.end());
}

void CAbilityOverlayedGoldMine::RestoreParent() {
    if (!parentUnit_) return;
    parentUnit_->SetHidden(false);
    parentUnit_->SetPaused(false);
}

void CAbilityOverlayedGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || unit->IsDead()) return;
    for (auto it = activeMiners_.begin(); it != activeMiners_.end();) {
        CBehaviorHarvest* miner = *it;
        if (!miner) {
            it = activeMiners_.erase(it);
            continue;
        }
        if (game.GetGameTurnTick() >= miner->GetPopoutFromMineTurnTick()) {
            int goldMined = 0;
            if (GetGold() > 0) {
                goldMined = std::min(GetGold(), miner->GetGoldCapacity());
                SetGold(GetGold() - goldMined);
            }
            miner->PopoutFromMine(goldMined);
            it = activeMiners_.erase(it);
        } else {
            ++it;
        }
    }
    if (GetGold() <= 0) unit->SetLife(game, 0);
}

void CAbilityOverlayedGoldMine::OnDeath(CSimulation& game, CUnit* unit) {
    (void)game;
    (void)unit;
    RestoreParent();
}

CMineAccess MineAccessOf(CUnit* unit) {
    CMineAccess access;
    if (!unit) return access;
    access.overlay = dynamic_cast<CAbilityOverlayedGoldMine*>(unit->GetAbility(War3ID("Agl2")));
    if (!access.overlay) {
        access.base = dynamic_cast<CAbilityGoldMine*>(unit->GetAbility(War3ID("Agld")));
    }
    return access;
}

int CMineAccess::Gold() const {
    if (overlay) return overlay->GetGold();
    if (base) return base->GetGold();
    return 0;
}

bool CMineAccess::CanAdd() const {
    if (overlay) return overlay->CanAddMiner();
    if (base) return base->CanAddMiner();
    return false;
}

void CMineAccess::Add(CBehaviorHarvest* miner) {
    if (overlay) overlay->AddMiner(miner);
    else if (base) base->AddMiner(miner);
}

void CMineAccess::Remove(CBehaviorHarvest* miner) {
    if (overlay) overlay->RemoveMiner(miner);
    else if (base) base->RemoveMiner(miner);
}

float CMineAccess::Duration() const {
    if (overlay) return overlay->GetMiningDuration();
    if (base) return base->GetMiningDuration();
    return 0.0f;
}

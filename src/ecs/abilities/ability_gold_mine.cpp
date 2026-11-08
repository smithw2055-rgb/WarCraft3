#include "ability_gold_mine.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include <algorithm>

namespace {

void TickActiveMiners(CSimulation& game,
                      std::vector<CBehaviorHarvest*>& activeMiners,
                      CAbility& mineAbility) {
    for (auto it = activeMiners.begin(); it != activeMiners.end();) {
        CBehaviorHarvest* miner = *it;
        if (!miner) {
            it = activeMiners.erase(it);
            continue;
        }
        if (game.GetGameTurnTick() >= miner->GetPopoutFromMineTurnTick()) {
            int goldMined = 0;
            const int available = mineAbility.GetGold();
            if (available > 0) {
                goldMined = std::min(available, miner->GetGoldCapacity());
                mineAbility.SetGold(available - goldMined);
            }
            miner->PopoutFromMine(goldMined);
            it = activeMiners.erase(it);
        } else {
            ++it;
        }
    }
}

void PushUniqueMiner(std::vector<CBehaviorHarvest*>& activeMiners,
                     int capacity,
                     CBehaviorHarvest* miner) {
    if (!miner || static_cast<int>(activeMiners.size()) >= capacity) return;
    if (std::find(activeMiners.begin(), activeMiners.end(), miner) == activeMiners.end()) {
        activeMiners.push_back(miner);
    }
}

} // namespace

CAbilityGoldMine::CAbilityGoldMine(int handleId)
    : CAbility(handleId, War3ID("Agld"), CAbilityCategory::GOLD_MINE) {
    iconShowing_ = false;
}

void CAbilityGoldMine::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
}

void CAbilityGoldMine::OnTick(CSimulation& game, CUnit* unit) {
    TickActiveMiners(game, activeMiners_, *this);
    if (unit && gold_ <= 0 && unit->GetLife() > 0.0f) {
        unit->SetLife(game, 0.0f);
    }
}

void CAbilityGoldMine::AddMiner(CBehaviorHarvest* miner) {
    PushUniqueMiner(activeMiners_, miningCapacity_, miner);
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
    if (parent_) return parent_->GetGold();
    if (parentUnit_) {
        if (CAbility* gold = parentUnit_->GetAbility(War3ID("Agld"))) return gold->GetGold();
    }
    return 0;
}

void CAbilityOverlayedGoldMine::SetGold(int v) {
    if (parent_) {
        parent_->SetGold(v);
        return;
    }
    if (parentUnit_) {
        if (CAbility* gold = parentUnit_->GetAbility(War3ID("Agld"))) gold->SetGold(v);
    }
}

void CAbilityOverlayedGoldMine::AddMiner(CBehaviorHarvest* miner) {
    PushUniqueMiner(activeMiners_, miningCapacity_, miner);
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
    TickActiveMiners(game, activeMiners_, *this);
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
    access.mine = unit->GetAbility(War3ID("Agl2"));
    if (!access.mine) {
        access.mine = unit->GetAbility(War3ID("Agld"));
    }
    return access;
}

int CMineAccess::Gold() const {
    return mine ? mine->GetGold() : 0;
}

bool CMineAccess::CanAdd() const {
    return mine ? mine->CanAddMiner() : false;
}

void CMineAccess::Add(CBehaviorHarvest* miner) {
    if (mine) mine->AddMiner(miner);
}

void CMineAccess::Remove(CBehaviorHarvest* miner) {
    if (mine) mine->RemoveMiner(miner);
}

float CMineAccess::Duration() const {
    return mine ? mine->GetMiningDuration() : 0.0f;
}

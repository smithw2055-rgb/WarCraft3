#include "ability_upgrade.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "data/unitdata.h"
#include <iostream>

CAbilityUpgrade::CAbilityUpgrade(int handleId)
    : CAbility(handleId, War3ID("Aupg"), CAbilityCategory::UPGRADE) {
    iconShowing_ = true;
}

bool CAbilityUpgrade::CheckCanTargetNoTarget(CSimulation& game, CUnit* unit, int orderId) const {
    (void)game; (void)unit;
    if (IsDisabled()) return false;

    War3ID rawcode(static_cast<uint32_t>(orderId));
    return upgradesTo_.count(rawcode) > 0;
}

CBehavior* CAbilityUpgrade::BeginNoTarget(CSimulation& game, CUnit* caster, int orderId) {
    War3ID upgradeId(static_cast<uint32_t>(orderId));

    if (upgradesTo_.count(upgradeId) == 0) {
        return nullptr;
    }

    // Check if player can afford it
    auto* player = game.GetPlayer(caster->GetPlayerIndex());
    auto* unitData = game.GetUnitData();
    if (!player || !unitData) return nullptr;

    auto* upgradeType = unitData->GetUnitType(upgradeId);
    if (!upgradeType) return nullptr;

    if (player->GetGold() < upgradeType->GetGoldCost()) {
        std::cout << "Not enough gold to upgrade!" << std::endl;
        return nullptr;
    }
    if (player->GetLumber() < upgradeType->GetLumberCost()) {
        std::cout << "Not enough lumber to upgrade!" << std::endl;
        return nullptr;
    }

    // Deduct resources and build the new form over its build time.
    player->AddGold(-upgradeType->GetGoldCost());
    player->AddLumber(-upgradeType->GetLumberCost());
    pendingId_ = upgradeId;
    progress_ = 0.0f;
    buildTime_ = static_cast<float>(upgradeType->GetBuildTime());
    if (buildTime_ <= 0.0f) buildTime_ = 1.0f;
    upgrading_ = true;
    caster->SetConstructing(true);
    caster->SetConstructionProgress(0.0f);
    if (const auto& cb = game.GetQueueEventCallback()) {
        cb(caster->GetHandleId(), (int)upgradeId.value, 2, true);
    }
    return nullptr;
}

void CAbilityUpgrade::OnTick(CSimulation& game, CUnit* unit) {
    if (!upgrading_ || !unit || unit->IsDead()) return;
    progress_ += game.GetSimStepTime();
    if (progress_ > buildTime_) progress_ = buildTime_;
    unit->SetConstructionProgress(progress_);
    if (progress_ < buildTime_) return;
    upgrading_ = false;
    unit->SetConstructing(false);
    CUnitData* data = game.GetUnitData();
    CUnitType* form = data ? data->GetUnitType(pendingId_) : nullptr;
    if (form) unit->MorphTo(form, pendingId_);
    if (const auto& cb = game.GetQueueEventCallback()) {
        cb(unit->GetHandleId(), (int)pendingId_.value, 2, false);
    }
}

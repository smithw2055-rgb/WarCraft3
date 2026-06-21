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

    // Deduct resources
    player->AddGold(-upgradeType->GetGoldCost());
    player->AddLumber(-upgradeType->GetLumberCost());

    // Start upgrade (change unit type in-place)
    // TODO: implement proper upgrade progress tracking
    // For now, instant upgrade
    std::cout << "Upgrading to: " << upgradeId.ToString() << std::endl;

    return nullptr;
}

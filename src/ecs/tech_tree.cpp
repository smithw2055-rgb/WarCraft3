#include "tech_tree.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "data/unitdata.h"
#include <iostream>

bool TechTree::MeetsRequirements(CSimulation& game, int playerIndex, War3ID unitTypeId) {
    auto* unitData = game.GetUnitData();
    if (!unitData) return true; // no data, allow

    auto* unitType = unitData->GetUnitType(unitTypeId);
    if (!unitType) return true;

    auto* player = game.GetPlayer(playerIndex);
    if (!player) return false;

    // Check requirements
    auto& requirements = unitType->GetRequirements();
    for (auto& req : requirements) {
        if (!HasTech(player, req.id)) {
            return false;
        }
    }

    return true;
}

bool TechTree::HasTech(CPlayer* player, War3ID techId) {
    if (!player) return false;

    // Check if player has unlocked this tech
    return player->IsTechUnlocked(techId);
}

std::vector<War3ID> TechTree::GetMissingRequirements(CSimulation& game, int playerIndex, War3ID unitTypeId) {
    std::vector<War3ID> missing;

    auto* unitData = game.GetUnitData();
    if (!unitData) return missing;

    auto* unitType = unitData->GetUnitType(unitTypeId);
    if (!unitType) return missing;

    auto* player = game.GetPlayer(playerIndex);
    if (!player) return missing;

    auto& requirements = unitType->GetRequirements();
    for (auto& req : requirements) {
        if (!HasTech(player, req.id)) {
            missing.push_back(req.id);
        }
    }

    return missing;
}

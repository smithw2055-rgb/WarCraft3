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

    // Check requirements (level-aware: possession counters and research
    // levels share the map, ref techtreeUnlocked).
    auto& requirements = unitType->GetRequirements();
    for (auto& req : requirements) {
        const int have = player->GetTechLevel(req.id);
        if (have < (req.level > 0 ? req.level : 1)) {
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
        const int have = player->GetTechLevel(req.id);
        if (have < (req.level > 0 ? req.level : 1)) {
            missing.push_back(req.id);
        }
    }

    return missing;
}

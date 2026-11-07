#include "tech_tree.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "data/unitdata.h"

bool TechTree::MeetsRequirements(const CSimulation& game, int playerIndex, War3ID unitTypeId) {
    if (const auto* mod = game.GetSimulationModule()) {
        return mod->MeetsTechRequirements(game, playerIndex, unitTypeId.value);
    }
    auto* unitData = game.GetUnitData();
    if (!unitData) return true;
    auto* unitType = unitData->GetUnitType(unitTypeId);
    if (!unitType) return true;
    auto* player = game.GetPlayer(playerIndex);
    if (!player) return false;
    for (const auto& req : unitType->GetRequirements()) {
        const int have = player->GetTechLevel(req.id);
        if (have < (req.level > 0 ? req.level : 1)) {
            return false;
        }
    }
    return true;
}

bool TechTree::HasTech(const CPlayer* player, War3ID techId) {
    if (!player) return false;
    return player->IsTechUnlocked(techId);
}

bool TechTree::HasTech(const CSimulation& game, int playerIndex, War3ID techId) {
    if (const auto* mod = game.GetSimulationModule()) {
        return mod->GetEffectiveTechLevel(game, playerIndex, techId.value) > 0;
    }
    const CPlayer* player = game.GetPlayer(playerIndex);
    return player && player->IsTechUnlocked(techId);
}

std::vector<War3ID> TechTree::GetMissingRequirements(const CSimulation& game, int playerIndex, War3ID unitTypeId) {
    std::vector<War3ID> missing;
    if (const auto* mod = game.GetSimulationModule()) {
        const auto rawMissing = mod->GetMissingTechRequirements(game, playerIndex, unitTypeId.value);
        missing.reserve(rawMissing.size());
        for (uint32_t raw : rawMissing) {
            missing.emplace_back(raw);
        }
        return missing;
    }
    auto* unitData = game.GetUnitData();
    if (!unitData) return missing;
    auto* unitType = unitData->GetUnitType(unitTypeId);
    if (!unitType) return missing;
    auto* player = game.GetPlayer(playerIndex);
    if (!player) return missing;
    for (const auto& req : unitType->GetRequirements()) {
        const int have = player->GetTechLevel(req.id);
        if (have < (req.level > 0 ? req.level : 1)) {
            missing.push_back(req.id);
        }
    }
    return missing;
}

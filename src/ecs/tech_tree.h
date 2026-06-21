#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "resource/slk.h"

class CSimulation;
class CPlayer;

// TechTree - validates building/training prerequisites
class TechTree {
public:
    // Check if a player meets the requirements to build/train a unit type
    static bool MeetsRequirements(CSimulation& game, int playerIndex, War3ID unitTypeId);

    // Check if a player has unlocked a specific tech
    static bool HasTech(CPlayer* player, War3ID techId);

    // Get missing requirements description
    static std::vector<War3ID> GetMissingRequirements(CSimulation& game, int playerIndex, War3ID unitTypeId);
};

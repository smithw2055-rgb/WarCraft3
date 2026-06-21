#pragma once
#include <cstdint>

// Item stat bonus types
enum class CItemBonusType : uint8_t {
    NONE,
    STRENGTH, AGILITY, INTELLIGENCE,
    DAMAGE, ARMOR, ATTACK_SPEED,
    MAX_LIFE, MAX_MANA, LIFE_REGEN, MANA_REGEN,
    MOVE_SPEED, SIGHT_RADIUS
};

// A single item stat bonus
struct CItemBonus {
    CItemBonusType type = CItemBonusType::NONE;
    int value = 0;
};

// Apply a list of item bonuses to a unit
#include <vector>

class CUnit;
class CSimulation;

void ApplyItemBonuses(CSimulation& game, CUnit* unit, const std::vector<CItemBonus>& bonuses);
void RemoveItemBonuses(CSimulation& game, CUnit* unit, const std::vector<CItemBonus>& bonuses);

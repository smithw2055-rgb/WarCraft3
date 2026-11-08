#pragma once
#include "data/unitdata.h"
#include "ecs/ability.h"
#include "ecs/unit.h"
#include "ecs/behaviors/behavior.h"
#include <string>
#include <vector>

namespace unit_text_utils {

std::string DescribeUnitType(War3ID typeId, const CUnitData& unitData);

inline const char* AbilityCategoryName(CAbilityCategory category) {
    switch (category) {
        case CAbilityCategory::CORE: return "CORE";
        case CAbilityCategory::ATTACK: return "ATTACK";
        case CAbilityCategory::MOVEMENT: return "MOVEMENT";
        case CAbilityCategory::BUFF: return "BUFF";
        case CAbilityCategory::ITEM: return "ITEM";
        case CAbilityCategory::HERO: return "HERO";
        case CAbilityCategory::BUILD: return "BUILD";
        case CAbilityCategory::INVENTORY: return "INVENTORY";
        case CAbilityCategory::HARVEST: return "HARVEST";
        case CAbilityCategory::CARGO_HOLD: return "CARGO_HOLD";
        case CAbilityCategory::UPGRADE: return "UPGRADE";
        case CAbilityCategory::QUEUE: return "QUEUE";
        case CAbilityCategory::RALLY: return "RALLY";
        case CAbilityCategory::NEUTRAL_BUILDING: return "NEUTRAL_BUILDING";
        case CAbilityCategory::GOLD_MINE: return "GOLD_MINE";
        case CAbilityCategory::SELL_ITEMS: return "SELL_ITEMS";
        case CAbilityCategory::ROOT: return "ROOT";
        case CAbilityCategory::MOON_WELL: return "MOON_WELL";
        case CAbilityCategory::BLIGHT: return "BLIGHT";
        case CAbilityCategory::SPELL: return "SPELL";
        default: return "UNKNOWN";
    }
}

inline const char* BehaviorCategoryName(CBehaviorCategory category) {
    switch (category) {
        case CBehaviorCategory::NONE: return "NONE";
        case CBehaviorCategory::STOP: return "STOP";
        case CBehaviorCategory::MOVE: return "MOVE";
        case CBehaviorCategory::ATTACK: return "ATTACK";
        case CBehaviorCategory::SPELL: return "SPELL";
        case CBehaviorCategory::ATTACK_MOVE: return "ATTACK_MOVE";
        case CBehaviorCategory::PATROL: return "PATROL";
        case CBehaviorCategory::FOLLOW: return "FOLLOW";
        case CBehaviorCategory::HOLD_POSITION: return "HOLD_POSITION";
        case CBehaviorCategory::BUILD: return "BUILD";
        case CBehaviorCategory::HARVEST: return "HARVEST";
        case CBehaviorCategory::RETURN_RESOURCES: return "RETURN_RESOURCES";
        case CBehaviorCategory::BOARD_TRANSPORT: return "BOARD_TRANSPORT";
        case CBehaviorCategory::STUN: return "STUN";
        case CBehaviorCategory::INVENTORY: return "INVENTORY";
        case CBehaviorCategory::ROOT: return "ROOT";
        default: return "UNKNOWN";
    }
}

std::string JoinStrings(const std::vector<std::string>& values, const char* delimiter);

std::string FormatSelectedUnitsBrief(const std::vector<CUnit*>& units);

inline CResourceType GetUnitCarriedResourceType(const CUnit* unit) {
    if (!unit) return CResourceType::NONE;
    for (const CAbility* ab : unit->GetAbilities()) {
        if (ab && ab->IsCarryingResources()) {
            return ab->GetCarriedResourceType();
        }
    }
    return CResourceType::NONE;
}

inline CBehaviorCategory GetUnitCurrentBehaviorCategory(const CUnit* unit) {
    if (!unit) {
        return CBehaviorCategory::NONE;
    }
    auto* behavior = unit->GetCurrentBehavior();
    return behavior ? behavior->GetCategory() : CBehaviorCategory::NONE;
}

inline bool IsMovementBehaviorCategory(CBehaviorCategory category) {
    switch (category) {
        case CBehaviorCategory::MOVE:
        case CBehaviorCategory::FOLLOW:
        case CBehaviorCategory::PATROL:
        case CBehaviorCategory::ATTACK_MOVE:
            return true;
        default:
            return false;
    }
}

} // namespace unit_text_utils
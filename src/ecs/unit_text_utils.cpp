#include "unit_text_utils.h"
#include <sstream>

namespace unit_text_utils {

std::string DescribeUnitType(War3ID typeId, const CUnitData& unitData) {
    auto* unitType = unitData.GetUnitType(typeId);
    if (!unitType) {
        return typeId.ToString();
    }
    return unitType->GetName() + " [" + typeId.ToString() + "]";
}

const char* BehaviorCategoryName(CBehaviorCategory category) {
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

std::string JoinStrings(const std::vector<std::string>& values, const char* delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            oss << delimiter;
        }
        oss << values[i];
    }
    return oss.str();
}

std::string FormatSelectedUnitsBrief(const std::vector<CUnit*>& units) {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto* unit : units) {
        if (!unit) {
            continue;
        }
        if (!first) {
            oss << ";";
        }
        first = false;
        oss << unit->GetHandleId() << ":" << unit->GetTypeId().ToString()
            << "@(" << unit->GetX() << "," << unit->GetY() << ")";
    }
    oss << "]";
    return oss.str();
}

CResourceType GetUnitCarriedResourceType(const CUnit* unit) {
    if (!unit) return CResourceType::NONE;
    auto* harvestAbility = dynamic_cast<CAbilityHarvest*>(unit->GetAbility(War3ID("Ahar")));
    if (!harvestAbility || !harvestAbility->IsCarryingResources()) {
        return CResourceType::NONE;
    }
    return harvestAbility->GetCarriedResourceType();
}

CBehaviorCategory GetUnitCurrentBehaviorCategory(const CUnit* unit) {
    if (!unit) {
        return CBehaviorCategory::NONE;
    }
    auto* behavior = unit->GetCurrentBehavior();
    return behavior ? behavior->GetCategory() : CBehaviorCategory::NONE;
}

bool IsMovementBehaviorCategory(CBehaviorCategory category) {
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
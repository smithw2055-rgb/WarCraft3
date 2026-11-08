#include "unit_text_utils.h"
#include "ecs/abilities/ability_harvest.h"
#include <sstream>

namespace unit_text_utils {

std::string DescribeUnitType(War3ID typeId, const CUnitData& unitData) {
    auto* unitType = unitData.GetUnitType(typeId);
    if (!unitType) {
        return typeId.ToString();
    }
    return unitType->GetName() + " [" + typeId.ToString() + "]";
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
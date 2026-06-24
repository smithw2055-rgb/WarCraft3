#pragma once
#include "data/unitdata.h"
#include "ecs/unit.h"
#include "ecs/behaviors/behavior.h"
#include "ecs/abilities/ability_harvest.h"
#include <string>
#include <vector>

namespace unit_text_utils {

std::string DescribeUnitType(War3ID typeId, const CUnitData& unitData);

const char* BehaviorCategoryName(CBehaviorCategory category);

std::string JoinStrings(const std::vector<std::string>& values, const char* delimiter);

std::string FormatSelectedUnitsBrief(const std::vector<CUnit*>& units);

CResourceType GetUnitCarriedResourceType(const CUnit* unit);

CBehaviorCategory GetUnitCurrentBehaviorCategory(const CUnit* unit);

bool IsMovementBehaviorCategory(CBehaviorCategory category);

} // namespace unit_text_utils
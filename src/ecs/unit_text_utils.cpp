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

} // namespace unit_text_utils
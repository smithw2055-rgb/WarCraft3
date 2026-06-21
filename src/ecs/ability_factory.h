#pragma once
#include <memory>
#include <vector>
#include "resource/slk.h"

class CAbility;
class CSimulation;
class CUnit;

// AbilityFactory - creates ability instances from rawcodes
// Maps WC3 ability rawcodes to C++ ability classes
class AbilityFactory {
public:
    // Create an ability from its rawcode
    static std::unique_ptr<CAbility> CreateAbility(int handleId, War3ID abilityCode);

    // Create all default abilities for a unit type
    static void AddDefaultAbilities(CSimulation& game, CUnit* unit,
                                     const std::vector<War3ID>& abilityList);

    // Create abilities from unit type's trained/researched lists
    static void AddQueueAbilities(CSimulation& game, CUnit* unit,
                                   const std::vector<War3ID>& unitsTrained,
                                   const std::vector<War3ID>& researchesAvailable);
};

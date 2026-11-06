#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>
#include "resource/slk.h"
#include "ecs/abilities/ability_builder.h"

class CAbility;
class CSimulation;
class CUnit;

// ============================================================
// AbilityFactory - O(1) Table-Driven & Data-Driven Ability Registry
//
// Replaces legacy O(N) if-else chains with a hash-indexed creator
// registry plus seamless integration with AbilityBuilderRegistry
// (ABSpellDescriptor & ABEffectChain) so new/modded abilities can
// be registered declaratively at runtime without C++ subclasses.
// ============================================================
class AbilityFactory {
public:
    using CreatorFn = std::function<std::unique_ptr<CAbility>(int handleId, War3ID code)>;

    // Create an ability from its rawcode (O(1) table lookup -> DataDriven descriptor -> Generic fallback)
    static std::unique_ptr<CAbility> CreateAbility(int handleId, War3ID abilityCode);

    // Dynamic registry API (Phase 2 modernization)
    static void RegisterCreator(War3ID code, CreatorFn creator);
    static void RegisterAlias(War3ID alias, War3ID targetCode);
    static void RegisterSpellDescriptor(const ABSpellDescriptor& desc);
    static bool HasRegisteredCreator(War3ID code);
    static size_t GetRegisteredCount();

    // Create all default abilities for a unit type
    static void AddDefaultAbilities(CSimulation& game, CUnit* unit,
                                    const std::vector<War3ID>& abilityList);

    // Create abilities from unit type's trained/researched lists
    static void AddQueueAbilities(CSimulation& game, CUnit* unit,
                                  const std::vector<War3ID>& unitsTrained,
                                  const std::vector<War3ID>& researchesAvailable);
};

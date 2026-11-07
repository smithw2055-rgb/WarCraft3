#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "resource/slk.h"
#include "ecs/abilities/ability_builder.h"

class CAbility;
class CSimulation;
class CUnit;

// ============================================================
// AbilityFactory - Module-Scoped O(1) Ability & Spell Registry
//
// Phase 2 & Phase 13 Modernization:
// Decouples the generic O(1) ability creation table from concrete
// game modules (such as War3GameModule or dynamic plugin modules).
// Each module can register and unload its ability creators by
// `moduleId` at runtime without recompiling the engine core.
// ============================================================
class AbilityFactory {
public:
    using CreatorFn = std::function<std::unique_ptr<CAbility>(int handleId, War3ID code)>;

    // Create an ability from its rawcode (O(1) table lookup -> DataDriven descriptor -> Generic fallback)
    static std::unique_ptr<CAbility> CreateAbility(int handleId, War3ID abilityCode);

    // Global & Module-Scoped Dynamic Registry API (Phase 2 & Phase 13 modernization)
    static void RegisterCreator(War3ID code, CreatorFn creator);
    static void RegisterModuleCreator(const std::string& moduleId, War3ID code, CreatorFn creator);
    static void RegisterAlias(War3ID alias, War3ID targetCode);
    static void RegisterModuleAlias(const std::string& moduleId, War3ID alias, War3ID targetCode);
    static void RegisterSpellDescriptor(const ABSpellDescriptor& desc);
    static void RegisterModuleSpellDescriptor(const std::string& moduleId, const ABSpellDescriptor& desc);

    // Unload / hot-swap support for dynamic game modules
    static bool UnregisterCreator(War3ID code);
    static size_t UnregisterModuleCreators(const std::string& moduleId);
    static size_t GetModuleRegisteredCount(const std::string& moduleId);
    static std::string GetCreatorModuleId(War3ID code);

    static bool HasRegisteredCreator(War3ID code);
    static size_t GetRegisteredCount();

    // Control whether the default War3GameModule catalog auto-bootstraps on first lookup
    static void SetAutoBootstrapDefaultCatalog(bool enabled);
    static bool IsAutoBootstrapDefaultCatalogEnabled();
    static void EnsureDefaultCatalogRegistered();

    // Attach abilities from a list onto a unit
    static void AddDefaultAbilities(CSimulation& game, CUnit* unit,
                                    const std::vector<War3ID>& abilityList);

    // Populate trained units and researches on a unit's CAbilityQueue (if present)
    static void AddQueueAbilities(CSimulation& game, CUnit* unit,
                                  const std::vector<War3ID>& unitsTrained,
                                  const std::vector<War3ID>& researchesAvailable);
};

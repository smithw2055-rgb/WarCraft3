#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "ecs/abilities/ability_builder.h"
#include "resource/slk.h"

class CAbility;
class CSimulation;
class CUnit;

// ============================================================
// AbilityFactory - Dynamic O(1) Ability & Spell Registry
//
// Phase 13 & Phase 16 Modernization:
// - Decoupled from WarCraft 3's 29 spell files and queue/alias headers.
// - Supports module-scoped registration (`RegisterModuleCreator`,
//   `RegisterModuleAlias`, `RegisterModuleSpellDescriptor`,
//   `UnregisterModuleCreators`) so game modules (such as `War3GameModule`
//   or custom expansion `.so`/`.dll` plugins) can dynamically mount and
//   unmount their ability catalogs at runtime.
// ============================================================
class AbilityFactory {
public:
    using CreatorFn = std::function<std::unique_ptr<CAbility>(int handleId, War3ID code)>;

    // Create an ability instance for a given rawcode.
    static std::unique_ptr<CAbility> CreateAbility(int handleId, War3ID code);

    // Global (legacy/default) registration entry points
    static void RegisterCreator(War3ID code, CreatorFn creator);
    static void RegisterAlias(War3ID alias, War3ID targetCode);
    static void RegisterSpellDescriptor(const ABSpellDescriptor& desc);
    static bool UnregisterCreator(War3ID code);

    // Module-scoped registration entry points (Phase 13 Dynamic Module System)
    static void RegisterModuleCreator(const std::string& moduleId, War3ID code, CreatorFn creator);
    static void RegisterModuleAlias(const std::string& moduleId, War3ID alias, War3ID targetCode);
    static void RegisterModuleSpellDescriptor(const std::string& moduleId, const ABSpellDescriptor& desc);
    static size_t UnregisterModuleCreators(const std::string& moduleId);
    static size_t GetModuleRegisteredCount(const std::string& moduleId);
    static std::string GetCreatorModuleId(War3ID code);

    static bool HasRegisteredCreator(War3ID code);
    static size_t GetRegisteredCount();

    // Control whether the default War3GameModule catalog is lazily auto-bootstrapped
    static void SetAutoBootstrapDefaultCatalog(bool enabled);
    static bool IsAutoBootstrapDefaultCatalogEnabled();
    static void EnsureDefaultCatalogRegistered();

    // Add all abilities from a unit's abilityList (skips Aatk/Amov which are added directly)
    static void AddDefaultAbilities(CSimulation& game, CUnit* unit,
                                    const std::vector<War3ID>& abilityList);
};

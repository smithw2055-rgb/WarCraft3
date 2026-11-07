#pragma once
// ============================================================================
// Phase 13 Engine Foundation: engine::DynamicModuleLoader
//
// Inspired by Godot 4's GDExtension loader and Unreal's FModuleManager.
// Supports both:
//   1. In-Process Built-in Module Factories (zero overhead for single-binary builds)
//   2. True OS Dynamic Shared Library (.so / .dll) hot-loading, symbol lookup,
//      and live hot-reloading via dlopen/dlsym/dlclose (POSIX) or
//      LoadLibraryA/GetProcAddress/FreeLibrary (Windows).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/game_module.h"
#include "engine/simulation_module.h"

// Standard C ABI export macros and function pointer signatures for dynamic plugins
#if defined(_WIN32)
#define WARSMASH_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define WARSMASH_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

extern "C" {
using WarsmashCreateSimModuleFn = engine::ISimulationModule* (*)();
using WarsmashDestroySimModuleFn = void (*)(engine::ISimulationModule*);
using WarsmashCreateGameModuleFn = engine::IGameModule* (*)();
using WarsmashDestroyGameModuleFn = void (*)(engine::IGameModule*);
using WarsmashGetPluginIdFn = const char* (*)();
}

namespace engine {

struct DynamicModuleLoaderStats {
    size_t builtInModulesRegistered = 0;
    size_t activeSharedLibraries = 0;
    uint64_t totalSharedLibrariesLoaded = 0;
    uint64_t totalSharedLibrariesUnloaded = 0;
    uint64_t totalHotReloads = 0;
    uint64_t totalSimModulesInstantiated = 0;
    uint64_t totalGameModulesInstantiated = 0;
};

class DynamicModuleLoader {
public:
    using SimModuleFactoryFn = std::function<std::shared_ptr<ISimulationModule>()>;
    using GameModuleFactoryFn = std::function<std::unique_ptr<IGameModule>()>;

    static DynamicModuleLoader& Instance();

    DynamicModuleLoader() = default;
    ~DynamicModuleLoader() { UnloadAllSharedLibraries(); }

    DynamicModuleLoader(const DynamicModuleLoader&) = delete;
    DynamicModuleLoader& operator=(const DynamicModuleLoader&) = delete;

    // 1. Register an in-process module factory by ID (e.g. "War3GameModule")
    void RegisterBuiltInModule(const std::string& moduleId,
                               SimModuleFactoryFn simFactory,
                               GameModuleFactoryFn gameFactory = nullptr);

    bool UnregisterBuiltInModule(const std::string& moduleId);
    bool HasModule(const std::string& moduleId) const;

    // 2. Load a dynamic shared library (.so / .dll) exporting WarsmashCreate* symbols
    bool LoadSharedLibrary(const std::string& moduleId, const std::string& sharedLibPath);
    bool UnloadSharedLibrary(const std::string& moduleId);
    bool HotReloadSharedLibrary(const std::string& moduleId, const std::string& newSharedLibPath = "");
    bool IsSharedLibraryLoaded(const std::string& moduleId) const;
    uint32_t GetModuleGeneration(const std::string& moduleId) const;
    const std::string& GetLastError() const { return lastError_; }

    // 3. Instantiate modules uniformly (prefers active shared library override, falls back to built-in)
    std::shared_ptr<ISimulationModule> CreateSimulationModule(const std::string& moduleId);
    std::unique_ptr<IGameModule> CreateGameModule(const std::string& moduleId);

    void UnloadAllSharedLibraries();
    std::vector<std::string> ListAvailableModules() const;
    const DynamicModuleLoaderStats& GetStats() const { return stats_; }

private:
    struct BuiltInEntry {
        SimModuleFactoryFn simFactory;
        GameModuleFactoryFn gameFactory;
    };

    struct SharedLibEntry {
        std::string moduleId;
        std::string path;
        void* nativeHandle = nullptr;
        uint32_t generation = 1;
        WarsmashCreateSimModuleFn createSimFn = nullptr;
        WarsmashDestroySimModuleFn destroySimFn = nullptr;
        WarsmashCreateGameModuleFn createGameFn = nullptr;
        WarsmashDestroyGameModuleFn destroyGameFn = nullptr;
        WarsmashGetPluginIdFn getPluginIdFn = nullptr;
    };

    std::unordered_map<std::string, BuiltInEntry> builtIns_;
    std::unordered_map<std::string, SharedLibEntry> sharedLibs_;
    std::string lastError_;
    DynamicModuleLoaderStats stats_{};
};

} // namespace engine

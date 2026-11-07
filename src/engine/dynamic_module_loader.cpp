#include "engine/dynamic_module_loader.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace engine {

namespace {

void* OpenNativeLibrary(const std::string& path, std::string& outError) {
#if defined(_WIN32)
    HMODULE h = LoadLibraryA(path.c_str());
    if (!h) {
        outError = "LoadLibraryA failed for: " + path;
        return nullptr;
    }
    return reinterpret_cast<void*>(h);
#else
    dlerror(); // Clear any prior error
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char* err = dlerror();
        outError = err ? std::string(err) : ("dlopen failed for: " + path);
        return nullptr;
    }
    return h;
#endif
}

void* LookupNativeSymbol(void* handle, const char* symbolName) {
    if (!handle || !symbolName) return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), symbolName));
#else
    return dlsym(handle, symbolName);
#endif
}

void CloseNativeLibrary(void* handle) {
    if (!handle) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

} // namespace

DynamicModuleLoader& DynamicModuleLoader::Instance() {
    static DynamicModuleLoader instance;
    return instance;
}

void DynamicModuleLoader::RegisterBuiltInModule(const std::string& moduleId,
                                                SimModuleFactoryFn simFactory,
                                                GameModuleFactoryFn gameFactory) {
    if (moduleId.empty()) return;
    builtIns_[moduleId] = BuiltInEntry{std::move(simFactory), std::move(gameFactory)};
    stats_.builtInModulesRegistered = builtIns_.size();
}

bool DynamicModuleLoader::UnregisterBuiltInModule(const std::string& moduleId) {
    const size_t erased = builtIns_.erase(moduleId);
    stats_.builtInModulesRegistered = builtIns_.size();
    return erased > 0;
}

bool DynamicModuleLoader::HasModule(const std::string& moduleId) const {
    return sharedLibs_.count(moduleId) > 0 || builtIns_.count(moduleId) > 0;
}

bool DynamicModuleLoader::LoadSharedLibrary(const std::string& moduleId, const std::string& sharedLibPath) {
    if (moduleId.empty() || sharedLibPath.empty()) {
        lastError_ = "Module ID and shared library path must not be empty";
        return false;
    }

    uint32_t nextGen = 1;
    auto existingIt = sharedLibs_.find(moduleId);
    if (existingIt != sharedLibs_.end()) {
        nextGen = existingIt->second.generation + 1;
        CloseNativeLibrary(existingIt->second.nativeHandle);
        sharedLibs_.erase(existingIt);
        ++stats_.totalSharedLibrariesUnloaded;
    }

    std::string err;
    void* handle = OpenNativeLibrary(sharedLibPath, err);
    if (!handle) {
        lastError_ = err;
        return false;
    }

    SharedLibEntry entry;
    entry.moduleId = moduleId;
    entry.path = sharedLibPath;
    entry.nativeHandle = handle;
    entry.generation = nextGen;
    entry.createSimFn = reinterpret_cast<WarsmashCreateSimModuleFn>(
        LookupNativeSymbol(handle, "WarsmashCreateSimulationModule"));
    entry.destroySimFn = reinterpret_cast<WarsmashDestroySimModuleFn>(
        LookupNativeSymbol(handle, "WarsmashDestroySimulationModule"));
    entry.createGameFn = reinterpret_cast<WarsmashCreateGameModuleFn>(
        LookupNativeSymbol(handle, "WarsmashCreateGameModule"));
    entry.destroyGameFn = reinterpret_cast<WarsmashDestroyGameModuleFn>(
        LookupNativeSymbol(handle, "WarsmashDestroyGameModule"));
    entry.getPluginIdFn = reinterpret_cast<WarsmashGetPluginIdFn>(
        LookupNativeSymbol(handle, "WarsmashGetModulePluginId"));

    if (!entry.createSimFn && !entry.createGameFn) {
        CloseNativeLibrary(handle);
        lastError_ = "Shared library missing WarsmashCreateSimulationModule / WarsmashCreateGameModule";
        return false;
    }

    sharedLibs_[moduleId] = std::move(entry);
    stats_.activeSharedLibraries = sharedLibs_.size();
    ++stats_.totalSharedLibrariesLoaded;
    lastError_.clear();
    return true;
}

bool DynamicModuleLoader::UnloadSharedLibrary(const std::string& moduleId) {
    auto it = sharedLibs_.find(moduleId);
    if (it == sharedLibs_.end()) return false;
    CloseNativeLibrary(it->second.nativeHandle);
    sharedLibs_.erase(it);
    stats_.activeSharedLibraries = sharedLibs_.size();
    ++stats_.totalSharedLibrariesUnloaded;
    return true;
}

bool DynamicModuleLoader::HotReloadSharedLibrary(const std::string& moduleId,
                                                 const std::string& newSharedLibPath) {
    auto it = sharedLibs_.find(moduleId);
    const std::string targetPath = !newSharedLibPath.empty()
        ? newSharedLibPath
        : (it != sharedLibs_.end() ? it->second.path : "");
    if (targetPath.empty()) {
        lastError_ = "No path available to hot-reload module: " + moduleId;
        return false;
    }
    if (!LoadSharedLibrary(moduleId, targetPath)) {
        return false;
    }
    ++stats_.totalHotReloads;
    return true;
}

bool DynamicModuleLoader::IsSharedLibraryLoaded(const std::string& moduleId) const {
    return sharedLibs_.find(moduleId) != sharedLibs_.end();
}

uint32_t DynamicModuleLoader::GetModuleGeneration(const std::string& moduleId) const {
    auto it = sharedLibs_.find(moduleId);
    if (it != sharedLibs_.end()) return it->second.generation;
    return builtIns_.count(moduleId) > 0 ? 1u : 0u;
}

std::shared_ptr<ISimulationModule> DynamicModuleLoader::CreateSimulationModule(const std::string& moduleId) {
    auto sIt = sharedLibs_.find(moduleId);
    if (sIt != sharedLibs_.end() && sIt->second.createSimFn) {
        ISimulationModule* raw = sIt->second.createSimFn();
        if (!raw) return nullptr;
        ++stats_.totalSimModulesInstantiated;
        WarsmashDestroySimModuleFn destroyFn = sIt->second.destroySimFn;
        if (destroyFn) {
            return std::shared_ptr<ISimulationModule>(raw, [destroyFn](ISimulationModule* p) {
                destroyFn(p);
            });
        }
        return std::shared_ptr<ISimulationModule>(raw);
    }

    auto bIt = builtIns_.find(moduleId);
    if (bIt != builtIns_.end() && bIt->second.simFactory) {
        auto inst = bIt->second.simFactory();
        if (inst) ++stats_.totalSimModulesInstantiated;
        return inst;
    }
    return nullptr;
}

std::unique_ptr<IGameModule> DynamicModuleLoader::CreateGameModule(const std::string& moduleId) {
    auto sIt = sharedLibs_.find(moduleId);
    if (sIt != sharedLibs_.end() && sIt->second.createGameFn) {
        IGameModule* raw = sIt->second.createGameFn();
        if (!raw) return nullptr;
        raw->SetCustomDestroyFn(sIt->second.destroyGameFn);
        ++stats_.totalGameModulesInstantiated;
        return std::unique_ptr<IGameModule>(raw);
    }

    auto bIt = builtIns_.find(moduleId);
    if (bIt != builtIns_.end() && bIt->second.gameFactory) {
        auto inst = bIt->second.gameFactory();
        if (inst) ++stats_.totalGameModulesInstantiated;
        return inst;
    }
    return nullptr;
}

void DynamicModuleLoader::UnloadAllSharedLibraries() {
    for (auto& [id, entry] : sharedLibs_) {
        (void)id;
        CloseNativeLibrary(entry.nativeHandle);
        ++stats_.totalSharedLibrariesUnloaded;
    }
    sharedLibs_.clear();
    stats_.activeSharedLibraries = 0;
}

std::vector<std::string> DynamicModuleLoader::ListAvailableModules() const {
    std::vector<std::string> names;
    names.reserve(builtIns_.size() + sharedLibs_.size());
    for (const auto& [id, _] : builtIns_) {
        names.push_back(id);
    }
    for (const auto& [id, _] : sharedLibs_) {
        if (builtIns_.find(id) == builtIns_.end()) {
            names.push_back(id);
        }
    }
    return names;
}

} // namespace engine

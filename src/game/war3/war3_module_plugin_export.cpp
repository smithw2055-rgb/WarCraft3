// ============================================================================
// Phase 13 Dynamic Plugin Entry Points for War3GameModule
//
// Exports the standard C ABI plugin hooks (WarsmashCreateSimulationModule,
// WarsmashDestroySimulationModule, WarsmashCreateGameModule,
// WarsmashDestroyGameModule, WarsmashGetModulePluginId) so War3GameModule can
// be compiled as a standalone shared library (.so / .dll) and loaded via
// engine::DynamicModuleLoader::LoadSharedLibrary().
// ============================================================================
#include "engine/dynamic_module_loader.h"
#include "game/war3/war3_game_module.h"

WARSMASH_PLUGIN_EXPORT const char* WarsmashGetModulePluginId() {
    return war3::War3GameModule::kModuleId;
}

WARSMASH_PLUGIN_EXPORT engine::ISimulationModule* WarsmashCreateSimulationModule() {
    return new war3::War3GameModule();
}

WARSMASH_PLUGIN_EXPORT void WarsmashDestroySimulationModule(engine::ISimulationModule* instance) {
    delete instance;
}

WARSMASH_PLUGIN_EXPORT engine::IGameModule* WarsmashCreateGameModule() {
    return new war3::War3GameModule();
}

WARSMASH_PLUGIN_EXPORT void WarsmashDestroyGameModule(engine::IGameModule* instance) {
    delete instance;
}

#pragma once
// ============================================================================
// Phase 13 Modernization: war3::War3GameModule (src/game/war3/war3_game_module.h)
//
// Extracts WarCraft III's unit/building/worker/hero ability assembly, ~190
// spell/ability catalog registrations, item & destructable data binding, and
// player food/techtree possession rules out of CSimulation into an independent,
// dynamically loadable module implementing both:
//   - engine::ISimulationModule (plugs into CSimulation)
//   - engine::IGameModule       (plugs into engine::EngineKernel)
// ============================================================================
#include <cstdint>
#include <memory>
#include <string>

#include "engine/dynamic_module_loader.h"
#include "engine/engine_kernel.h"
#include "engine/game_module.h"
#include "engine/simulation_module.h"
#include "resource/slk.h"

namespace war3 {

class War3GameModule final : public engine::IGameModule, public engine::ISimulationModule {
public:
    static constexpr const char* kModuleId = "War3GameModule";

    War3GameModule();
    ~War3GameModule() override = default;

    // Shared default instance used when CSimulation boots in standard WarCraft 3 mode
    static std::shared_ptr<War3GameModule> GetSharedDefaultInstance();

    // Register / unregister "War3GameModule" in engine::DynamicModuleLoader
    static void EnsureRegisteredInDynamicLoader();

    // Register / unregister the ~190 WarCraft 3 abilities & canonical spell descriptors
    static void RegisterWar3AbilityCatalog();
    static size_t UnregisterWar3AbilityCatalog();

    // --- engine::IGameModule interface ---
    const char* GetModuleName() const override { return kModuleId; }
    bool OnInit(engine::EngineKernel& kernel) override;
    void OnFixedTick(engine::EngineKernel& kernel, float fixedDt) override;
    void OnFrameUpdate(engine::EngineKernel& kernel, float dt, float interpolationAlpha) override;
    void OnShutdown(engine::EngineKernel& kernel) override;

    // --- engine::ISimulationModule interface ---
    const char* GetModuleId() const override { return kModuleId; }
    uint32_t GetModuleVersion() const override { return 13u; }

    void OnAttachToSimulation(CSimulation& sim) override;
    void OnDetachFromSimulation(CSimulation& sim) override;

    void OnRegisterAbilities() override;
    void OnUnregisterAbilities() override;

    void OnAssembleUnitAbilities(CSimulation& sim,
                                 CUnit& unit,
                                 const CUnitType& unitType) override;

    void OnConfigureCreatedItem(CSimulation& sim, CItem& item) override;

    void OnConfigureCreatedDestructable(CSimulation& sim,
                                        CDestructable& destructable,
                                        float initialLife,
                                        int rotationDeg) override;

    void OnUnitCommittedToWorld(CSimulation& sim, CUnit& unit) override;

    void OnUnitRemovedFromWorld(CSimulation& sim,
                                CUnit& unit,
                                bool wasActiveUnit) override;

    // Optional bound CSimulation when hosted inside EngineKernel
    void BindSimulation(CSimulation* sim) { boundSim_ = sim; }
    CSimulation* GetBoundSimulation() const { return boundSim_; }

    // WarCraft 3 domain classification helpers
    static bool IsRepairAbilityAlias(War3ID abilityCode);
    static bool IsTownHallReturnResourceType(War3ID typeId);
    static bool IsStandardRaceWorkerType(War3ID typeId);

private:
    CSimulation* boundSim_ = nullptr;
};

} // namespace war3

#pragma once
// ============================================================================
// Phase 13 Engine Foundation: engine::ISimulationModule & NullSimulationModule
//
// Decouples the deterministic ECS world kernel (CSimulation) from game-specific
// unit/ability/economy/techtree rulesets (such as War3GameModule, a custom
// sci-fi RTS module, or a headless pure-ECS benchmark harness).
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class CSimulation;
class CUnit;
class CUnitType;
class CItem;
class CDestructable;

namespace engine {

struct SimulationModuleStats {
    uint64_t unitsAssembled = 0;
    uint64_t abilitiesAttached = 0;
    uint64_t itemsConfigured = 0;
    uint64_t destructablesConfigured = 0;
    uint64_t unitsCommitted = 0;
    uint64_t unitsRemoved = 0;
    uint64_t attachCount = 0;
    uint64_t detachCount = 0;
};

class ISimulationModule {
public:
    virtual ~ISimulationModule() = default;

    virtual const char* GetModuleId() const = 0;
    virtual uint32_t GetModuleVersion() const { return 1; }

    // Lifecycle hooks when attached to or detached from a CSimulation world
    virtual void OnAttachToSimulation(CSimulation& /*sim*/) {
        ++stats_.attachCount;
    }
    virtual void OnDetachFromSimulation(CSimulation& /*sim*/) {
        ++stats_.detachCount;
    }

    // Dynamic ability catalog lifecycle
    virtual void OnRegisterAbilities() {}
    virtual void OnUnregisterAbilities() {}

    // Entity archetype assembly & configuration hooks invoked by CSimulation
    virtual void OnAssembleUnitAbilities(CSimulation& sim,
                                         CUnit& unit,
                                         const CUnitType& unitType) = 0;

    virtual void OnConfigureCreatedItem(CSimulation& /*sim*/, CItem& /*item*/) {}

    virtual void OnConfigureCreatedDestructable(CSimulation& /*sim*/,
                                                CDestructable& /*destructable*/,
                                                float /*initialLife*/,
                                                int /*rotationDeg*/) {}

    // World commit & removal hooks (player possession, food cap, techtree unlock)
    virtual void OnUnitCommittedToWorld(CSimulation& /*sim*/, CUnit& /*unit*/) {}

    virtual void OnUnitRemovedFromWorld(CSimulation& /*sim*/,
                                        CUnit& /*unit*/,
                                        bool /*wasActiveUnit*/) {}

    const SimulationModuleStats& GetModuleStats() const { return stats_; }
    void ResetModuleStats() { stats_ = {}; }

protected:
    SimulationModuleStats stats_{};
};

// Pure Engine Mode module: creates bare ECS entities without any WarCraft 3
// race/worker/building abilities, food accounting, or techtree rules.
class NullSimulationModule final : public ISimulationModule {
public:
    const char* GetModuleId() const override { return "NullPureEngineModule"; }

    void OnAssembleUnitAbilities(CSimulation& /*sim*/,
                                 CUnit& /*unit*/,
                                 const CUnitType& /*unitType*/) override {
        ++stats_.unitsAssembled;
    }
};

} // namespace engine

#pragma once
// ============================================================================
// Phase 12 Engine Foundation: engine::IGameModule
//
// Defines the clean plug-in boundary between the reusable Engine Foundation
// (EngineKernel + Subsystems) and a concrete game module (such as WarCraft 3,
// a custom RTS/MOBA/TD mod, or an automated headless simulation test harness).
// ============================================================================

namespace engine {

class EngineKernel;

class IGameModule {
public:
    virtual ~IGameModule() = default;

    virtual const char* GetModuleName() const = 0;

    // Optional hook to register game-specific subsystems before InitializeAll()
    virtual void OnRegisterSubsystems(EngineKernel& /*kernel*/) {}

    // Called once when the module is mounted and engine subsystems are ready
    virtual bool OnInit(EngineKernel& /*kernel*/) { return true; }

    // Deterministic fixed-frequency simulation tick (e.g., 20Hz lockstep)
    virtual void OnFixedTick(EngineKernel& /*kernel*/, float /*fixedDt*/) {}

    // Variable-frequency per-frame update with precomputed [0..1] interpolation alpha
    virtual void OnFrameUpdate(EngineKernel& /*kernel*/, float /*dt*/, float /*interpolationAlpha*/) {}

    // Per-frame rendering callback
    virtual void OnRender(EngineKernel& /*kernel*/, int /*screenW*/, int /*screenH*/) {}

    // Clean teardown before engine subsystems shut down
    virtual void OnShutdown(EngineKernel& /*kernel*/) {}
};

} // namespace engine

#pragma once
// ============================================================================
// Phase 12 Engine Foundation: engine::EngineKernel & Standard Server Subsystems
//
// Unifies the modernized engine servers (JobSystem, ResourceServer, AudioServer,
// RenderServer + RenderGraph, InputActionMap, ScriptHostRouter, EngineProfiler)
// under a single reusable EngineKernel (inspired by Godot's OS/MainLoop +
// Server Singletons and Unity's PlayerLoop + SubsystemManager).
//
// Supports two operating modes:
//   1. Standalone Generic Engine Mode (`BootstrapStandardSubsystems()`):
//      EngineKernel owns all standard servers and hosts any `IGameModule`.
//   2. Embedded Host Mode (`BindExternalServices(...)`):
//      EngineKernel wraps existing host instances (e.g., inside GameController)
//      with zero memory duplication while orchestrating the unified frame loop.
// ============================================================================
#include <cstdint>
#include <memory>
#include <string>

#include "audio/audio_server.h"
#include "core/engine_profiler.h"
#include "core/input_action_map.h"
#include "core/job_system.h"
#include "core/unit_presentation_system.h"
#include "engine/engine_subsystem.h"
#include "engine/fixed_timestep_stepper.h"
#include "engine/game_module.h"
#include "render/render_graph.h"
#include "render/render_server.h"
#include "resource/resource_server.h"
#include "scripting/script_engine_host.h"

namespace engine {

// ----------------------------------------------------------------------------
// Standard Engine Server Subsystem Adapters
// ----------------------------------------------------------------------------

class JobEngineSubsystem final : public ISubsystem {
public:
    explicit JobEngineSubsystem(uint32_t workerCount = 2)
        : workerCount_(workerCount), ownedJobs_(std::make_unique<core::JobSystem>(workerCount))
        , jobs_(ownedJobs_.get()) {}

    explicit JobEngineSubsystem(core::JobSystem* externalJobs)
        : jobs_(externalJobs) {}

    const char* GetName() const override { return "JobEngineSubsystem"; }
    SubsystemPhase GetPhase() const override { return SubsystemPhase::Jobs; }

    bool OnInit() override {
        if (jobs_ && !jobs_->IsInitialized()) {
            jobs_->Initialize(workerCount_);
        }
        return jobs_ != nullptr;
    }

    void OnShutdown() override {
        if (ownedJobs_) {
            ownedJobs_->Shutdown();
        }
    }

    core::JobSystem* GetJobSystem() const { return jobs_; }

private:
    uint32_t workerCount_ = 2;
    std::unique_ptr<core::JobSystem> ownedJobs_;
    core::JobSystem* jobs_ = nullptr;
};

class ResourceEngineSubsystem final : public ISubsystem {
public:
    ResourceEngineSubsystem()
        : ownedServer_(std::make_unique<::resource::ResourceServer>())
        , server_(ownedServer_.get()) {}

    explicit ResourceEngineSubsystem(::resource::ResourceServer* externalServer)
        : server_(externalServer) {}

    const char* GetName() const override { return "ResourceEngineSubsystem"; }
    SubsystemPhase GetPhase() const override { return SubsystemPhase::Resource; }

    bool OnInit() override { return server_ != nullptr; }

    void OnFrameUpdate(float /*dt*/) override {
        if (server_) {
            server_->PollAsyncCompletions();
        }
    }

    void OnShutdown() override {
        if (ownedServer_) {
            ownedServer_->StopAsyncWorker();
            ownedServer_->ClearAll();
        }
    }

    ::resource::ResourceServer* GetResourceServer() const { return server_; }

private:
    std::unique_ptr<::resource::ResourceServer> ownedServer_;
    ::resource::ResourceServer* server_ = nullptr;
};

class AudioEngineSubsystem final : public ISubsystem {
public:
    AudioEngineSubsystem()
        : ownedServer_(std::make_unique<audio::AudioServer>())
        , server_(ownedServer_.get()) {}

    explicit AudioEngineSubsystem(audio::AudioServer* externalServer)
        : server_(externalServer) {}

    const char* GetName() const override { return "AudioEngineSubsystem"; }
    SubsystemPhase GetPhase() const override { return SubsystemPhase::Audio; }

    bool OnInit() override { return server_ != nullptr; }

    void OnFrameUpdate(float dt) override {
        if (server_) {
            server_->Update(dt);
        }
    }

    void OnShutdown() override {
        if (ownedServer_) {
            ownedServer_->Reset();
        }
    }

    audio::AudioServer* GetAudioServer() const { return server_; }

private:
    std::unique_ptr<audio::AudioServer> ownedServer_;
    audio::AudioServer* server_ = nullptr;
};

class InputEngineSubsystem final : public ISubsystem {
public:
    InputEngineSubsystem()
        : ownedInput_(std::make_unique<core::InputActionMap>())
        , input_(ownedInput_.get()) {}

    explicit InputEngineSubsystem(core::InputActionMap* externalInput)
        : input_(externalInput) {}

    const char* GetName() const override { return "InputEngineSubsystem"; }
    SubsystemPhase GetPhase() const override { return SubsystemPhase::Platform; }

    bool OnInit() override { return input_ != nullptr; }

    core::InputActionMap* GetInputActionMap() const { return input_; }

private:
    std::unique_ptr<core::InputActionMap> ownedInput_;
    core::InputActionMap* input_ = nullptr;
};

class ScriptEngineSubsystem final : public ISubsystem {
public:
    ScriptEngineSubsystem()
        : ownedRouter_(std::make_unique<scripting::ScriptHostRouter>(
              std::make_unique<scripting::JassScriptHostAdapter>()))
        , router_(ownedRouter_.get()) {}

    explicit ScriptEngineSubsystem(scripting::ScriptHostRouter* externalRouter)
        : router_(externalRouter) {}

    const char* GetName() const override { return "ScriptEngineSubsystem"; }
    SubsystemPhase GetPhase() const override { return SubsystemPhase::Scripting; }

    bool OnInit() override { return router_ != nullptr; }

    void OnFixedUpdate(float /*fixedDt*/) override {}

    scripting::ScriptHostRouter* GetScriptRouter() const { return router_; }

private:
    std::unique_ptr<scripting::ScriptHostRouter> ownedRouter_;
    scripting::ScriptHostRouter* router_ = nullptr;
};

class RenderEngineSubsystem final : public ISubsystem {
public:
    RenderEngineSubsystem()
        : ownedRenderServer_(std::make_unique<::render::RenderServer>())
        , ownedRenderGraph_(std::make_unique<::render::RenderGraph>())
        , renderServer_(ownedRenderServer_.get())
        , renderGraph_(ownedRenderGraph_.get()) {}

    RenderEngineSubsystem(::render::RenderServer* externalServer, ::render::RenderGraph* externalGraph)
        : renderServer_(externalServer)
        , renderGraph_(externalGraph) {}

    const char* GetName() const override { return "RenderEngineSubsystem"; }
    SubsystemPhase GetPhase() const override { return SubsystemPhase::Render; }

    bool OnInit() override { return renderServer_ != nullptr && renderGraph_ != nullptr; }

    ::render::RenderServer* GetRenderServer() const { return renderServer_; }
    ::render::RenderGraph* GetRenderGraph() const { return renderGraph_; }

private:
    std::unique_ptr<::render::RenderServer> ownedRenderServer_;
    std::unique_ptr<::render::RenderGraph> ownedRenderGraph_;
    ::render::RenderServer* renderServer_ = nullptr;
    ::render::RenderGraph* renderGraph_ = nullptr;
};

class TelemetryEngineSubsystem final : public ISubsystem {
public:
    explicit TelemetryEngineSubsystem(core::EngineProfiler* profiler = &core::EngineProfiler::Instance())
        : profiler_(profiler) {}

    const char* GetName() const override { return "TelemetryEngineSubsystem"; }
    SubsystemPhase GetPhase() const override { return SubsystemPhase::Telemetry; }

    bool OnInit() override { return profiler_ != nullptr; }

    core::EngineProfiler* GetProfiler() const { return profiler_; }

private:
    core::EngineProfiler* profiler_ = nullptr;
};

// ----------------------------------------------------------------------------
// EngineKernel — Unified Engine Foundation Host
// ----------------------------------------------------------------------------

struct EngineKernelStats {
    uint64_t framesExecuted = 0;
    uint64_t fixedTicksExecuted = 0;
    float lastInterpolationAlpha = 0.0f;
    size_t activeSubsystems = 0;
    bool hasMountedGameModule = false;
    std::string activeGameModuleName;
};

class EngineKernel {
public:
    EngineKernel() = default;
    ~EngineKernel() { Shutdown(); }

    EngineKernel(const EngineKernel&) = delete;
    EngineKernel& operator=(const EngineKernel&) = delete;

    // 1. Standalone bootstrap: creates and initializes all standard engine foundation servers
    bool BootstrapStandardSubsystems(uint32_t workerThreads = 2, float fixedStepSec = 0.05f);

    // 2. Non-owning bind: attaches existing servers (e.g., from GameController) to the kernel
    bool BindExternalServices(core::JobSystem* jobs,
                              ::resource::ResourceServer* resources,
                              audio::AudioServer* audio,
                              ::render::RenderServer* renderServer,
                              ::render::RenderGraph* renderGraph,
                              core::InputActionMap* input,
                              core::EngineProfiler* profiler,
                              float fixedStepSec = 0.05f,
                              UnitPresentationSystem* presentation = nullptr,
                              CSimulation* simulation = nullptr);

    // Mount a pluggable game module (e.g., War3GameModule or a custom game/test module)
    bool MountGameModule(std::unique_ptr<IGameModule> module);
    IGameModule* GetGameModule() const { return gameModule_.get(); }

    // Execute one complete engine frame (Profiler -> Async Resource Poll -> Fixed Steps -> Frame Update -> Render)
    int StepFrame(float dt, int screenW = 1280, int screenH = 720);

    void Shutdown();

    // Direct accessors to standard foundation servers
    SubsystemRegistry& Subsystems() { return subsystems_; }
    const SubsystemRegistry& Subsystems() const { return subsystems_; }

    FixedTimestepStepper& Stepper() { return stepper_; }
    const FixedTimestepStepper& Stepper() const { return stepper_; }

    core::JobSystem* Jobs() const;
    ::resource::ResourceServer* Resources() const;
    audio::AudioServer* Audio() const;
    ::render::RenderServer* RenderServer() const;
    ::render::RenderGraph* RenderGraph() const;
    core::InputActionMap* Input() const;
    scripting::ScriptHostRouter* Scripting() const;
    core::EngineProfiler* Profiler() const;
    UnitPresentationSystem* Presentation() const;

    EngineKernelStats GetStats() const;

private:
    SubsystemRegistry subsystems_;
    FixedTimestepStepper stepper_{0.05f, 3};
    std::unique_ptr<IGameModule> gameModule_;
    bool initialized_ = false;
    uint64_t frameCounter_ = 0;
};

} // namespace engine

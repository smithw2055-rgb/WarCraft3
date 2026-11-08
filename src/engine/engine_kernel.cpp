#include "engine/engine_kernel.h"

namespace engine {

bool EngineKernel::BootstrapStandardSubsystems(uint32_t workerThreads, float fixedStepSec) {
    Shutdown();
    stepper_.Configure(fixedStepSec, 3);
    stepper_.Reset();

    subsystems_.Register<InputEngineSubsystem>();
    subsystems_.Register<JobEngineSubsystem>(workerThreads);
    subsystems_.Register<ResourceEngineSubsystem>();
    subsystems_.Register<AudioEngineSubsystem>();
    subsystems_.Register<ScriptEngineSubsystem>();
    auto& renderSub = subsystems_.Register<RenderEngineSubsystem>();
    subsystems_.Register<WorldPresentationSubsystem>(&stepper_, renderSub.GetRenderServer());
    subsystems_.Register<TelemetryEngineSubsystem>();

    initialized_ = subsystems_.InitializeAll();
    return initialized_;
}

bool EngineKernel::BindExternalServices(core::JobSystem* jobs,
                                        resource::ResourceServer* resources,
                                        audio::AudioServer* audio,
                                        render::RenderServer* renderServer,
                                        render::RenderGraph* renderGraph,
                                        core::InputActionMap* input,
                                        core::EngineProfiler* profiler,
                                        float fixedStepSec,
                                        UnitPresentationSystem* presentation,
                                        CSimulation* simulation) {
    Shutdown();
    stepper_.Configure(fixedStepSec, 3);
    stepper_.Reset();

    if (input) subsystems_.Register<InputEngineSubsystem>(input);
    if (jobs) subsystems_.Register<JobEngineSubsystem>(jobs);
    if (resources) subsystems_.Register<ResourceEngineSubsystem>(resources);
    if (audio) subsystems_.Register<AudioEngineSubsystem>(audio);
    if (presentation) {
        subsystems_.Register<WorldPresentationSubsystem>(presentation, simulation, &stepper_, renderServer);
    }
    if (renderServer && renderGraph) {
        subsystems_.Register<RenderEngineSubsystem>(renderServer, renderGraph);
    }
    if (profiler) subsystems_.Register<TelemetryEngineSubsystem>(profiler);

    initialized_ = subsystems_.InitializeAll();
    return initialized_;
}

bool EngineKernel::MountGameModule(std::unique_ptr<IGameModule> module) {
    if (gameModule_) {
        gameModule_->OnShutdown(*this);
        gameModule_.reset();
    }
    if (!module) return false;

    gameModule_ = std::move(module);
    gameModule_->OnRegisterSubsystems(*this);
    if (!subsystems_.InitializeAll()) {
        gameModule_.reset();
        return false;
    }
    initialized_ = true;
    if (!gameModule_->OnInit(*this)) {
        gameModule_->OnShutdown(*this);
        gameModule_.reset();
        return false;
    }
    return true;
}

int EngineKernel::StepFrame(float dt, int screenW, int screenH) {
    if (!initialized_) return 0;

    ++frameCounter_;
    core::EngineProfiler* prof = Profiler();
    if (prof) {
        prof->BeginFrame(frameCounter_);
    }

    int subSteps = 0;
    {
        if (prof) prof->RecordZoneSample("EngineKernel::FixedUpdate", "Engine", frameCounter_ * 1000, 10, 1, 1);
        subSteps = stepper_.AdvanceFrame(dt, [this](float fixedDt) {
            subsystems_.FixedUpdateAll(fixedDt);
            if (gameModule_) {
                gameModule_->OnFixedTick(*this, fixedDt);
            }
        });
    }

    const float alpha = stepper_.GetInterpolationAlpha();
    if (auto* pres = Presentation()) {
        pres->SetInterpolationAlpha(alpha);
    }
    {
        if (prof) prof->RecordZoneSample("EngineKernel::FrameUpdate", "Engine", frameCounter_ * 1000 + 20, 10, 1, 1);
        subsystems_.FrameUpdateAll(dt);
        if (gameModule_) {
            gameModule_->OnFrameUpdate(*this, dt, alpha);
        }
    }

    {
        if (prof) prof->RecordZoneSample("EngineKernel::Render", "Engine", frameCounter_ * 1000 + 40, 10, 1, 1);
        subsystems_.RenderAll();
        if (gameModule_) {
            gameModule_->OnRender(*this, screenW, screenH);
        }
    }

    if (prof) {
        prof->EndFrame();
    }
    return subSteps;
}

void EngineKernel::Shutdown() {
    if (gameModule_) {
        gameModule_->OnShutdown(*this);
        gameModule_.reset();
    }
    subsystems_.Clear();
    stepper_.Reset();
    initialized_ = false;
    frameCounter_ = 0;
}

core::JobSystem* EngineKernel::Jobs() const {
    auto* sub = subsystems_.TryGet<JobEngineSubsystem>();
    return sub ? sub->GetJobSystem() : nullptr;
}

resource::ResourceServer* EngineKernel::Resources() const {
    auto* sub = subsystems_.TryGet<ResourceEngineSubsystem>();
    return sub ? sub->GetResourceServer() : nullptr;
}

audio::AudioServer* EngineKernel::Audio() const {
    auto* sub = subsystems_.TryGet<AudioEngineSubsystem>();
    return sub ? sub->GetAudioServer() : nullptr;
}

render::RenderServer* EngineKernel::RenderServer() const {
    auto* sub = subsystems_.TryGet<RenderEngineSubsystem>();
    return sub ? sub->GetRenderServer() : nullptr;
}

render::RenderGraph* EngineKernel::RenderGraph() const {
    auto* sub = subsystems_.TryGet<RenderEngineSubsystem>();
    return sub ? sub->GetRenderGraph() : nullptr;
}

core::InputActionMap* EngineKernel::Input() const {
    auto* sub = subsystems_.TryGet<InputEngineSubsystem>();
    return sub ? sub->GetInputActionMap() : nullptr;
}

scripting::ScriptHostRouter* EngineKernel::Scripting() const {
    auto* sub = subsystems_.TryGet<ScriptEngineSubsystem>();
    return sub ? sub->GetScriptRouter() : nullptr;
}

core::EngineProfiler* EngineKernel::Profiler() const {
    auto* sub = subsystems_.TryGet<TelemetryEngineSubsystem>();
    return sub ? sub->GetProfiler() : nullptr;
}

UnitPresentationSystem* EngineKernel::Presentation() const {
    auto* sub = subsystems_.TryGet<WorldPresentationSubsystem>();
    return sub ? sub->GetPresentation() : nullptr;
}

EngineKernelStats EngineKernel::GetStats() const {
    EngineKernelStats st;
    st.framesExecuted = stepper_.GetTotalFrames();
    st.fixedTicksExecuted = stepper_.GetTotalFixedTicks();
    st.lastInterpolationAlpha = stepper_.GetInterpolationAlpha();
    st.activeSubsystems = subsystems_.Count();
    st.hasMountedGameModule = (gameModule_ != nullptr);
    if (gameModule_) {
        st.activeGameModuleName = gameModule_->GetModuleName();
    }
    return st;
}

} // namespace engine

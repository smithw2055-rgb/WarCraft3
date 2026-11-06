#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class CSimulation;

// ============================================================
// SimulationSystemPipeline (Phase 5 Engine Modernization - Stage C)
//
// Inspired by Unity PlayerLoop / DOTS ComponentSystemGroup and
// Godot SceneTree process groups. Decomposes the monolithic
// CSimulation::Update() into ordered, inspectable, and extensible
// deterministic simulation phases.
// ============================================================

enum class SimSystemPhase : int {
    PreTick = 0,       // Fog timer tick, spatial grid pre-build
    UnitLogic = 100,   // Unit regen, buffs, behavior & order execution
    DelayedActions = 200, // Timed script callbacks (UnitDamagePoint, etc.)
    Projectiles = 300, // Projectile flight & impact
    Hazards = 400,     // Area hazards (Stasis Trap, Tornado)
    SpatialSeparation = 500, // Unit overlap push-apart (SpatialHashGrid)
    LifecycleCommit = 600,   // Process removals & newly spawned units/items/destructables
    VisionAndFog = 700,      // Sight grid rebuild
    ClockAndPost = 800       // Time-of-day advance & post-update hooks
};

class ISimulationSystem {
public:
    virtual ~ISimulationSystem() = default;
    virtual const char* GetName() const = 0;
    virtual SimSystemPhase GetPhase() const = 0;
    virtual void Execute(CSimulation& sim, float dt) = 0;
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

private:
    bool enabled_ = true;
};

class FunctionalSimSystem final : public ISimulationSystem {
public:
    using TickFn = std::function<void(CSimulation&, float)>;

    FunctionalSimSystem(std::string name, SimSystemPhase phase, TickFn fn)
        : name_(std::move(name)), phase_(phase), fn_(std::move(fn)) {}

    const char* GetName() const override { return name_.c_str(); }
    SimSystemPhase GetPhase() const override { return phase_; }
    void Execute(CSimulation& sim, float dt) override {
        if (fn_) fn_(sim, dt);
    }

private:
    std::string name_;
    SimSystemPhase phase_;
    TickFn fn_;
};

class SimulationSystemPipeline {
public:
    SimulationSystemPipeline() = default;

    void RegisterSystem(std::unique_ptr<ISimulationSystem> sys) {
        if (!sys) return;
        systems_.push_back(std::move(sys));
        std::stable_sort(systems_.begin(), systems_.end(),
            [](const std::unique_ptr<ISimulationSystem>& a,
               const std::unique_ptr<ISimulationSystem>& b) {
                return static_cast<int>(a->GetPhase()) < static_cast<int>(b->GetPhase());
            });
    }

    ISimulationSystem* RegisterLambda(const std::string& name,
                                      SimSystemPhase phase,
                                      FunctionalSimSystem::TickFn fn) {
        auto sys = std::make_unique<FunctionalSimSystem>(name, phase, std::move(fn));
        ISimulationSystem* raw = sys.get();
        RegisterSystem(std::move(sys));
        return raw;
    }

    ISimulationSystem* FindSystem(const std::string& name) const {
        for (const auto& s : systems_) {
            if (s && s->GetName() == name) return s.get();
        }
        return nullptr;
    }

    bool SetSystemEnabled(const std::string& name, bool enabled) {
        if (auto* s = FindSystem(name)) {
            s->SetEnabled(enabled);
            return true;
        }
        return false;
    }

    void ExecuteAll(CSimulation& sim, float dt) {
        for (auto& s : systems_) {
            if (s && s->IsEnabled()) {
                s->Execute(sim, dt);
                lastExecutedCount_++;
            }
        }
        totalTicksExecuted_++;
    }

    size_t SystemCount() const { return systems_.size(); }
    uint64_t GetTotalTicksExecuted() const { return totalTicksExecuted_; }
    uint64_t GetLastExecutedSystemCount() const { return lastExecutedCount_; }
    void ResetStepStats() { lastExecutedCount_ = 0; }

    const std::vector<std::unique_ptr<ISimulationSystem>>& GetSystems() const { return systems_; }

private:
    std::vector<std::unique_ptr<ISimulationSystem>> systems_;
    uint64_t totalTicksExecuted_ = 0;
    uint64_t lastExecutedCount_ = 0;
};

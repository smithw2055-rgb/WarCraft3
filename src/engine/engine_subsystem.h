#pragma once
// ============================================================================
// Phase 12 Engine Foundation: engine::ISubsystem & engine::SubsystemRegistry
//
// Inspired by Unity's SubsystemManager / PlayerLoop and Godot's Server
// Architecture (RenderingServer, AudioServer, PhysicsServer, NavigationServer).
// Provides a game-agnostic lifecycle bus and O(1) type-indexed service locator
// so engine foundation modules (Jobs, VFS/Resource, Audio, Scripting, ECS,
// Presentation, Render, UI, Telemetry) are cleanly decoupled from game rules.
// ============================================================================
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include "engine/core/type_id.h"
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine {

enum class SubsystemPhase : int {
    Platform     = 0,
    Jobs         = 50,
    Resource     = 100,
    Audio        = 200,
    Scripting    = 300,
    Simulation   = 400,
    Presentation = 500,
    Render       = 600,
    UI           = 700,
    Telemetry    = 800
};

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    virtual const char* GetName() const = 0;
    virtual SubsystemPhase GetPhase() const = 0;

    virtual bool OnInit() { return true; }
    virtual void OnFixedUpdate(float /*fixedDt*/) {}
    virtual void OnFrameUpdate(float /*dt*/) {}
    virtual void OnRender() {}
    virtual void OnShutdown() {}

    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    bool IsInitialized() const { return initialized_; }
    void MarkInitialized(bool v) { initialized_ = v; }

private:
    bool enabled_ = true;
    bool initialized_ = false;
};

struct SubsystemRegistryStats {
    size_t registeredCount = 0;
    size_t initializedCount = 0;
    uint64_t totalFixedUpdatesDispatched = 0;
    uint64_t totalFrameUpdatesDispatched = 0;
    uint64_t totalRenderPassesDispatched = 0;
};

class SubsystemRegistry {
public:
    SubsystemRegistry() = default;
    ~SubsystemRegistry() { ShutdownAll(); }

    SubsystemRegistry(const SubsystemRegistry&) = delete;
    SubsystemRegistry& operator=(const SubsystemRegistry&) = delete;

    template <typename T, typename... Args>
    T& Register(Args&&... args) {
        const TypeId key = StaticTypeId<T>();
        auto it = byType_.find(key);
        if (it != byType_.end()) {
            return *static_cast<T*>(it->second);
        }
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        byType_[key] = raw;
        ordered_.push_back(std::move(ptr));
        SortByPhase();
        stats_.registeredCount = ordered_.size();
        return *raw;
    }

    template <typename T>
    T* TryGet() const {
        const auto it = byType_.find(StaticTypeId<T>());
        return (it != byType_.end()) ? static_cast<T*>(it->second) : nullptr;
    }

    template <typename T>
    T& Get() const {
        return *TryGet<T>();
    }

    ISubsystem* FindByName(const std::string& name) const {
        for (const auto& sub : ordered_) {
            if (sub && sub->GetName() == name) return sub.get();
        }
        return nullptr;
    }

    bool SetSubsystemEnabled(const std::string& name, bool enabled) {
        if (auto* sub = FindByName(name)) {
            sub->SetEnabled(enabled);
            return true;
        }
        return false;
    }

    bool InitializeAll() {
        size_t initCount = 0;
        for (auto& sub : ordered_) {
            if (!sub) continue;
            if (!sub->IsInitialized()) {
                if (!sub->OnInit()) {
                    return false;
                }
                sub->MarkInitialized(true);
            }
            ++initCount;
        }
        stats_.initializedCount = initCount;
        return true;
    }

    void FixedUpdateAll(float fixedDt) {
        for (auto& sub : ordered_) {
            if (sub && sub->IsEnabled() && sub->IsInitialized()) {
                sub->OnFixedUpdate(fixedDt);
                ++stats_.totalFixedUpdatesDispatched;
            }
        }
    }

    void FrameUpdateAll(float dt) {
        for (auto& sub : ordered_) {
            if (sub && sub->IsEnabled() && sub->IsInitialized()) {
                sub->OnFrameUpdate(dt);
                ++stats_.totalFrameUpdatesDispatched;
            }
        }
    }

    void RenderAll() {
        for (auto& sub : ordered_) {
            if (sub && sub->IsEnabled() && sub->IsInitialized()) {
                sub->OnRender();
                ++stats_.totalRenderPassesDispatched;
            }
        }
    }

    void ShutdownAll() {
        // Reverse phase order teardown so high-level systems release resources before low-level servers
        for (auto it = ordered_.rbegin(); it != ordered_.rend(); ++it) {
            if (*it && (*it)->IsInitialized()) {
                (*it)->OnShutdown();
                (*it)->MarkInitialized(false);
            }
        }
        stats_.initializedCount = 0;
    }

    void Clear() {
        ShutdownAll();
        byType_.clear();
        ordered_.clear();
        stats_ = {};
    }

    size_t Count() const { return ordered_.size(); }
    const std::vector<std::unique_ptr<ISubsystem>>& GetOrderedSubsystems() const { return ordered_; }
    const SubsystemRegistryStats& GetStats() const { return stats_; }

private:
    void SortByPhase() {
        std::stable_sort(ordered_.begin(), ordered_.end(),
            [](const std::unique_ptr<ISubsystem>& a, const std::unique_ptr<ISubsystem>& b) {
                return static_cast<int>(a->GetPhase()) < static_cast<int>(b->GetPhase());
            });
    }

    std::vector<std::unique_ptr<ISubsystem>> ordered_;
    std::unordered_map<TypeId, ISubsystem*> byType_;
    SubsystemRegistryStats stats_{};
};

} // namespace engine

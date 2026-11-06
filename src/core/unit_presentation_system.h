#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/signal.h"
#include "ecs/animation_utils.h"
#include "ecs/csimulation.h"
#include "ecs/item.h"
#include "ecs/unit.h"
#include "render/scene.h"
#include "render/render_server.h"

// ============================================================================
// UnitPresentationSystem (Phase 5 Engine Modernization - Stage B)
//
// Godot Node / Unity PresentationSystem-inspired decoupled presentation layer.
// Replaces the 15+ scattered `simUnit*` / `simItem*` / `effectVisuals_` maps
// previously hardcoded inside the `GameController` God Class, and subscribes
// directly to `CSimulation::Events()` (SimEventBus) to automatically clean up
// visual state when units or items are removed.
// ============================================================================
class UnitPresentationSystem {
public:
    struct UnitVisualSeed {
        float scale[3] = {1.0f, 1.0f, 1.0f};
        uint32_t skinId = 0;
        int hitpoints = 0;
        int flags = 0;
    };

    UnitPresentationSystem() = default;
    ~UnitPresentationSystem() { UnbindSimulationEvents(); }

    // Subscribe to CSimulation's multicast SimEventBus for automatic reactive cleanup
    void BindSimulationEvents(CSimulation& sim) {
        UnbindSimulationEvents();
        boundSim_ = &sim;
        connUnitRemoved_ = sim.Events().onUnitRemoved.Connect([this](int unitHandle) {
            OnUnitRemoved(unitHandle);
        });
        connItemRemoved_ = sim.Events().onItemRemoved.Connect([this](int itemHandle) {
            OnItemRemoved(itemHandle);
        });
    }

    void UnbindSimulationEvents() {
        if (boundSim_) {
            if (connUnitRemoved_ != 0) boundSim_->Events().onUnitRemoved.Disconnect(connUnitRemoved_);
            if (connItemRemoved_ != 0) boundSim_->Events().onItemRemoved.Disconnect(connItemRemoved_);
        }
        boundSim_ = nullptr;
        connUnitRemoved_ = 0;
        connItemRemoved_ = 0;
    }

    // Reactive cleanup handler invoked automatically on `onUnitRemoved` signal
    void OnUnitRemoved(int handleId, SceneManager* scene = nullptr) {
        auto it = unitToVisualIdx.find(handleId);
        if (it != unitToVisualIdx.end()) {
            if (scene && it->second >= 0) {
                scene->SetInstanceVisible(it->second, false);
            }
            unitToVisualIdx.erase(it);
        }
        unitVisualRegistrationFailed.erase(handleId);
        unitVisualMovingStates.erase(handleId);
        unitVisualLastPositions.erase(handleId);
        unitPendingAnimations.erase(handleId);
        unitQueuedAnimations.erase(handleId);
        unitScriptedAnimations.erase(handleId);
        unitVisualAnimationModes.erase(handleId);
        unitVisualLastAttackAnimationTicks.erase(handleId);
        unitVisualResourceTags.erase(handleId);
        unitVisualConstructionStates.erase(handleId);
        unitVisualPropertyTags.erase(handleId);
        unitAnimationPropertyTags.erase(handleId);
        removedUnitNotificationsCount_++;
    }

    // Reactive cleanup handler invoked automatically on `onItemRemoved` signal
    void OnItemRemoved(int handleId, SceneManager* scene = nullptr) {
        auto it = itemToVisualIdx.find(handleId);
        if (it != itemToVisualIdx.end()) {
            if (scene && it->second >= 0) {
                scene->SetInstanceVisible(it->second, false);
            }
            itemToVisualIdx.erase(it);
        }
        itemVisualRegistrationFailed.erase(handleId);
        removedItemNotificationsCount_++;
    }

    void ClearAll() {
        unitVisualSeeds.clear();
        unitPendingAnimations.clear();
        unitToVisualIdx.clear();
        unitVisualLastPositions.clear();
        unitVisualMovingStates.clear();
        unitVisualAnimationModes.clear();
        unitVisualLastAttackAnimationTicks.clear();
        unitVisualResourceTags.clear();
        unitVisualConstructionStates.clear();
        unitVisualPropertyTags.clear();
        unitScriptedAnimations.clear();
        unitQueuedAnimations.clear();
        unitAnimationPropertyTags.clear();
        unitVisualRegistrationFailed.clear();
        itemToVisualIdx.clear();
        effectVisuals.clear();
        itemVisualRegistrationFailed.clear();
        interpolationBuffer_.Clear();
    }

    // Stage A Modernization: Fixed-tick simulation to high-refresh display interpolation
    void CaptureSimulationTick(const CSimulation& sim) {
        interpolationBuffer_.CaptureSimulationTick(sim);
    }

    void SetInterpolationAlpha(float alpha) {
        interpolationAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
    }

    float GetInterpolationAlpha() const { return interpolationAlpha_; }

    render::RenderInterpolationBuffer& InterpolationBuffer() { return interpolationBuffer_; }
    const render::RenderInterpolationBuffer& InterpolationBuffer() const { return interpolationBuffer_; }

    bool SampleInterpolatedUnit(int handleId, render::RenderTransformSnapshot& outSnap) const {
        return interpolationBuffer_.SampleEntity(handleId, interpolationAlpha_, outSnap);
    }

    // Query helpers
    int GetUnitVisualIndex(int handleId) const {
        auto it = unitToVisualIdx.find(handleId);
        return (it != unitToVisualIdx.end()) ? it->second : -1;
    }

    int GetItemVisualIndex(int handleId) const {
        auto it = itemToVisualIdx.find(handleId);
        return (it != itemToVisualIdx.end()) ? it->second : -1;
    }

    void RequestUnitAnimationByName(int handleId, const std::string& animName, float speedRatio = 1.0f) {
        animation_utils::PendingUnitAnimation req;
        req.sequenceName = animName;
        req.speedRatio = speedRatio;
        unitQueuedAnimations.erase(handleId);
        unitPendingAnimations[handleId] = std::move(req);
    }

    void QueueUnitAnimation(int handleId, const animation_utils::PendingUnitAnimation& req) {
        unitQueuedAnimations[handleId].push_back(req);
    }

    bool SetUnitSecondaryAnimationTag(int handleId, const std::string& tag, bool add) {
        auto& activeTags = unitAnimationPropertyTags[handleId];
        bool changed = false;
        if (add) {
            if (std::find(activeTags.begin(), activeTags.end(), tag) == activeTags.end()) {
                activeTags.push_back(tag);
                changed = true;
            }
        } else {
            const auto oldSize = activeTags.size();
            activeTags.erase(std::remove(activeTags.begin(), activeTags.end(), tag), activeTags.end());
            changed = activeTags.size() != oldSize;
            if (activeTags.empty()) unitAnimationPropertyTags.erase(handleId);
        }
        if (changed) unitVisualAnimationModes.erase(handleId);
        return changed;
    }

    void ClearUnitAnimations(int handleId) {
        unitPendingAnimations.erase(handleId);
        unitQueuedAnimations.erase(handleId);
        unitScriptedAnimations.erase(handleId);
        unitVisualAnimationModes.erase(handleId);
    }

    size_t TrackedUnitCount() const { return unitToVisualIdx.size(); }
    size_t TrackedItemCount() const { return itemToVisualIdx.size(); }
    uint64_t GetRemovedUnitNotificationsCount() const { return removedUnitNotificationsCount_; }
    uint64_t GetRemovedItemNotificationsCount() const { return removedItemNotificationsCount_; }

    // Encapsulated presentation state maps
    std::unordered_map<int, UnitVisualSeed> unitVisualSeeds;
    std::unordered_map<int, animation_utils::PendingUnitAnimation> unitPendingAnimations;
    std::unordered_map<int, int> unitToVisualIdx;
    std::unordered_map<int, std::pair<float, float>> unitVisualLastPositions;
    std::unordered_map<int, bool> unitVisualMovingStates;
    std::unordered_map<int, animation_utils::UnitVisualAnimationMode> unitVisualAnimationModes;
    std::unordered_map<int, int> unitVisualLastAttackAnimationTicks;
    std::unordered_map<int, CResourceType> unitVisualResourceTags;
    std::unordered_map<int, bool> unitVisualConstructionStates;
    std::unordered_map<int, std::vector<std::string>> unitVisualPropertyTags;
    std::unordered_map<int, animation_utils::ScriptedUnitAnimationState> unitScriptedAnimations;
    std::unordered_map<int, std::deque<animation_utils::PendingUnitAnimation>> unitQueuedAnimations;
    std::unordered_map<int, std::vector<std::string>> unitAnimationPropertyTags;
    std::unordered_set<int> unitVisualRegistrationFailed;
    std::unordered_map<int, int> itemToVisualIdx;
    std::unordered_map<int, int> effectVisuals;
    std::unordered_set<int> itemVisualRegistrationFailed;

private:
    render::RenderInterpolationBuffer interpolationBuffer_;
    float interpolationAlpha_ = 1.0f;
    CSimulation* boundSim_ = nullptr;
    SignalConnectionId connUnitRemoved_ = 0;
    SignalConnectionId connItemRemoved_ = 0;
    uint64_t removedUnitNotificationsCount_ = 0;
    uint64_t removedItemNotificationsCount_ = 0;
};

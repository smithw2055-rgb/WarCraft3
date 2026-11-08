#pragma once
// ============================================================================
// Stage O2 & Phase 28: UIDataBinder — Reactive MVVM Data-Binding Pipeline
// for UIFrame Tree with Pluggable Upkeep Formatting & Custom Property Bindings
// (Inspired by Godot Control Signals & Unity UI Toolkit Data Binding)
// ============================================================================
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/csimulation.h"
#include "ui/TextFrame.h"
#include "ui/UIFrame.h"

namespace ui {

struct HudBoundStateSnapshot {
    bool initialized = false;
    int gold = -1;
    int lumber = -1;
    int foodUsed = -1;
    int foodCap = -1;
    int upkeepLevel = -1;
    std::string upkeepLabel;
    int selectedUnitHandle = -1;
    std::string selectedUnitName;
    int selectedUnitHp = -1;
    int selectedUnitMaxHp = -1;
    int selectedUnitMana = -1;
    int selectedUnitMaxMana = -1;
};

struct UIDataBinderStats {
    uint64_t totalSyncCalls = 0;
    uint64_t dirtyFieldUpdates = 0;
    uint64_t skippedRedundantFieldChecks = 0;
    uint64_t layoutPassesTriggered = 0;
};

class UIDataBinder {
public:
    using CustomTextPropertyFn = std::function<std::string(const CSimulation& sim, int localPlayerIndex, int selectedUnitHandle)>;

    UIDataBinder() = default;

    void BindResourceFrames(TextFrame* goldFrame,
                            TextFrame* lumberFrame,
                            TextFrame* foodFrame,
                            TextFrame* upkeepFrame = nullptr);

    void BindSelectionFrames(TextFrame* unitNameFrame,
                             TextFrame* unitHpFrame,
                             TextFrame* unitManaFrame);

    // Phase 28: Bind any custom reactive text property (e.g. wave timer, kill score, custom resource)
    void BindCustomTextProperty(const std::string& propKey,
                                TextFrame* frame,
                                CustomTextPropertyFn getter);
    bool UnbindCustomTextProperty(const std::string& propKey);
    size_t GetCustomPropertyCount() const { return customProperties_.size(); }

    // Evaluates current player, selected unit & custom properties against `lastState_`.
    // Only mutates TextFrame nodes whose bound value actually changed, and
    // triggers `rootFrame->PositionBounds(gui)` only when a bound frame dirtied layout!
    bool SyncAndRelayoutIfChanged(const CSimulation& sim,
                                  int localPlayerIndex,
                                  int selectedUnitHandle,
                                  GameUI& gui);

    void InvalidateAll() {
        lastState_ = HudBoundStateSnapshot{};
        for (auto& cp : customProperties_) {
            cp.initialized = false;
            cp.lastValue.clear();
        }
    }
    const HudBoundStateSnapshot& GetLastSnapshot() const { return lastState_; }
    const UIDataBinderStats& GetStats() const { return stats_; }

private:
    struct CustomBoundProperty {
        std::string key;
        TextFrame* frame = nullptr;
        CustomTextPropertyFn getter;
        std::string lastValue;
        bool initialized = false;
    };

    void ApplyTextFrameUpdate(TextFrame* frame, const std::string& text, bool& inoutAnyChanged);

    TextFrame* goldFrame_ = nullptr;
    TextFrame* lumberFrame_ = nullptr;
    TextFrame* foodFrame_ = nullptr;
    TextFrame* upkeepFrame_ = nullptr;
    TextFrame* unitNameFrame_ = nullptr;
    TextFrame* unitHpFrame_ = nullptr;
    TextFrame* unitManaFrame_ = nullptr;
    std::vector<CustomBoundProperty> customProperties_;

    HudBoundStateSnapshot lastState_{};
    UIDataBinderStats stats_{};
};

} // namespace ui

#pragma once
// ============================================================================
// Stage O2, Phase 28 & Stage 4: UIDataBinder — Reactive MVVM Data-Binding
// Pipeline Powered by `engine::ui::HudViewModel` & `HudViewBinder`
//
// `UIDataBinder` implements `engine::ui::IHudViewListener` and subscribes to
// `hudBinder_` so every resource, upkeep, selection, vital, and custom property
// update flows reactively through `HudViewModel -> HudViewBinder::Dispatch ->
// IHudViewListener` before mutating dirty `TextFrame` nodes.
// ============================================================================
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ecs/csimulation.h"
#include "engine/ui/hud_view_model.h"
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

class UIDataBinder final : public engine::ui::IHudViewListener {
public:
    using CustomTextPropertyFn =
        std::function<std::string(const CSimulation& sim, int localPlayerIndex, int selectedUnitHandle)>;

    UIDataBinder();
    ~UIDataBinder() override;

    UIDataBinder(const UIDataBinder&) = delete;
    UIDataBinder& operator=(const UIDataBinder&) = delete;

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

    // Subscribe additional external MVVM listeners (e.g., ConsoleHUD / HUD telemetry)
    void AttachHudListener(engine::ui::IHudViewListener* listener) {
        hudBinder_.AttachListener(listener);
    }
    void DetachHudListener(engine::ui::IHudViewListener* listener) {
        if (listener != this) {
            hudBinder_.DetachListener(listener);
        }
    }

    const engine::ui::HudViewModel& GetHudViewModel() const { return hudViewModel_; }
    const engine::ui::HudViewBinder& GetHudViewBinder() const { return hudBinder_; }

    // Evaluates current player, selected unit & custom properties into `hudViewModel_`,
    // dispatches dirty flags through `hudBinder_`, mutates only changed `TextFrame` nodes,
    // and triggers `rootFrame->PositionBounds(gui)` only when layout was dirtied.
    bool SyncAndRelayoutIfChanged(const CSimulation& sim,
                                  int localPlayerIndex,
                                  int selectedUnitHandle,
                                  GameUI& gui);

    void InvalidateAll() {
        lastState_ = HudBoundStateSnapshot{};
        hudViewModel_ = engine::ui::HudViewModel{};
        for (auto& cp : customProperties_) {
            cp.initialized = false;
            cp.lastValue.clear();
        }
    }

    const HudBoundStateSnapshot& GetLastSnapshot() const { return lastState_; }
    const UIDataBinderStats& GetStats() const { return stats_; }

    // engine::ui::IHudViewListener callbacks invoked during `hudBinder_.Dispatch(hudViewModel_)`
    void OnHudResourcesChanged(const engine::ui::HudViewModel& vm) override;
    void OnHudUpkeepChanged(const engine::ui::HudViewModel& vm) override;
    void OnHudSelectionChanged(const engine::ui::HudViewModel& vm) override;
    void OnHudVitalsChanged(const engine::ui::HudViewModel& vm) override;
    void OnHudCustomPropertiesChanged(const engine::ui::HudViewModel& vm) override;

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

    engine::ui::HudViewModel hudViewModel_;
    engine::ui::HudViewBinder hudBinder_;
    bool dispatchAnyChanged_ = false;
    bool currentHasSelectedUnit_ = false;

    HudBoundStateSnapshot lastState_{};
    UIDataBinderStats stats_{};
};

} // namespace ui

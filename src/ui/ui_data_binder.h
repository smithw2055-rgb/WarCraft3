#pragma once
// ============================================================================
// Stage O2: UIDataBinder — Reactive MVVM Data-Binding Pipeline for UIFrame Tree
//           (Inspired by Godot Control Signals & Unity UI Toolkit Data Binding)
// ============================================================================
#include <cstdint>
#include <string>

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
    UIDataBinder() = default;

    void BindResourceFrames(TextFrame* goldFrame,
                            TextFrame* lumberFrame,
                            TextFrame* foodFrame,
                            TextFrame* upkeepFrame = nullptr);

    void BindSelectionFrames(TextFrame* unitNameFrame,
                             TextFrame* unitHpFrame,
                             TextFrame* unitManaFrame);

    // Evaluates current player & selected unit state against `lastState_`.
    // Only mutates TextFrame nodes whose bound value actually changed, and
    // triggers `rootFrame->PositionBounds(gui)` only when a bound frame dirtied layout!
    bool SyncAndRelayoutIfChanged(const CSimulation& sim,
                                  int localPlayerIndex,
                                  int selectedUnitHandle,
                                  GameUI& gui);

    void InvalidateAll() { lastState_ = HudBoundStateSnapshot{}; }
    const HudBoundStateSnapshot& GetLastSnapshot() const { return lastState_; }
    const UIDataBinderStats& GetStats() const { return stats_; }

private:
    TextFrame* goldFrame_ = nullptr;
    TextFrame* lumberFrame_ = nullptr;
    TextFrame* foodFrame_ = nullptr;
    TextFrame* upkeepFrame_ = nullptr;
    TextFrame* unitNameFrame_ = nullptr;
    TextFrame* unitHpFrame_ = nullptr;
    TextFrame* unitManaFrame_ = nullptr;

    HudBoundStateSnapshot lastState_{};
    UIDataBinderStats stats_{};
};

} // namespace ui

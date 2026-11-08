#include "ui/ui_data_binder.h"

#include <algorithm>
#include <cmath>
#include "ecs/player.h"
#include "ecs/unit.h"

namespace ui {

void UIDataBinder::BindResourceFrames(TextFrame* goldFrame,
                                      TextFrame* lumberFrame,
                                      TextFrame* foodFrame,
                                      TextFrame* upkeepFrame) {
    goldFrame_ = goldFrame;
    lumberFrame_ = lumberFrame;
    foodFrame_ = foodFrame;
    upkeepFrame_ = upkeepFrame;
    InvalidateAll();
}

void UIDataBinder::BindSelectionFrames(TextFrame* unitNameFrame,
                                       TextFrame* unitHpFrame,
                                       TextFrame* unitManaFrame) {
    unitNameFrame_ = unitNameFrame;
    unitHpFrame_ = unitHpFrame;
    unitManaFrame_ = unitManaFrame;
    InvalidateAll();
}

void UIDataBinder::BindCustomTextProperty(const std::string& propKey,
                                          TextFrame* frame,
                                          CustomTextPropertyFn getter) {
    if (propKey.empty() || !getter) return;
    for (auto& cp : customProperties_) {
        if (cp.key == propKey) {
            cp.frame = frame;
            cp.getter = std::move(getter);
            cp.initialized = false;
            cp.lastValue.clear();
            return;
        }
    }
    customProperties_.push_back({propKey, frame, std::move(getter), "", false});
}

bool UIDataBinder::UnbindCustomTextProperty(const std::string& propKey) {
    auto it = std::remove_if(customProperties_.begin(), customProperties_.end(),
                             [&](const CustomBoundProperty& cp) { return cp.key == propKey; });
    if (it == customProperties_.end()) return false;
    customProperties_.erase(it, customProperties_.end());
    return true;
}

void UIDataBinder::ApplyTextFrameUpdate(TextFrame* frame,
                                        const std::string& text,
                                        bool& inoutAnyChanged) {
    if (frame) {
        frame->SetText(text);
        frame->MarkLayoutDirty();
    }
    stats_.dirtyFieldUpdates++;
    inoutAnyChanged = true;
}

bool UIDataBinder::SyncAndRelayoutIfChanged(const CSimulation& sim,
                                            int localPlayerIndex,
                                            int selectedUnitHandle,
                                            GameUI& gui) {
    stats_.totalSyncCalls++;
    bool anyChanged = false;

    const CPlayer* player = sim.GetPlayer(localPlayerIndex);
    const int gold = player ? player->GetGold() : 0;
    const int lumber = player ? player->GetLumber() : 0;
    const int foodUsed = player ? player->GetFoodUsed() : 0;
    const int foodCap = player ? player->GetFoodCap() : 0;
    const int upkeep = player ? player->GetUpkeepLevel() : 0;
    const auto* mod = sim.GetSimulationModule();
    const std::string upkeepLabel = mod
        ? mod->GetUpkeepDisplayLabel(upkeep)
        : ((upkeep == 2) ? "High Upkeep" : (upkeep == 1) ? "Low Upkeep" : "No Upkeep");

    if (!lastState_.initialized || gold != lastState_.gold) {
        lastState_.gold = gold;
        ApplyTextFrameUpdate(goldFrame_, std::to_string(gold), anyChanged);
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || lumber != lastState_.lumber) {
        lastState_.lumber = lumber;
        ApplyTextFrameUpdate(lumberFrame_, std::to_string(lumber), anyChanged);
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || foodUsed != lastState_.foodUsed || foodCap != lastState_.foodCap) {
        lastState_.foodUsed = foodUsed;
        lastState_.foodCap = foodCap;
        ApplyTextFrameUpdate(foodFrame_, std::to_string(foodUsed) + "/" + std::to_string(foodCap), anyChanged);
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || upkeep != lastState_.upkeepLevel || upkeepLabel != lastState_.upkeepLabel) {
        lastState_.upkeepLevel = upkeep;
        lastState_.upkeepLabel = upkeepLabel;
        ApplyTextFrameUpdate(upkeepFrame_, upkeepLabel, anyChanged);
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    const CUnit* sel = (selectedUnitHandle > 0) ? sim.GetUnit(selectedUnitHandle) : nullptr;
    const std::string unitName = (sel && sel->GetUnitType()) ? sel->GetUnitType()->GetName() : "";
    const int hp = sel ? static_cast<int>(std::round(sel->GetLife())) : 0;
    const int maxHp = sel ? static_cast<int>(std::round(sel->GetMaxLife())) : 0;
    const int mp = sel ? static_cast<int>(std::round(sel->GetMana())) : 0;
    const int maxMp = sel ? sel->GetMaxMana() : 0;

    if (!lastState_.initialized || selectedUnitHandle != lastState_.selectedUnitHandle ||
        unitName != lastState_.selectedUnitName) {
        lastState_.selectedUnitHandle = selectedUnitHandle;
        lastState_.selectedUnitName = unitName;
        ApplyTextFrameUpdate(unitNameFrame_, unitName, anyChanged);
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || hp != lastState_.selectedUnitHp || maxHp != lastState_.selectedUnitMaxHp) {
        lastState_.selectedUnitHp = hp;
        lastState_.selectedUnitMaxHp = maxHp;
        ApplyTextFrameUpdate(unitHpFrame_, sel ? (std::to_string(hp) + " / " + std::to_string(maxHp)) : "", anyChanged);
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || mp != lastState_.selectedUnitMana || maxMp != lastState_.selectedUnitMaxMana) {
        lastState_.selectedUnitMana = mp;
        lastState_.selectedUnitMaxMana = maxMp;
        ApplyTextFrameUpdate(unitManaFrame_, (sel && maxMp > 0) ? (std::to_string(mp) + " / " + std::to_string(maxMp)) : "", anyChanged);
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    for (auto& cp : customProperties_) {
        if (!cp.getter) continue;
        const std::string val = cp.getter(sim, localPlayerIndex, selectedUnitHandle);
        if (!cp.initialized || val != cp.lastValue) {
            cp.initialized = true;
            cp.lastValue = val;
            ApplyTextFrameUpdate(cp.frame, val, anyChanged);
        } else {
            stats_.skippedRedundantFieldChecks++;
        }
    }

    lastState_.initialized = true;

    if (anyChanged && gui.rootFrame && gui.rootFrame->IsLayoutDirty()) {
        gui.rootFrame->PositionBounds(gui);
        stats_.layoutPassesTriggered++;
    }
    return anyChanged;
}

} // namespace ui

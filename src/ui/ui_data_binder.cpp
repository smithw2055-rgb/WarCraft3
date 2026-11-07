#include "ui/ui_data_binder.h"

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

    if (!lastState_.initialized || gold != lastState_.gold) {
        lastState_.gold = gold;
        if (goldFrame_) {
            goldFrame_->SetText(std::to_string(gold));
            goldFrame_->MarkLayoutDirty();
        }
        stats_.dirtyFieldUpdates++;
        anyChanged = true;
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || lumber != lastState_.lumber) {
        lastState_.lumber = lumber;
        if (lumberFrame_) {
            lumberFrame_->SetText(std::to_string(lumber));
            lumberFrame_->MarkLayoutDirty();
        }
        stats_.dirtyFieldUpdates++;
        anyChanged = true;
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || foodUsed != lastState_.foodUsed || foodCap != lastState_.foodCap) {
        lastState_.foodUsed = foodUsed;
        lastState_.foodCap = foodCap;
        if (foodFrame_) {
            foodFrame_->SetText(std::to_string(foodUsed) + "/" + std::to_string(foodCap));
            foodFrame_->MarkLayoutDirty();
        }
        stats_.dirtyFieldUpdates++;
        anyChanged = true;
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || upkeep != lastState_.upkeepLevel) {
        lastState_.upkeepLevel = upkeep;
        if (upkeepFrame_) {
            const char* label = (upkeep == 2) ? "High Upkeep" : (upkeep == 1) ? "Low Upkeep" : "No Upkeep";
            upkeepFrame_->SetText(label);
            upkeepFrame_->MarkLayoutDirty();
        }
        stats_.dirtyFieldUpdates++;
        anyChanged = true;
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
        if (unitNameFrame_) {
            unitNameFrame_->SetText(unitName);
            unitNameFrame_->MarkLayoutDirty();
        }
        stats_.dirtyFieldUpdates++;
        anyChanged = true;
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || hp != lastState_.selectedUnitHp || maxHp != lastState_.selectedUnitMaxHp) {
        lastState_.selectedUnitHp = hp;
        lastState_.selectedUnitMaxHp = maxHp;
        if (unitHpFrame_) {
            unitHpFrame_->SetText(sel ? (std::to_string(hp) + " / " + std::to_string(maxHp)) : "");
            unitHpFrame_->MarkLayoutDirty();
        }
        stats_.dirtyFieldUpdates++;
        anyChanged = true;
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    if (!lastState_.initialized || mp != lastState_.selectedUnitMana || maxMp != lastState_.selectedUnitMaxMana) {
        lastState_.selectedUnitMana = mp;
        lastState_.selectedUnitMaxMana = maxMp;
        if (unitManaFrame_) {
            unitManaFrame_->SetText((sel && maxMp > 0) ? (std::to_string(mp) + " / " + std::to_string(maxMp)) : "");
            unitManaFrame_->MarkLayoutDirty();
        }
        stats_.dirtyFieldUpdates++;
        anyChanged = true;
    } else {
        stats_.skippedRedundantFieldChecks++;
    }

    lastState_.initialized = true;

    if (anyChanged && gui.rootFrame && gui.rootFrame->IsLayoutDirty()) {
        gui.rootFrame->PositionBounds(gui);
        stats_.layoutPassesTriggered++;
    }
    return anyChanged;
}

} // namespace ui

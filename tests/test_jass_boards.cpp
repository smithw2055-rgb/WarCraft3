// Batch-6b native tests: leaderboard/multiboard setters, player board
// linkage, timer-dialog styling (ref board API).
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

struct Fixture {
    JassVM vm;
    CSimulation sim;
    std::unique_ptr<JassNatives> natives;
    std::shared_ptr<JassNativesData> data;

    Fixture() {
        sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
    }
};

} // namespace

bool TestLeaderboardNatives() {
    Fixture f;
    const int lb = CallNative(f.vm, "CreateLeaderboard", {}).intVal;
    TEST_ASSERT(lb != 0);
    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    const int p1 = CallNative(f.vm, "Player", {JassValue::Int(1)}).intVal;
    CallNative(f.vm, "LeaderboardAddItem", {JassValue::Int(lb), JassValue::Str("Bob"), JassValue::Int(30), JassValue::Int(p0)});
    CallNative(f.vm, "LeaderboardAddItem", {JassValue::Int(lb), JassValue::Str("Al"), JassValue::Int(50), JassValue::Int(p1)});

    // Label text + styles + colors round-trip through storage.
    TEST_ASSERT(CallNative(f.vm, "LeaderboardGetLabelText", {JassValue::Int(lb)}).strVal.empty());
    CallNative(f.vm, "LeaderboardSetStyle", {JassValue::Int(lb), JassValue::Bool(true), JassValue::Bool(false), JassValue::Bool(true), JassValue::Bool(false)});
    CallNative(f.vm, "LeaderboardSetLabelColor", {JassValue::Int(lb), JassValue::Int(1), JassValue::Int(2), JassValue::Int(3), JassValue::Int(4)});
    CallNative(f.vm, "LeaderboardSetValueColor", {JassValue::Int(lb), JassValue::Int(5), JassValue::Int(6), JassValue::Int(7), JassValue::Int(8)});
    CallNative(f.vm, "LeaderboardSetSizeByItemCount", {JassValue::Int(lb), JassValue::Int(2)});
    CallNative(f.vm, "LeaderboardSetItemLabel", {JassValue::Int(lb), JassValue::Int(0), JassValue::Str("Bobby")});
    CallNative(f.vm, "LeaderboardSetItemStyle", {JassValue::Int(lb), JassValue::Int(0), JassValue::Bool(true), JassValue::Bool(false), JassValue::Bool(true)});
    CallNative(f.vm, "LeaderboardSetItemLabelColor", {JassValue::Int(lb), JassValue::Int(0), JassValue::Int(9), JassValue::Int(9), JassValue::Int(9), JassValue::Int(9)});
    CallNative(f.vm, "LeaderboardSetItemValueColor", {JassValue::Int(lb), JassValue::Int(1), JassValue::Int(8), JassValue::Int(8), JassValue::Int(8), JassValue::Int(8)});
    {
        const int simId = f.data->GetSimHandle(lb, JassHandleInfo::LEADERBOARD);
        auto it = f.data->leaderboards.find(simId);
        TEST_ASSERT(it != f.data->leaderboards.end());
        TEST_ASSERT(!it->second->showNames && it->second->showValues);
        TEST_ASSERT(it->second->labelColor[0] == 1 && it->second->valueColor[3] == 8);
        TEST_ASSERT(it->second->sizeByItemCount == 2);
        TEST_ASSERT(it->second->items[0].label == "Bobby");
        TEST_ASSERT(!it->second->items[0].showValue && it->second->items[0].showIcon);
        TEST_ASSERT(it->second->items[0].labelColor[0] == 9);
        TEST_ASSERT(it->second->items[1].valueColor[0] == 8);
    }

    // Sorts reorder; removal drops by index.
    CallNative(f.vm, "LeaderboardSortItemsByValue", {JassValue::Int(lb), JassValue::Bool(true)});
    {
        const int simId = f.data->GetSimHandle(lb, JassHandleInfo::LEADERBOARD);
        auto& items = f.data->leaderboards[simId]->items;
        TEST_ASSERT(items[0].value == 30 && items[1].value == 50);
    }
    CallNative(f.vm, "LeaderboardSortItemsByLabel", {JassValue::Int(lb), JassValue::Bool(false)});
    {
        const int simId = f.data->GetSimHandle(lb, JassHandleInfo::LEADERBOARD);
        auto& items = f.data->leaderboards[simId]->items;
        TEST_ASSERT(items[0].label == "Bobby");
    }
    CallNative(f.vm, "LeaderboardRemoveItem", {JassValue::Int(lb), JassValue::Int(0)});
    {
        const int simId = f.data->GetSimHandle(lb, JassHandleInfo::LEADERBOARD);
        TEST_ASSERT(f.data->leaderboards[simId]->items.size() == 1);
    }
    // Out-of-range removal is safe.
    CallNative(f.vm, "LeaderboardRemoveItem", {JassValue::Int(lb), JassValue::Int(9)});

    // Player linkage round-trips through a fresh handle.
    CallNative(f.vm, "PlayerSetLeaderboard", {JassValue::Int(p0), JassValue::Int(lb)});
    const int back = CallNative(f.vm, "PlayerGetLeaderboard", {JassValue::Int(p0)}).intVal;
    TEST_ASSERT(back != 0);
    TEST_ASSERT(f.data->GetSimHandle(back, JassHandleInfo::LEADERBOARD) ==
                f.data->GetSimHandle(lb, JassHandleInfo::LEADERBOARD));
    TEST_ASSERT(CallNative(f.vm, "PlayerGetLeaderboard", {JassValue::Int(p1)}).type == JassValue::NULL_VAL);
    return true;
}

bool TestMultiboardNatives() {
    Fixture f;
    const int mb = CallNative(f.vm, "CreateMultiboard", {}).intVal;
    TEST_ASSERT(mb != 0);
    CallNative(f.vm, "MultiboardSetRowCount", {JassValue::Int(mb), JassValue::Int(2)});
    CallNative(f.vm, "MultiboardSetColumnCount", {JassValue::Int(mb), JassValue::Int(2)});
    const int item = CallNative(f.vm, "MultiboardGetItem",
        {JassValue::Int(mb), JassValue::Int(0), JassValue::Int(1)}).intVal;
    TEST_ASSERT(item != 0);

    CallNative(f.vm, "MultiboardSetTitleTextColor", {JassValue::Int(mb), JassValue::Int(1), JassValue::Int(2), JassValue::Int(3), JassValue::Int(4)});
    CallNative(f.vm, "MultiboardSetItemsStyle", {JassValue::Int(mb), JassValue::Bool(false), JassValue::Bool(true)});
    CallNative(f.vm, "MultiboardSetItemsValue", {JassValue::Int(mb), JassValue::Str("v")});
    CallNative(f.vm, "MultiboardSetItemsWidth", {JassValue::Int(mb), JassValue::Real(0.05f)});
    CallNative(f.vm, "MultiboardSetItemsIcon", {JassValue::Int(mb), JassValue::Str("icon.blp")});
    CallNative(f.vm, "MultiboardSetItemsValueColor",
        {JassValue::Int(mb), JassValue::Int(7), JassValue::Int(7), JassValue::Int(7), JassValue::Int(7)});
    {
        const int simId = f.data->GetSimHandle(mb, JassHandleInfo::MULTIBOARD);
        auto bit = f.data->multiboards.find(simId);
        TEST_ASSERT(bit != f.data->multiboards.end());
        TEST_ASSERT(bit->second->titleColor[0] == 7); // broadcast proxy
        const int itemSim = f.data->GetSimHandle(item, JassHandleInfo::MULTIBOARDITEM);
        auto iit = f.data->multiboardItems.find(itemSim);
        TEST_ASSERT(iit != f.data->multiboardItems.end());
        TEST_ASSERT(!iit->second->showValue && iit->second->showIcon);
        TEST_ASSERT(iit->second->value == "v");
        TEST_ASSERT(iit->second->width == 0.05f);
        TEST_ASSERT(iit->second->icon == "icon.blp");
    }
    // Per-item color validates the handle, styling lands with the item pass.
    CallNative(f.vm, "MultiboardSetItemValueColor",
        {JassValue::Int(item), JassValue::Int(1), JassValue::Int(1), JassValue::Int(1), JassValue::Int(1)});
    CallNative(f.vm, "MultiboardSetItemValueColor", {JassValue::Int(999999), JassValue::Int(1), JassValue::Int(1), JassValue::Int(1), JassValue::Int(1)});
    return true;
}

bool TestTimerDialogStyleNatives() {
    Fixture f;
    const int timer = CallNative(f.vm, "CreateTimer", {}).intVal;
    const int dialog = CallNative(f.vm, "CreateTimerDialog", {JassValue::Int(timer)}).intVal;
    TEST_ASSERT(dialog != 0);
    TEST_ASSERT(!CallNative(f.vm, "IsTimerDialogDisplayed", {JassValue::Int(dialog)}).boolVal);
    CallNative(f.vm, "TimerDialogDisplay", {JassValue::Int(dialog), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "IsTimerDialogDisplayed", {JassValue::Int(dialog)}).boolVal);
    CallNative(f.vm, "TimerDialogSetTitleColor", {JassValue::Int(dialog), JassValue::Int(1), JassValue::Int(2), JassValue::Int(3), JassValue::Int(4)});
    CallNative(f.vm, "TimerDialogSetTimeColor", {JassValue::Int(dialog), JassValue::Int(5), JassValue::Int(6), JassValue::Int(7), JassValue::Int(8)});
    CallNative(f.vm, "TimerDialogSetSpeed", {JassValue::Int(dialog), JassValue::Real(2.5f)});
    {
        const int simId = f.data->GetSimHandle(dialog, JassHandleInfo::TIMER_DIALOG);
        auto it = f.data->timerDialogs.find(simId);
        TEST_ASSERT(it != f.data->timerDialogs.end());
        TEST_ASSERT(it->second->titleColor[0] == 1 && it->second->titleColor[3] == 4);
        TEST_ASSERT(it->second->timeColor[0] == 5 && it->second->timeColor[3] == 8);
        TEST_ASSERT(it->second->speed == 2.5f);
    }
    return true;
}

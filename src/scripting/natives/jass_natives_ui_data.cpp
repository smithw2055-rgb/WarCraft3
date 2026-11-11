#include "scripting/jass_natives_internal.h"
#include "ui/UIFrame.h"
#include "ui/SimpleFrame.h"
#include "ui/ButtonFrame.h"
#include "ui/TextFrame.h"
#include "ui/BackdropFrame.h"
void JassNatives::RegisterPoolStockNatives() {
    vm_.RegisterNative("CreateItemPool", [data = data_](const std::vector<JassValue>&) -> JassValue { auto pool = std::make_shared<JassNativesData::JassWeightedPool>(); int id = data->nextItemPoolId++; data->itemPools[id] = pool; return JassValue::Int(id); });
    vm_.RegisterNative("DestroyItemPool", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty()) data->itemPools.erase(args[0].intVal); return JassValue::Null(); });
    vm_.RegisterNative("ItemPoolAddItemType", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); auto it = data->itemPools.find(args[0].intVal); if (it == data->itemPools.end()) return JassValue::Null(); it->second->entries.emplace_back((uint32_t)args[1].intVal, args[2].realVal); return JassValue::Null(); });
    vm_.RegisterNative("ItemPoolRemoveItemType", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); auto it = data->itemPools.find(args[0].intVal); if (it == data->itemPools.end()) return JassValue::Null(); auto& entries = it->second->entries; entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto& e) { return e.first == (uint32_t)args[1].intVal; }), entries.end()); return JassValue::Null(); });
    vm_.RegisterNative("PlaceRandomItem", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Null();
        auto it = data->itemPools.find(args[0].intVal);
        if (it == data->itemPools.end()) return JassValue::Null();
        const uint32_t picked = PickWeightedPoolEntry(it->second->entries, sim->SimRand01());
        if (picked == 0) return JassValue::Null();
        return JassValue::Int(data->CreateItemHandle(sim, War3ID(picked), args[1].realVal, args[2].realVal));
    });
    vm_.RegisterNative("CreateUnitPool", [data = data_](const std::vector<JassValue>&) -> JassValue { auto pool = std::make_shared<JassNativesData::JassWeightedPool>(); int id = data->nextUnitPoolId++; data->unitPools[id] = pool; return JassValue::Int(id); });
    vm_.RegisterNative("DestroyUnitPool", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty()) data->unitPools.erase(args[0].intVal); return JassValue::Null(); });
    vm_.RegisterNative("UnitPoolAddUnitType", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); auto it = data->unitPools.find(args[0].intVal); if (it == data->unitPools.end()) return JassValue::Null(); it->second->entries.emplace_back((uint32_t)args[1].intVal, args[2].realVal); return JassValue::Null(); });
    vm_.RegisterNative("UnitPoolRemoveUnitType", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); auto it = data->unitPools.find(args[0].intVal); if (it == data->unitPools.end()) return JassValue::Null(); auto& entries = it->second->entries; entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto& e) { return e.first == (uint32_t)args[1].intVal; }), entries.end()); return JassValue::Null(); });
    vm_.RegisterNative("PlaceRandomUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5 || !sim) return JassValue::Null();
        auto it = data->unitPools.find(args[0].intVal);
        if (it == data->unitPools.end()) return JassValue::Null();
        const uint32_t picked = PickWeightedPoolEntry(it->second->entries, sim->SimRand01());
        if (picked == 0) return JassValue::Null();
        CUnit* unit = sim->CreateUnit(War3ID(picked), ResolvePlayerId(data, args[1].intVal),
                                      args[2].realVal, args[3].realVal, args[4].realVal);
        if (!unit) return JassValue::Null();
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
    });
    vm_.RegisterNative("ChooseRandomCreep", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.empty() || !sim) return JassValue::Int(0);
        const CUnitData* unitData = sim->GetUnitData();
        if (!unitData) return JassValue::Int(0);
        std::vector<uint32_t> candidates;
        for (const auto& [id, type] : unitData->GetAllTypes()) {
            if (!type || type->IsBuilding() || type->IsHero()) continue;
            if (type->GetLevel() != args[0].intVal) continue;
            const std::string& race = type->GetUnitRace().name;
            if (race == "creeps" || race == "demon" || race == "naga") { candidates.push_back(id.value); }
        }
        if (candidates.empty()) return JassValue::Int(0);
        return JassValue::Int((int)candidates[(size_t)(sim->SimRand01() * (float)candidates.size()) % candidates.size()]);
    });
    vm_.RegisterNative("ChooseRandomNPBuilding", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        (void)args; (void)data;
        if (!sim) return JassValue::Int(0);
        const CUnitData* unitData = sim->GetUnitData();
        if (!unitData) return JassValue::Int(0);
        std::vector<uint32_t> candidates;
        for (const auto& [id, type] : unitData->GetAllTypes()) {
            if (!type || !type->IsBuilding()) continue;
            if (type->GetUnitRace().name == "other") { candidates.push_back(id.value); }
        }
        if (candidates.empty()) return JassValue::Int(0);
        return JassValue::Int((int)candidates[(size_t)(sim->SimRand01() * (float)candidates.size()) % candidates.size()]);
    });
    auto findShop = [data = data_, sim = sim_](int unitHandle) -> CAbility* { return FindUnitShop(data, sim, unitHandle); };
    auto allShops = [sim = sim_]() -> std::vector<CAbility*> { return FindAllWorldShops(sim); };
    vm_.RegisterNative("AddItemToStock", [findShop](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto* shop = findShop(args[0].intVal)) { shop->AddStockItem(War3ID((uint32_t)args[1].intVal), args[2].intVal, args[3].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("AddItemToAllStock", [data = data_, sim = sim_, allShops](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); const uint32_t id = (uint32_t)args[0].intVal; data->globalShopStock.push_back({id, args[1].intVal, args[2].intVal, false}); if (sim) sim->AddGlobalShopStock(War3ID(id), args[1].intVal, args[2].intVal, false); for (auto* shop : allShops()) shop->AddStockItem(War3ID(id), args[1].intVal, args[2].intVal); return JassValue::Null(); });
    vm_.RegisterNative("RemoveItemFromStock", [findShop](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* shop = findShop(args[0].intVal)) { shop->RemoveStock(War3ID((uint32_t)args[1].intVal)); } return JassValue::Null(); });
    vm_.RegisterNative("RemoveItemFromAllStock", [data = data_, sim = sim_, allShops](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const uint32_t id = (uint32_t)args[0].intVal;
        auto& global = data->globalShopStock;
        global.erase(std::remove_if(global.begin(), global.end(), [&](const auto& e) {
            return e.id == id && !e.isUnit;
        }), global.end());
        if (sim) sim->RemoveGlobalShopStock(War3ID(id), false);
        for (auto* shop : allShops()) shop->RemoveStock(War3ID(id));
        return JassValue::Null();
    });
    vm_.RegisterNative("AddUnitToStock", [findShop](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto* shop = findShop(args[0].intVal)) { shop->AddStockUnit(War3ID((uint32_t)args[1].intVal), args[2].intVal, args[3].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("AddUnitToAllStock", [data = data_, sim = sim_, allShops](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); const uint32_t id = (uint32_t)args[0].intVal; data->globalShopStock.push_back({id, args[1].intVal, args[2].intVal, true}); if (sim) sim->AddGlobalShopStock(War3ID(id), args[1].intVal, args[2].intVal, true); for (auto* shop : allShops()) shop->AddStockUnit(War3ID(id), args[1].intVal, args[2].intVal); return JassValue::Null(); });
    vm_.RegisterNative("RemoveUnitFromStock", [findShop](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto* shop = findShop(args[0].intVal)) { shop->RemoveStock(War3ID((uint32_t)args[1].intVal)); } return JassValue::Null(); });
    vm_.RegisterNative("RemoveUnitFromAllStock", [data = data_, sim = sim_, allShops](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const uint32_t id = (uint32_t)args[0].intVal;
        auto& global = data->globalShopStock;
        global.erase(std::remove_if(global.begin(), global.end(), [&](const auto& e) {
            return e.id == id && e.isUnit;
        }), global.end());
        if (sim) sim->RemoveGlobalShopStock(War3ID(id), true);
        for (auto* shop : allShops()) shop->RemoveStock(War3ID(id));
        return JassValue::Null();
    });
    vm_.RegisterNative("AddResourceAmount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); const int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::DESTRUCTABLE); if (simId >= 0) { if (auto* destructable = sim->GetDestructable(simId)) { destructable->SetLife(*sim, destructable->GetLife() + (float)args[1].intVal); } } return JassValue::Null(); });
}
void JassNatives::RegisterBoardNatives() {
    auto getBoard = [data = data_](int boardHandle) -> std::shared_ptr<JassNativesData::JassLeaderboard> { int simId = data->GetSimHandle(boardHandle, JassHandleInfo::LEADERBOARD); if (simId < 0) return nullptr; auto it = data->leaderboards.find(simId); return it != data->leaderboards.end() ? it->second : nullptr; };
    auto boardItem = [](const std::shared_ptr<JassNativesData::JassLeaderboard>& board, int index)
        -> JassNativesData::JassLeaderboardItem* {
        if (!board || index < 0 || index >= (int)board->items.size()) return nullptr;
        return &board->items[(size_t)index];
    };
    vm_.RegisterNative("LeaderboardGetLabelText", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Str(""); auto board = getBoard(args[0].intVal); return JassValue::Str(board ? board->label : ""); });
    vm_.RegisterNative("LeaderboardRemoveItem", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) { const int index = args[1].intVal; if (index >= 0 && index < (int)board->items.size()) { board->items.erase(board->items.begin() + index); } } return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSetItemLabel", [getBoard, boardItem](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto* item = boardItem(getBoard(args[0].intVal), args[1].intVal)) { item->label = args[2].strVal; } return JassValue::Null(); });
    auto itemColor = [getBoard, boardItem](const std::vector<JassValue>& args, bool label) -> JassValue { if (args.size() < 6) return JassValue::Null(); if (auto* item = boardItem(getBoard(args[0].intVal), args[1].intVal)) { int* color = label ? item->labelColor : item->valueColor; color[0] = args[2].intVal; color[1] = args[3].intVal; color[2] = args[4].intVal; color[3] = args[5].intVal; } return JassValue::Null(); };
    vm_.RegisterNative("LeaderboardSetItemLabelColor", [itemColor](const std::vector<JassValue>& args) -> JassValue { return itemColor(args, true); });
    vm_.RegisterNative("LeaderboardSetItemValueColor", [itemColor](const std::vector<JassValue>& args) -> JassValue { return itemColor(args, false); });
    vm_.RegisterNative("LeaderboardSetItemStyle", [getBoard, boardItem](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto* item = boardItem(getBoard(args[0].intVal), args[1].intVal)) { item->showLabel = args[2].boolVal; item->showValue = args[3].boolVal; item->showIcon = args[4].boolVal; } return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSetLabelColor", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) { board->labelColor[0] = args[1].intVal; board->labelColor[1] = args[2].intVal; board->labelColor[2] = args[3].intVal; board->labelColor[3] = args[4].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSetValueColor", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) { board->valueColor[0] = args[1].intVal; board->valueColor[1] = args[2].intVal; board->valueColor[2] = args[3].intVal; board->valueColor[3] = args[4].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSetSizeByItemCount", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) board->sizeByItemCount = args[1].intVal; return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSetStyle", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) { board->showLabel = args[1].boolVal; board->showNames = args[2].boolVal; board->showValues = args[3].boolVal; board->showIcons = args[4].boolVal; } return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSortItemsByValue", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) { const bool asc = args[1].boolVal; std::stable_sort(board->items.begin(), board->items.end(), [asc](const auto& a, const auto& b) { return asc ? a.value < b.value : a.value > b.value; }); } return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSortItemsByPlayer", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) { const bool asc = args[1].boolVal; std::stable_sort(board->items.begin(), board->items.end(), [asc](const auto& a, const auto& b) { return asc ? a.playerId < b.playerId : a.playerId > b.playerId; }); } return JassValue::Null(); });
    vm_.RegisterNative("LeaderboardSortItemsByLabel", [getBoard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getBoard(args[0].intVal)) { const bool asc = args[1].boolVal; std::stable_sort(board->items.begin(), board->items.end(), [asc](const auto& a, const auto& b) { return asc ? a.label < b.label : a.label > b.label; }); } return JassValue::Null(); });
    vm_.RegisterNative("PlayerSetLeaderboard", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); const int player = ResolvePlayerId(data, args[0].intVal); const int boardSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::LEADERBOARD); if (player >= 0 && boardSim >= 0 && data->leaderboards.count(boardSim)) { data->playerLeaderboards[player] = boardSim; } return JassValue::Null(); });
    vm_.RegisterNative("PlayerGetLeaderboard", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int player = ResolvePlayerId(data, args[0].intVal); auto it = data->playerLeaderboards.find(player); if (it == data->playerLeaderboards.end()) return JassValue::Null(); return JassValue::Int(data->AllocHandle(JassHandleInfo::LEADERBOARD, it->second)); });
    auto getMultiboard = [data = data_](int boardHandle) -> std::shared_ptr<JassNativesData::JassMultiboard> { int simId = data->GetSimHandle(boardHandle, JassHandleInfo::MULTIBOARD); if (simId < 0) return nullptr; auto it = data->multiboards.find(simId); return it != data->multiboards.end() ? it->second : nullptr; };
    vm_.RegisterNative("MultiboardSetTitleTextColor", [getMultiboard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto board = getMultiboard(args[0].intVal)) { board->titleColor[0] = args[1].intVal; board->titleColor[1] = args[2].intVal; board->titleColor[2] = args[3].intVal; board->titleColor[3] = args[4].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemsStyle", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto board = getMultiboard(args[0].intVal)) { for (const auto& [cell, itemId] : board->items) { auto it = data->multiboardItems.find(itemId); if (it == data->multiboardItems.end()) continue; it->second->showValue = args[1].boolVal; it->second->showIcon = args[2].boolVal; } } return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemsValue", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getMultiboard(args[0].intVal)) { for (const auto& [cell, itemId] : board->items) { auto it = data->multiboardItems.find(itemId); if (it != data->multiboardItems.end()) it->second->value = args[1].strVal; } } return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemsValueColor", [getMultiboard](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto board = getMultiboard(args[0].intVal)) { board->titleColor[0] = args[1].intVal; board->titleColor[1] = args[2].intVal; board->titleColor[2] = args[3].intVal; board->titleColor[3] = args[4].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemsWidth", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getMultiboard(args[0].intVal)) { for (const auto& [cell, itemId] : board->items) { auto it = data->multiboardItems.find(itemId); if (it != data->multiboardItems.end()) it->second->width = args[1].realVal; } } return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemsIcon", [getMultiboard, data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto board = getMultiboard(args[0].intVal)) { for (const auto& [cell, itemId] : board->items) { auto it = data->multiboardItems.find(itemId); if (it != data->multiboardItems.end()) it->second->icon = args[1].strVal; } } return JassValue::Null(); });
    vm_.RegisterNative("MultiboardSetItemValueColor", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 6) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::MULTIBOARDITEM); if (simId < 0 || !data->multiboardItems.count(simId)) return JassValue::Null(); return JassValue::Null(); });
    auto getTimerDialog = [data = data_](int dialogHandle) -> std::shared_ptr<JassTimerDialog> { int simId = data->GetSimHandle(dialogHandle, JassHandleInfo::TIMER_DIALOG); if (simId < 0) return nullptr; auto it = data->timerDialogs.find(simId); return it != data->timerDialogs.end() ? it->second : nullptr; };
    vm_.RegisterNative("TimerDialogSetTitleColor", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto dialog = getTimerDialog(args[0].intVal)) { dialog->titleColor[0] = args[1].intVal; dialog->titleColor[1] = args[2].intVal; dialog->titleColor[2] = args[3].intVal; dialog->titleColor[3] = args[4].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("TimerDialogSetTimeColor", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 5) return JassValue::Null(); if (auto dialog = getTimerDialog(args[0].intVal)) { dialog->timeColor[0] = args[1].intVal; dialog->timeColor[1] = args[2].intVal; dialog->timeColor[2] = args[3].intVal; dialog->timeColor[3] = args[4].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("TimerDialogSetSpeed", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto dialog = getTimerDialog(args[0].intVal)) dialog->speed = args[1].realVal; return JassValue::Null(); });
    vm_.RegisterNative("IsTimerDialogDisplayed", [getTimerDialog](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); auto dialog = getTimerDialog(args[0].intVal); return JassValue::Bool(dialog && dialog->displayed); });
}
void JassNatives::RegisterUIStateNatives() {
    auto uiFlag = [data = data_](const std::vector<JassValue>& args, bool JassNativesData::JassUIConfig::*field, size_t valueIndex = 0) -> JassValue { if (args.size() <= valueIndex || !data) return JassValue::Null(); data->uiConfig.*field = args[valueIndex].boolVal; return JassValue::Null(); };
    vm_.RegisterNative("EnableDragSelect", [uiFlag](const std::vector<JassValue>& args) -> JassValue { return uiFlag(args, &JassNativesData::JassUIConfig::dragSelect, 0); });
    vm_.RegisterNative("EnablePreSelect", [uiFlag](const std::vector<JassValue>& args) -> JassValue { return uiFlag(args, &JassNativesData::JassUIConfig::preSelect, 0); });
    vm_.RegisterNative("EnableSelect", [uiFlag](const std::vector<JassValue>& args) -> JassValue { return uiFlag(args, &JassNativesData::JassUIConfig::select, 0); });
    vm_.RegisterNative("EnableUserControl", [uiFlag](const std::vector<JassValue>& args) -> JassValue { return uiFlag(args, &JassNativesData::JassUIConfig::userControl, 0); });
    vm_.RegisterNative("EnableOcclusion", [uiFlag](const std::vector<JassValue>& args) -> JassValue { return uiFlag(args, &JassNativesData::JassUIConfig::occlusion, 0); });
    vm_.RegisterNative("EnableWorldFogBoundary", [uiFlag](const std::vector<JassValue>& args) -> JassValue { return uiFlag(args, &JassNativesData::JassUIConfig::worldFogBoundary, 0); });
    vm_.RegisterNative("ShowInterface", [uiFlag](const std::vector<JassValue>& args) -> JassValue { return uiFlag(args, &JassNativesData::JassUIConfig::showInterface, 0); });
    vm_.RegisterNative("EnableMinimapFilterButtons", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); data->uiConfig.minimapAllyFilter = args[0].boolVal; data->uiConfig.minimapCreepFilter = args[1].boolVal; data->uiConfig.minimapFilterButtons = args[0].boolVal || args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("GetAllyColorFilterState", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(data && data->allyColorFilter ? 1 : 0); });
    vm_.RegisterNative("GetCreepCampFilterState", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(data && data->creepCampFilter); });
    vm_.RegisterNative("SetCreepCampFilterState", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty() && data) data->creepCampFilter = args[0].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("ForceUICancel", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("ForceUIKey", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("PingMinimap", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); JassNativesData::JassMinimapPing ping; ping.x = args[0].realVal; ping.y = args[1].realVal; ping.duration = args[2].realVal; data->minimapPings.push_back(ping); return JassValue::Null(); });
    vm_.RegisterNative("PingMinimapEx", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 7) return JassValue::Null(); JassNativesData::JassMinimapPing ping; ping.x = args[0].realVal; ping.y = args[1].realVal; ping.duration = args[2].realVal; ping.red = args[3].intVal; ping.green = args[4].intVal; ping.blue = args[5].intVal; ping.extra = args[6].boolVal; data->minimapPings.push_back(ping); return JassValue::Null(); });
    vm_.RegisterNative("IsCineFilterDisplayed", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(data && data->cineFilter.displayed); });
    vm_.RegisterNative("CreateDefeatCondition", [data = data_](const std::vector<JassValue>&) -> JassValue { auto dc = std::make_shared<JassNativesData::JassDefeatCondition>(); int id = data->nextDefeatConditionId++; data->defeatConditions[id] = dc; return JassValue::Int(id); });
    vm_.RegisterNative("DefeatConditionSetDescription", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); auto it = data->defeatConditions.find(args[0].intVal); if (it != data->defeatConditions.end()) it->second->description = args[1].strVal; return JassValue::Null(); });
    vm_.RegisterNative("DestroyDefeatCondition", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty()) data->defeatConditions.erase(args[0].intVal); return JassValue::Null(); });
    vm_.RegisterNative("DestroyFilter", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("DestroyCondition", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("AddIndicator", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        static const JassHandleInfo::Type kinds[] = {
            JassHandleInfo::UNIT, JassHandleInfo::ITEM, JassHandleInfo::DESTRUCTABLE,
        };
        for (auto kind : kinds) { const int simId = data->GetSimHandle(args[0].intVal, kind); if (simId < 0) continue; JassNativesData::JassUnitIndicator ind; ind.red = args[1].intVal; ind.green = args[2].intVal; ind.blue = args[3].intVal; ind.alpha = args[4].intVal; data->unitIndicators[simId] = ind; return JassValue::Null(); }
        return JassValue::Null();
    });
}
void JassNatives::RegisterFrameNatives() {
    auto getFrame = [data = data_](int jassHandle) -> std::shared_ptr<JassNativesData::JassFrame> { if (!data) return nullptr; int simId = data->GetSimHandle(jassHandle, JassHandleInfo::FRAME); if (simId < 0) return nullptr; auto it = data->frames.find(simId); return it != data->frames.end() ? it->second : nullptr; };
    auto syncUiNode = [data = data_](const std::shared_ptr<JassNativesData::JassFrame>& f) {
        if (!f || !f->uiNode) return;
        f->uiNode->SetName(f->name);
        f->uiNode->SetVisible(f->visible);
        if (f->width > 0.0f || f->height > 0.0f) {
            const float sw = (data && data->boundGameUi) ? (float)data->boundGameUi->screenW : 1280.0f;
            const float sh = (data && data->boundGameUi) ? (float)data->boundGameUi->screenH : 720.0f;
            f->uiNode->SetSize((f->width / 0.8f) * sw, (f->height / 0.6f) * sh);
        }
        if (auto* btn = engine::ui::DynamicCastFrame<ButtonFrame>(f->uiNode.get())) {
            btn->SetText(f->text);
            btn->SetEnabled(f->enabled);
            btn->SetTextColor(f->textColor);
        }
        if (data && data->boundGameUi) { data->boundGameUi->frameRegistry[f->name] = f->uiNode.get(); }
    };
    auto allocFrame = [this, data = data_, syncUiNode](const std::string& tpl, int owner, int ctx, int prio,
                                                       bool simple) -> JassValue {
        auto f = std::make_shared<JassNativesData::JassFrame>();
        f->templateName = tpl;
        f->name = tpl;
        f->ownerHandle = owner;
        f->context = ctx;
        f->priority = prio;
        f->simple = simple;
        int id = data->nextFrameId++;
        UIFrame* parentUi = nullptr;
        if (owner > 0) {
            int ownerSim = data->GetSimHandle(owner, JassHandleInfo::FRAME);
            if (ownerSim >= 0 && data->frames.count(ownerSim) && data->frames[ownerSim]) {
                f->parentSimId = ownerSim;
                auto& kids = data->frames[ownerSim]->childSimIds;
                if (std::find(kids.begin(), kids.end(), id) == kids.end()) kids.push_back(id);
                parentUi = data->frames[ownerSim]->uiNode.get();
            }
        }
        if (!parentUi && data->boundGameUi) { parentUi = data->boundGameUi->rootFrame; }
        auto btn = std::make_shared<ButtonFrame>(tpl, parentUi);
        btn->SetClickCallback([this, id, weakF = std::weak_ptr<JassNativesData::JassFrame>(f)]() {
            if (auto locked = weakF.lock()) { if (!locked->enabled || !locked->visible) return; FireFrameEvent(id, 1, locked->value, locked->text); }
        });
        f->uiNode = btn;
        syncUiNode(f);
        data->frames[id] = f;
        if (data->framesByName.find(f->name) == data->framesByName.end())
            data->framesByName[f->name] = id;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::FRAME, id));
    };
    vm_.RegisterNative("ConvertColor", [](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Int(0); uint32_t packed = ((uint32_t)(args[0].intVal & 0xFF) << 24) | ((uint32_t)(args[1].intVal & 0xFF) << 16) | ((uint32_t)(args[2].intVal & 0xFF) << 8) | ((uint32_t)(args[3].intVal & 0xFF)); return JassValue::Int((int32_t)packed); });
    vm_.RegisterNative("CreateRootFrame", [data = data_, allocFrame](const std::vector<JassValue>& args) -> JassValue { if (!data) return JassValue::Int(0); std::string skin = args.empty() ? "" : args[0].strVal; data->skinName = skin; return allocFrame("ROOT", 0, 0, 0, false); });
    vm_.RegisterNative("LoadTOCFile", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Bool(false); data->loadedTocFiles.push_back(args[0].strVal); return JassValue::Bool(true); });
    vm_.RegisterNative("CreateSimpleFrame", [allocFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Int(0); return allocFrame(args[0].strVal, args[1].intVal, args[2].intVal, 0, true); });
    vm_.RegisterNative("CreateFrame", [allocFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Int(0); return allocFrame(args[0].strVal, args[1].intVal, args[3].intVal, args[2].intVal, false); });
    vm_.RegisterNative("GetFrameByName", [data = data_, allocFrame](const std::vector<JassValue>& args) -> JassValue { if (!data || args.size() < 2) return JassValue::Int(0); auto it = data->framesByName.find(args[0].strVal); if (it != data->framesByName.end()) return JassValue::Int(data->AllocHandle(JassHandleInfo::FRAME, it->second)); return allocFrame(args[0].strVal, 0, args[1].intVal, 0, false); });
    vm_.RegisterNative("FrameSetAnchor",
        [getFrame](const std::vector<JassValue>& args) -> JassValue {
            if (args.size() < 4) return JassValue::Null();
            if (auto f = getFrame(args[0].intVal)) { JassNativesData::JassFrameAnchor a; a.point = args[1].intVal; a.x = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal; a.y = args[3].type == JassValue::REAL ? args[3].realVal : (float)args[3].intVal; f->anchors.push_back(a); f->positioned = true; }
            return JassValue::Null();
        });
    vm_.RegisterNative("FrameSetAbsPoint",
        [getFrame](const std::vector<JassValue>& args) -> JassValue {
            if (args.size() < 4) return JassValue::Null();
            if (auto f = getFrame(args[0].intVal)) { JassNativesData::JassFrameAnchor a; a.point = args[1].intVal; a.absolute = true; a.x = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal; a.y = args[3].type == JassValue::REAL ? args[3].realVal : (float)args[3].intVal; f->anchors.push_back(a); f->positioned = true; }
            return JassValue::Null();
        });
    vm_.RegisterNative("FrameSetPoint",
        [getFrame](const std::vector<JassValue>& args) -> JassValue {
            if (args.size() < 6) return JassValue::Null();
            if (auto f = getFrame(args[0].intVal)) { JassNativesData::JassFrameAnchor a; a.point = args[1].intVal; a.otherHandle = args[2].intVal; a.otherPoint = args[3].intVal; a.x = args[4].type == JassValue::REAL ? args[4].realVal : (float)args[4].intVal; a.y = args[5].type == JassValue::REAL ? args[5].realVal : (float)args[5].intVal; f->anchors.push_back(a); f->positioned = true; }
            return JassValue::Null();
        });
    vm_.RegisterNative("FrameSetText", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->text = args[1].strVal; return JassValue::Null(); });
    vm_.RegisterNative("FrameSetTextColor", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->textColor = (uint32_t)args[1].intVal; return JassValue::Null(); });
    vm_.RegisterNative("FramePositionBounds", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->positioned = true; return JassValue::Null(); });
    vm_.RegisterNative("SkinGetField", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Str(""); auto it = data->skinFields.find(args[0].strVal); return JassValue::Str(it != data->skinFields.end() ? it->second : ""); });
    vm_.RegisterNative("SkinGetField", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Str(""); auto it = data->skinFields.find(args[0].strVal); return JassValue::Str(it != data->skinFields.end() ? it->second : ""); });
    auto frameSimOf = [data = data_](int jassHandle) -> int { if (!data) return -1; return data->GetSimHandle(jassHandle, JassHandleInfo::FRAME); };
    auto allocFrameHandle = [data = data_](int simId) -> JassValue { if (!data || simId < 0) return JassValue::Int(0); return JassValue::Int(data->AllocHandle(JassHandleInfo::FRAME, simId)); };
    auto numArg = [](const JassValue& v) -> float { return v.type == JassValue::REAL ? v.realVal : (float)v.intVal; };
    vm_.RegisterNative("BlzCreateFrame", [data = data_, allocFrame](const std::vector<JassValue>& args) -> JassValue { if (!data || args.size() < 4) return JassValue::Int(0); return allocFrame(args[0].strVal, args[1].intVal, args[3].intVal, args[2].intVal, false); });
    vm_.RegisterNative("BlzCreateSimpleFrame", [data = data_, allocFrame](const std::vector<JassValue>& args) -> JassValue { if (!data || args.size() < 3) return JassValue::Int(0); return allocFrame(args[0].strVal, args[1].intVal, args[2].intVal, 0, true); });
    vm_.RegisterNative("BlzDestroyFrame", [data = data_, frameSimOf](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Null();
        const int simId = frameSimOf(args[0].intVal);
        if (simId < 0) return JassValue::Null();
        auto it = data->frames.find(simId);
        if (it == data->frames.end()) return JassValue::Null();
        for (auto& [id, f] : data->frames) {
            (void)id;
            if (!f) continue;
            auto& kids = f->childSimIds;
            kids.erase(std::remove(kids.begin(), kids.end(), simId), kids.end());
            if (f->parentSimId == simId) f->parentSimId = -1;
            if (f->tooltipSimId == simId) f->tooltipSimId = -1;
        }
        if (it->second && data->boundGameUi) {
            auto regIt = data->boundGameUi->frameRegistry.find(it->second->name);
            if (regIt != data->boundGameUi->frameRegistry.end() && regIt->second == it->second->uiNode.get()) { data->boundGameUi->frameRegistry.erase(regIt); }
        }
        for (auto nit = data->framesByName.begin(); nit != data->framesByName.end();) { if (nit->second == simId) nit = data->framesByName.erase(nit); else ++nit; }
        data->frames.erase(it);
        data->FreeHandle(args[0].intVal);
        return JassValue::Null();
    });
    auto blzGetByName = [data = data_, allocFrame](const std::string& name, int ctx) -> JassValue { auto it = data->framesByName.find(name); if (it != data->framesByName.end()) return JassValue::Int(data->AllocHandle(JassHandleInfo::FRAME, it->second)); return allocFrame(name, 0, ctx, 0, false); };
    vm_.RegisterNative("BlzGetFrameByName", [data = data_, blzGetByName](const std::vector<JassValue>& args) -> JassValue { if (!data || args.size() < 2) return JassValue::Int(0); return blzGetByName(args[0].strVal, args[1].intVal); });
    vm_.RegisterNative("BlzGetOriginFrame", [data = data_, blzGetByName](const std::vector<JassValue>& args) -> JassValue { if (!data || args.size() < 2) return JassValue::Int(0); return blzGetByName("ORIGIN_" + std::to_string(args[0].intVal) + "_" + std::to_string(args[1].intVal), 0); });
    vm_.RegisterNative("BlzFrameGetName", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Str(""); if (auto f = getFrame(args[0].intVal)) return JassValue::Str(f->name); return JassValue::Str(""); });
    vm_.RegisterNative("BlzFrameSetPoint", [getFrame, frameSimOf, numArg](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 6) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { JassNativesData::JassFrameAnchor a; a.point = args[1].intVal; a.otherHandle = args[2].intVal; a.otherPoint = args[3].intVal; a.x = numArg(args[4]); a.y = numArg(args[5]); f->anchors.push_back(a); f->positioned = true; } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetAbsPoint", [getFrame, numArg](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { JassNativesData::JassFrameAnchor a; a.point = args[1].intVal; a.absolute = true; a.x = numArg(args[2]); a.y = numArg(args[3]); f->anchors.push_back(a); f->positioned = true; } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameClearAllPoints", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->anchors.clear(); f->positioned = false; } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetAllPoints", [getFrame, frameSimOf](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->anchors.clear(); f->positioned = true; (void)frameSimOf(args[1].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetVisible", [getFrame, syncUiNode](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->visible = args[1].boolVal; syncUiNode(f); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameIsVisible", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); if (auto f = getFrame(args[0].intVal)) return JassValue::Bool(f->visible); return JassValue::Bool(false); });
    vm_.RegisterNative("BlzFrameClick", [this, getFrame, frameSimOf](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); const int simId = frameSimOf(args[0].intVal); if (auto f = getFrame(args[0].intVal)) { if (f->enabled) { FireFrameEvent(simId, 1, f->value, f->text); } } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetText", [getFrame, syncUiNode](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->text = args[1].strVal; syncUiNode(f); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameGetText", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Str(""); if (auto f = getFrame(args[0].intVal)) return JassValue::Str(f->text); return JassValue::Str(""); });
    vm_.RegisterNative("BlzFrameAddText", [getFrame, syncUiNode](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->text += args[1].strVal; syncUiNode(f); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetTextSizeLimit", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->textSizeLimit = args[1].intVal; return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameGetTextSizeLimit", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (auto f = getFrame(args[0].intVal)) return JassValue::Int(f->textSizeLimit); return JassValue::Int(0); });
    vm_.RegisterNative("BlzFrameSetFocus", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); if (auto f = getFrame(args[0].intVal)) { f->focused = args[1].boolVal; return JassValue::Bool(true); } return JassValue::Bool(false); });
    vm_.RegisterNative("BlzFrameSetModel", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->model = args[1].strVal; f->modelCamera = args[2].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetEnable", [getFrame, syncUiNode](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->enabled = args[1].boolVal; syncUiNode(f); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameGetEnable", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); if (auto f = getFrame(args[0].intVal)) return JassValue::Bool(f->enabled); return JassValue::Bool(false); });
    vm_.RegisterNative("BlzFrameSetAlpha", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->alpha = args[1].intVal; return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameGetAlpha", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (auto f = getFrame(args[0].intVal)) return JassValue::Int(f->alpha); return JassValue::Int(0); });
    vm_.RegisterNative("BlzFrameSetSpriteAnimate", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->spritePrimaryProp = args[1].intVal; f->spriteFlags = args[2].intVal; } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetTexture", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->texture = args[1].strVal; f->textureFlag = args[2].intVal; f->textureBlend = args[3].boolVal; } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetScale", [getFrame, numArg](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->scale = numArg(args[1]); return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetTooltip", [data = data_, getFrame, frameSimOf](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->tooltipSimId = frameSimOf(args[1].intVal); } (void)data; return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameCageMouse", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->cageMouse = args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameGetValue", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0.0f); if (auto f = getFrame(args[0].intVal)) return JassValue::Real(f->value); return JassValue::Real(0.0f); });
    vm_.RegisterNative("BlzFrameSetValue", [getFrame, numArg](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->value = numArg(args[1]); return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetMinMaxValue", [getFrame, numArg](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->minValue = numArg(args[1]); f->maxValue = numArg(args[2]); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetStepSize", [getFrame, numArg](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->stepSize = numArg(args[1]); return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetSize", [getFrame, numArg, syncUiNode](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->width = numArg(args[1]); f->height = numArg(args[2]); syncUiNode(f); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameSetVertexColor", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) f->vertexColor = (uint32_t)args[1].intVal; return JassValue::Null(); });
    vm_.RegisterNative("BlzEnableUIAutoPosition", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Null(); data->uiAutoPosition = args[0].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("BlzHideOriginFrames", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Null(); data->hideOriginFrames = args[0].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("BlzLoadTOCFile", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Bool(false); data->loadedTocFiles.push_back(args[0].strVal); return JassValue::Bool(true); });
    vm_.RegisterNative("BlzFrameGetParent", [data = data_, getFrame, allocFrameHandle](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto f = getFrame(args[0].intVal); if (!f) return JassValue::Int(0); (void)data; return allocFrameHandle(f->parentSimId); });
    vm_.RegisterNative("BlzFrameSetParent", [data = data_, getFrame, frameSimOf](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        auto f = getFrame(args[0].intVal);
        if (!f) return JassValue::Null();
        const int newParent = frameSimOf(args[1].intVal);
        if (f->parentSimId >= 0) {
            if (auto it = data->frames.find(f->parentSimId); it != data->frames.end() && it->second) { auto& kids = it->second->childSimIds; kids.erase(std::remove(kids.begin(), kids.end(), data->GetSimHandle(args[0].intVal, JassHandleInfo::FRAME)), kids.end()); }
        }
        f->parentSimId = newParent;
        if (newParent >= 0) {
            if (auto it = data->frames.find(newParent); it != data->frames.end() && it->second) { const int self = data->GetSimHandle(args[0].intVal, JassHandleInfo::FRAME); auto& kids = it->second->childSimIds; if (std::find(kids.begin(), kids.end(), self) == kids.end()) kids.push_back(self); if (f->uiNode && it->second->uiNode) { f->uiNode->SetParent(it->second->uiNode.get()); } }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("BlzFrameGetHeight", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0.0f); if (auto f = getFrame(args[0].intVal)) return JassValue::Real(f->height); return JassValue::Real(0.0f); });
    vm_.RegisterNative("BlzFrameGetWidth", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0.0f); if (auto f = getFrame(args[0].intVal)) return JassValue::Real(f->width); return JassValue::Real(0.0f); });
    vm_.RegisterNative("BlzFrameSetLevel", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto f = getFrame(args[0].intVal)) { f->level = args[1].intVal; if (f->uiNode) f->uiNode->SetZOrder(f->level); } return JassValue::Null(); });
    vm_.RegisterNative("BlzFrameGetChildrenCount", [getFrame](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); if (auto f = getFrame(args[0].intVal)) return JassValue::Int((int)f->childSimIds.size()); return JassValue::Int(0); });
    vm_.RegisterNative("BlzFrameGetChild", [data = data_, getFrame, allocFrameHandle](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Int(0); auto f = getFrame(args[0].intVal); if (!f) return JassValue::Int(0); (void)data; const int idx = args[1].intVal; if (idx < 0 || idx >= (int)f->childSimIds.size()) return JassValue::Int(0); return allocFrameHandle(f->childSimIds[(size_t)idx]); });
    jass::RegisterNativeAuto(vm_, "ConvertFrameEventType", [](int i) -> int { return i; });
    jass::RegisterNativeAuto(vm_, "ConvertOriginFrameType", [](int i) -> int { return i; });
    jass::RegisterNativeAuto(vm_, "ConvertTextAlignType", [](int i) -> int { return i; });
    jass::RegisterNativeAuto(vm_, "BlzCreateFrameByType",
        [data = data_, allocFrame, getFrame, syncUiNode](const std::string& typeName, const std::string& name, int owner, const std::string& inherits, int createContext) -> int {
            (void)typeName;
            (void)inherits;
            JassValue h = allocFrame(name, owner, createContext, 0, false);
            if (auto f = getFrame(h.intVal)) { f->templateName = typeName; syncUiNode(f); }
            return h.intVal;
        });
    jass::RegisterNativeAuto(vm_, "BlzTriggerRegisterFrameEvent",
        [data = data_, frameSimOf](int trigHandle, int frameHandle, int eventId) -> int {
            if (!data) return 0;
            int frameSim = frameSimOf(frameHandle);
            if (frameSim < 0) return 0;
            auto it = data->triggers.find(trigHandle);
            if (it == data->triggers.end()) {
                int trigId = data->GetSimHandle(trigHandle, JassHandleInfo::TRIGGER);
                it = data->triggers.find(trigId);
            }
            if (it == data->triggers.end() || !it->second) return 0;
            it->second->frameEvents.push_back({frameSim, eventId});
            return data->AllocHandle(JassHandleInfo::TRIGGER, eventId);
        });
    jass::RegisterNativeAuto(vm_, "BlzGetTriggerFrame", [data = data_]() -> int {
        return data ? data->eventTriggerFrameHandle : 0;
    });
    jass::RegisterNativeAuto(vm_, "BlzGetTriggerFrameEvent", [data = data_]() -> int {
        return data ? data->eventTriggerFrameEvent : 0;
    });
    jass::RegisterNativeAuto(vm_, "BlzGetTriggerFrameValue", [data = data_]() -> float {
        return data ? data->eventTriggerFrameValue : 0.0f;
    });
    jass::RegisterNativeAuto(vm_, "BlzGetTriggerFrameText", [data = data_]() -> std::string {
        return data ? data->eventTriggerFrameText : std::string();
    });
}
void JassNatives::RegisterAbilityBuilderNatives() {
    auto getStore = [data = data_](int jassHandle) -> std::shared_ptr<JassNativesData::JassLocalStore> { if (!data) return nullptr; int simId = data->GetSimHandle(jassHandle, JassHandleInfo::LOCALSTORE); if (simId < 0) return nullptr; auto it = data->localStores.find(simId); return it != data->localStores.end() ? it->second : nullptr; };
    vm_.RegisterNative("CreateLocalStore", [data = data_](const std::vector<JassValue>&) -> JassValue { auto s = std::make_shared<JassNativesData::JassLocalStore>(); int id = data->nextLocalStoreId++; data->localStores[id] = s; return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCALSTORE, id)); });
    vm_.RegisterNative("DestroyLocalStore", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty() && data) { int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCALSTORE); if (simId >= 0) data->localStores.erase(simId); } return JassValue::Null(); });
    vm_.RegisterNative("SetLocalStore", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto s = getStore(args[0].intVal)) s->handles[args[1].strVal] = args[2].intVal; return JassValue::Null(); });
    vm_.RegisterNative("SetLocalStoreBoolean", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto s = getStore(args[0].intVal)) s->bools[args[1].strVal] = args[2].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetLocalStoreInteger", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto s = getStore(args[0].intVal)) s->ints[args[1].strVal] = args[2].intVal; return JassValue::Null(); });
    vm_.RegisterNative("SetLocalStoreReal", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); float v = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal; if (auto s = getStore(args[0].intVal)) s->reals[args[1].strVal] = v; return JassValue::Null(); });
    vm_.RegisterNative("SetLocalStoreString", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto s = getStore(args[0].intVal)) s->strs[args[1].strVal] = args[2].strVal; return JassValue::Null(); });
    vm_.RegisterNative("SetLocalStoreCode", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto s = getStore(args[0].intVal)) s->codes[args[1].strVal] = args[2].intVal; return JassValue::Null(); });
    vm_.RegisterNative("SetLocalStoreHandle", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); if (auto s = getStore(args[0].intVal)) s->handles[args[1].strVal] = args[2].intVal; return JassValue::Null(); });
    vm_.RegisterNative("GetLocalStore", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Int(0); if (auto s = getStore(args[0].intVal)) { auto it = s->handles.find(args[1].strVal); if (it != s->handles.end()) return JassValue::Int(it->second); } return JassValue::Int(0); });
    vm_.RegisterNative("GetLocalStoreBoolean", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); if (auto s = getStore(args[0].intVal)) { auto it = s->bools.find(args[1].strVal); if (it != s->bools.end()) return JassValue::Bool(it->second); } return JassValue::Bool(false); });
    vm_.RegisterNative("GetLocalStoreInteger", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Int(0); if (auto s = getStore(args[0].intVal)) { auto it = s->ints.find(args[1].strVal); if (it != s->ints.end()) return JassValue::Int(it->second); } return JassValue::Int(0); });
    vm_.RegisterNative("GetLocalStoreReal", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Real(0); if (auto s = getStore(args[0].intVal)) { auto it = s->reals.find(args[1].strVal); if (it != s->reals.end()) return JassValue::Real((float)it->second); } return JassValue::Real(0); });
    vm_.RegisterNative("GetLocalStoreString", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Str(""); if (auto s = getStore(args[0].intVal)) { auto it = s->strs.find(args[1].strVal); if (it != s->strs.end()) return JassValue::Str(it->second); } return JassValue::Str(""); });
    vm_.RegisterNative("GetLocalStoreCode", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Int(0); if (auto s = getStore(args[0].intVal)) { auto it = s->codes.find(args[1].strVal); if (it != s->codes.end()) return JassValue::Int(it->second); } return JassValue::Int(0); });
    vm_.RegisterNative("GetLocalStoreLocationHandle", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Int(0); if (auto s = getStore(args[0].intVal)) { auto it = s->handles.find(args[1].strVal); if (it != s->handles.end()) return JassValue::Int(it->second); } return JassValue::Int(0); });
    vm_.RegisterNative("LocalStoreContainsKey", [getStore](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Bool(false); if (auto s = getStore(args[0].intVal)) { const std::string& k = args[1].strVal; if (s->ints.count(k) || s->reals.count(k) || s->strs.count(k) || s->bools.count(k) || s->handles.count(k) || s->codes.count(k)) return JassValue::Bool(true); } return JassValue::Bool(false); });
    vm_.RegisterNative("LocalStoreCleanUpCastInstance", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("FlushChildLocalStore", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("FlushParentLocalStore", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty() && data) { int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCALSTORE); if (simId >= 0) data->localStores.erase(simId); } return JassValue::Null(); });
    vm_.RegisterNative("GetTriggerLocalStore", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Int(0); auto s = std::make_shared<JassNativesData::JassLocalStore>(); int id = data->nextLocalStoreId++; data->localStores[id] = s; return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCALSTORE, id)); });
    vm_.RegisterNative("GetAbilityLocalStore", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Int(0); auto s = std::make_shared<JassNativesData::JassLocalStore>(); int id = data->nextLocalStoreId++; data->localStores[id] = s; return JassValue::Int(data->AllocHandle(JassHandleInfo::LOCALSTORE, id)); });
    auto retNull = [](const std::vector<JassValue>&) -> JassValue { return JassValue::Null(); };
    auto retZero = [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); };
    auto retFalse = [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); };
    auto retEmpty = [](const std::vector<JassValue>&) -> JassValue { return JassValue::Str(""); };
    const char* nullNatives[] = {
        "RegisterABConf", "AddABConfActivateAction", "AddABConfAddAbilityAction",
        "AddABConfAddDisabledAbilityAction", "AddABConfBeginCastingAction",
        "AddABConfCancelPreCastAction", "AddABConfChannelTickAction",
        "AddABConfDeactivateAction", "AddABConfDeathPreCastAction",
        "AddABConfEndCastingAction", "AddABConfEndChannelAction",
        "AddABConfLevelChangeAction", "AddABConfOrderIssuedAction",
        "AddABConfRemoveAbilityAction", "AddABConfRemoveDisabledAbilityAction",
        "SetABConfAutoCastOffId", "SetABConfAutoCastOnId", "SetABConfAutoCastType",
        "SetABConfCastId", "SetABConfType", "SetABConfUncastId",
        "SetAbilityEnabledWhileUnderConstruction", "SetAbilityEnabledWhileUpgrading",
        "AbilityActivate", "AbilityDeactivate", "AbilityAddOrderButton",
        "AbilityRemoveOrderButton", "DestroyAbilityBehavior", "DestroyAbilityTargetVisitor",
        "DestroyAbstractRangedBehavior", "DestroyRangedBehavior", "DestroyOrderButton",
        "AbilityTypeLevelDataAddTargetAllowed",
        "AbilityTypeLevelDataRemoveTargetAllowed", "SetOrderButtonAOE",
        "SetOrderButtonAutoCastActive", "SetOrderButtonAutoCastOrderId",
        "SetOrderButtonButtonPositionX", "SetOrderButtonButtonPositionY",
        "SetOrderButtonByIconUI", "SetOrderButtonCharges",
        "SetOrderButtonContainerMenuOrderId", "SetOrderButtonDisabled",
        "SetOrderButtonFoodCost", "SetOrderButtonGoldCost", "SetOrderButtonHidden",
        "SetOrderButtonHotKey", "SetOrderButtonIconPath", "SetOrderButtonLumberCost",
        "SetOrderButtonManaCost", "SetOrderButtonPreviewBuildUnitId",
        "SetOrderButtonToolTip", "SetOrderButtonUberTip", "SetOrderButtonUnAutoCastOrderId",
        "AddDestructableBuff", "RemoveDestructableBuff",
        "AddUnitNonStackingDisplayBuff", "RemoveUnitNonStackingDisplayBuff",
        "CreateABTimeOfDayEvent", "RegisterABTimeOfDayEvent", "UnregisterABTimeOfDayEvent",
        "ABTimerStart", "ABTimerSetRepeats", "ABTimerSetTimeoutTime",
        "ABTimerStartRepeatingTimerWithDelay", "RegisterAbilityStructType",
        "RegisterAbilityType", "StartSacrificingUnit",
    };
    for (const char* n : nullNatives) vm_.RegisterNative(n, retNull);
    const char* zeroNatives[] = {
        "CreateAbilityBuilderConfiguration", "CreateAbilityBehavior",
        "CreateAbilityTargetVisitor", "CreateAbstractRangedBehavior",
        "CreateBehaviorExpr", "CreateRangedBehavior", "CreateOrderButton",
        "CreatePassiveBuff", "CreateStunBuff",
        "CreateTargetingBuff", "CreateTimedArtBuff", "CreateTimedBuff",
        "CreateTimedLifeBuff", "CreateTimedTargetingBuff", "CreateTimedTickingBuff",
        "CreateTimedTickingPausedBuff", "CreateTimedTickingPostDeathBuff",
        "CreateDestructableBuff", "CreateABTimer", "GetAbilityTypeLevelDataAsID",
        "GetAbilityTypeLevelDataInteger", "GetAbilityTypeLevelUnitID",
        "GetGameObjectById", "GetGameObjectFieldAsID", "GetGameObjectFieldAsInteger",
        "GetAbilityItem", "GetRangedBehaviorTarget", "GetAbstractRangedBehaviorSourceUnit",
        "GetUnitAttackGroundBehavior", "GetUnitAttackGroundBehaviorLoc",
        "GetUnitAttackMovePointBehavior", "GetUnitAttackMovePointBehaviorLoc",
        "GetUnitAttackWidgetBehavior", "GetUnitMoveFollowBehavior",
        "GetUnitMovePointBehavior", "GetUnitMovePointBehaviorLoc",
        "GetOrderButtonAOE", "GetOrderButtonAutoCastOrderId",
        "GetOrderButtonButtonPositionX", "GetOrderButtonButtonPositionY",
        "GetOrderButtonCharges", "GetOrderButtonContainerMenuOrderId",
        "GetOrderButtonFoodCost", "GetOrderButtonGoldCost", "GetOrderButtonHotKey",
        "GetOrderButtonLumberCost", "GetOrderButtonManaCost", "GetOrderButtonOrderId",
        "GetOrderButtonPreviewBuildUnitId", "GetOrderButtonToolTip",
        "GetOrderButtonUberTip", "GetOrderButtonUnAutoCastOrderId",
        "GetGameObjectField", "WarsmashGetAbilityClassName",
    };
    for (const char* n : zeroNatives) vm_.RegisterNative(n, retZero);
    const char* falseNatives[] = {
        "IsOrderButtonAutoCastActive", "IsOrderButtonDisabled", "IsToggleAbilityActive",
        "PassTargetCheck", "PassUsableCheck",
        "FailTargetCheckWithMessage", "FailUsableCheckOnCooldown",
        "FailUsableCheckOnHeroLevelRequirement", "FailUsableCheckOnRequirement",
        "FailUsableCheckWithMessage", "CheckUnitForAbilityProjReaction",
        "AbilityTargetAcceptVisitor",
    };
    for (const char* n : falseNatives) vm_.RegisterNative(n, retFalse);
    const char* strNatives[] = {
        "GetGameObjectFieldAsString", "GetGameObjectFieldAsStringList",
        "GetAbilityLearnIconUI", "GetAbilityOffIconUI", "GetAbilityOnIconUI",
        "GetItemIconUI", "GetUnitIconUI",
    };
    for (const char* n : strNatives) vm_.RegisterNative(n, retEmpty);
    vm_.RegisterNative("GetAbilityTypeLevelDataBoolean", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("GetAbilityTypeLevelDataCastTime", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetAbilityTypeLevelDataDurationHero", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetAbilityTypeLevelDataDurationNormal", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetAbilityTypeLevelDataReal", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("GetAbilityTypeLevelDataString", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Str(""); });
    vm_.RegisterNative("GetAbilityTypeLevelDataFirstBuffId", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Str(""); });
    vm_.RegisterNative("GetGameObjectFieldAsBoolean", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Bool(false); });
    vm_.RegisterNative("GetGameObjectFieldAsReal", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0); });
    vm_.RegisterNative("AbstractRangedBehaviorResetI", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("AbstractRangedBehaviorResetII", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("CreateJassAbility", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Int(0); return JassValue::Int(AbilityToHandle(*data, War3ID(static_cast<uint32_t>(args[0].intVal)))); });
    vm_.RegisterNative("CreateJassBuff", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.size() < 2) return JassValue::Int(0);
        const int abilityId = data->nextAbilityId++;
        auto handle = std::make_shared<JassAbilityHandle>();
        handle->code = War3ID(static_cast<uint32_t>(args[0].intVal));
        handle->alias = War3ID(static_cast<uint32_t>(args[1].intVal));
        handle->isBuff = true;
        data->abilities[abilityId] = handle;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::ABILITY, abilityId));
    });
}
void JassNatives::RegisterHashtableNatives() {
    auto getTable = [data = data_](int tableHandle) -> std::shared_ptr<JassNativesData::JassHashtable> { int simId = data->GetSimHandle(tableHandle, JassHandleInfo::HASHTABLE); if (simId < 0) return nullptr; auto it = data->hashtables.find(simId); return it != data->hashtables.end() ? it->second : nullptr; };
    auto cellKey = [](int parentKey, int childKey) -> int64_t { return (static_cast<int64_t>(parentKey) << 32) | static_cast<uint32_t>(childKey); };
    vm_.RegisterNative("InitHashtable", [data = data_](const std::vector<JassValue>&) -> JassValue { auto table = std::make_shared<JassNativesData::JassHashtable>(); int id = data->nextHashtableId++; data->hashtables[id] = table; return JassValue::Int(data->AllocHandle(JassHandleInfo::HASHTABLE, id)); });
    vm_.RegisterNative("SaveInteger", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto t = getTable(args[0].intVal)) { t->values[cellKey(args[1].intVal, args[2].intVal)] = JassValue::Int(args[3].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("SaveReal", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto t = getTable(args[0].intVal)) { t->values[cellKey(args[1].intVal, args[2].intVal)] = JassValue::Real(args[3].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("SaveBoolean", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto t = getTable(args[0].intVal)) { t->values[cellKey(args[1].intVal, args[2].intVal)] = JassValue::Bool(args[3].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("SaveStr", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); if (auto t = getTable(args[0].intVal)) { t->values[cellKey(args[1].intVal, args[2].intVal)] = JassValue::Str(args[3].strVal); return JassValue::Bool(true); } return JassValue::Bool(false); });
    vm_.RegisterNative("LoadInteger", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Int(0); if (auto t = getTable(args[0].intVal)) { auto it = t->values.find(cellKey(args[1].intVal, args[2].intVal)); if (it != t->values.end()) return JassValue::Int(it->second.intVal); } return JassValue::Int(0); });
    vm_.RegisterNative("LoadReal", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Real(0.0f); if (auto t = getTable(args[0].intVal)) { auto it = t->values.find(cellKey(args[1].intVal, args[2].intVal)); if (it != t->values.end()) return JassValue::Real(it->second.realVal); } return JassValue::Real(0.0f); });
    vm_.RegisterNative("LoadBoolean", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); if (auto t = getTable(args[0].intVal)) { auto it = t->values.find(cellKey(args[1].intVal, args[2].intVal)); if (it != t->values.end()) return JassValue::Bool(it->second.boolVal); } return JassValue::Bool(false); });
    vm_.RegisterNative("LoadStr", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Str(""); if (auto t = getTable(args[0].intVal)) { auto it = t->values.find(cellKey(args[1].intVal, args[2].intVal)); if (it != t->values.end()) return JassValue::Str(it->second.strVal); } return JassValue::Str(""); });
    auto haveCell = [getTable, cellKey](const std::vector<JassValue>& args) -> bool { if (args.size() < 3) return false; auto t = getTable(args[0].intVal); if (!t) return false; const int64_t key = cellKey(args[1].intVal, args[2].intVal); return t->values.count(key) > 0 || t->handles.count(key) > 0; };
    vm_.RegisterNative("HaveSavedInteger", [haveCell](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveCell(args)); });
    vm_.RegisterNative("HaveSavedReal", [haveCell](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveCell(args)); });
    vm_.RegisterNative("HaveSavedBoolean", [haveCell](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveCell(args)); });
    vm_.RegisterNative("HaveSavedString", [haveCell](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveCell(args)); });
    vm_.RegisterNative("HaveSavedHandle", [haveCell](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveCell(args)); });
    auto removeCell = [getTable, cellKey](const std::vector<JassValue>& args) -> void { if (args.size() < 3) return; if (auto t = getTable(args[0].intVal)) { const int64_t key = cellKey(args[1].intVal, args[2].intVal); t->values.erase(key); t->handles.erase(key); } };
    vm_.RegisterNative("RemoveSavedInteger", [removeCell](const std::vector<JassValue>& args) -> JassValue { removeCell(args); return JassValue::Null(); });
    vm_.RegisterNative("RemoveSavedReal", [removeCell](const std::vector<JassValue>& args) -> JassValue { removeCell(args); return JassValue::Null(); });
    vm_.RegisterNative("RemoveSavedBoolean", [removeCell](const std::vector<JassValue>& args) -> JassValue { removeCell(args); return JassValue::Null(); });
    vm_.RegisterNative("RemoveSavedString", [removeCell](const std::vector<JassValue>& args) -> JassValue { removeCell(args); return JassValue::Null(); });
    vm_.RegisterNative("RemoveSavedHandle", [removeCell](const std::vector<JassValue>& args) -> JassValue { removeCell(args); return JassValue::Null(); });
    vm_.RegisterNative("FlushParentHashtable", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); if (auto t = getTable(args[0].intVal)) { t->values.clear(); t->handles.clear(); } return JassValue::Null(); });
    vm_.RegisterNative("FlushChildHashtable", [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        if (auto t = getTable(args[0].intVal)) { const int parent = args[1].intVal; for (auto it = t->values.begin(); it != t->values.end();) { if ((int)(it->first >> 32) == parent) { it = t->values.erase(it); } else { ++it; } } for (auto it = t->handles.begin(); it != t->handles.end();) { if ((int)(it->first >> 32) == parent) { it = t->handles.erase(it); } else { ++it; } } }
        return JassValue::Null();
    });
    auto saveHandle = [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); auto t = getTable(args[0].intVal); if (!t || args[3].intVal == 0) return JassValue::Bool(false); t->handles[cellKey(args[1].intVal, args[2].intVal)] = args[3].intVal; return JassValue::Bool(true); };
    auto loadHandle = [getTable, cellKey](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Int(0); auto t = getTable(args[0].intVal); if (!t) return JassValue::Int(0); auto it = t->handles.find(cellKey(args[1].intVal, args[2].intVal)); return JassValue::Int(it != t->handles.end() ? it->second : 0); };
    const char* handleSaves[] = {
        "SavePlayerHandle", "SaveWidgetHandle", "SaveDestructableHandle",
        "SaveItemHandle", "SaveUnitHandle", "SaveAbilityHandle",
        "SaveTimerHandle", "SaveTriggerHandle", "SaveTriggerConditionHandle",
        "SaveTriggerActionHandle", "SaveTriggerEventHandle", "SaveForceHandle",
        "SaveGroupHandle", "SaveLocationHandle", "SaveRectHandle",
        "SaveBooleanExprHandle", "SaveSoundHandle", "SaveEffectHandle",
        "SaveUnitPoolHandle", "SaveItemPoolHandle", "SaveQuestHandle",
        "SaveQuestItemHandle", "SaveDefeatConditionHandle", "SaveTimerDialogHandle",
        "SaveLeaderboardHandle", "SaveMultiboardHandle", "SaveMultiboardItemHandle",
        "SaveTrackableHandle", "SaveDialogHandle", "SaveButtonHandle",
        "SaveTextTagHandle", "SaveLightningHandle", "SaveImageHandle",
        "SaveUbersplatHandle", "SaveRegionHandle", "SaveFogStateHandle",
        "SaveFogModifierHandle", "SaveAgentHandle", "SaveHashtableHandle",
    };
    for (const char* name : handleSaves) {
        vm_.RegisterNative(name, saveHandle);
    }
    const char* handleLoads[] = {
        "LoadPlayerHandle", "LoadWidgetHandle", "LoadDestructableHandle",
        "LoadItemHandle", "LoadUnitHandle", "LoadAbilityHandle",
        "LoadTimerHandle", "LoadTriggerHandle", "LoadTriggerConditionHandle",
        "LoadTriggerActionHandle", "LoadTriggerEventHandle", "LoadForceHandle",
        "LoadGroupHandle", "LoadLocationHandle", "LoadRectHandle",
        "LoadBooleanExprHandle", "LoadSoundHandle", "LoadEffectHandle",
        "LoadUnitPoolHandle", "LoadItemPoolHandle", "LoadQuestHandle",
        "LoadQuestItemHandle", "LoadDefeatConditionHandle", "LoadTimerDialogHandle",
        "LoadLeaderboardHandle", "LoadMultiboardHandle", "LoadMultiboardItemHandle",
        "LoadTrackableHandle", "LoadDialogHandle", "LoadButtonHandle",
        "LoadTextTagHandle", "LoadLightningHandle", "LoadImageHandle",
        "LoadUbersplatHandle", "LoadRegionHandle", "LoadFogStateHandle",
        "LoadFogModifierHandle", "LoadHashtableHandle", "LoadAgentHandle",
    };
    for (const char* name : handleLoads) {
        vm_.RegisterNative(name, loadHandle);
    }
}
namespace {
std::string GameCacheFileFor(const std::string& campaignFile) {
    std::string safe;
    for (char c : campaignFile) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-') { safe += c; } else {
            safe += '_';
        }
    }
    if (safe.empty()) safe = "default";
    return (fs::path(GameCacheSaveDir()) / ("warsmash_" + safe + ".w3v")).string();
}
std::string EscapeCacheString(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}
std::string UnescapeCacheString(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            out += (n == 'n') ? '\n' : (n == 't') ? '\t' : n;
        } else {
            out += s[i];
        }
    }
    return out;
}
bool WriteGameCacheFile(const std::string& path,
                        const std::string& campaignFile,
                        const JassNativesData::JassGameCache& cache) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << "W3V1\nC " << EscapeCacheString(campaignFile) << "\n";
    for (const auto& [mission, keys] : cache.values) {
        for (const auto& [key, value] : keys) {
            const std::string m = EscapeCacheString(mission);
            const std::string k = EscapeCacheString(key);
            switch (value.type) {
                case JassValue::INTEGER:
                    f << "M " << m << '\t' << k << "\tI " << value.intVal << "\n";
                    break;
                case JassValue::REAL:
                    f << "M " << m << '\t' << k << "\tR " << value.realVal << "\n";
                    break;
                case JassValue::BOOLEAN:
                    f << "M " << m << '\t' << k << "\tB " << (value.boolVal ? 1 : 0) << "\n";
                    break;
                case JassValue::STRING:
                    f << "M " << m << '\t' << k << "\tS " << EscapeCacheString(value.strVal) << "\n";
                    break;
                default:
                    break;
            }
        }
    }
    for (const auto& [mission, keys] : cache.units) {
        for (const auto& [key, unit] : keys) {
            f << "U " << EscapeCacheString(mission) << '\t' << EscapeCacheString(key) << ' '
              << unit.typeId << ' ' << unit.level << ' ' << unit.lifeFraction << ' ' << unit.owner << "\n";
        }
    }
    f.flush();
    return (bool)f;
}
bool ReadGameCacheFile(const std::string& path, JassNativesData::JassGameCache& cache) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string line;
    if (!std::getline(f, line) || line != "W3V1") return false;
    bool ok = false;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 2) continue;
        if (line[0] == 'M' && line[1] == ' ') {
            const size_t t1 = line.find('\t', 2);
            const size_t t2 = (t1 == std::string::npos) ? std::string::npos : line.find('\t', t1 + 1);
            if (t1 == std::string::npos || t2 == std::string::npos || t2 + 3 >= line.size()) continue;
            const std::string mission = UnescapeCacheString(line.substr(2, t1 - 2));
            const std::string key = UnescapeCacheString(line.substr(t1 + 1, t2 - t1 - 1));
            const char type = line[t2 + 1];
            const std::string payload = line.substr(t2 + 3);
            if (type == 'I') { cache.values[mission][key] = JassValue::Int(std::atoi(payload.c_str())); } else if (type == 'R') { cache.values[mission][key] = JassValue::Real((float)std::atof(payload.c_str())); } else if (type == 'B') { cache.values[mission][key] = JassValue::Bool(payload == "1"); } else if (type == 'S') { cache.values[mission][key] = JassValue::Str(UnescapeCacheString(payload)); }
            ok = true;
        } else if (line[0] == 'U' && line[1] == ' ') {
            const size_t t1 = line.find('\t', 2);
            const size_t sp = (t1 == std::string::npos) ? std::string::npos : line.find(' ', t1 + 1);
            if (t1 == std::string::npos || sp == std::string::npos) continue;
            const std::string mission = UnescapeCacheString(line.substr(2, t1 - 2));
            const std::string key = UnescapeCacheString(line.substr(t1 + 1, sp - t1 - 1));
            unsigned typeId = 0;
            int level = 1, owner = -1;
            float lifeFraction = 1.0f;
            if (std::sscanf(line.c_str() + sp, "%u %d %f %d", &typeId, &level, &lifeFraction, &owner) == 4) {
                JassNativesData::JassStoredUnit unit;
                unit.typeId = typeId;
                unit.level = level;
                unit.lifeFraction = lifeFraction;
                unit.owner = owner;
                cache.units[mission][key] = unit;
                ok = true;
            }
        }
    }
    return ok;
}
}
void JassNatives::RegisterGameCacheNatives() {
    auto getCache = [data = data_](int cacheHandle) -> std::shared_ptr<JassNativesData::JassGameCache> { int simId = data->GetSimHandle(cacheHandle, JassHandleInfo::GAMECACHE); if (simId < 0) return nullptr; auto it = data->gameCaches.find(simId); return it != data->gameCaches.end() ? it->second : nullptr; };
    vm_.RegisterNative("InitGameCache", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty()) return JassValue::Int(0);
        auto cache = std::make_shared<JassNativesData::JassGameCache>();
        cache->campaignFile = args[0].strVal;
        ReadGameCacheFile(GameCacheFileFor(cache->campaignFile), *cache);
        int id = data->nextGameCacheId++;
        data->gameCaches[id] = cache;
        data->gameCacheIdsByFile[cache->campaignFile] = id;
        return JassValue::Int(data->AllocHandle(JassHandleInfo::GAMECACHE, id));
    });
    vm_.RegisterNative("SaveGameCache", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.empty()) return JassValue::Bool(false);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GAMECACHE);
        if (simId < 0) return JassValue::Bool(false);
        auto it = data->gameCaches.find(simId);
        if (it == data->gameCaches.end()) return JassValue::Bool(false);
        return JassValue::Bool(WriteGameCacheFile(GameCacheFileFor(it->second->campaignFile),
                                                  it->second->campaignFile, *it->second));
    });
    vm_.RegisterNative("ReloadGameCachesFromDisk", [data = data_](const std::vector<JassValue>&) -> JassValue {
        bool any = false;
        for (auto& [id, cache] : data->gameCaches) {
            if (!cache || cache->campaignFile.empty()) continue;
            JassNativesData::JassGameCache fresh;
            fresh.campaignFile = cache->campaignFile;
            if (ReadGameCacheFile(GameCacheFileFor(cache->campaignFile), fresh)) {
                *cache = std::move(fresh);
                any = true;
            }
        }
        std::error_code ec;
        fs::path dir(GameCacheSaveDir());
        if (fs::is_directory(dir, ec)) {
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (!entry.is_regular_file()) continue;
                const std::string name = entry.path().filename().string();
                if (name.compare(0, 9, "warsmash_") != 0) continue;
                bool known = false;
                for (const auto& [id, cache] : data->gameCaches) {
                    if (cache && GameCacheFileFor(cache->campaignFile) == entry.path().string()) {
                        known = true;
                        break;
                    }
                }
                if (known) continue;
                auto cache = std::make_shared<JassNativesData::JassGameCache>();
                if (ReadGameCacheFile(entry.path().string(), *cache)) {
                    int id = data->nextGameCacheId++;
                    data->gameCacheIdsByFile[cache->campaignFile] = id;
                    data->gameCaches[id] = cache;
                    any = true;
                }
            }
        }
        return JassValue::Bool(any);
    });
    vm_.RegisterNative("StoreInteger", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto c = getCache(args[0].intVal)) { c->values[args[1].strVal][args[2].strVal] = JassValue::Int(args[3].intVal); } return JassValue::Null(); });
    vm_.RegisterNative("StoreReal", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto c = getCache(args[0].intVal)) { c->values[args[1].strVal][args[2].strVal] = JassValue::Real(args[3].realVal); } return JassValue::Null(); });
    vm_.RegisterNative("StoreBoolean", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Null(); if (auto c = getCache(args[0].intVal)) { c->values[args[1].strVal][args[2].strVal] = JassValue::Bool(args[3].boolVal); } return JassValue::Null(); });
    vm_.RegisterNative("StoreString", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4) return JassValue::Bool(false); if (auto c = getCache(args[0].intVal)) { c->values[args[1].strVal][args[2].strVal] = JassValue::Str(args[3].strVal); return JassValue::Bool(true); } return JassValue::Bool(false); });
    vm_.RegisterNative("StoreUnit", [data = data_, sim = sim_, getCache](const std::vector<JassValue>& args) -> JassValue {
        (void)data;
        if (args.size() < 4 || !sim) return JassValue::Bool(false);
        auto c = getCache(args[0].intVal);
        if (!c) return JassValue::Bool(false);
        int uSim = data->GetSimHandle(args[3].intVal, JassHandleInfo::UNIT);
        CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr;
        if (!unit) return JassValue::Bool(false);
        JassNativesData::JassStoredUnit stored;
        stored.typeId = unit->GetTypeId().value;
        stored.level = unit->GetHeroLevel();
        const float maxLife = unit->GetMaxLife();
        stored.lifeFraction = maxLife > 0.0f ? unit->GetLife() / maxLife : 1.0f;
        stored.owner = unit->GetPlayerIndex();
        c->units[args[1].strVal][args[2].strVal] = stored;
        return JassValue::Bool(true);
    });
    auto haveValue = [getCache](const std::vector<JassValue>& args) -> bool { if (args.size() < 3) return false; auto c = getCache(args[0].intVal); if (!c) return false; auto mit = c->values.find(args[1].strVal); return mit != c->values.end() && mit->second.count(args[2].strVal) > 0; };
    auto haveUnit = [getCache](const std::vector<JassValue>& args) -> bool { if (args.size() < 3) return false; auto c = getCache(args[0].intVal); if (!c) return false; auto mit = c->units.find(args[1].strVal); return mit != c->units.end() && mit->second.count(args[2].strVal) > 0; };
    vm_.RegisterNative("HaveStoredInteger", [haveValue](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveValue(args)); });
    vm_.RegisterNative("HaveStoredReal", [haveValue](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveValue(args)); });
    vm_.RegisterNative("HaveStoredBoolean", [haveValue](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveValue(args)); });
    vm_.RegisterNative("HaveStoredString", [haveValue](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveValue(args)); });
    vm_.RegisterNative("HaveStoredUnit", [haveUnit](const std::vector<JassValue>& args) -> JassValue { return JassValue::Bool(haveUnit(args)); });
    vm_.RegisterNative("GetStoredInteger", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Int(0); auto c = getCache(args[0].intVal); if (!c) return JassValue::Int(0); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Int(0); auto it = mit->second.find(args[2].strVal); return JassValue::Int(it != mit->second.end() ? it->second.intVal : 0); });
    vm_.RegisterNative("GetStoredReal", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Real(0.0f); auto c = getCache(args[0].intVal); if (!c) return JassValue::Real(0.0f); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Real(0.0f); auto it = mit->second.find(args[2].strVal); return JassValue::Real(it != mit->second.end() ? it->second.realVal : 0.0f); });
    vm_.RegisterNative("GetStoredBoolean", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); auto c = getCache(args[0].intVal); if (!c) return JassValue::Bool(false); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Bool(false); auto it = mit->second.find(args[2].strVal); return JassValue::Bool(it != mit->second.end() ? it->second.boolVal : false); });
    vm_.RegisterNative("GetStoredString", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Str(""); auto c = getCache(args[0].intVal); if (!c) return JassValue::Str(""); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Str(""); auto it = mit->second.find(args[2].strVal); return JassValue::Str(it != mit->second.end() ? it->second.strVal : ""); });
    vm_.RegisterNative("SyncStoredInteger", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Int(0); auto c = getCache(args[0].intVal); if (!c) return JassValue::Int(0); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Int(0); auto it = mit->second.find(args[2].strVal); return JassValue::Int(it != mit->second.end() ? it->second.intVal : 0); });
    vm_.RegisterNative("SyncStoredReal", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Real(0.0f); auto c = getCache(args[0].intVal); if (!c) return JassValue::Real(0.0f); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Real(0.0f); auto it = mit->second.find(args[2].strVal); return JassValue::Real(it != mit->second.end() ? it->second.realVal : 0.0f); });
    vm_.RegisterNative("SyncStoredBoolean", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Bool(false); auto c = getCache(args[0].intVal); if (!c) return JassValue::Bool(false); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Bool(false); auto it = mit->second.find(args[2].strVal); return JassValue::Bool(it != mit->second.end() ? it->second.boolVal : false); });
    vm_.RegisterNative("SyncStoredString", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Str(""); auto c = getCache(args[0].intVal); if (!c) return JassValue::Str(""); auto mit = c->values.find(args[1].strVal); if (mit == c->values.end()) return JassValue::Str(""); auto it = mit->second.find(args[2].strVal); return JassValue::Str(it != mit->second.end() ? it->second.strVal : ""); });
    vm_.RegisterNative("SyncStoredUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 3 || !sim) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::GAMECACHE);
        if (simId < 0) return JassValue::Int(0);
        auto it = data->gameCaches.find(simId);
        if (it == data->gameCaches.end()) return JassValue::Int(0);
        auto mit = it->second->units.find(args[1].strVal);
        if (mit == it->second->units.end()) return JassValue::Int(0);
        auto uit = mit->second.find(args[2].strVal);
        if (uit == mit->second.end()) return JassValue::Int(0);
        const auto& stored = uit->second;
        CUnit* unit = sim->CreateUnit(War3ID(stored.typeId), stored.owner, 0.0f, 0.0f, 0.0f);
        if (!unit) return JassValue::Int(0);
        if (stored.level > 1) unit->SetHeroLevel(stored.level);
        unit->SetLife(*sim, unit->GetMaxLife() * stored.lifeFraction);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
    });
    vm_.RegisterNative("RestoreUnit", [data = data_, sim = sim_, getCache](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 7 || !sim) return JassValue::Null();
        auto c = getCache(args[0].intVal);
        if (!c) return JassValue::Null();
        auto mit = c->units.find(args[1].strVal);
        if (mit == c->units.end()) return JassValue::Null();
        auto it = mit->second.find(args[2].strVal);
        if (it == mit->second.end()) return JassValue::Null();
        const auto& stored = it->second;
        const int playerId = ResolvePlayerId(data, args[3].intVal);
        CUnit* unit = sim->CreateUnit(War3ID(stored.typeId), playerId,
                                      args[4].realVal, args[5].realVal, args[6].realVal);
        if (!unit) return JassValue::Null();
        if (stored.level > 1) unit->SetHeroLevel(stored.level);
        unit->SetLife(*sim, unit->GetMaxLife() * stored.lifeFraction);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId()));
    });
    auto flushKey = [getCache](const std::vector<JassValue>& args, bool units) -> void { if (args.size() < 3) return; if (auto c = getCache(args[0].intVal)) { if (units) { auto mit = c->units.find(args[1].strVal); if (mit != c->units.end()) mit->second.erase(args[2].strVal); } else { auto mit = c->values.find(args[1].strVal); if (mit != c->values.end()) mit->second.erase(args[2].strVal); } } };
    vm_.RegisterNative("FlushStoredMission", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (auto c = getCache(args[0].intVal)) { c->values.erase(args[1].strVal); c->units.erase(args[1].strVal); } return JassValue::Null(); });
    vm_.RegisterNative("FlushStoredInteger", [flushKey](const std::vector<JassValue>& args) -> JassValue { flushKey(args, false); return JassValue::Null(); });
    vm_.RegisterNative("FlushStoredReal", [flushKey](const std::vector<JassValue>& args) -> JassValue { flushKey(args, false); return JassValue::Null(); });
    vm_.RegisterNative("FlushStoredBoolean", [flushKey](const std::vector<JassValue>& args) -> JassValue { flushKey(args, false); return JassValue::Null(); });
    vm_.RegisterNative("FlushStoredString", [flushKey](const std::vector<JassValue>& args) -> JassValue { flushKey(args, false); return JassValue::Null(); });
    vm_.RegisterNative("FlushStoredUnit", [flushKey](const std::vector<JassValue>& args) -> JassValue { flushKey(args, true); return JassValue::Null(); });
    vm_.RegisterNative("FlushGameCache", [getCache](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Null(); if (auto c = getCache(args[0].intVal)) { c->values.clear(); c->units.clear(); } return JassValue::Null(); });
}
void JassNatives::RegisterMiscNatives() {
    auto getUnit = [data = data_, sim = sim_](int unitHandle) -> CUnit* { if (!sim) return nullptr; int uSim = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT); return uSim >= 0 ? sim->GetUnit(uSim) : nullptr; };
    auto getDestructable = [data = data_, sim = sim_](int handle) -> CDestructable* { if (!sim) return nullptr; int simId = data->GetSimHandle(handle, JassHandleInfo::DESTRUCTABLE); return simId >= 0 ? sim->GetDestructable(simId) : nullptr; };
    vm_.RegisterNative("ReviveHero", [data = data_, sim = sim_, getUnit](const std::vector<JassValue>& args) -> JassValue { (void)data; if (args.size() < 4 || !sim) return JassValue::Bool(false); CUnit* hero = getUnit(args[0].intVal); if (!hero || !hero->IsHero() || !hero->IsDead()) return JassValue::Bool(false); hero->Revive(*sim, args[1].realVal, args[2].realVal); return JassValue::Bool(true); });
    auto shiftAbilityLevel = [getUnit](const std::vector<JassValue>& args, int delta) -> JassValue { if (args.size() < 2) return JassValue::Int(0); CUnit* unit = getUnit(args[0].intVal); if (!unit) return JassValue::Int(0); CAbility* ability = unit->GetAbility(War3ID((uint32_t)args[1].intVal)); if (!ability) return JassValue::Int(0); ability->SetLevel(std::max(1, ability->GetLevel() + delta)); return JassValue::Int(ability->GetLevel()); };
    vm_.RegisterNative("IncUnitAbilityLevel", [shiftAbilityLevel](const std::vector<JassValue>& args) -> JassValue { return shiftAbilityLevel(args, 1); });
    vm_.RegisterNative("DecUnitAbilityLevel", [shiftAbilityLevel](const std::vector<JassValue>& args) -> JassValue { return shiftAbilityLevel(args, -1); });
    vm_.RegisterNative("SelectUnit", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null(); if (args[1].boolVal) { data->selectedUnits.insert(uSim); } else { data->selectedUnits.erase(uSim); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitBlendTime", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim >= 0 && getUnit(args[0].intVal)) { data->unitBlendTimes[uSim] = args[1].realVal; } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitCreepGuard", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null(); if (args[1].boolVal) { data->creepGuardUnits.insert(uSim); } else { data->creepGuardUnits.erase(uSim); } return JassValue::Null(); });
    vm_.RegisterNative("SetUnitRescuable", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3) return JassValue::Null(); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null(); auto& info = data->unitRescue[uSim]; info.player = ResolvePlayerId(data, args[1].intVal); info.rescuable = args[2].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetUnitRescueRange", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim < 0 || !getUnit(args[0].intVal)) return JassValue::Null(); data->unitRescue[uSim].range = args[1].realVal; return JassValue::Null(); });
    vm_.RegisterNative("SetUnitUseFood", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CUnit* unit = getUnit(args[0].intVal)) unit->SetUseFood(args[1].boolVal); return JassValue::Null(); });
    vm_.RegisterNative("GetUnitPointValueByType", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { (void)data; if (args.empty() || !sim || !sim->GetUnitData()) return JassValue::Int(0); const CUnitType* type = sim->GetUnitData()->GetUnitType(War3ID((uint32_t)args[0].intVal)); return JassValue::Int(type ? type->GetPointValue() : 0); });
    vm_.RegisterNative("GetHeroProperName", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Str(""); CUnit* unit = getUnit(args[0].intVal); if (!unit) return JassValue::Str(""); CAbility* hero = unit->GetAbility(War3ID("AHer")); return JassValue::Str(hero ? hero->GetProperName() : ""); });
    vm_.RegisterNative("IsSuspendedXP", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { (void)data; (void)args; return JassValue::Bool(sim && sim->IsHeroXPSuspended()); });
    vm_.RegisterNative("GetUnitDefaultFlyHeight", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0.0f); CUnit* unit = getUnit(args[0].intVal); const CUnitType* type = unit ? unit->GetUnitType() : nullptr; return JassValue::Real(type ? type->GetDefaultFlyingHeight() : 0.0f); });
    vm_.RegisterNative("GetUnitDefaultPropWindow", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0.0f); CUnit* unit = getUnit(args[0].intVal); const CUnitType* type = unit ? unit->GetUnitType() : nullptr; return JassValue::Real(type ? type->GetPropWindow() : 0.0f); });
    vm_.RegisterNative("GetUnitDefaultTurnSpeed", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0.0f); CUnit* unit = getUnit(args[0].intVal); const CUnitType* type = unit ? unit->GetUnitType() : nullptr; return JassValue::Real(type ? type->GetTurnRate() : 0.0f); });
    vm_.RegisterNative("GetDestructableName", [data = data_, sim = sim_, getDestructable](const std::vector<JassValue>& args) -> JassValue { (void)data; if (args.empty() || !sim || !sim->GetDestructableData()) return JassValue::Str(""); CDestructable* dest = getDestructable(args[0].intVal); if (!dest) return JassValue::Str(""); const CDestructableType* type = sim->GetDestructableData()->GetDestructable(dest->GetTypeId()); return JassValue::Str(type ? type->name : ""); });
    vm_.RegisterNative("IsDestructableInvulnerable", [getDestructable](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CDestructable* dest = getDestructable(args[0].intVal); return JassValue::Bool(dest && dest->IsInvulnerable()); });
    vm_.RegisterNative("SetDestructableInvulnerable", [getDestructable](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CDestructable* dest = getDestructable(args[0].intVal)) dest->SetInvulnerable(args[1].boolVal); return JassValue::Null(); });
    vm_.RegisterNative("ShowDestructable", [getDestructable](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CDestructable* dest = getDestructable(args[0].intVal)) dest->SetHidden(!args[1].boolVal); return JassValue::Null(); });
    vm_.RegisterNative("SetDestructableAnimationSpeed", [getDestructable](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CDestructable* dest = getDestructable(args[0].intVal)) dest->SetAnimationSpeed(args[1].realVal); return JassValue::Null(); });
    vm_.RegisterNative("GetDestructableOccluderHeight", [getDestructable](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Real(0.0f); CDestructable* dest = getDestructable(args[0].intVal); return JassValue::Real(dest ? dest->GetOccluderHeight() : 0.0f); });
    vm_.RegisterNative("SetDestructableOccluderHeight", [getDestructable](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (CDestructable* dest = getDestructable(args[0].intVal)) dest->SetOccluderHeight(args[1].realVal); return JassValue::Null(); });
    vm_.RegisterNative("SetCampaignAvailable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); data->campaignAvailable[args[0].intVal] = args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetMissionAvailable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); if (args.size() < 3) { const int index = args[0].intVal; if (index >= 0 && index < 12) data->gameConfig.missionAvailable[index] = args[1].boolVal; return JassValue::Null(); } data->missionAvailable[{args[0].intVal, args[1].intVal}] = args[2].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetEdCinematicAvailable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); data->edCinematics[args[0].intVal] = args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetOpCinematicAvailable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); data->opCinematics[args[0].intVal] = args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetTutorialCleared", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty()) data->tutorialCleared = args[0].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetCustomCampaignButtonVisible", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); data->customCampaignButtons[args[0].intVal] = args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("GetCustomCampaignButtonVisible", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); auto it = data->customCampaignButtons.find(args[0].intVal); return JassValue::Bool(it != data->customCampaignButtons.end() && it->second); });
    vm_.RegisterNative("ForceCampaignSelectScreen", [data = data_](const std::vector<JassValue>&) -> JassValue { data->forceCampaignSelectScreen = true; return JassValue::Null(); });
    vm_.RegisterNative("SetFogStateRadiusLoc", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 5) return JassValue::Null();
        int locSim = data->GetSimHandle(args[2].intVal, JassHandleInfo::LOCATION);
        if (locSim < 0) return JassValue::Null();
        auto it = data->locations.find(locSim);
        if (it == data->locations.end()) return JassValue::Null();
        JassNativesData::JassFogRadiusRequest req;
        req.player = ResolvePlayerId(data, args[0].intVal);
        req.state = args[1].intVal;
        req.x = it->second->x;
        req.y = it->second->y;
        req.radius = args[3].realVal;
        req.sharedVision = args[4].boolVal;
        data->fogRadiusRequests.push_back(req);
        if (sim && req.player >= 0) {
            const CFogArea area = CFogArea::Circle(req.x, req.y, req.radius);
            const auto state = static_cast<CFogState>(std::clamp(req.state, 0, 2));
            sim->GetFogSystem()->AddOverride(req.player, area, state);
            if (req.sharedVision) {
                const CPlayer* self = sim->GetPlayer(req.player);
                for (int i = 0; i < sim->GetPlayerCount(); i++) {
                    if (i == req.player) continue;
                    const CPlayer* other = sim->GetPlayer(i);
                    if (self && other && self->HasAlliance(*other, CAllianceType::SHARED_VISION)) { sim->GetFogSystem()->AddOverride(i, area, state); }
                }
            }
        }
        return JassValue::Null();
    });
    vm_.RegisterNative("SetDefaultDifficulty", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!args.empty() && data) data->gameConfig.defaultDifficulty = args[0].intVal; return JassValue::Null(); });
    vm_.RegisterNative("ChangeLevel", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); data->pendingChangeLevel = args[0].strVal; data->pendingChangeLevelScore = args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("DoNotSaveReplay", [data = data_](const std::vector<JassValue>&) -> JassValue { data->noReplaySave = true; return JassValue::Null(); });
    vm_.RegisterNative("SyncSelections", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    auto rallyTargetOf = [data = data_, sim = sim_](int unitHandle) -> CWidget* { if (!sim) return nullptr; const int simId = data->GetSimHandle(unitHandle, JassHandleInfo::UNIT); CUnit* unit = sim->GetUnit(simId); if (!unit) return nullptr; CAbility* rally = unit->GetAbility(War3ID("ARal")); if (!rally || !rally->IsExplicitlySet()) return nullptr; return rally->GetRallyTarget(); };
    vm_.RegisterNative("GetUnitRallyUnit", [data = data_, rallyTargetOf](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto* target = rallyTargetOf(args[0].intVal); auto* unit = ((target) ? (target)->AsUnit() : nullptr); if (!unit) return JassValue::Int(0); return JassValue::Int(data->AllocHandle(JassHandleInfo::UNIT, unit->GetHandleId())); });
    vm_.RegisterNative("GetUnitRallyDestructable", [data = data_, rallyTargetOf](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Int(0); auto* target = rallyTargetOf(args[0].intVal); auto* dest = ((target) ? (target)->AsDestructable() : nullptr); if (!dest) return JassValue::Int(0); return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId())); });
    vm_.RegisterNative("SetUnitMovementTypeNoCollision", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); if (uSim < 0) return JassValue::Null(); if (!sim || !sim->GetUnit(uSim)) return JassValue::Null(); if (args[1].boolVal) { data->noCollisionUnits.insert(uSim); } else { data->noCollisionUnits.erase(uSim); } return JassValue::Null(); });
    vm_.RegisterNative("IsUnitMovementDisabled", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !data) return JassValue::Bool(false); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); return JassValue::Bool(uSim >= 0 && data->noCollisionUnits.count(uSim) > 0); });
    vm_.RegisterNative("GetPlayerTypedUnitCount", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 4 || !sim) return JassValue::Int(0);
        const int playerId = ResolvePlayerId(data, args[0].intVal);
        const std::string legacyName = args[1].strVal;
        const bool includeIncomplete = args[2].boolVal;
        int count = 0;
        for (CUnit* unit : sim->GetUnits()) {
            if (!unit || unit->GetPlayerIndex() != playerId) continue;
            const CUnitType* type = unit->GetUnitType();
            if (!type || type->GetLegacyName() != legacyName) continue;
            if (!includeIncomplete && unit->IsConstructing()) continue;
            count++;
        }
        return JassValue::Int(count);
    });
    vm_.RegisterNative("GetGameTurnTick", [sim = sim_](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(sim ? sim->GetGameTurnTick() : 0); });
    vm_.RegisterNative("GetSimulationStepTime", [sim = sim_](const std::vector<JassValue>&) -> JassValue { (void)sim; return JassValue::Real(CSimulation::SIM_STEP_TIME); });
    vm_.RegisterNative("GetGamePlacement", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(data ? data->gameConfig.placement : 0); });
    vm_.RegisterNative("GetResourceDensity", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(data ? data->resourceDensity : 0); });
    vm_.RegisterNative("GetCreatureDensity", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(data ? data->creatureDensity : 0); });
    vm_.RegisterNative("GetPlayerNeutralAggressive", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(24); });
    vm_.RegisterNative("GetBJPlayerNeutralVictim", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(25); });
    vm_.RegisterNative("GetBJPlayerNeutralExtra", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(26); });
    vm_.RegisterNative("GetPlayerNeutralPassive", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(27); });
    vm_.RegisterNative("GetBJMaxPlayers", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(24); });
    vm_.RegisterNative("GetBJMaxPlayerSlots", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(28); });
    vm_.RegisterNative("GetObjectName", [sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.empty() || !sim) return JassValue::Str("");
        const War3ID id((uint32_t)args[0].intVal);
        if (sim->GetUnitData()) { if (const CUnitType* t = sim->GetUnitData()->GetUnitType(id)) return JassValue::Str(t->GetName()); }
        if (sim->GetAbilityData()) { if (const CAbilityDefFull* a = sim->GetAbilityData()->GetAbility(id)) return JassValue::Str(a->name); }
        if (sim->GetItemData()) { if (const CItemType* it = sim->GetItemData()->GetItem(id)) return JassValue::Str(it->name); }
        if (sim->GetDestructableData()) {
            if (const CDestructableType* d = sim->GetDestructableData()->GetDestructable(id)) { return JassValue::Str(d->name); }
        }
        return JassValue::Str("");
    });
    vm_.RegisterNative("LogError", [](const std::vector<JassValue>& args) -> JassValue { if (!args.empty()) std::cerr << args[0].strVal << std::endl; return JassValue::Null(); });
    vm_.RegisterNative("StartThread", [this](const std::vector<JassValue>& args) -> JassValue { if (!args.empty() && !args[0].strVal.empty()) QueueFunction(args[0].strVal); return JassValue::Null(); });
    vm_.RegisterNative("StartAbilityBuilderThread", [this](const std::vector<JassValue>& args) -> JassValue { if (!args.empty() && !args[0].strVal.empty()) QueueFunction(args[0].strVal); return JassValue::Null(); });
    vm_.RegisterNative("Sleep", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("SetPreferredSelectionReplacement", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); const int gone = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); const int want = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT); if (gone >= 0 && want >= 0) data->preferredSelectionReplacement[gone] = want; return JassValue::Null(); });
    vm_.RegisterNative("BlzCreateDestructableWithSkin", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 6 || !sim) return JassValue::Int(0);
        War3ID w3id((uint32_t)args[0].intVal);
        float x = args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal;
        float y = args[2].type == JassValue::REAL ? args[2].realVal : (float)args[2].intVal;
        auto* dest = sim->CreateDestructable(w3id, x, y, 100.0f);
        if (!dest) return JassValue::Int(0);
        return JassValue::Int(data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId()));
    });
    vm_.RegisterNative("BlzSetUnitFacingEx", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim) return JassValue::Null(); int uSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT); CUnit* unit = uSim >= 0 ? sim->GetUnit(uSim) : nullptr; if (!unit) return JassValue::Null(); unit->SetFacing(args[1].type == JassValue::REAL ? args[1].realVal : (float)args[1].intVal); return JassValue::Null(); });
    auto groupAddGroup = [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int dstSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        int srcSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::GROUP);
        if (dstSim < 0 || srcSim < 0) return JassValue::Null();
        auto dit = data->groups.find(dstSim);
        auto sit = data->groups.find(srcSim);
        if (dit == data->groups.end() || sit == data->groups.end()) return JassValue::Null();
        for (int uh : sit->second->unitHandles) {
            auto& vec = dit->second->unitHandles;
            if (std::find(vec.begin(), vec.end(), uh) == vec.end()) vec.push_back(uh);
        }
        return JassValue::Null();
    };
    auto groupRemoveGroup = [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Null();
        int dstSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::GROUP);
        int srcSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::GROUP);
        if (dstSim < 0 || srcSim < 0) return JassValue::Null();
        auto dit = data->groups.find(dstSim);
        auto sit = data->groups.find(srcSim);
        if (dit == data->groups.end() || sit == data->groups.end()) return JassValue::Null();
        auto& vec = dit->second->unitHandles;
        for (int uh : sit->second->unitHandles) {
            vec.erase(std::remove(vec.begin(), vec.end(), uh), vec.end());
        }
        return JassValue::Null();
    };
    vm_.RegisterNative("GroupAddGroupFast", groupAddGroup);
    vm_.RegisterNative("BlzGroupAddGroupFast", groupAddGroup);
    vm_.RegisterNative("GroupRemoveGroupFast", groupRemoveGroup);
    vm_.RegisterNative("BlzGroupRemoveGroupFast", groupRemoveGroup);
    vm_.RegisterNative("CheckUnitForAbilityEffectReaction", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Bool(false);
        int tSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        int cSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::UNIT);
        CUnit* target = tSim >= 0 ? sim->GetUnit(tSim) : nullptr;
        CUnit* caster = cSim >= 0 ? sim->GetUnit(cSim) : nullptr;
        if (!target) return JassValue::Bool(false);
        return JassValue::Bool(target->CheckSpellEffectReaction(*sim, caster));
    });
    vm_.RegisterNative("ShowInterfaceError", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2) return JassValue::Null(); const int player = ResolvePlayerId(data, args[0].intVal); std::cout << "[JASS-ERR->P" << player << "] " << args[1].strVal << std::endl; return JassValue::Null(); });
    vm_.RegisterNative("StartTrainingUnit", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !sim) return JassValue::Null();
        int fSim = data->GetSimHandle(args[0].intVal, JassHandleInfo::UNIT);
        CUnit* factory = fSim >= 0 ? sim->GetUnit(fSim) : nullptr;
        if (!factory) return JassValue::Null();
        const War3ID resultId((uint32_t)args[1].intVal);
        for (CAbility* ability : factory->GetAbilities()) {
            if (ability && ability->GetCategory() == CAbilityCategory::QUEUE) {
                ability->QueueUnit(*sim, factory, resultId);
                return JassValue::Null();
            }
        }
        return JassValue::Null();
    });
    auto loadScriptFile = [this](const std::vector<JassValue>& args, const std::string& dflt) -> JassValue {
        if (args.empty() || !data_) return JassValue::Null();
        const std::string& path = args[0].strVal;
        std::string func = dflt;
        if (args.size() > 1 && !args[1].strVal.empty()) func = args[1].strVal;
        if (path.empty() || func.empty() || !data_->scriptReader) return JassValue::Null();
        std::vector<uint8_t> bytes = data_->scriptReader(path);
        if (bytes.empty()) return JassValue::Null();
        const bool hadFunc = vm_.HasFunction(func);
        if (!vm_.LoadScript(std::string((const char*)bytes.data(), bytes.size()))) { return JassValue::Null(); }
        if (!hadFunc && vm_.HasFunction(func)) QueueFunction(func);
        return JassValue::Null();
    };
    vm_.RegisterNative("LoadScriptFile", [loadScriptFile](const std::vector<JassValue>& args) -> JassValue { return loadScriptFile(args, "main"); });
    vm_.RegisterNative("Preloader", [loadScriptFile](const std::vector<JassValue>& args) -> JassValue { return loadScriptFile(args, "PreloadFiles"); });
    auto jassValueEquals = [](const JassValue& a, const JassValue& b) -> bool { if (a.type != b.type) return false; switch (a.type) { case JassValue::INTEGER: return a.intVal == b.intVal; case JassValue::REAL: return a.realVal == b.realVal; case JassValue::BOOLEAN: return a.boolVal == b.boolVal; case JassValue::STRING: return a.strVal == b.strVal; default: return true; } };
    vm_.RegisterNative("CreateHandleList", [data = data_](const std::vector<JassValue>&) -> JassValue { if (!data) return JassValue::Int(0); int id = data->nextHandleListId++; data->handleLists[id] = {}; return JassValue::Int(data->AllocHandle(JassHandleInfo::HANDLE_LIST, id)); });
    vm_.RegisterNative("HandleListAdd", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::HANDLE_LIST); if (simId < 0) return JassValue::Null(); data->handleLists[simId].push_back(args[1]); return JassValue::Null(); });
    vm_.RegisterNative("HandleListRemove", [data = data_, jassValueEquals](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::HANDLE_LIST);
        if (simId < 0) return JassValue::Null();
        auto it = data->handleLists.find(simId);
        if (it == data->handleLists.end()) return JassValue::Null();
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [&](const JassValue& v) { return jassValueEquals(v, args[1]); }),
                  vec.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("HandleListGet", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Int(0);
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::HANDLE_LIST);
        if (simId < 0) return JassValue::Int(0);
        auto it = data->handleLists.find(simId);
        if (it == data->handleLists.end() || args[1].intVal < 0 ||
            args[1].intVal >= (int)it->second.size()) {
            return JassValue::Int(0);
        }
        return it->second[(size_t)args[1].intVal];
    });
    vm_.RegisterNative("HandleListSize", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !data) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::HANDLE_LIST); if (simId < 0) return JassValue::Int(0); auto it = data->handleLists.find(simId); return JassValue::Int(it != data->handleLists.end() ? (int)it->second.size() : 0); });
    vm_.RegisterNative("CreateStringList", [data = data_](const std::vector<JassValue>&) -> JassValue { if (!data) return JassValue::Int(0); int id = data->nextStringListId++; data->stringLists[id] = {}; return JassValue::Int(data->AllocHandle(JassHandleInfo::STRING_LIST, id)); });
    vm_.RegisterNative("StringListAdd", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::STRING_LIST); if (simId < 0) return JassValue::Null(); data->stringLists[simId].push_back(args[1].strVal); return JassValue::Null(); });
    vm_.RegisterNative("StringListRemove", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::STRING_LIST);
        if (simId < 0) return JassValue::Null();
        auto it = data->stringLists.find(simId);
        if (it == data->stringLists.end()) return JassValue::Null();
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), args[1].strVal), vec.end());
        return JassValue::Null();
    });
    vm_.RegisterNative("StringListGet", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Str("");
        int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::STRING_LIST);
        if (simId < 0) return JassValue::Str("");
        auto it = data->stringLists.find(simId);
        if (it == data->stringLists.end() || args[1].intVal < 0 ||
            args[1].intVal >= (int)it->second.size()) {
            return JassValue::Str("");
        }
        return JassValue::Str(it->second[(size_t)args[1].intVal]);
    });
    vm_.RegisterNative("StringListSize", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !data) return JassValue::Int(0); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::STRING_LIST); if (simId < 0) return JassValue::Int(0); auto it = data->stringLists.find(simId); return JassValue::Int(it != data->stringLists.end() ? (int)it->second.size() : 0); });
    vm_.RegisterNative("IntExpr", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !data) return JassValue::Int(0); int id = data->nextIntExprId++; data->intExprs[id] = args[0].strVal; return JassValue::Int(data->AllocHandle(JassHandleInfo::INT_EXPR, id)); });
    vm_.RegisterNative("DestroyIntExpr", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !data) return JassValue::Null(); int simId = data->GetSimHandle(args[0].intVal, JassHandleInfo::INT_EXPR); if (simId >= 0) data->intExprs.erase(simId); return JassValue::Null(); });
    vm_.RegisterNative("ForFiles", [data = data_, vmPtr = vmPtr_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Null();
        const std::string& dir = args[0].strVal;
        const std::string& func = args[1].strVal;
        if (dir.empty() || func.empty() || !vmPtr || !data->fileLister) return JassValue::Null();
        auto lower = [](std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; };
        const std::string prefix = lower(dir);
        std::string prefixSlash = prefix;
        for (auto& c : prefixSlash) {
            if (c == '\\') c = '/';
        }
        std::string pattern = dir;
        for (auto& c : pattern) {
            if (c == '/') c = '\\';
        }
        pattern += '*';
        std::unordered_map<std::string, std::string> dedup;
        for (const std::string& path : data->fileLister(pattern)) {
            dedup.try_emplace(lower(path), path);
        }
        std::vector<std::string> matches;
        for (const auto& [low, real] : dedup) {
            std::string lowSlash = low;
            for (auto& c : lowSlash) {
                if (c == '\\') c = '/';
            }
            if (low.rfind(prefix, 0) == 0 || lowSlash.rfind(prefixSlash, 0) == 0) { matches.push_back(real); }
        }
        std::sort(matches.begin(), matches.end());
        const std::string prev = data->enumFilePath;
        for (const std::string& real : matches) {
            data->enumFilePath = real;
            ExecuteSyncFunction(vmPtr, func);
        }
        data->enumFilePath = prev;
        return JassValue::Null();
    });
    vm_.RegisterNative("GetEnumFilePath", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Str(data ? data->enumFilePath : ""); });
    auto eventUnitOrNull = [data = data_](int simHandle) -> JassValue { return simHandle >= 0 ? JassValue::Int(simHandle) : JassValue::Null(); };
    vm_.RegisterNative("GetDecayingUnit", [data = data_, eventUnitOrNull](const std::vector<JassValue>&) -> JassValue { return eventUnitOrNull(data->eventDecayingUnitHandle); });
    vm_.RegisterNative("GetDetectedUnit", [data = data_, eventUnitOrNull](const std::vector<JassValue>&) -> JassValue { return eventUnitOrNull(data->currentTriggerUnitHandle); });
    vm_.RegisterNative("GetEventDetectingPlayer", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("GetEventPlayerState", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetEventTargetUnit", [data = data_, eventUnitOrNull](const std::vector<JassValue>&) -> JassValue { if (data->eventSpellTargetUnitHandle >= 0) return eventUnitOrNull(data->eventSpellTargetUnitHandle); return eventUnitOrNull(data->eventOrderTargetHandle); });
    vm_.RegisterNative("GetEventUnitState", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetLearnedSkill", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(data ? data->eventLearnedSkillId : 0); });
    vm_.RegisterNative("GetLearnedSkillLevel", [data = data_](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(data ? data->eventLearnedSkillLevel : 0); });
    vm_.RegisterNative("GetPlayerScore", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetRescuer", [data = data_](const std::vector<JassValue>&) -> JassValue { return data->eventRescuerUnitHandle >= 0 ? JassValue::Int(data->eventRescuerUnitHandle) : JassValue::Int(0); });
    vm_.RegisterNative("GetRevivableUnit", [data = data_, eventUnitOrNull](const std::vector<JassValue>&) -> JassValue { return eventUnitOrNull(data->eventDyingUnitHandle); });
    vm_.RegisterNative("GetSpellAbility", [data = data_, sim = sim_](const std::vector<JassValue>&) -> JassValue {
        if (!data || !sim || data->eventSpellAbilityId == 0) return JassValue::Null();
        CUnit* caster = sim->GetUnit(data->eventSpellAbilityUnitHandle);
        CAbility* ability = caster ? caster->GetAbility(War3ID((uint32_t)data->eventSpellAbilityId)) : nullptr;
        const int level = ability ? ability->GetLevel() : 1;
        const int owner = caster ? caster->GetHandleId() : -1;
        return JassValue::Int(AbilityToHandle(*data, War3ID((uint32_t)data->eventSpellAbilityId), level, owner));
    });
    vm_.RegisterNative("GetSpellTargetDestructable", [data = data_](const std::vector<JassValue>&) -> JassValue { if (!data || data->eventSpellTargetDestructableHandle < 0) return JassValue::Int(0); return JassValue::Int(data->eventSpellTargetDestructableHandle); });
    vm_.RegisterNative("GetSpellTargetItem", [data = data_](const std::vector<JassValue>&) -> JassValue { if (!data || data->eventSpellTargetItemHandle < 0) return JassValue::Int(0); return JassValue::Int(data->eventSpellTargetItemHandle); });
    vm_.RegisterNative("GetTournamentFinishNowPlayer", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    vm_.RegisterNative("GetTournamentFinishNowRule", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetTournamentFinishSoonTimeRemaining", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Real(0.0f); });
    vm_.RegisterNative("GetTournamentScore", [](const std::vector<JassValue>&) -> JassValue { return JassValue::Int(0); });
    vm_.RegisterNative("GetWinningPlayer", [](const std::vector<JassValue>&) { return JassValue::Null(); });
    auto fogStateAt = [data = data_, sim = sim_](int playerArg, float x, float y, CFogState want) -> JassValue { if (!sim || !data) return JassValue::Bool(false); const int player = ResolvePlayerId(data, playerArg); if (player < 0) return JassValue::Bool(false); return JassValue::Bool(sim->GetFogSystem()->GetState(player, x, y) == want); };
    vm_.RegisterNative("IsFoggedToPlayer", [data = data_, sim = sim_, fogStateAt](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Bool(false); return fogStateAt(args[2].intVal, args[0].realVal, args[1].realVal, CFogState::FOGGED); });
    vm_.RegisterNative("IsMaskedToPlayer", [data = data_, sim = sim_, fogStateAt](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !sim) return JassValue::Bool(false); return fogStateAt(args[2].intVal, args[0].realVal, args[1].realVal, CFogState::MASKED); });
    vm_.RegisterNative("IsLocationFoggedToPlayer", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !sim) return JassValue::Bool(false);
        const int locId = data->GetSimHandle(args[0].intVal, JassHandleInfo::LOCATION);
        auto it = data->locations.find(locId);
        if (locId < 0 || it == data->locations.end() || !it->second) return JassValue::Bool(false);
        const int player = ResolvePlayerId(data, args[1].intVal);
        if (player < 0) return JassValue::Bool(false);
        return JassValue::Bool(sim->GetFogSystem()->GetState(player, it->second->x, it->second->y) == CFogState::FOGGED);
    });
    vm_.RegisterNative("IsVisibleToPlayer", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 3 || !data || !sim) return JassValue::Bool(false); const int player = ResolvePlayerId(data, args[2].intVal); if (player < 0) return JassValue::Bool(false); return JassValue::Bool(sim->GetFogSystem()->IsVisibleTo(player, args[0].realVal, args[1].realVal)); });
    auto unitFogged = [data = data_, sim = sim_, getUnit](int unitHandle, CFogState want) -> JassValue { CUnit* unit = getUnit(unitHandle); if (!unit || !sim) return JassValue::Bool(false); const int local = data ? data->localPlayerIndex : 0; return JassValue::Bool(sim->GetFogSystem()->GetState(local, unit->GetX(), unit->GetY()) == want); };
    vm_.RegisterNative("IsUnitFogged", [unitFogged](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); return unitFogged(args[0].intVal, CFogState::FOGGED); });
    vm_.RegisterNative("IsUnitMasked", [unitFogged](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); return unitFogged(args[0].intVal, CFogState::MASKED); });
    vm_.RegisterNative("IsUnitDetected", [data = data_, sim = sim_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data || !sim) return JassValue::Bool(false);
        CUnit* unit = getUnit(args[0].intVal);
        if (!unit) return JassValue::Bool(false);
        const int player = ResolvePlayerId(data, args[1].intVal);
        if (player < 0) return JassValue::Bool(false);
        if (unit->IsInvisible()) return JassValue::Bool(false);
        return JassValue::Bool(sim->GetFogSystem()->IsVisibleTo(player, unit->GetX(), unit->GetY()));
    });
    vm_.RegisterNative("IsUnitIllusion", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CUnit* unit = getUnit(args[0].intVal); return JassValue::Bool(unit && unit->IsIllusion()); });
    vm_.RegisterNative("IsUnitInTransport", [getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2) return JassValue::Bool(false);
        CUnit* unit = getUnit(args[0].intVal);
        CUnit* transport = getUnit(args[1].intVal);
        if (!unit || !transport) return JassValue::Bool(false);
        if (!unit->IsLoadedAboard()) return JassValue::Bool(false);
        for (CUnit* passenger : transport->GetCargoUnits()) {
            if (passenger == unit) return JassValue::Bool(true);
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsUnitLoaded", [getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.empty()) return JassValue::Bool(false); CUnit* unit = getUnit(args[0].intVal); return JassValue::Bool(unit && unit->IsLoadedAboard()); });
    vm_.RegisterNative("IsUnitOwnedByPlayer", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Bool(false); CUnit* unit = getUnit(args[0].intVal); if (!unit) return JassValue::Bool(false); return JassValue::Bool(unit->GetPlayerIndex() == ResolvePlayerId(data, args[1].intVal)); });
    vm_.RegisterNative("IsUnitInForce", [data = data_, getUnit](const std::vector<JassValue>& args) -> JassValue {
        if (args.size() < 2 || !data) return JassValue::Bool(false);
        CUnit* unit = getUnit(args[0].intVal);
        if (!unit) return JassValue::Bool(false);
        const int forceSim = data->GetSimHandle(args[1].intVal, JassHandleInfo::FORCE);
        auto it = data->forces.find(forceSim);
        if (it == data->forces.end() || !it->second) return JassValue::Bool(false);
        for (int pid : it->second->playerIds) {
            if (pid == unit->GetPlayerIndex()) return JassValue::Bool(true);
        }
        return JassValue::Bool(false);
    });
    vm_.RegisterNative("IsHeroUnitId", [data = data_, sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.empty() || !sim || !sim->GetUnitData()) return JassValue::Bool(false); CUnitType* type = sim->GetUnitData()->GetUnitType(War3ID((uint32_t)args[0].intVal)); return JassValue::Bool(type && type->IsHero()); });
    vm_.RegisterNative("SetUnitPointValueByType", [sim = sim_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !sim || !sim->GetUnitData()) return JassValue::Null(); CUnitType* type = sim->GetUnitData()->GetUnitType(War3ID((uint32_t)args[0].intVal)); if (type) type->SetPointValue(args[1].intVal); return JassValue::Null(); });
    vm_.RegisterNative("SaveGame", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.empty()) return JassValue::Bool(false);
        const std::string slot = args[0].strVal;
        if (!WriteSaveSlotMetadata(slot, data->saveMapPath)) return JassValue::Bool(false);
        for (const auto& [id, cache] : data->gameCaches) {
            (void)id;
            if (cache && !cache->campaignFile.empty()) {
                WriteGameCacheFile(GameCacheFileFor(cache->campaignFile),
                                   cache->campaignFile, *cache);
            }
        }
        data->lastSaveSlot = SaveSlotSafeName(slot);
        return JassValue::Bool(true);
    });
    vm_.RegisterNative("LoadGame", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.size() < 1) return JassValue::Null(); std::error_code ec; if (!fs::is_regular_file(SaveSlotMetadata(args[0].strVal), ec) || ec) return JassValue::Null(); data->pendingLoad.kind = 1; data->pendingLoad.slot = SaveSlotSafeName(args[0].strVal); data->pendingLoad.scoreScreen = args.size() > 1 && args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("ReloadGame", [data = data_](const std::vector<JassValue>&) -> JassValue { if (!data) return JassValue::Null(); data->pendingLoad.kind = 2; data->pendingLoad.slot.clear(); data->pendingLoad.scoreScreen = false; return JassValue::Null(); });
    vm_.RegisterNative("RestartGame", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data) return JassValue::Null(); data->pendingLoad.kind = 3; data->pendingLoad.slot.clear(); data->pendingLoad.scoreScreen = !args.empty() && args[0].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("CopySaveGame", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.size() < 2) return JassValue::Bool(false);
        (void)data;
        std::error_code ec;
        const std::string src = SaveSlotDir(args[0].strVal);
        const std::string dst = SaveSlotDir(args[1].strVal);
        if (!fs::is_directory(src, ec) || ec || src == dst) return JassValue::Bool(false);
        ec.clear();
        fs::remove_all(dst, ec);
        if (ec) return JassValue::Bool(false);
        ec.clear();
        fs::copy(src, dst, fs::copy_options::recursive, ec);
        return JassValue::Bool(!ec);
    });
    vm_.RegisterNative("RemoveSaveDirectory", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (!data || args.empty()) return JassValue::Bool(false); (void)data; std::error_code ec; const std::string dir = SaveSlotDir(args[0].strVal); if (!fs::is_directory(dir, ec) || ec) return JassValue::Bool(false); fs::remove_all(dir, ec); return JassValue::Bool(!ec); });
    vm_.RegisterNative("RenameSaveDirectory", [data = data_](const std::vector<JassValue>& args) -> JassValue {
        if (!data || args.size() < 2) return JassValue::Bool(false);
        (void)data;
        std::error_code ec;
        const std::string src = SaveSlotDir(args[0].strVal);
        const std::string dst = SaveSlotDir(args[1].strVal);
        if (!fs::is_directory(src, ec) || ec || src == dst) return JassValue::Bool(false);
        ec.clear();
        if (fs::exists(dst, ec) || ec) return JassValue::Bool(false);
        ec.clear();
        fs::rename(src, dst, ec);
        return JassValue::Bool(!ec);
    });
    vm_.RegisterNative("SetPlayerController", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); const int p = ResolvePlayerId(data, args[0].intVal); if (p >= 0 && p < 24) data->playerMapConfig[p].controller = args[1].intVal; return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerRacePreference", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); const int p = ResolvePlayerId(data, args[0].intVal); if (p >= 0 && p < 24) data->playerMapConfig[p].racePreference = args[1].intVal; return JassValue::Null(); });
    vm_.RegisterNative("SetPlayerRaceSelectable", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); const int p = ResolvePlayerId(data, args[0].intVal); if (p >= 0 && p < 24) data->playerMapConfig[p].raceSelectable = args[1].boolVal; return JassValue::Null(); });
    vm_.RegisterNative("SetStartLocPrioCount", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 2 || !data) return JassValue::Null(); const int idx = args[0].intVal; if (idx >= 0 && idx < 24) data->startLocations[idx].prio.assign(std::max(0, args[1].intVal), {-1, 0}); return JassValue::Null(); });
    vm_.RegisterNative("SetStartLocPrio", [data = data_](const std::vector<JassValue>& args) -> JassValue { if (args.size() < 4 || !data) return JassValue::Null(); const int idx = args[0].intVal, slot = args[1].intVal; if (idx >= 0 && idx < 24 && slot >= 0) { auto& prio = data->startLocations[idx].prio; if ((int)prio.size() <= slot) prio.resize(slot + 1, {-1, 0}); prio[slot] = {args[2].intVal, args[3].intVal}; } return JassValue::Null(); });
}

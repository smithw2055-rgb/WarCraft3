// Batch-3 native tests: item/unit pools, random creep/NP building picks,
// shop stock state machine + purchase gating (ref pool/stock natives).
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/gameplay_constants.h"
#include "data/itemdata.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_sell_items.h"
#include "ecs/csimulation.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakePoolType(const char* name, War3ID id, bool hero) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        name, name, "icon", id,
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::NONE,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, hero, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

void OpenGameData(MpqManager& mpq) {
    mpq.Open((fs::path(PROJECT_SOURCE_DIR) / "resources" / "War3Patch.mpq").string());
    mpq.Open((fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq").string());
    mpq.Open((fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3.mpq").string());
}

} // namespace

bool TestPoolPlaceRandomItem() {
    if (!ResourcesPresent()) return true; // SKIP without game data
    MpqManager mpq;
    OpenGameData(mpq);
    CItemData itemData = CItemData::LoadFromMpq(mpq);
    TEST_ASSERT(itemData.HasItem(War3ID("modt")));
    TEST_ASSERT(itemData.HasItem(War3ID("gcel")));

    JassVM vm;
    CSimulation sim;
    sim.SetItemData(&itemData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    const int pool = CallNative(vm, "CreateItemPool", {}).intVal;
    TEST_ASSERT(pool != 0);
    CallNative(vm, "ItemPoolAddItemType", {JassValue::Int(pool), JassValue::Int(0x6D6F6474 /*modt*/), JassValue::Real(1)});
    CallNative(vm, "ItemPoolAddItemType", {JassValue::Int(pool), JassValue::Int(0x6763656C /*gcel*/), JassValue::Real(3)});

    bool sawModt = false, sawGcel = false;
    for (int i = 0; i < 20; i++) {
        const int item = CallNative(vm, "PlaceRandomItem",
            {JassValue::Int(pool), JassValue::Real(100), JassValue::Real(100)}).intVal;
        TEST_ASSERT(item != 0);
        const int itemSim = data->GetSimHandle(item, JassHandleInfo::ITEM);
        TEST_ASSERT(itemSim >= 0);
        CItem* placed = sim.GetItem(itemSim);
        TEST_ASSERT(placed != nullptr);
        if (placed->GetTypeId() == War3ID("modt")) sawModt = true;
        if (placed->GetTypeId() == War3ID("gcel")) sawGcel = true;
    }
    TEST_ASSERT(sawModt && sawGcel);

    // Removal narrows the pool to gcel only.
    CallNative(vm, "ItemPoolRemoveItemType", {JassValue::Int(pool), JassValue::Int(0x6D6F6474)});
    for (int i = 0; i < 5; i++) {
        const int item = CallNative(vm, "PlaceRandomItem",
            {JassValue::Int(pool), JassValue::Real(0), JassValue::Real(0)}).intVal;
        TEST_ASSERT(item != 0);
        TEST_ASSERT(sim.GetItem(data->GetSimHandle(item, JassHandleInfo::ITEM))->GetTypeId() == War3ID("gcel"));
    }
    CallNative(vm, "DestroyItemPool", {JassValue::Int(pool)});
    TEST_ASSERT(CallNative(vm, "PlaceRandomItem",
        {JassValue::Int(pool), JassValue::Real(0), JassValue::Real(0)}).type == JassValue::NULL_VAL);
    return true;
}

bool TestPoolPlaceRandomUnit() {
    if (!ResourcesPresent()) return true; // SKIP without game data
    MpqManager mpq;
    OpenGameData(mpq);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    TEST_ASSERT(unitData.HasUnitType(War3ID("hfoo")));

    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.SetUnitData(&unitData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    const int pool = CallNative(vm, "CreateUnitPool", {}).intVal;
    TEST_ASSERT(pool != 0);
    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;
    CallNative(vm, "UnitPoolAddUnitType", {JassValue::Int(pool), JassValue::Int(0x68666F6F /*hfoo*/), JassValue::Real(1)});
    const int uh = CallNative(vm, "PlaceRandomUnit",
        {JassValue::Int(pool), JassValue::Int(p0), JassValue::Real(300), JassValue::Real(300), JassValue::Real(0)}).intVal;
    TEST_ASSERT(uh != 0);
    const int uSim = data->GetSimHandle(uh, JassHandleInfo::UNIT);
    TEST_ASSERT(uSim >= 0);
    TEST_ASSERT(sim.GetUnit(uSim) != nullptr);
    TEST_ASSERT(sim.GetUnit(uSim)->GetTypeId() == War3ID("hfoo"));

    CallNative(vm, "UnitPoolRemoveUnitType", {JassValue::Int(pool), JassValue::Int(0x68666F6F)});
    TEST_ASSERT(CallNative(vm, "PlaceRandomUnit",
        {JassValue::Int(pool), JassValue::Int(p0), JassValue::Real(0), JassValue::Real(0), JassValue::Real(0)}).type == JassValue::NULL_VAL);
    CallNative(vm, "DestroyUnitPool", {JassValue::Int(pool)});
    return true;
}

bool TestChooseRandomCreep() {
    // No unit data: safe zero.
    {
        JassVM vm;
        CSimulation sim;
        JassNatives natives(vm, &sim);
        natives.RegisterAll();
        TEST_ASSERT(CallNative(vm, "ChooseRandomCreep", {JassValue::Int(1)}).intVal == 0);
        TEST_ASSERT(CallNative(vm, "ChooseRandomNPBuilding", {}).intVal == 0);
    }
    if (!ResourcesPresent()) return true; // SKIP data-driven part
    MpqManager mpq;
    OpenGameData(mpq);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);

    JassVM vm;
    CSimulation sim;
    sim.SetUnitData(&unitData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    const int creep = CallNative(vm, "ChooseRandomCreep", {JassValue::Int(1)}).intVal;
    TEST_ASSERT(creep != 0);
    {
        const CUnitType* type = unitData.GetUnitType(War3ID((uint32_t)creep));
        TEST_ASSERT(type != nullptr);
        TEST_ASSERT(type->GetLevel() == 1);
        TEST_ASSERT(!type->IsBuilding());
    }
    // NP building: either a building type or 0 when the data has none.
    const int npb = CallNative(vm, "ChooseRandomNPBuilding", {}).intVal;
    if (npb != 0) {
        const CUnitType* type = unitData.GetUnitType(War3ID((uint32_t)npb));
        TEST_ASSERT(type != nullptr);
        TEST_ASSERT(type->IsBuilding());
    }
    return true;
}

bool TestShopStockStateMachine() {
    CSimulation sim;
    CAbilitySellItems shop(9001);
    shop.AddStockItem(War3ID("modt"), 2, 3);
    TEST_ASSERT(shop.FindStock(War3ID("modt")) != nullptr);
    TEST_ASSERT(shop.FindStock(War3ID("modt"))->current == 2);
    TEST_ASSERT(shop.ConsumeStock(War3ID("modt")));
    TEST_ASSERT(shop.ConsumeStock(War3ID("modt")));
    TEST_ASSERT(!shop.ConsumeStock(War3ID("modt"))); // exhausted
    TEST_ASSERT(shop.FindStock(War3ID("XXXX")) == nullptr);
    TEST_ASSERT(shop.ConsumeStock(War3ID("XXXX"))); // no entry: unlimited
    TEST_ASSERT(shop.RemoveStock(War3ID("modt")));
    TEST_ASSERT(shop.FindStock(War3ID("modt")) == nullptr);

    // Replenish: interval 30s, sim ticks at 20/s; 0 -> 1 after 30s.
    shop.AddStockItem(War3ID("gcel"), 0, 2);
    shop.SetStockReplenishInterval(30.0f);
    CUnit* dummy = sim.AdoptUnit(std::make_unique<CUnit>(8301, MakePoolType("Shop", War3ID("ngol"), false), 0, War3ID("ngol"), 0, 0, 0));
    sim.Update();
    for (int i = 0; i < 20 * 31; i++) shop.OnTick(sim, dummy);
    TEST_ASSERT(shop.FindStock(War3ID("gcel"))->current == 1);
    return true;
}

bool TestShopPurchaseConsumesStock() {
    if (!ResourcesPresent()) return true; // SKIP without game data
    MpqManager mpq;
    OpenGameData(mpq);
    CItemData itemData = CItemData::LoadFromMpq(mpq);
    TEST_ASSERT(itemData.HasItem(War3ID("gcel")));

    JassVM vm;
    CSimulation sim;
    auto player = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
    player->SetGold(100000);
    player->SetLumber(100000);
    sim.AddPlayer(std::move(player));
    sim.SetItemData(&itemData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // Shop with sell ability + hero with inventory.
    auto* shopUnit = sim.AdoptUnit(std::make_unique<CUnit>(8401, MakePoolType("Shop", War3ID("ngol"), false), 0, War3ID("ngol"), 0, 0, 0));
    auto sell = std::make_unique<CAbilitySellItems>(8402, std::vector<War3ID>{War3ID("gcel")});
    CAbilitySellItems* sellPtr = sell.get();
    shopUnit->AddAbility(sim, std::move(sell));
    auto* hero = sim.AdoptUnit(std::make_unique<CUnit>(8403, MakePoolType("Hero", War3ID("Hpal"), true), 0, War3ID("Hpal"), 500, 500, 0));
    hero->AddAbility(sim, std::make_unique<CAbilityHero>(8404));
    hero->AddAbility(sim, std::make_unique<CAbilityInventory>(8405));
    sim.GetPlayer(0)->AddUnit(shopUnit);
    sim.GetPlayer(0)->AddUnit(hero);
    sim.Update();
    const int shopHandle = data->AllocHandle(JassHandleInfo::UNIT, 8401);

    // Stock 1 via native; purchase order 0x100|player(0) buys index 0.
    CallNative(vm, "AddItemToStock", {JassValue::Int(shopHandle), JassValue::Int(0x6763656C /*gcel*/), JassValue::Int(1), JassValue::Int(1)});
    TEST_ASSERT(sellPtr->FindStock(War3ID("gcel"))->current == 1);
    TEST_ASSERT(!sellPtr->CheckBeforeQueueNoTarget(sim, shopUnit, 0x100 | 0));
    TEST_ASSERT(sellPtr->FindStock(War3ID("gcel"))->current == 0);
    // Hero received the item.
    auto* inv = dynamic_cast<CAbilityInventory*>(hero->GetAbility(War3ID("AInv")));
    TEST_ASSERT(inv != nullptr);
    TEST_ASSERT(inv->GetItemCount() == 1);
    // Second purchase fails: out of stock.
    TEST_ASSERT(!sellPtr->CheckBeforeQueueNoTarget(sim, shopUnit, 0x100 | 0));
    TEST_ASSERT(inv->GetItemCount() == 1);

    // Global stock lands on all shops.
    CallNative(vm, "AddItemToAllStock", {JassValue::Int(0x6763656C), JassValue::Int(2), JassValue::Int(2)});
    TEST_ASSERT(sellPtr->FindStock(War3ID("gcel"))->current == 2);
    auto* lateShop = sim.AdoptUnit(std::make_unique<CUnit>(8406, MakePoolType("Shop2", War3ID("ngol"), false), 0, War3ID("ngol"), 20, 0, 0));
    auto lateSell = std::make_unique<CAbilitySellItems>(8407, std::vector<War3ID>{War3ID("gcel")});
    CAbilitySellItems* latePtr = lateSell.get();
    lateShop->AddAbility(sim, std::move(lateSell));
    TEST_ASSERT(latePtr->FindStock(War3ID("gcel")) != nullptr);
    TEST_ASSERT(latePtr->FindStock(War3ID("gcel"))->current == 2);
    CallNative(vm, "RemoveItemFromAllStock", {JassValue::Int(0x6763656C)});
    TEST_ASSERT(sellPtr->FindStock(War3ID("gcel")) == nullptr);
    return true;
}

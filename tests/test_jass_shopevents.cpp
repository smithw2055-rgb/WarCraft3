// Shop sale + ownership-change JASS events (ref SELL_ITEM / CHANGE_OWNER):
// sim fire sites through to trigger actions and event getters.
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_builder.h"
#include "ecs/abilities/ability_sell_items.h"
#include "data/itemdata.h"
#include "data/unitdata.h"
#include "ecs/item.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
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

CUnitType* MakeShopEventType(const char* name, War3ID id, bool hero) {
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

const char* kShopEventScript = R"JASS(
globals
    integer sellFires = 0
    integer sellBuyer = 0
    integer sellSeller = 0
    integer sellItem = 0
    integer ownerFires = 0
    integer changedSeen = 0
    integer prevOwnerSeen = 0
    integer hireFires = 0
    integer hiredSeen = 0
    integer hireSellerSeen = 0
    integer hireBuyerSeen = 0
    integer pawnFires = 0
    integer pawnSellerSeen = 0
    integer pawnBuyerSeen = 0
    integer pawnItemSeen = 0
endglobals

function OnSell takes nothing returns nothing
    set sellFires = sellFires + 1
    set sellBuyer = GetBuyingUnit()
    set sellSeller = GetSellingUnit()
    set sellItem = GetSoldItem()
endfunction

function OnChangeOwner takes nothing returns nothing
    set ownerFires = ownerFires + 1
    set changedSeen = GetChangingUnit()
    set prevOwnerSeen = GetChangingUnitPrevOwner()
endfunction

function OnHire takes nothing returns nothing
    set hireFires = hireFires + 1
    set hiredSeen = GetSoldUnit()
    set hireSellerSeen = GetSellingUnit()
    set hireBuyerSeen = GetBuyingUnit()
endfunction

function OnPawn takes nothing returns nothing
    set pawnFires = pawnFires + 1
    set pawnSellerSeen = GetSellingUnit()
    set pawnBuyerSeen = GetBuyingUnit()
    set pawnItemSeen = GetSoldItem()
endfunction
)JASS";

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) {
        std::cerr << "missing native " << name << "\n";
        return JassValue::Null();
    }
    return it->second(args);
}

} // namespace

bool TestShopSellItemEvent() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kShopEventScript));
    auto data = natives.GetData();
    sim.SetShopEventCallback([&](int buyer, int seller, int item) {
        natives.FireSellItemEvent(buyer, seller, item);
    });

    auto* shop = sim.AdoptUnit(std::make_unique<CUnit>(
        8501, MakeShopEventType("Shop", War3ID("ngol"), false), 0, War3ID("ngol"), 0, 0, 0));
    auto sell = std::make_unique<CAbilitySellItems>(8502, std::vector<War3ID>{War3ID("gcel")});
    CAbilitySellItems* sellPtr = sell.get();
    shop->AddAbility(sim, std::move(sell));
    auto* hero = sim.AdoptUnit(std::make_unique<CUnit>(
        8503, MakeShopEventType("Hero", War3ID("Hpal"), true), 0, War3ID("Hpal"), 500, 500, 0));
    hero->AddAbility(sim, std::make_unique<CAbilityInventory>(8504));
    TEST_ASSERT(shop && hero);
    sim.Update();

    const int playerHandle = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trig), JassValue::Int(playerHandle), JassValue::Int(271)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnSell")});

    // Order 0x100|player(0) buys stock index 0; no item data means cost 0.
    sellPtr->CheckBeforeQueueNoTarget(sim, shop, 0x100 | 0);
    natives.PumpQueuedThreads(1024);

    TEST_ASSERT(vm.GetGlobal("sellFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("sellBuyer").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("sellSeller").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("sellBuyer").intVal != vm.GetGlobal("sellSeller").intVal);
    TEST_ASSERT(vm.GetGlobal("sellItem").intVal != 0);
    auto* inv = dynamic_cast<CAbilityInventory*>(hero->GetAbility(War3ID("AInv")));
    TEST_ASSERT(inv != nullptr);
    TEST_ASSERT(inv->GetItemCount() == 1);
    return true;
}

bool TestChangeOwnerEvent() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kShopEventScript));
    auto data = natives.GetData();

    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(
        8601, MakeShopEventType("Foot", War3ID("hfoo"), false), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(u);
    sim.Update();
    const int uh = data->AllocHandle(JassHandleInfo::UNIT, 8601);
    const int p0 = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int p1 = data->AllocHandle(JassHandleInfo::PLAYER, 1);

    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trig), JassValue::Int(p1), JassValue::Int(270)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnChangeOwner")});
    const int trigUnit = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterUnitEvent",
               {JassValue::Int(trigUnit), JassValue::Int(uh), JassValue::Int(287)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trigUnit), JassValue::Str("OnChangeOwner")});

    // Same-owner assignment is a no-op (no event).
    CallNative(vm, "SetUnitOwner", {JassValue::Int(uh), JassValue::Int(p0)});
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("ownerFires").intVal == 0);
    TEST_ASSERT(u->GetPlayerIndex() == 0);

    // Real change fires both the unit and the new-owner player variant,
    // with unit + previous-owner context.
    CallNative(vm, "SetUnitOwner", {JassValue::Int(uh), JassValue::Int(p1)});
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(u->GetPlayerIndex() == 1);
    TEST_ASSERT(vm.GetGlobal("ownerFires").intVal == 2);
    TEST_ASSERT(vm.GetGlobal("changedSeen").intVal != 0);
    // Handles allocate fresh per event; compare the underlying player index.
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("changedSeen").intVal, JassHandleInfo::UNIT) == 8601);
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("prevOwnerSeen").intVal, JassHandleInfo::PLAYER) == 0);
    return true;
}

bool TestSimDrivenChangeOwnerEvent() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kShopEventScript));
    auto data = natives.GetData();
    sim.SetOwnerChangedCallback([&](int unitHandle, int prevOwner) {
        natives.FireChangeOwnerEvent(unitHandle, prevOwner);
    });

    auto* caster = sim.AdoptUnit(std::make_unique<CUnit>(
        8701, MakeShopEventType("Mage", War3ID("Hsor"), false), 0, War3ID("Hsor"), 0, 0, 0));
    auto* victim = sim.AdoptUnit(std::make_unique<CUnit>(
        8702, MakeShopEventType("Creep", War3ID("nwol"), false), 1, War3ID("nwol"), 100, 0, 0));
    TEST_ASSERT(caster && victim);
    sim.Update();
    const int p0 = data->AllocHandle(JassHandleInfo::PLAYER, 0);

    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trig), JassValue::Int(p0), JassValue::Int(270)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnChangeOwner")});

    ABActionTransferToCaster charm;
    charm.Run(sim, caster, victim, false, 0.0f, 0.0f);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(victim->GetPlayerIndex() == 0);
    TEST_ASSERT(vm.GetGlobal("ownerFires").intVal == 1);
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("changedSeen").intVal, JassHandleInfo::UNIT) == 8702);
    TEST_ASSERT(data->GetSimHandle(vm.GetGlobal("prevOwnerSeen").intVal, JassHandleInfo::PLAYER) == 1);

    // Re-charming into the same owner is a no-op (no event).
    charm.Run(sim, caster, victim, false, 0.0f, 0.0f);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("ownerFires").intVal == 1);
    return true;
}

bool TestShopHireUnitEvent() {
    JassVM vm;
    CSimulation sim;
    auto player = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
    player->SetGold(1000);
    player->SetLumber(1000);
    sim.AddPlayer(std::move(player));
    // Unit data with a 100g/50l mercenary (MakeShopEventType costs).
    CUnitData unitData;
    unitData.RegisterUnitType(War3ID("nmer"),
                              std::make_unique<CUnitType>(*MakeShopEventType("Merc", War3ID("nmer"), false)));
    sim.SetUnitData(&unitData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kShopEventScript));
    auto data = natives.GetData();
    sim.SetShopUnitEventCallback([&](int buyer, int seller, int hired) {
        natives.FireSellUnitEvent(buyer, seller, hired);
    });

    auto* shop = sim.AdoptUnit(std::make_unique<CUnit>(
        8801, MakeShopEventType("Tavern", War3ID("ntav"), false), 0, War3ID("ntav"), 0, 0, 0));
    auto sell = std::make_unique<CAbilitySellItems>(8802, std::vector<War3ID>{});
    CAbilitySellItems* sellPtr = sell.get();
    shop->AddAbility(sim, std::move(sell));
    TEST_ASSERT(shop);
    sim.Update();
    sellPtr->AddStockUnit(War3ID("nmer"), 1, 1);

    const int playerHandle = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trig), JassValue::Int(playerHandle), JassValue::Int(269)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnHire")});

    // Unit-slot order: 0x10000 | ((slot+1) << 8) | player.
    const size_t unitsBefore = sim.GetUnits().size();
    sellPtr->CheckBeforeQueueNoTarget(sim, shop, 0x10000 | (1 << 8) | 0);
    sim.Update(); // publish the hired unit from newUnits_
    natives.PumpQueuedThreads(1024);

    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 900);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 950);
    TEST_ASSERT(sellPtr->FindStock(War3ID("nmer"))->current == 0);
    TEST_ASSERT(sim.GetUnits().size() == unitsBefore + 1);
    CUnit* hired = sim.GetUnits().back();
    TEST_ASSERT(hired->GetTypeId() == War3ID("nmer"));
    TEST_ASSERT(hired->GetPlayerIndex() == 0);
    TEST_ASSERT(vm.GetGlobal("hireFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("hiredSeen").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("hireSellerSeen").intVal != 0);
    // Player-level purchase: no buyer unit.
    TEST_ASSERT(vm.GetGlobal("hireBuyerSeen").intVal == 0);

    // Stock exhausted: second hire fails silently (no charge, no event).
    sellPtr->CheckBeforeQueueNoTarget(sim, shop, 0x10000 | (1 << 8) | 0);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 900);
    TEST_ASSERT(vm.GetGlobal("hireFires").intVal == 1);
    return true;
}

bool TestShopPawnItemEvent() {
    JassVM vm;
    CSimulation sim;
    auto player = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
    player->SetGold(0);
    sim.AddPlayer(std::move(player));
    // Item data: 200g pawnable trinket (rate 0.5 -> 100g pawn value).
    CItemData itemData;
    auto trinket = std::make_unique<CItemType>();
    trinket->id = War3ID("gcel");
    trinket->goldCost = 200;
    trinket->pawnable = true;
    itemData.RegisterItemType(War3ID("gcel"), std::move(trinket));
    sim.SetItemData(&itemData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kShopEventScript));
    auto data = natives.GetData();
    sim.SetPawnEventCallback([&](int buyer, int seller, int item) {
        natives.FirePawnItemEvent(seller, buyer, item);
    });

    auto* shop = sim.AdoptUnit(std::make_unique<CUnit>(
        8901, MakeShopEventType("Shop", War3ID("ngol"), false), 0, War3ID("ngol"), 0, 0, 0));
    auto sell = std::make_unique<CAbilitySellItems>(8902, std::vector<War3ID>{});
    shop->AddAbility(sim, std::move(sell));
    auto* hero = sim.AdoptUnit(std::make_unique<CUnit>(
        8903, MakeShopEventType("Hero", War3ID("Hpal"), true), 0, War3ID("Hpal"), 500, 500, 0));
    hero->AddAbility(sim, std::make_unique<CAbilityInventory>(8904));
    TEST_ASSERT(shop && hero);
    sim.Update();
    auto* inv = dynamic_cast<CAbilityInventory*>(hero->GetAbility(War3ID("AInv")));
    TEST_ASSERT(inv != nullptr);
    CItem* item = sim.CreateItem(War3ID("gcel"), 500, 500);
    TEST_ASSERT(item && item->IsPawnable());
    TEST_ASSERT(inv->PickUpItem(sim, hero, item));

    const int playerHandle = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterPlayerUnitEvent",
               {JassValue::Int(trig), JassValue::Int(playerHandle), JassValue::Int(277)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnPawn")});

    // Pawn order: 0x20000 | ((slot+1) << 8) | player; slot 0 holds the item.
    auto* sellPtr = dynamic_cast<CAbilitySellItems*>(shop->GetAbility(War3ID("Asei")));
    TEST_ASSERT(sellPtr != nullptr);
    sellPtr->CheckBeforeQueueNoTarget(sim, shop, 0x20000 | (1 << 8) | 0);
    natives.PumpQueuedThreads(1024);

    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 100);
    TEST_ASSERT(inv->GetItemCount() == 0);
    TEST_ASSERT(vm.GetGlobal("pawnFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("pawnSellerSeen").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("pawnBuyerSeen").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("pawnSellerSeen").intVal != vm.GetGlobal("pawnBuyerSeen").intVal);
    TEST_ASSERT(vm.GetGlobal("pawnItemSeen").intVal != 0);

    // Non-pawnable items are refused (no gold, no event).
    CItem* cursed = sim.CreateItem(War3ID("gcel"), 500, 500);
    TEST_ASSERT(cursed);
    cursed->SetPawnable(false);
    TEST_ASSERT(inv->PickUpItem(sim, hero, cursed));
    sellPtr->CheckBeforeQueueNoTarget(sim, shop, 0x20000 | (1 << 8) | 0);
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 100);
    TEST_ASSERT(inv->GetItemCount() == 1);
    TEST_ASSERT(vm.GetGlobal("pawnFires").intVal == 1);
    return true;
}

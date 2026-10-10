// Gap-fill tests: lockstep orders, item behaviors, timed fog, FDF frames.
// Ref: GameTurnManager, CBehaviorGetItem/DropItem/GiveItemToHero,
// CTimedCircleFogModifier, parsers/fdf/frames/*.
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/behaviors/behavior_item.h"
#include "ecs/csimulation.h"
#include "ecs/fog_state.h"
#include "ecs/lockstep.h"
#include "ecs/orders/order.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "ui/ExtendedFrames.h"
#include "ui/FrameBuilder.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

#include <iostream>

namespace {

CUnitType* MakeItemType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
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
        0.5f, 10.0f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

} // namespace

bool TestIssueUnitOrderLogsAndForwards() {
    CSimulation sim;
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(101, MakeItemType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update(); // join units_
    TEST_ASSERT(sim.LoggedCommands().empty());
    TEST_ASSERT(sim.IssueUnitOrder(101, std::make_unique<COrderTargetPoint>(OrderIds::MOVE, 10, 20)));
    TEST_ASSERT(sim.LoggedCommands().size() == 1);
    TEST_ASSERT(sim.LoggedCommands()[0].orderId == OrderIds::MOVE);
    TEST_ASSERT(sim.LoggedCommands()[0].unit == 101);
    TEST_ASSERT(!sim.IssueUnitOrder(9999, std::make_unique<COrderNoTarget>(OrderIds::STOP)));
    TEST_ASSERT(sim.LoggedCommands().size() == 1);
    (void)u;
    return true;
}

bool TestHashStateDeterministic() {
    CSimulation sim;
    sim.AdoptUnit(std::make_unique<CUnit>(201, MakeItemType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update();
    const uint64_t h1 = sim.HashState();
    sim.Update();
    const uint64_t h2 = sim.HashState();
    // No orders, no combat: turn tick advances so hashes differ across ticks...
    TEST_ASSERT(h1 != h2);
    CSimulation sim2;
    sim2.AdoptUnit(std::make_unique<CUnit>(201, MakeItemType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim2.Update();
    TEST_ASSERT(sim2.HashState() == h1); // same setup reproduces
    return true;
}

#include <filesystem>

bool TestReplayLogRoundTrip() {
    ReplayLog log;
    log.Begin(1234);
    SimCommand cmd;
    cmd.turn = 7;
    cmd.player = 0;
    cmd.unit = 101;
    cmd.orderId = OrderIds::MOVE;
    cmd.targetKind = 1;
    cmd.x = 10.0f;
    cmd.y = 20.0f;
    log.Record(cmd);
    const std::string path = (std::filesystem::temp_directory_path() / "test_replay_ws.bin").string();
    TEST_ASSERT(log.Save(path));
    ReplayLog loaded;
    TEST_ASSERT(loaded.Load(path));
    TEST_ASSERT(loaded.Seed() == 1234);
    TEST_ASSERT(loaded.Commands().size() == 1);
    TEST_ASSERT(loaded.Commands()[0].orderId == OrderIds::MOVE);
    std::remove(path.c_str());
    return true;
}

bool TestGetItemBehaviorPicksUp() {
    CSimulation sim;
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(301, MakeItemType(), 0, War3ID("hfoo"), 0, 0, 0));
    u->AddAbility(sim, std::make_unique<CAbilityInventory>(3101));
    CItem* item = sim.CreateItem(War3ID("I000"), 50, 0);
    auto beh = std::make_unique<CBehaviorGetItem>(96.0f);
    beh->SetUnit(u);
    beh->SetTarget(item);
    beh->Begin(sim);
    bool done = false;
    for (int i = 0; i < 200 && !done; i++) done = beh->Update(sim);
    TEST_ASSERT(done);
    auto* inv = dynamic_cast<CAbilityInventory*>(u->GetAbility(War3ID("AInv")));
    TEST_ASSERT(inv && inv->GetItemCount() == 1);
    return true;
}

bool TestDropItemBehaviorDrops() {
    CSimulation sim;
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(401, MakeItemType(), 0, War3ID("hfoo"), 0, 0, 0));
    u->AddAbility(sim, std::make_unique<CAbilityInventory>(4101));
    CItem* item = sim.CreateItem(War3ID("I000"), 0, 0);
    auto* inv = dynamic_cast<CAbilityInventory*>(u->GetAbility(War3ID("AInv")));
    TEST_ASSERT(inv);
    TEST_ASSERT(inv->PickUpItem(sim, u, item));
    TEST_ASSERT(inv->GetItemCount() == 1);
    auto beh = std::make_unique<CBehaviorDropItem>(0, true, 500, 500);
    beh->SetUnit(u);
    TEST_ASSERT(beh->Update(sim));
    TEST_ASSERT(inv->GetItemCount() == 0);
    return true;
}

bool TestGiveItemBehaviorTransfers() {
    CSimulation sim;
    auto* giver = sim.AdoptUnit(std::make_unique<CUnit>(501, MakeItemType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* recv = sim.AdoptUnit(std::make_unique<CUnit>(502, MakeItemType(), 0, War3ID("hfoo"), 50, 0, 0));
    giver->AddAbility(sim, std::make_unique<CAbilityInventory>(5101));
    recv->AddAbility(sim, std::make_unique<CAbilityInventory>(5102));
    CItem* item = sim.CreateItem(War3ID("I000"), 0, 0);
    auto* ginv = dynamic_cast<CAbilityInventory*>(giver->GetAbility(War3ID("AInv")));
    auto* rinv = dynamic_cast<CAbilityInventory*>(recv->GetAbility(War3ID("AInv")));
    TEST_ASSERT(ginv && rinv);
    TEST_ASSERT(ginv->PickUpItem(sim, giver, item));
    auto beh = std::make_unique<CBehaviorGiveItem>(0, 150.0f);
    beh->SetUnit(giver);
    beh->SetTarget(recv);
    beh->Begin(sim);
    bool done = false;
    for (int i = 0; i < 200 && !done; i++) done = beh->Update(sim);
    TEST_ASSERT(done);
    TEST_ASSERT(ginv->GetItemCount() == 0);
    TEST_ASSERT(rinv->GetItemCount() == 1);
    return true;
}

bool TestTimedFogModifierExpires() {
    CFogSystem fog;
    const int id = fog.AddTimedModifier(0, CFogArea::Circle(0, 0, 500), CFogState::VISIBLE, 1.0f);
    TEST_ASSERT(id > 0);
    TEST_ASSERT(fog.GetState(0, 0, 0) == CFogState::VISIBLE);
    fog.Tick(0.6f);
    TEST_ASSERT(fog.GetState(0, 0, 0) == CFogState::VISIBLE);
    fog.Tick(0.6f); // expired
    // Back to grid default (unexplored => masked).
    TEST_ASSERT(fog.GetState(0, 0, 0) == CFogState::MASKED);
    return true;
}

bool TestExtendedFramesBuild() {
    GameUI gui;
    gui.Init(1280, 720);
    FrameBuilder builder(gui);
    FrameDefinition check;
    check.name = "MyCheck";
    check.frameType = "CHECKBOX";
    UIFrame* f1 = builder.ConstructFrame(check, nullptr);
    TEST_ASSERT(dynamic_cast<CheckBoxFrame*>(f1) != nullptr);
    FrameDefinition edit;
    edit.name = "MyEdit";
    edit.frameType = "EDITBOX";
    UIFrame* f2 = builder.ConstructFrame(edit, nullptr);
    TEST_ASSERT(dynamic_cast<EditBoxFrame*>(f2) != nullptr);
    FrameDefinition list;
    list.name = "MyList";
    list.frameType = "LISTBOX";
    TEST_ASSERT(dynamic_cast<ListBoxFrame*>(builder.ConstructFrame(list, nullptr)) != nullptr);
    FrameDefinition bar;
    bar.name = "MyBar";
    bar.frameType = "SIMPLESTATUSBAR";
    TEST_ASSERT(dynamic_cast<StatusBarFrame*>(builder.ConstructFrame(bar, nullptr)) != nullptr);
    // MENU builds a MenuFrame fed by MenuItem entries (ref MenuFrame).
    FrameDefinition menu;
    menu.name = "MyMenu";
    menu.frameType = "MENU";
    menu.menuItems.emplace_back("Okay", 0);
    menu.menuItems.emplace_back("Cancel", 1);
    UIFrame* fMenu = builder.ConstructFrame(menu, nullptr);
    auto* menuFrame = dynamic_cast<MenuFrame*>(fMenu);
    TEST_ASSERT(menuFrame != nullptr);
    TEST_ASSERT(menuFrame->GetCount() == 2);
    TEST_ASSERT(menuFrame->GetItem(0) == "Okay");
    // POPUPMENU entries likewise land in the item store.
    FrameDefinition popup;
    popup.name = "MyPopup";
    popup.frameType = "POPUPMENU";
    popup.menuItems.emplace_back("A", 0);
    auto* popupFrame = dynamic_cast<PopupMenuFrame*>(builder.ConstructFrame(popup, nullptr));
    TEST_ASSERT(popupFrame != nullptr);
    TEST_ASSERT(popupFrame->GetCount() == 1);
    // GLUETEXTBUTTON maps to ButtonFrame (dead branch removed).
    FrameDefinition glue;
    glue.name = "MyGlue";
    glue.frameType = "GLUETEXTBUTTON";
    TEST_ASSERT(dynamic_cast<ButtonFrame*>(builder.ConstructFrame(glue, nullptr)) != nullptr);
    // Extended state survives ApplyProperties.
    FrameDefinition checked;
    checked.name = "MyChecked";
    checked.frameType = "CHECKBOX";
    checked.flags.insert("Checked");
    auto* checkFrame = dynamic_cast<CheckBoxFrame*>(builder.ConstructFrame(checked, nullptr));
    TEST_ASSERT(checkFrame != nullptr && checkFrame->IsChecked());
    FrameDefinition scroll;
    scroll.name = "MyScroll";
    scroll.frameType = "SCROLLBAR";
    scroll.floatFields["MinValue"] = 10.0f;
    scroll.floatFields["MaxValue"] = 90.0f;
    scroll.floatFields["Value"] = 42.0f;
    auto* scrollFrame = dynamic_cast<ScrollBarFrame*>(builder.ConstructFrame(scroll, nullptr));
    TEST_ASSERT(scrollFrame != nullptr);
    TEST_ASSERT(scrollFrame->GetMin() == 10.0f && scrollFrame->GetMax() == 90.0f);
    TEST_ASSERT(scrollFrame->GetValue() == 42.0f);
    FrameDefinition status;
    status.name = "MyStatus";
    status.frameType = "STATUSBAR";
    status.floatFields["MaxValue"] = 500.0f;
    status.floatFields["Value"] = 123.0f;
    auto* statusFrame = dynamic_cast<StatusBarFrame*>(builder.ConstructFrame(status, nullptr));
    TEST_ASSERT(statusFrame != nullptr);
    TEST_ASSERT(statusFrame->GetMax() == 500.0f && statusFrame->GetValue() == 123.0f);
    FrameDefinition editBox;
    editBox.name = "MyEditLen";
    editBox.frameType = "EDITBOX";
    editBox.floatFields["MaxLength"] = 32.0f;
    auto* editFrame = dynamic_cast<EditBoxFrame*>(builder.ConstructFrame(editBox, nullptr));
    TEST_ASSERT(editFrame != nullptr && editFrame->GetMaxLength() == 32);
    FrameDefinition sprite;
    sprite.name = "MySprite";
    sprite.frameType = "SPRITE";
    sprite.stringFields["Model"] = "UI\\Test.mdx";
    auto* spriteFrame = dynamic_cast<SpriteFrame*>(builder.ConstructFrame(sprite, nullptr));
    TEST_ASSERT(spriteFrame != nullptr && spriteFrame->GetModel() == "UI\\Test.mdx");
    return true;
}

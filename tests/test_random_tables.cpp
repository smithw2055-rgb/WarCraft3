// P1-13 w3i global random tables: group spawns and death drops.
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/itemdata.h"
#include "ecs/csimulation.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "map/w3i.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

CUnitType* MakeRandomType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 0.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 300, 0, War3ID(),
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

W3iInfo MakeGroupW3i() {
    W3iInfo w3i;
    W3iRandomUnitTable table;
    table.id = 7;
    table.name = "Creeps";
    table.positions = 2;
    table.columnTypes = {0, 0};
    W3iRandomUnitEntry row1;
    row1.chance = 30;
    row1.ids = {War3ID("hfoo").value, War3ID("hpea").value};
    W3iRandomUnitEntry row2;
    row2.chance = 70;
    row2.ids = {War3ID("hkni").value, War3ID("hgyr").value};
    table.units = {row1, row2};
    w3i.randomUnitTables.push_back(table);
    return w3i;
}

} // namespace

// Group lookup by id, column by position, chance roll like flag==2.
bool TestResolveGroupUnitById() {
    const W3iInfo w3i = MakeGroupW3i();
    War3ID out;
    TEST_ASSERT(ResolveRandomGroupUnit(w3i, 7, 0, 0.0f, out));
    TEST_ASSERT(out == War3ID("hfoo"));
    TEST_ASSERT(ResolveRandomGroupUnit(w3i, 7, 1, 0.0f, out));
    TEST_ASSERT(out == War3ID("hpea"));
    TEST_ASSERT(ResolveRandomGroupUnit(w3i, 7, 0, 0.5f, out)); // 50 >= 30
    TEST_ASSERT(out == War3ID("hkni"));
    TEST_ASSERT(ResolveRandomGroupUnit(w3i, 7, 1, 0.99f, out));
    TEST_ASSERT(out == War3ID("hgyr"));
    return true;
}

// No id match: the group number addresses the table by index.
bool TestResolveGroupUnitIndexFallback() {
    const W3iInfo w3i = MakeGroupW3i();
    War3ID out;
    TEST_ASSERT(ResolveRandomGroupUnit(w3i, 0, 0, 0.0f, out));
    TEST_ASSERT(out == War3ID("hfoo"));
    return true;
}

// Unresolvable groups/columns/rolls keep the fixed id (return false).
bool TestResolveGroupUnitMisses() {
    const W3iInfo w3i = MakeGroupW3i();
    War3ID out;
    W3iInfo empty;
    TEST_ASSERT(!ResolveRandomGroupUnit(empty, 7, 0, 0.0f, out));
    TEST_ASSERT(!ResolveRandomGroupUnit(w3i, 7, 5, 0.0f, out)); // no column
    TEST_ASSERT(!ResolveRandomGroupUnit(w3i, 7, 0, 1.0f, out)); // past total
    TEST_ASSERT(!ResolveRandomGroupUnit(w3i, 42, 0, 0.0f, out)); // no table
    return true;
}

// Item table collection: all sets appended; bad numbers rejected.
bool TestCollectItemTableDrops() {
    W3iInfo w3i;
    W3iRandomItemTable table;
    table.id = War3ID("YTi/").value;
    table.name = "Any";
    table.sets = {{{30, War3ID("ratf").value}}, {{100, War3ID("rde1").value}}};
    w3i.randomItemTables.push_back(table);
    std::vector<W3iDropItem> out;
    TEST_ASSERT(CollectRandomItemTableDrops(w3i, 0, out));
    TEST_ASSERT(out.size() == 2);
    TEST_ASSERT(out[0].itemId == War3ID("ratf") && out[0].chancePercent == 30);
    TEST_ASSERT(out[1].itemId == War3ID("rde1") && out[1].chancePercent == 100);
    std::vector<W3iDropItem> none;
    TEST_ASSERT(!CollectRandomItemTableDrops(w3i, -1, none));
    TEST_ASSERT(!CollectRandomItemTableDrops(w3i, 3, none));
    return true;
}

// Death drops materialize at the corpse on the death tick (100% drops,
// 0% and unknown items skipped).
bool TestDeathDropsOnDeathTick() {
    CSimulation sim;
    CItemData itemData;
    auto sword = std::make_unique<CItemType>();
    sword->id = War3ID("ratf");
    itemData.RegisterItemType(War3ID("ratf"), std::move(sword));
    sim.SetItemData(&itemData);
    auto* creep = sim.AdoptUnit(std::make_unique<CUnit>(
        601, MakeRandomType(), 0, War3ID("ngnv"), 0, 0, 0));
    sim.Update();
    sim.Update();
    TEST_ASSERT(sim.GetItems().empty());
    creep->SetDeathDrops({{War3ID("ratf"), 100}, {War3ID("rde1"), 100}, {War3ID("ratf"), 0}});
    creep->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL,
                  "", 1e9f, 0);
    sim.Update(); // death tick drops the won item
    TEST_ASSERT(sim.GetItems().size() == 1);
    TEST_ASSERT(sim.GetItems()[0]->GetTypeId() == War3ID("ratf"));
    return true;
}

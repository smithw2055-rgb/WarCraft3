// P1-15 destructable targeting + death drops: targType gate, doo drop
// sets materializing on the killing blow, occluder init from the type.
#include <filesystem>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/destructabledata.h"
#include "data/itemdata.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "resource/mpq_storm.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

void AddTreeType(CDestructableData& data) {
    auto type = std::make_unique<CDestructableType>();
    type->id = War3ID("LTlt");
    type->maxLife = 100;
    type->occlusionHeight = 200.0f;
    type->targetedAs = {CTargetType::TREE};
    data.RegisterDestructable(War3ID("LTlt"), std::move(type));
}

void AddWallType(CDestructableData& data) {
    auto type = std::make_unique<CDestructableType>();
    type->id = War3ID("YTwc");
    type->maxLife = 200;
    type->targetedAs = {CTargetType::WALL};
    data.RegisterDestructable(War3ID("YTwc"), std::move(type));
}

} // namespace

bool TestTargetTypeInfoParses() {
    CTargetType out = CTargetType::GROUND;
    TEST_ASSERT(CTargetTypeInfo::FromString("tree", out) && out == CTargetType::TREE);
    TEST_ASSERT(CTargetTypeInfo::FromString("TREE", out) && out == CTargetType::TREE);
    TEST_ASSERT(CTargetTypeInfo::FromString("debr", out) && out == CTargetType::DEBRIS);
    TEST_ASSERT(CTargetTypeInfo::FromString("bridge", out) && out == CTargetType::BRIDGE);
    TEST_ASSERT(CTargetTypeInfo::FromString("deco", out) && out == CTargetType::DECORATION);
    TEST_ASSERT(CTargetTypeInfo::FromString("vuln", out) && out == CTargetType::VULNERABLE);
    TEST_ASSERT(CTargetTypeInfo::FromString("wall", out) && out == CTargetType::WALL);
    TEST_ASSERT(CTargetTypeInfo::FromString("dead", out) && out == CTargetType::DEAD);
    // Ref-only / unknown names are skipped, never crash the load.
    TEST_ASSERT(!CTargetTypeInfo::FromString("alive", out));
    TEST_ASSERT(!CTargetTypeInfo::FromString("sapper", out));
    TEST_ASSERT(!CTargetTypeInfo::FromString("not_a_target", out));
    TEST_ASSERT(!CTargetTypeInfo::FromString("", out));
    return true;
}

bool TestDestructableTargetedAsGate() {
    // Ref CDestructable.canBeTargetedBy: allowed must contain every
    // targetedAs entry; dead bodies need DEAD, living ones fail
    // corpse-only sets.
    CSimulation sim;
    CDestructableData data;
    AddTreeType(data);
    AddWallType(data);
    sim.SetDestructableData(&data);
    CDestructable tree(1, War3ID("LTlt"), 0.0f, 0.0f, 100.0f);
    CDestructable wall(2, War3ID("YTwc"), 0.0f, 0.0f, 200.0f);
    const std::vector<CTargetType> groundTree = {CTargetType::GROUND, CTargetType::TREE};
    const std::vector<CTargetType> groundOnly = {CTargetType::GROUND};
    const std::vector<CTargetType> corpseTree = {CTargetType::TREE, CTargetType::DEAD};
    const std::vector<CTargetType> deadOnly = {CTargetType::DEAD};
    TEST_ASSERT(tree.CanBeTargetedBy(sim, nullptr, groundTree));
    TEST_ASSERT(!tree.CanBeTargetedBy(sim, nullptr, groundOnly)); // no TREE
    TEST_ASSERT(!wall.CanBeTargetedBy(sim, nullptr, groundTree)); // no WALL
    TEST_ASSERT(wall.CanBeTargetedBy(sim, nullptr,
                                     {CTargetType::GROUND, CTargetType::WALL}));
    TEST_ASSERT(!tree.CanBeTargetedBy(sim, nullptr, corpseTree)); // living vs DEAD set
    // Kill the tree: corpse needs DEAD, plain sets fail.
    tree.Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 1e9f, 0);
    TEST_ASSERT(tree.IsDead());
    TEST_ASSERT(!tree.CanBeTargetedBy(sim, nullptr, groundTree));
    TEST_ASSERT(tree.CanBeTargetedBy(sim, nullptr, deadOnly) == false); // no TREE in set
    TEST_ASSERT(tree.CanBeTargetedBy(sim, nullptr, corpseTree));
    return true;
}

bool TestDestructableDeathDrops() {
    // doo itemSets roll per item on the killing blow; 0% and unknown
    // items are skipped (same rule as CUnit::DropDeathItems).
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CDestructableData data;
    AddTreeType(data);
    sim.SetDestructableData(&data);
    CItemData itemData;
    auto sword = std::make_unique<CItemType>();
    sword->id = War3ID("ratf");
    itemData.RegisterItemType(War3ID("ratf"), std::move(sword));
    sim.SetItemData(&itemData);
    CDestructable* tree = sim.CreateDestructable(War3ID("LTlt"), 0.0f, 0.0f, 100.0f, 0);
    TEST_ASSERT(tree != nullptr);
    TEST_ASSERT(sim.GetItems().empty());
    tree->SetDeathDrops({{War3ID("ratf"), 100}, {War3ID("rde1"), 100}, {War3ID("ratf"), 0}});
    tree->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 1e9f, 0);
    TEST_ASSERT(tree->IsDead());
    TEST_ASSERT(sim.GetItems().size() == 1);
    TEST_ASSERT(sim.GetItems()[0]->GetTypeId() == War3ID("ratf"));
    return true;
}

bool TestDestructableOccluderInit() {
    // Instance occluder starts at the type value so GetOccluderHeight
    // agrees with the vision block stamped by BindPathing.
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CDestructableData data;
    AddTreeType(data);
    sim.SetDestructableData(&data);
    CDestructable* tree = sim.CreateDestructable(War3ID("LTlt"), 0.0f, 0.0f, 100.0f, 0);
    TEST_ASSERT(tree != nullptr);
    TEST_ASSERT(tree->GetOccluderHeight() == 200.0f);
    return true;
}

bool TestRetailTargTypeParses() {
    // Retail DestructableData.slk targType column feeds targetedAs when
    // game MPQs are present (skipped in CI without them).
    namespace fs = std::filesystem;
    if (!fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq")) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CDestructableData data = CDestructableData::LoadFromMpq(mpq);
    const CDestructableType* tree = data.GetDestructable(War3ID("LTlt"));
    TEST_ASSERT(tree != nullptr);
    TEST_ASSERT(tree->targetedAs.count(CTargetType::TREE) > 0);
    return true;
}

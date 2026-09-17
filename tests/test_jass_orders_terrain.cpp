// Batch-1 native tests: order-by-id, terrain queries, rect/region cells,
// player/team/taxrate, blight, world bounds (ref common.j).
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "map/w3e.h"
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

CUnitType* MakeOrderType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Footman", "Footman", "icon", War3ID("hfoo"),
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
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
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

// 2x2 terrain: tile (0,0) plain ground palette 0, tile (1,0) water,
// tile (0,1) blighted ground, tile (1,1) cliff layer 2.
W3EMap MakeTestTerrain() {
    W3EMap map;
    map.version = 11;
    map.tileset = 'A';
    map.tileWidth = 2;
    map.tileHeight = 2;
    map.offsetX = 0.0f;
    map.offsetY = 0.0f;
    map.groundTilePalette = {0x7472644Cu /*Ldrt*/, 0x72745368u /*hSrt*/};
    map.cliffTilePalette = {};
    map.tiles.resize(4);
    // (0,0): ground texture 0, variation 3, layer 1
    map.tiles[0].textureBits = 0x00;
    map.tiles[0].variationBits = 0x03;
    map.tiles[0].cliffAndLayer = 0x01;
    // (1,0): water, ground texture 1
    map.tiles[1].textureBits = 0x40 | 0x01;
    // (0,1): blighted ground
    map.tiles[2].textureBits = 0x20;
    // (1,1): cliff layer 2
    map.tiles[3].cliffAndLayer = 0x02;
    return map;
}

} // namespace

bool TestIssueOrderByIdNatives() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* u1 = sim.AdoptUnit(std::make_unique<CUnit>(8101, MakeOrderType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* u2 = sim.AdoptUnit(std::make_unique<CUnit>(8102, MakeOrderType(), 0, War3ID("hfoo"), 500, 0, 0));
    sim.Update();
    const int h1 = data->AllocHandle(JassHandleInfo::UNIT, 8101);
    const int h2 = data->AllocHandle(JassHandleInfo::UNIT, 8102);
    TEST_ASSERT(u1->GetLastOrderId() == 0);

    // Immediate + point + target by id.
    TEST_ASSERT(CallNative(vm, "IssueImmediateOrderById", {JassValue::Int(h1), JassValue::Int(OrderIds::STOP)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::STOP);
    TEST_ASSERT(CallNative(vm, "GetUnitCurrentOrder", {JassValue::Int(h1)}).intVal == OrderIds::STOP);
    TEST_ASSERT(CallNative(vm, "IssuePointOrderById", {JassValue::Int(h1), JassValue::Int(OrderIds::MOVE), JassValue::Real(100), JassValue::Real(200)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::MOVE);
    TEST_ASSERT(CallNative(vm, "IssueTargetOrderById", {JassValue::Int(h1), JassValue::Int(OrderIds::ATTACK), JassValue::Int(h2)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::ATTACK);

    // Point-by-id through a location handle.
    const int loc = CallNative(vm, "Location", {JassValue::Real(300), JassValue::Real(400)}).intVal;
    TEST_ASSERT(CallNative(vm, "IssuePointOrderByIdLoc", {JassValue::Int(h1), JassValue::Int(OrderIds::PATROL), JassValue::Int(loc)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::PATROL);

    // Instant + neutral twins dispatch identically.
    TEST_ASSERT(CallNative(vm, "IssueInstantPointOrderById", {JassValue::Int(h1), JassValue::Int(OrderIds::MOVE), JassValue::Real(1), JassValue::Real(2)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::MOVE);
    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;
    TEST_ASSERT(CallNative(vm, "IssueNeutralPointOrderById", {JassValue::Int(p0), JassValue::Int(h1), JassValue::Int(OrderIds::MOVE), JassValue::Real(1), JassValue::Real(2)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::MOVE);
    TEST_ASSERT(CallNative(vm, "IssueNeutralTargetOrderById", {JassValue::Int(p0), JassValue::Int(h1), JassValue::Int(OrderIds::ATTACK), JassValue::Int(h2)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::ATTACK);

    // Group orders fan out.
    const int grp = CallNative(vm, "CreateGroup", {}).intVal;
    CallNative(vm, "GroupAddUnit", {JassValue::Int(grp), JassValue::Int(h1)});
    CallNative(vm, "GroupAddUnit", {JassValue::Int(grp), JassValue::Int(h2)});
    TEST_ASSERT(CallNative(vm, "GroupPointOrderById", {JassValue::Int(grp), JassValue::Int(OrderIds::MOVE), JassValue::Real(9), JassValue::Real(9)}).boolVal);
    TEST_ASSERT(u2->GetLastOrderId() == OrderIds::MOVE);
    TEST_ASSERT(CallNative(vm, "GroupTargetOrderById", {JassValue::Int(grp), JassValue::Int(OrderIds::ATTACK), JassValue::Int(h1)}).boolVal);
    TEST_ASSERT(u2->GetLastOrderId() == OrderIds::ATTACK);
    TEST_ASSERT(CallNative(vm, "GroupImmediateOrderById", {JassValue::Int(grp), JassValue::Int(OrderIds::STOP)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::STOP);
    TEST_ASSERT(CallNative(vm, "GroupPointOrderByIdLoc", {JassValue::Int(grp), JassValue::Int(OrderIds::MOVE), JassValue::Int(loc)}).boolVal);
    TEST_ASSERT(u1->GetLastOrderId() == OrderIds::MOVE);

    // Bad handles fail closed.
    TEST_ASSERT(!CallNative(vm, "IssuePointOrderById", {JassValue::Int(999999), JassValue::Int(OrderIds::MOVE), JassValue::Real(1), JassValue::Real(1)}).boolVal);
    return true;
}

bool TestTerrainQueryNatives() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    // No terrain map: safe defaults.
    TEST_ASSERT(CallNative(vm, "GetTerrainType", {JassValue::Real(10), JassValue::Real(10)}).intVal == 0);
    TEST_ASSERT(!CallNative(vm, "IsTerrainPathable", {JassValue::Real(10), JassValue::Real(10), JassValue::Int(1)}).boolVal);

    W3EMap terrain = MakeTestTerrain();
    sim.SetTerrainMap(&terrain);
    auto data = natives.GetData();
    (void)data;

    // Tile (0,0) center = (64,64): palette 0, variation 3, layer 1.
    TEST_ASSERT(CallNative(vm, "GetTerrainType", {JassValue::Real(64), JassValue::Real(64)}).intVal == (int)0x7472644Cu);
    TEST_ASSERT(CallNative(vm, "GetTerrainVariance", {JassValue::Real(64), JassValue::Real(64)}).intVal == 3);
    TEST_ASSERT(CallNative(vm, "GetTerrainCliffLevel", {JassValue::Real(64), JassValue::Real(64)}).intVal == 1);
    // Tile (1,1) center = (192,192): cliff layer 2.
    TEST_ASSERT(CallNative(vm, "GetTerrainCliffLevel", {JassValue::Real(192), JassValue::Real(192)}).intVal == 2);
    // Outside the map: defaults.
    TEST_ASSERT(CallNative(vm, "GetTerrainType", {JassValue::Real(9999), JassValue::Real(9999)}).intVal == 0);

    // Walkability without a pathing map falls back to tile flags.
    TEST_ASSERT(CallNative(vm, "IsTerrainPathable", {JassValue::Real(64), JassValue::Real(64), JassValue::Int(1)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsTerrainPathable", {JassValue::Real(192), JassValue::Real(64), JassValue::Int(1)}).boolVal); // water
    TEST_ASSERT(CallNative(vm, "IsTerrainPathable", {JassValue::Real(192), JassValue::Real(64), JassValue::Int(2)}).boolVal);  // fly over water
    TEST_ASSERT(!CallNative(vm, "IsTerrainPathable", {JassValue::Real(192), JassValue::Real(64), JassValue::Int(2)}).boolVal == false);
    TEST_ASSERT(CallNative(vm, "IsTerrainPathable", {JassValue::Real(192), JassValue::Real(64), JassValue::Int(6)}).boolVal);  // float on water
    TEST_ASSERT(!CallNative(vm, "IsTerrainPathable", {JassValue::Real(192), JassValue::Real(64), JassValue::Int(5)}).boolVal); // blight query, none yet

    // Baked blight bit on tile (0,1) center = (64,192).
    TEST_ASSERT(CallNative(vm, "IsPointBlighted", {JassValue::Real(64), JassValue::Real(192)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsPointBlighted", {JassValue::Real(64), JassValue::Real(64)}).boolVal);
    TEST_ASSERT(CallNative(vm, "IsTerrainPathable", {JassValue::Real(64), JassValue::Real(192), JassValue::Int(5)}).boolVal);

    // Script blight circle + clear.
    CallNative(vm, "SetBlightPoint", {JassValue::Real(64), JassValue::Real(64), JassValue::Real(100), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "IsPointBlighted", {JassValue::Real(64), JassValue::Real(64)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsTerrainPathable", {JassValue::Real(64), JassValue::Real(64), JassValue::Int(3)}).boolVal); // build blocked on blight
    CallNative(vm, "SetBlightPoint", {JassValue::Real(64), JassValue::Real(64), JassValue::Real(100), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(vm, "IsPointBlighted", {JassValue::Real(64), JassValue::Real(64)}).boolVal);
    return true;
}

bool TestRectRegionCellNatives() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    const int rect = CallNative(vm, "Rect", {JassValue::Real(0), JassValue::Real(0), JassValue::Real(100), JassValue::Real(50)}).intVal;
    CallNative(vm, "SetRect", {JassValue::Int(rect), JassValue::Real(10), JassValue::Real(20), JassValue::Real(110), JassValue::Real(70)});
    TEST_ASSERT(CallNative(vm, "GetRectMinX", {JassValue::Int(rect)}).realVal == 10.0f);
    TEST_ASSERT(CallNative(vm, "GetRectMaxX", {JassValue::Int(rect)}).realVal == 110.0f);
    CallNative(vm, "MoveRectTo", {JassValue::Int(rect), JassValue::Real(200), JassValue::Real(200)});
    TEST_ASSERT(CallNative(vm, "GetRectMinX", {JassValue::Int(rect)}).realVal == 150.0f);
    const int locA = CallNative(vm, "Location", {JassValue::Real(0), JassValue::Real(0)}).intVal;
    const int locB = CallNative(vm, "Location", {JassValue::Real(40), JassValue::Real(30)}).intVal;
    CallNative(vm, "SetRectFromLoc", {JassValue::Int(rect), JassValue::Int(locA), JassValue::Int(locB)});
    TEST_ASSERT(CallNative(vm, "GetRectMaxX", {JassValue::Int(rect)}).realVal == 40.0f);

    // Blight-by-rect through the rect handle.
    W3EMap terrain = MakeTestTerrain();
    sim.SetTerrainMap(&terrain);
    CallNative(vm, "SetBlightRect", {JassValue::Int(rect), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "IsPointBlighted", {JassValue::Real(20), JassValue::Real(15)}).boolVal);

    // Region cells: cell (1,1) covers world [32,64)x[32,64).
    const int region = CallNative(vm, "CreateRegion", {}).intVal;
    TEST_ASSERT(!CallNative(vm, "IsPointInRegion", {JassValue::Int(region), JassValue::Real(40), JassValue::Real(40)}).boolVal);
    CallNative(vm, "RegionAddCell", {JassValue::Int(region), JassValue::Real(1), JassValue::Real(1)});
    TEST_ASSERT(CallNative(vm, "IsPointInRegion", {JassValue::Int(region), JassValue::Real(40), JassValue::Real(40)}).boolVal);
    const int loc = CallNative(vm, "Location", {JassValue::Real(40), JassValue::Real(40)}).intVal;
    TEST_ASSERT(CallNative(vm, "IsLocationInRegion", {JassValue::Int(region), JassValue::Int(loc)}).boolVal);
    CallNative(vm, "RegionClearCell", {JassValue::Int(region), JassValue::Real(1), JassValue::Real(1)});
    TEST_ASSERT(!CallNative(vm, "IsPointInRegion", {JassValue::Int(region), JassValue::Real(40), JassValue::Real(40)}).boolVal);
    CallNative(vm, "RegionAddCellAtLoc", {JassValue::Int(region), JassValue::Int(loc)});
    TEST_ASSERT(CallNative(vm, "IsPointInRegion", {JassValue::Int(region), JassValue::Real(40), JassValue::Real(40)}).boolVal);
    CallNative(vm, "RegionClearCellAtLoc", {JassValue::Int(region), JassValue::Int(loc)});
    TEST_ASSERT(!CallNative(vm, "IsPointInRegion", {JassValue::Int(region), JassValue::Real(40), JassValue::Real(40)}).boolVal);
    return true;
}

bool TestPlayerTeamNatives() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    (void)data;

    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;
    const int p1 = CallNative(vm, "Player", {JassValue::Int(1)}).intVal;
    sim.GetPlayer(0)->SetTeam(1);
    sim.GetPlayer(1)->SetTeam(2);
    TEST_ASSERT(CallNative(vm, "GetPlayerTeam", {JassValue::Int(p0)}).intVal == 1);
    TEST_ASSERT(CallNative(vm, "GetPlayerTeam", {JassValue::Int(p1)}).intVal == 2);

    const int players = CallNative(vm, "GetPlayers", {}).intVal;
    TEST_ASSERT(CallNative(vm, "CountPlayersInForce", {JassValue::Int(players)}).intVal == 2);
    const int teams = CallNative(vm, "GetTeams", {}).intVal;
    TEST_ASSERT(CallNative(vm, "CountPlayersInForce", {JassValue::Int(teams)}).intVal == 2);

    // Tax rates default 0, round-trip when set.
    TEST_ASSERT(CallNative(vm, "GetPlayerTaxRate", {JassValue::Int(p0), JassValue::Int(p1), JassValue::Int(1)}).intVal == 0);
    CallNative(vm, "SetPlayerTaxRate", {JassValue::Int(p0), JassValue::Int(p1), JassValue::Int(1), JassValue::Int(30)});
    TEST_ASSERT(CallNative(vm, "GetPlayerTaxRate", {JassValue::Int(p0), JassValue::Int(p1), JassValue::Int(1)}).intVal == 30);
    TEST_ASSERT(CallNative(vm, "GetPlayerTaxRate", {JassValue::Int(p1), JassValue::Int(p0), JassValue::Int(1)}).intVal == 0);

    TEST_ASSERT(CallNative(vm, "GetPlayerSelectable", {JassValue::Int(p0)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetStartLocPrio", {JassValue::Int(0)}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetStartLocPrioSlot", {JassValue::Int(0), JassValue::Int(0)}).intVal == -1);

    // Map flags share the mask with IsMapFlagSet.
    CallNative(vm, "SetMapFlag", {JassValue::Int(4)});
    TEST_ASSERT(CallNative(vm, "IsMapFlagSet", {JassValue::Int(4)}).boolVal);

    // World bounds from the synthetic 2x2 terrain at origin.
    W3EMap terrain = MakeTestTerrain();
    sim.SetTerrainMap(&terrain);
    const int bounds = CallNative(vm, "GetWorldBounds", {}).intVal;
    TEST_ASSERT(CallNative(vm, "GetRectMinX", {JassValue::Int(bounds)}).realVal == 0.0f);
    TEST_ASSERT(CallNative(vm, "GetRectMaxX", {JassValue::Int(bounds)}).realVal == 256.0f);
    return true;
}

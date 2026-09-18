// Terrain-edit natives: SetTerrainType tile mutation, SetTerrainPathable
// cell flags, water-deform toggle, doodad animation requests, deform
// registry lifecycle (ref terrain API).
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "map/w3e.h"
#include "map/wpm.h"
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

W3EMap MakeEditTerrain() {
    W3EMap map;
    map.version = 11;
    map.tileset = 'A';
    map.tileWidth = 2;
    map.tileHeight = 2;
    map.offsetX = 0.0f;
    map.offsetY = 0.0f;
    map.groundTilePalette = {0x7472644Cu /*Ldrt*/, 0x72745368u /*hSrt*/};
    map.tiles.resize(4);
    return map;
}

WpmData MakeEditPathing() {
    WpmData wpm;
    wpm.width = 4;
    wpm.height = 4;
    wpm.pathing.assign(16, 0);
    return wpm;
}

struct Fixture {
    JassVM vm;
    CSimulation sim;
    std::unique_ptr<JassNatives> natives;
    std::shared_ptr<JassNativesData> data;
    W3EMap terrain;
    WpmData pathing;

    Fixture() {
        sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
        terrain = MakeEditTerrain();
        pathing = MakeEditPathing();
        sim.SetTerrainMap(&terrain);
        sim.SetPathingMap(&pathing, 0.0f, 0.0f);
    }
};

} // namespace

bool TestSetTerrainType() {
    Fixture f;
    TEST_ASSERT(f.terrain.editVersion == 0);
    // Repaint tile (0,0) center to palette 1, variation 5, radius 1 tile.
    CallNative(f.vm, "SetTerrainType",
        {JassValue::Real(64), JassValue::Real(64), JassValue::Int((int)0x72745368u),
         JassValue::Int(5), JassValue::Real(1), JassValue::Int(0)});
    TEST_ASSERT(f.terrain.editVersion == 1);
    {
        const W3ETileCorner* corner = W3ECornerAt(f.terrain, 64, 64);
        TEST_ASSERT(corner != nullptr);
        TEST_ASSERT(corner->GroundTexture() == 1);
        TEST_ASSERT(corner->GroundVariation() == 5);
    }
    // Queries observe the edit.
    TEST_ASSERT(CallNative(f.vm, "GetTerrainType", {JassValue::Real(64), JassValue::Real(64)}).intVal == (int)0x72745368u);
    TEST_ASSERT(CallNative(f.vm, "GetTerrainVariance", {JassValue::Real(64), JassValue::Real(64)}).intVal == 5);
    // Unknown palette code: no-op, version untouched.
    CallNative(f.vm, "SetTerrainType",
        {JassValue::Real(64), JassValue::Real(64), JassValue::Int(0x58585858),
         JassValue::Int(0), JassValue::Real(1), JassValue::Int(0)});
    TEST_ASSERT(f.terrain.editVersion == 1);
    return true;
}

bool TestSetTerrainPathable() {
    Fixture f;
    // Cell (0,0) covers world [0,32)x[32): block and unblock walk.
    TEST_ASSERT(f.pathing.IsWalkable(0, 0));
    CallNative(f.vm, "SetTerrainPathable",
        {JassValue::Real(16), JassValue::Real(16), JassValue::Int(1), JassValue::Bool(false)});
    TEST_ASSERT(!f.pathing.IsWalkable(0, 0));
    TEST_ASSERT(!CallNative(f.vm, "IsTerrainPathable",
        {JassValue::Real(16), JassValue::Real(16), JassValue::Int(1)}).boolVal);
    CallNative(f.vm, "SetTerrainPathable",
        {JassValue::Real(16), JassValue::Real(16), JassValue::Int(1), JassValue::Bool(true)});
    TEST_ASSERT(f.pathing.IsWalkable(0, 0));
    // Build + blight bits address their own flags. Note blight polarity:
    // flag=false marks BLIGHTPATHING impassable, i.e. adds blight.
    CallNative(f.vm, "SetTerrainPathable",
        {JassValue::Real(16), JassValue::Real(16), JassValue::Int(3), JassValue::Bool(false)});
    TEST_ASSERT(!f.pathing.IsBuildable(0, 0));
    TEST_ASSERT(f.pathing.IsWalkable(0, 0));
    CallNative(f.vm, "SetTerrainPathable",
        {JassValue::Real(16), JassValue::Real(16), JassValue::Int(5), JassValue::Bool(false)});
    TEST_ASSERT(f.pathing.IsBlight(0, 0));
    TEST_ASSERT(CallNative(f.vm, "IsPointBlighted", {JassValue::Real(16), JassValue::Real(16)}).boolVal);
    CallNative(f.vm, "SetTerrainPathable",
        {JassValue::Real(16), JassValue::Real(16), JassValue::Int(5), JassValue::Bool(true)});
    TEST_ASSERT(!f.pathing.IsBlight(0, 0));
    // Outside the map: safe no-op.
    CallNative(f.vm, "SetTerrainPathable",
        {JassValue::Real(99999), JassValue::Real(99999), JassValue::Int(1), JassValue::Bool(false)});
    return true;
}

bool TestWaterDeformsAndDoodadAnim() {
    Fixture f;
    TEST_ASSERT(f.sim.GetWaterDeforms());
    CallNative(f.vm, "SetWaterDeforms", {JassValue::Bool(false)});
    TEST_ASSERT(!f.sim.GetWaterDeforms());
    CallNative(f.vm, "SetDoodadAnimation",
        {JassValue::Real(100), JassValue::Real(200), JassValue::Real(300), JassValue::Int(0x44303030),
         JassValue::Bool(true), JassValue::Str("stand"), JassValue::Bool(false)});
    TEST_ASSERT(f.data->doodadAnims.size() == 1);
    TEST_ASSERT(f.data->doodadAnims[0].animName == "stand");
    TEST_ASSERT(f.data->doodadAnims[0].nearestOnly);
    return true;
}

bool TestTerrainDeformRegistry() {
    Fixture f;
    const int crater = CallNative(f.vm, "TerrainDeformCrater",
        {JassValue::Real(0), JassValue::Real(0), JassValue::Real(100), JassValue::Real(-50),
         JassValue::Int(10), JassValue::Bool(false)}).intVal;
    TEST_ASSERT(crater != 0);
    const int ripple = CallNative(f.vm, "TerrainDeformRipple",
        {JassValue::Real(0), JassValue::Real(0), JassValue::Real(10), JassValue::Real(5),
         JassValue::Int(3), JassValue::Int(2), JassValue::Real(1), JassValue::Real(1),
         JassValue::Real(0), JassValue::Bool(false)}).intVal;
    TEST_ASSERT(ripple != 0 && ripple != crater);
    const int wave = CallNative(f.vm, "TerrainDeformWave",
        {JassValue::Real(0), JassValue::Real(0), JassValue::Real(1), JassValue::Real(0),
         JassValue::Real(100), JassValue::Real(10), JassValue::Real(5), JassValue::Real(2),
         JassValue::Int(1), JassValue::Int(1)}).intVal;
    const int random = CallNative(f.vm, "TerrainDeformRandom",
        {JassValue::Real(0), JassValue::Real(0), JassValue::Real(50), JassValue::Real(-5),
         JassValue::Real(5), JassValue::Int(4), JassValue::Int(1)}).intVal;
    TEST_ASSERT(wave != 0 && random != 0);
    // Short arg lists fail closed with the null handle.
    TEST_ASSERT(CallNative(f.vm, "TerrainDeformRipple", {JassValue::Real(0)}).intVal == 0);

    CallNative(f.vm, "TerrainDeformStop", {JassValue::Int(crater), JassValue::Int(1)});
    TEST_ASSERT(f.data->terrainDeforms[crater]->stopped);
    TEST_ASSERT(!f.data->terrainDeforms[ripple]->stopped);
    CallNative(f.vm, "TerrainDeformStopAll", {});
    TEST_ASSERT(f.data->terrainDeforms[ripple]->stopped);
    TEST_ASSERT(f.data->terrainDeforms[wave]->stopped);
    return true;
}

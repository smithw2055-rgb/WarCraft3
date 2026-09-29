// Hashtable + gamecache native tests (ref hashtable/gamecache API):
// cell round-trips, handle cells, flush scopes, disk persistence,
// unit snapshot/restore.
#include <iostream>
#include <memory>
#include <vector>

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

CUnitType* MakeCacheType() {
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

struct Fixture {
    JassVM vm;
    CSimulation sim;
    std::unique_ptr<JassNatives> natives;
    std::shared_ptr<JassNativesData> data;

    Fixture() {
        sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
    }
};

} // namespace

bool TestHashtableValues() {
    Fixture f;
    const int table = CallNative(f.vm, "InitHashtable", {}).intVal;
    TEST_ASSERT(table != 0);

    // Defaults on empty cells.
    TEST_ASSERT(CallNative(f.vm, "LoadInteger", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2)}).intVal == 0);
    TEST_ASSERT(!CallNative(f.vm, "HaveSavedInteger", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2)}).boolVal);

    CallNative(f.vm, "SaveInteger", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2), JassValue::Int(42)});
    CallNative(f.vm, "SaveReal", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(3), JassValue::Real(2.5f)});
    CallNative(f.vm, "SaveBoolean", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(2), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "SaveStr", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(3), JassValue::Str("hi")}).boolVal);

    TEST_ASSERT(CallNative(f.vm, "LoadInteger", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2)}).intVal == 42);
    TEST_ASSERT(CallNative(f.vm, "LoadReal", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(3)}).realVal == 2.5f);
    TEST_ASSERT(CallNative(f.vm, "LoadBoolean", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(2)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "LoadStr", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(3)}).strVal == "hi");
    TEST_ASSERT(CallNative(f.vm, "HaveSavedString", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(3)}).boolVal);

    // Parent/child keys are independent.
    CallNative(f.vm, "SaveInteger", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(3), JassValue::Int(7)});
    TEST_ASSERT(CallNative(f.vm, "LoadInteger", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2)}).intVal == 42);

    // FlushChildHashtable drops one parent; FlushParentHashtable all.
    CallNative(f.vm, "FlushChildHashtable", {JassValue::Int(table), JassValue::Int(1)});
    TEST_ASSERT(!CallNative(f.vm, "HaveSavedInteger", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "HaveSavedBoolean", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(2)}).boolVal);
    CallNative(f.vm, "RemoveSavedBoolean", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(2)});
    TEST_ASSERT(!CallNative(f.vm, "HaveSavedBoolean", {JassValue::Int(table), JassValue::Int(2), JassValue::Int(2)}).boolVal);
    CallNative(f.vm, "SaveInteger", {JassValue::Int(table), JassValue::Int(9), JassValue::Int(9), JassValue::Int(1)});
    CallNative(f.vm, "FlushParentHashtable", {JassValue::Int(table)});
    TEST_ASSERT(!CallNative(f.vm, "HaveSavedInteger", {JassValue::Int(table), JassValue::Int(9), JassValue::Int(9)}).boolVal);
    return true;
}

bool TestHashtableHandles() {
    Fixture f;
    const int table = CallNative(f.vm, "InitHashtable", {}).intVal;
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8801, MakeCacheType(), 0, War3ID("hfoo"), 0, 0, 0));
    f.sim.Update();
    (void)u;
    const int uh = f.data->AllocHandle(JassHandleInfo::UNIT, 8801);

    TEST_ASSERT(CallNative(f.vm, "SaveUnitHandle", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(1), JassValue::Int(uh)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "LoadUnitHandle", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(1)}).intVal == uh);
    TEST_ASSERT(CallNative(f.vm, "HaveSavedHandle", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(1)}).boolVal);
    // Null handles are rejected, missing cells read 0.
    TEST_ASSERT(!CallNative(f.vm, "SaveUnitHandle", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2), JassValue::Int(0)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "LoadUnitHandle", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(2)}).intVal == 0);
    // Bad table handles fail closed.
    TEST_ASSERT(CallNative(f.vm, "LoadInteger", {JassValue::Int(999999), JassValue::Int(1), JassValue::Int(1)}).intVal == 0);
    CallNative(f.vm, "RemoveSavedHandle", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(1)});
    TEST_ASSERT(!CallNative(f.vm, "HaveSavedHandle", {JassValue::Int(table), JassValue::Int(1), JassValue::Int(1)}).boolVal);
    return true;
}

bool TestGameCacheValuesAndFlush() {
    Fixture f;
    const int cache = CallNative(f.vm, "InitGameCache", {JassValue::Str("test-campaign")}).intVal;
    TEST_ASSERT(cache != 0);

    TEST_ASSERT(CallNative(f.vm, "GetStoredInteger", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("k")}).intVal == 0);
    CallNative(f.vm, "StoreInteger", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("gold"), JassValue::Int(500)});
    CallNative(f.vm, "StoreReal", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("time"), JassValue::Real(1.5f)});
    CallNative(f.vm, "StoreBoolean", {JassValue::Int(cache), JassValue::Str("m2"), JassValue::Str("won"), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "StoreString", {JassValue::Int(cache), JassValue::Str("m2"), JassValue::Str("hero"), JassValue::Str("Paladin")}).boolVal);

    TEST_ASSERT(CallNative(f.vm, "GetStoredInteger", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("gold")}).intVal == 500);
    TEST_ASSERT(CallNative(f.vm, "GetStoredBoolean", {JassValue::Int(cache), JassValue::Str("m2"), JassValue::Str("won")}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "GetStoredString", {JassValue::Int(cache), JassValue::Str("m2"), JassValue::Str("hero")}).strVal == "Paladin");
    TEST_ASSERT(CallNative(f.vm, "HaveStoredReal", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("time")}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "HaveStoredUnit", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("nope")}).boolVal);

    CallNative(f.vm, "FlushStoredInteger", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("gold")});
    TEST_ASSERT(!CallNative(f.vm, "HaveStoredInteger", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("gold")}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "HaveStoredReal", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("time")}).boolVal);
    CallNative(f.vm, "FlushStoredMission", {JassValue::Int(cache), JassValue::Str("m1")});
    TEST_ASSERT(!CallNative(f.vm, "HaveStoredReal", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("time")}).boolVal);
    CallNative(f.vm, "FlushGameCache", {JassValue::Int(cache)});
    TEST_ASSERT(!CallNative(f.vm, "HaveStoredString", {JassValue::Int(cache), JassValue::Str("m2"), JassValue::Str("hero")}).boolVal);
    return true;
}

bool TestGameCacheDiskPersistence() {
    Fixture f;
    const std::string file = "test-persist-campaign";
    const int cache = CallNative(f.vm, "InitGameCache", {JassValue::Str(file)}).intVal;
    CallNative(f.vm, "StoreInteger", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("gold"), JassValue::Int(777)});
    CallNative(f.vm, "StoreString", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("note"), JassValue::Str("a\tb\nc\\d")});
    TEST_ASSERT(CallNative(f.vm, "SaveGameCache", {JassValue::Int(cache)}).boolVal);

    // A fresh VM+cache for the same file restores persisted values.
    Fixture g;
    const int cache2 = CallNative(g.vm, "InitGameCache", {JassValue::Str(file)}).intVal;
    TEST_ASSERT(CallNative(g.vm, "GetStoredInteger", {JassValue::Int(cache2), JassValue::Str("m1"), JassValue::Str("gold")}).intVal == 777);
    TEST_ASSERT(CallNative(g.vm, "GetStoredString", {JassValue::Int(cache2), JassValue::Str("m1"), JassValue::Str("note")}).strVal == "a\tb\nc\\d");

    // ReloadGameCachesFromDisk picks up external edits.
    CallNative(g.vm, "StoreInteger", {JassValue::Int(cache2), JassValue::Str("m1"), JassValue::Str("gold"), JassValue::Int(1)});
    TEST_ASSERT(CallNative(g.vm, "ReloadGameCachesFromDisk", {}).boolVal);
    TEST_ASSERT(CallNative(g.vm, "GetStoredInteger", {JassValue::Int(cache2), JassValue::Str("m1"), JassValue::Str("gold")}).intVal == 777);
    return true;
}

bool TestGameCacheStoreRestoreUnit() {
    Fixture f;
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8811, MakeCacheType(), 0, War3ID("hfoo"), 100, 200, 0));
    f.sim.Update();
    (void)u;
    const int uh = f.data->AllocHandle(JassHandleInfo::UNIT, 8811);

    const int cache = CallNative(f.vm, "InitGameCache", {JassValue::Str("test-units")}).intVal;
    TEST_ASSERT(CallNative(f.vm, "StoreUnit", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("guard"), JassValue::Int(uh)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "HaveStoredUnit", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("guard")}).boolVal);

    // RestoreUnit without unit data fails closed (no crash).
    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    TEST_ASSERT(CallNative(f.vm, "RestoreUnit",
        {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("guard"), JassValue::Int(p0),
         JassValue::Real(500), JassValue::Real(600), JassValue::Real(90)}).type == JassValue::NULL_VAL);
    CallNative(f.vm, "FlushStoredUnit", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("guard")});
    TEST_ASSERT(!CallNative(f.vm, "HaveStoredUnit", {JassValue::Int(cache), JassValue::Str("m1"), JassValue::Str("guard")}).boolVal);
    return true;
}

bool TestGameCacheSyncStored() {
    // SyncStored* close the last common.j native-declaration gap: with no
    // net layer the local value is authoritative, so scalars read through
    // like their GetStored* counterparts.
    Fixture f;
    const int cache = CallNative(f.vm, "InitGameCache", {JassValue::Str("test-sync")}).intVal;
    TEST_ASSERT(cache != 0);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredInteger", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("n")}).intVal == 0);
    CallNative(f.vm, "StoreInteger", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("n"), JassValue::Int(314)});
    CallNative(f.vm, "StoreReal", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("r"), JassValue::Real(2.75f)});
    CallNative(f.vm, "StoreBoolean", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("b"), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "StoreString", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("s"), JassValue::Str("synced")}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredInteger", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("n")}).intVal == 314);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredReal", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("r")}).realVal == 2.75f);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredBoolean", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("b")}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredString", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("s")}).strVal == "synced");
    // Missing keys fail closed like the Get* variants.
    TEST_ASSERT(!CallNative(f.vm, "SyncStoredBoolean", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("nope")}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredString", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("nope")}).strVal.empty());
    // Unit sync without a stored unit fails closed (no crash); with unit
    // data absent the restore path also fails closed like RestoreUnit.
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8821, MakeCacheType(), 0, War3ID("hfoo"), 0, 0, 0));
    f.sim.Update();
    (void)u;
    const int uh = f.data->AllocHandle(JassHandleInfo::UNIT, 8821);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredUnit", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("nope")}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "StoreUnit", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("guard"), JassValue::Int(uh)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "SyncStoredUnit", {JassValue::Int(cache), JassValue::Str("m"), JassValue::Str("guard")}).intVal == 0);
    return true;
}

// Phase 3 Engine Modernization Tests:
// 1. Sparse-Set ECS Registry (SimEcsRegistry / SparseComponentPool<T>) & CUnit Orthogonal Subsystem Components
// 2. JassVM Bytecode Compiler, Stack VM Executor & O(1) Indexed Fast-Local Slots
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ecs/ecs_registry.h"
#include "ecs/unit_components.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "scripting/JassVM.h"
#include "scripting/JassBytecode.h"
#include "scripting/jass_binder.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

#define TEST_NEAR(a, b, eps)                                                \
    do {                                                                    \
        if (std::fabs((a) - (b)) > (eps)) {                                 \
            std::cerr << "NEAR @" << __LINE__ << ": " #a " (" << (a)        \
                      << ") vs " #b " (" << (b) << ")\n";                   \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

CUnitType* MakePhase3UnitType(War3ID code, int maxLife = 500, int speed = 270, bool building = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 20;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", code,
        maxLife, 1.0f, 0.5f, CRegenType::ALWAYS,
        200, 200, speed, 2, War3ID(),
        std::vector<War3ID>{}, building, PathingGridMovementType::FOOT,
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

struct CustomTagComp {
    int tagId = 0;
    float weight = 0.0f;
};

} // namespace

bool TestSparseSetEcsRegistryAndUnitComponents() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    // 1. Test SparseComponentPool & SimEcsRegistry contiguous storage and swap-and-pop removal
    SimEcsRegistry& reg = sim.Registry();
    reg.Emplace<CustomTagComp>(10, CustomTagComp{100, 1.5f});
    reg.Emplace<CustomTagComp>(20, CustomTagComp{200, 2.5f});
    reg.Emplace<CustomTagComp>(30, CustomTagComp{300, 3.5f});

    TEST_ASSERT(reg.Pool<CustomTagComp>().Size() == 3);
    TEST_ASSERT(reg.Has<CustomTagComp>(20));
    TEST_ASSERT(reg.TryGet<CustomTagComp>(20)->tagId == 200);

    // Swap-and-pop entity 20; entity 30 moves into its dense slot seamlessly
    TEST_ASSERT(reg.Remove<CustomTagComp>(20));
    TEST_ASSERT(!reg.Has<CustomTagComp>(20));
    TEST_ASSERT(reg.Pool<CustomTagComp>().Size() == 2);
    TEST_ASSERT(reg.TryGet<CustomTagComp>(10)->tagId == 100);
    TEST_ASSERT(reg.TryGet<CustomTagComp>(30)->tagId == 300);

    float sumWeights = 0.0f;
    reg.Each<CustomTagComp>([&](int /*entityId*/, const CustomTagComp& c) {
        sumWeights += c.weight;
    });
    TEST_NEAR(sumWeights, 5.0f, 1e-4f);

    // 2. Test CUnit orthogonal subsystem components (Construction, Cargo, Morph, StructureNav, Visual, CorpseDrop)
    CUnitType* baseType = MakePhase3UnitType(War3ID("hfoo"), 500, 270, false);
    CUnitType* morphType = MakePhase3UnitType(War3ID("hdhw"), 900, 350, false);

    CUnit* u1 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), baseType, 0, War3ID("hfoo"), 10.0f, 20.0f, 0.0f));
    CUnit* u2 = sim.AdoptUnit(std::make_unique<CUnit>(
        sim.AllocateHandleId(), baseType, 0, War3ID("hfoo"), 30.0f, 40.0f, 0.0f));
    sim.Update();

    // Attach custom ECS component to u1 and verify auto-cleanup on RemoveUnit
    reg.Emplace<CustomTagComp>(u1->GetHandleId(), CustomTagComp{999, 9.0f});
    TEST_ASSERT(reg.Has<CustomTagComp>(u1->GetHandleId()));

    // Construction component
    u1->SetConstructing(true);
    u1->SetConstructionProgress(65.0f);
    u1->SetWorker(u2->GetHandleId(), true);
    TEST_ASSERT(u1->ConstructionComp().constructing);
    TEST_NEAR(u1->ConstructionComp().constructionProgress, 65.0f, 1e-4f);
    TEST_ASSERT(u1->ConstructionComp().workerHandleId == u2->GetHandleId());

    // Cargo component
    TEST_ASSERT(u1->LoadUnit(u2));
    TEST_ASSERT(u1->CargoComp().cargoUnits.size() == 1);
    TEST_ASSERT(u2->CargoComp().loadedAboard);
    u1->UnloadUnitAt(u2, 50.0f, 60.0f);
    TEST_ASSERT(u1->CargoComp().cargoUnits.empty());
    TEST_ASSERT(!u2->CargoComp().loadedAboard);

    // Morph component
    u1->MorphTo(morphType, War3ID("hdhw"));
    TEST_ASSERT(u1->MorphComp().morphed);
    TEST_ASSERT(u1->MorphComp().originalTypeId == War3ID("hfoo"));
    TEST_NEAR(u1->GetMaxLife(), 900.0f, 1e-4f);
    u1->Unmorph();
    TEST_ASSERT(!u1->MorphComp().morphed);
    TEST_NEAR(u1->GetMaxLife(), 500.0f, 1e-4f);

    // StructureNav & Visual components
    u1->SetRallyPoint(128.0f, 256.0f, 851971);
    u1->SetWaygateDestination(512.0f, 1024.0f);
    u1->SetWaygateActive(true);
    TEST_NEAR(u1->StructureNavComp().rallyX, 128.0f, 1e-4f);
    TEST_ASSERT(u1->StructureNavComp().waygateActive);

    u1->SetScale(1.75f);
    u1->SetVertexColor(128, 64, 32, 200);
    TEST_NEAR(u1->VisualComp().scale, 1.75f, 1e-4f);
    TEST_ASSERT(u1->VisualComp().vertexR == 128 && u1->VisualComp().vertexA == 200);

    // Remove u1 and verify SimEcsRegistry destroys its attached components on ProcessUnitRemovals
    const int u1Handle = u1->GetHandleId();
    sim.RemoveUnit(u1);
    sim.Update();
    TEST_ASSERT(!reg.Has<CustomTagComp>(u1Handle));

    return true;
}

bool TestJassBytecodeCompilerAndIndexedLocals() {
    JassVM vm;
    jass::RegisterNativeAuto(vm, "NativeMulAdd", [](int a, int b, int c) -> int {
        return a * b + c;
    });

    const std::string script = R"(
        native NativeMulAdd takes integer a, integer b, integer c returns integer

        globals
            integer g_fibResult = 0
            integer g_branchResult = 0
            string g_strResult = ""
            integer array g_arr
        endglobals

        function ComputeFib takes integer n returns integer
            local integer a = 0
            local integer b = 1
            local integer i = 0
            local integer tmp = 0
            loop
                exitwhen i >= n
                set tmp = a + b
                set a = b
                set b = tmp
                set i = i + 1
            endloop
            return a
        endfunction

        function EvaluateBranches takes integer x, boolean flag returns integer
            local integer acc = 0
            if x > 10 and flag then
                set acc = NativeMulAdd(x, 2, 5)
            elseif x > 5 or flag then
                set acc = x * 3
            else
                set acc = -x
            endif
            return acc
        endfunction

        function main takes nothing returns nothing
            set g_fibResult = ComputeFib(10)
            set g_branchResult = EvaluateBranches(12, true) + EvaluateBranches(7, false) + EvaluateBranches(2, false)
            set g_arr[3] = g_fibResult + g_branchResult
            set g_strResult = "FIB=" + "55"
        endfunction
    )";

    TEST_ASSERT(vm.LoadScript(script));

    // Verify functions were compiled into flat bytecode chunks with indexed local slots
    const JassBytecodeChunk* fibChunk = vm.GetCompiledBytecode("ComputeFib");
    const JassBytecodeChunk* branchChunk = vm.GetCompiledBytecode("EvaluateBranches");
    const JassBytecodeChunk* mainChunk = vm.GetCompiledBytecode("main");
    TEST_ASSERT(fibChunk != nullptr);
    TEST_ASSERT(branchChunk != nullptr);
    TEST_ASSERT(mainChunk != nullptr);
    TEST_ASSERT(fibChunk->numLocalSlots == 5); // n, a, b, i, tmp
    TEST_ASSERT(!fibChunk->code.empty());

    // Execute via Bytecode VM
    vm.SetBytecodeEnabled(true);
    const uint64_t beforeCount = vm.GetBytecodeExecutedCount();
    vm.Execute("main");
    TEST_ASSERT(vm.GetBytecodeExecutedCount() >= beforeCount + 5);

    const int bcFib = vm.GetGlobal("g_fibResult").AsInt();
    const int bcBranch = vm.GetGlobal("g_branchResult").AsInt();
    const std::string bcStr = vm.GetGlobal("g_strResult").strVal;
    TEST_ASSERT(bcFib == 55); // Fib(10) = 55
    // EvaluateBranches(12, true) = 12*2+5 = 29; (7, false) = 21; (2, false) = -2 -> 29 + 21 - 2 = 48
    TEST_ASSERT(bcBranch == 48);
    TEST_ASSERT(bcStr == "FIB=55");

    // Reset globals, disable bytecode to force AST + Indexed fastLocals path, and verify bit-identical results
    vm.SetGlobal("g_fibResult", JassValue::Int(0));
    vm.SetGlobal("g_branchResult", JassValue::Int(0));
    vm.SetGlobal("g_strResult", JassValue::Str(""));
    vm.SetBytecodeEnabled(false);
    vm.Execute("main");

    TEST_ASSERT(vm.GetGlobal("g_fibResult").AsInt() == bcFib);
    TEST_ASSERT(vm.GetGlobal("g_branchResult").AsInt() == bcBranch);
    TEST_ASSERT(vm.GetGlobal("g_strResult").strVal == bcStr);

    return true;
}

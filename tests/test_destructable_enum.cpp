// EnumDestructablesInRect + GetEnum/GetFilterDestructable tests.
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

const char* kDestScript = R"JASS(
globals
    integer enumCount = 0
    integer lastEnum = 0
    integer filterSeen = 0
endglobals

function DestFilter takes nothing returns boolean
    set filterSeen = GetFilterDestructable()
    return true
endfunction

function DestAction takes nothing returns nothing
    set enumCount = enumCount + 1
    set lastEnum = GetEnumDestructable()
endfunction
)JASS";

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

} // namespace

bool TestEnumDestructablesInRect() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kDestScript));

    auto* inside = sim.CreateDestructable(War3ID("ATtr"), 10.0f, 10.0f, 100.0f);
    auto* outside = sim.CreateDestructable(War3ID("ATtr"), 900.0f, 900.0f, 100.0f);
    TEST_ASSERT(inside && outside);
    sim.Update(); // publish to GetDestructables()

    const int rect = CallNative(vm, "Rect", {JassValue::Real(-100.0f), JassValue::Real(-100.0f),
                                             JassValue::Real(100.0f), JassValue::Real(100.0f)}).intVal;
    TEST_ASSERT(rect != 0);

    CallNative(vm, "EnumDestructablesInRect",
               {JassValue::Int(rect), JassValue::Str("DestFilter"), JassValue::Str("DestAction")});
    TEST_ASSERT(vm.GetGlobal("enumCount").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("lastEnum").intVal != 0);
    TEST_ASSERT(vm.GetGlobal("filterSeen").intVal != 0);

    // Filtered-out enumeration runs nothing.
    TEST_ASSERT(vm.LoadScript(R"JASS(
function NeverFilter takes nothing returns boolean
    return false
endfunction
)JASS"));
    CallNative(vm, "EnumDestructablesInRect",
               {JassValue::Int(rect), JassValue::Str("NeverFilter"), JassValue::Str("DestAction")});
    TEST_ASSERT(vm.GetGlobal("enumCount").intVal == 1);
    return true;
}

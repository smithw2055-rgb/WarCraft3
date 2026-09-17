// ForGroup sleep-drain test: TriggerSleepAction inside an enumeration
// callback cannot suspend the queue model, but it must not poison outer
// threads either (sleepRequested_ drained after each sync call).
#include <iostream>

#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

const char* kSleepScript = R"JASS(
globals
    integer probe = 0
endglobals

function SleepyCb takes nothing returns nothing
    call TriggerSleepAction(5.0)
    set probe = 99
endfunction

function HealthyFn takes nothing returns nothing
    set probe = 42
endfunction
)JASS";

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

} // namespace

bool TestForGroupSleepDoesNotPoisonVm() {
    JassVM vm;
    JassNatives natives(vm, nullptr);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kSleepScript));

    const int group = CallNative(vm, "CreateGroup", {}).intVal;
    TEST_ASSERT(group != 0);
    // GroupAddUnit stores the raw handle; resolution is unnecessary here.
    CallNative(vm, "GroupAddUnit", {JassValue::Int(group), JassValue::Int(1234)});
    CallNative(vm, "ForGroup", {JassValue::Int(group), JassValue::Str("SleepyCb")});

    // The sleep aborted the callback (99 never set) and was drained:
    // a later synchronous call must execute normally.
    TEST_ASSERT(vm.GetGlobal("probe").intVal == 0);
    vm.CallFunction("HealthyFn");
    TEST_ASSERT(vm.GetGlobal("probe").intVal == 42);
    return true;
}

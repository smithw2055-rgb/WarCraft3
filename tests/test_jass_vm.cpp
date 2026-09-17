#include "scripting/JassVM.h"
#include <cmath>
#include <iostream>

bool TestJassVmThreadSleepResumesAfterWait() {
    JassVM vm;
    const char* script = R"JASS(
native TriggerSleepAction takes real timeout returns nothing

globals
    integer progress = 0
    integer inner = 0
    integer loopIndex = 0
endglobals

function WaitThenContinue takes nothing returns nothing
    set progress = 1
    call TriggerSleepAction(1.50)
    set progress = 2
endfunction

function WaitInsideIf takes nothing returns nothing
    set progress = 10
    if true then
        set progress = 20
        call TriggerSleepAction(0.50)
        set inner = 30
    endif
    set progress = 40
endfunction

function WaitInsideLoop takes nothing returns nothing
    loop
        set loopIndex = loopIndex + 1
        call TriggerSleepAction(0.25)
        set progress = loopIndex
        exitwhen loopIndex >= 2
    endloop
endfunction
)JASS";

    if (!vm.LoadScript(script)) {
        std::cerr << "JASS VM sleep test failed to load script" << std::endl;
        return false;
    }

    vm.RegisterNative("TriggerSleepAction", [&vm](const std::vector<JassValue>& args) -> JassValue {
        const float seconds = args.empty() ? 0.0f : args[0].realVal;
        vm.RequestSleep(seconds);
        return JassValue::Null();
    });

    JassVM::ThreadState state;
    JassVM::ThreadRunResult first = vm.RunThread(state, "WaitThenContinue");
    if (!first.sleeping || first.completed || std::fabs(first.sleepSeconds - 1.5f) > 0.001f) {
        std::cerr << "JASS VM sleep test did not yield with expected duration" << std::endl;
        return false;
    }
    if (vm.GetGlobal("progress").intVal != 1) {
        std::cerr << "JASS VM sleep test advanced past wait before resume" << std::endl;
        return false;
    }

    JassVM::ThreadRunResult second = vm.RunThread(state, "WaitThenContinue");
    if (!second.completed || second.sleeping) {
        std::cerr << "JASS VM sleep test did not complete after resume" << std::endl;
        return false;
    }
    if (vm.GetGlobal("progress").intVal != 2) {
        std::cerr << "JASS VM sleep test did not resume after wait" << std::endl;
        return false;
    }

    state = JassVM::ThreadState();
    vm.SetGlobal("progress", JassValue::Int(0));
    vm.SetGlobal("inner", JassValue::Int(0));
    first = vm.RunThread(state, "WaitInsideIf");
    if (!first.sleeping || first.completed || std::fabs(first.sleepSeconds - 0.5f) > 0.001f) {
        std::cerr << "JASS VM if sleep test did not yield inside if block" << std::endl;
        return false;
    }
    if (vm.GetGlobal("progress").intVal != 20 || vm.GetGlobal("inner").intVal != 0) {
        std::cerr << "JASS VM if sleep test advanced past wait before resume" << std::endl;
        return false;
    }
    second = vm.RunThread(state, "WaitInsideIf");
    if (!second.completed || second.sleeping ||
        vm.GetGlobal("progress").intVal != 40 ||
        vm.GetGlobal("inner").intVal != 30) {
        std::cerr << "JASS VM if sleep test did not resume inside if block" << std::endl;
        return false;
    }

    state = JassVM::ThreadState();
    vm.SetGlobal("progress", JassValue::Int(0));
    vm.SetGlobal("loopIndex", JassValue::Int(0));
    first = vm.RunThread(state, "WaitInsideLoop");
    if (!first.sleeping || first.completed || vm.GetGlobal("loopIndex").intVal != 1 ||
        vm.GetGlobal("progress").intVal != 0) {
        std::cerr << "JASS VM loop sleep test did not yield on first iteration" << std::endl;
        return false;
    }
    second = vm.RunThread(state, "WaitInsideLoop");
    if (!second.sleeping || second.completed || vm.GetGlobal("loopIndex").intVal != 2 ||
        vm.GetGlobal("progress").intVal != 1) {
        std::cerr << "JASS VM loop sleep test did not resume through first iteration" << std::endl;
        return false;
    }
    JassVM::ThreadRunResult third = vm.RunThread(state, "WaitInsideLoop");
    if (!third.completed || third.sleeping || vm.GetGlobal("progress").intVal != 2) {
        std::cerr << "JASS VM loop sleep test did not complete after second resume" << std::endl;
        return false;
    }

    return true;
}

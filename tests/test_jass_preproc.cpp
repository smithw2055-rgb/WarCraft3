// vJASS preprocessor tests: textmacro expansion and static if branching
// (compile-time conditionals with LIBRARY_* presence).
#include <iostream>
#include <string>

#include "scripting/JassVM.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

bool Load(JassVM& vm, const std::string& source) {
    if (!vm.LoadScript(source)) return false;
    for (const auto& e : vm.GetErrors()) {
        std::cerr << "script error: " << e << "\n";
    }
    return vm.GetErrors().empty();
}

JassValue Run(JassVM& vm, const std::string& source, const std::string& entry) {
    if (!Load(vm, source)) return JassValue::Null();
    vm.ExecuteInitializers();
    return vm.CallFunction(entry);
}

} // namespace

bool TestTextmacroBasic() {
    JassVM vm;
    const std::string src = R"(
        //! textmacro ADDER name, val
        function $name$ takes nothing returns integer
            return $val$ + $val$
        endfunction
        //! endtextmacro
        //! runtextmacro ADDER(DoubleSeven, 7)
        function TestMain takes nothing returns integer
            return DoubleSeven()
        endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 14);
    return true;
}

bool TestTextmacroNested() {
    JassVM vm;
    const std::string src = R"(
        //! textmacro INNER
        function InnerFn takes nothing returns integer
            return 41
        endfunction
        //! endtextmacro
        //! textmacro OUTER
        //! runtextmacro INNER()
        function OuterFn takes nothing returns integer
            return InnerFn() + 1
        endfunction
        //! endtextmacro
        //! runtextmacro OUTER()
        function TestMain takes nothing returns integer
            return OuterFn()
        endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 42);
    return true;
}

bool TestStaticIfBranches() {
    JassVM vm;
    const std::string src = R"(
        function First takes nothing returns integer
            static if 1 + 1 == 2 then
                return 10
            elseif false then
                return 20
            else
                return 30
            endif
            return -1
        endfunction
        function Second takes nothing returns integer
            static if false then
                return 10
            elseif 2 * 3 == 6 then
                return 20
            else
                return 30
            endif
            return -1
        endfunction
        function Third takes nothing returns integer
            static if false then
                return 10
            elseif false then
                return 20
            else
                return 30
            endif
            return -1
        endfunction
        function TestMain takes nothing returns integer
            return First() * 100 + Second() * 10 + Third() - 1000 - 200 - 30
        endfunction
)";
    // 10*100 + 20*10 + 30 - 1230 = 0
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 0);
    return true;
}

bool TestStaticIfSkipsNestedIf() {
    JassVM vm;
    const std::string src = R"(
        function TestMain takes nothing returns integer
            static if false then
                if true then
                    return 111
                endif
            else
                return 222
            endif
            return -1
        endfunction
)";
    // The vanilla if/endif inside the skipped region must not confuse
    // the token-level skip depth counting.
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 222);
    return true;
}

bool TestStaticIfLibraryForward() {    JassVM vm;
    const std::string src = R"(
        static if LIBRARY_LateLib then
            globals
                integer libFlag = 1
            endglobals
        else
            globals
                integer libFlag = 2
            endglobals
        endif
        static if not LIBRARY_Nonexistent then
            function AltPath takes nothing returns integer
                return 5
            endfunction
        endif
        library LateLib
        endlibrary
        function TestMain takes nothing returns integer
            return libFlag * 10 + AltPath()
        endfunction
)";
    // Library defined after use still counts (pre-scan); missing one does not.
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 15);
    return true;
}

bool TestHookRedirects() {
    JassVM vm;
    const std::string src = R"(
        function Orig takes nothing returns integer
            return 1
        endfunction
        function Repl takes nothing returns integer
            return Orig() + 10
        endfunction
        hook Orig Repl
        function TestMain takes nothing returns integer
            return Orig()
        endfunction
)";
    // Orig() runs Repl(); Repl's own Orig() call reaches the saved original.
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 11);
    return true;
}

bool TestHookInScope() {
    JassVM vm;
    const std::string src = R"(
        scope HookScope
            function Target takes nothing returns integer
                return 1
            endfunction
            function Wrap takes nothing returns integer
                return Target() * 100
            endfunction
            hook Target Wrap
        endscope
        function TestMain takes nothing returns integer
            return Target()
        endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 100);
    return true;
}

bool TestHookUnknownErrors() {
    JassVM vm;
    const std::string src = R"(
        function Real takes nothing returns integer
            return 1
        endfunction
        hook Real Missing
)";
    // Unknown replacement is a loud load error, not a silent no-op.
    TEST_ASSERT(!Load(vm, src));
    return true;
}

bool TestLibraryNeedsOrder() {
    JassVM vm;
    const std::string src = R"(
        globals
            integer order = 0
        endglobals
        library LibB initializer InitB needs LibA
            private function InitB takes nothing returns nothing
                set order = order * 10 + 2
            endfunction
        endlibrary
        library LibA initializer InitA
            private function InitA takes nothing returns nothing
                set order = order * 10 + 1
            endfunction
        endlibrary
        function TestMain takes nothing returns integer
            return order
        endfunction
)";
    // LibB is listed first but needs LibA: dependency runs first.
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 12);
    return true;
}

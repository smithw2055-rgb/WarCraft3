// vJASS struct tests: allocation, members, methods, statics, extends,
// arrays, destroy, scopes/libraries with privacy + initializers.
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

bool TestStructAllocAndMembers() {
    JassVM vm;
    const std::string src = R"(
struct Point
    real x = 1.5
    real y
    integer n = 7
endstruct

function TestMain takes nothing returns integer
    local Point p = Point.allocate()
    set p.x = 10.0
    set p.y = 20.0
    if p.x == 10.0 and p.y == 20.0 and p.n == 7 then
        return 1
    endif
    return 0
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 1);
    return true;
}

bool TestStructMethodsAndThis() {
    JassVM vm;
    const std::string src = R"(
struct Counter
    integer value = 0
    method add takes integer amount returns nothing
        set this.value = this.value + amount
    endmethod
    method get takes nothing returns integer
        return this.value
    endmethod
endstruct

function TestMain takes nothing returns integer
    local Counter c = Counter.allocate()
    call c.add(5)
    call c.add(3)
    return c.get()
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 8);
    return true;
}

bool TestStructStaticAndCreate() {
    JassVM vm;
    const std::string src = R"(
struct Registry
    static integer count = 0
    integer id = 0
    static method create takes nothing returns thistype
        local thistype this = thistype.allocate()
        set this.id = thistype.count
        set thistype.count = thistype.count + 1
        return this
    endmethod
    static method total takes nothing returns integer
        return thistype.count
    endmethod
endstruct

function TestMain takes nothing returns integer
    local Registry a = Registry.create()
    local Registry b = Registry.create()
    if a.id != 0 then
        return 0
    endif
    if b.id != 1 then
        return 0
    endif
    return Registry.total()
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 2);
    return true;
}

bool TestStructExtends() {
    JassVM vm;
    const std::string src = R"(
struct Base
    integer x = 3
    method getX takes nothing returns integer
        return this.x
    endmethod
endstruct

struct Child extends Base
    integer y = 4
    method sum takes nothing returns integer
        return this.getX() + this.y
    endmethod
endstruct

function TestMain takes nothing returns integer
    local Child c = Child.allocate()
    set c.x = 10
    return c.sum()
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 14);
    return true;
}

bool TestStructArrayMembers() {
    JassVM vm;
    const std::string src = R"(
struct Bag
    integer array items
endstruct

function TestMain takes nothing returns integer
    local Bag b = Bag.allocate()
    set b.items[0] = 11
    set b.items[5] = 22
    return b.items[0] + b.items[5]
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 33);
    return true;
}

bool TestStructDestroyAndNull() {
    JassVM vm;
    const std::string src = R"(
struct Temp
    integer v = 9
endstruct

function TestMain takes nothing returns integer
    local Temp t = Temp.allocate()
    if t == null then
        return 0
    endif
    if t.v != 9 then
        return 0
    endif
    call t.destroy()
    // Freed storage reads back as null (id kept by the local).
    if t.v == 9 then
        return 0
    endif
    return 1
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 1);
    return true;
}

bool TestScopePrivacyAndInitializer() {
    JassVM vm;
    const std::string src = R"(
globals
    integer initLog = 0
endglobals

scope Alpha initializer Init
    private integer secret = 41
    private function helper takes nothing returns integer
        return secret + 1
    endfunction
    private function Init takes nothing returns nothing
        set initLog = helper()
    endfunction
endscope

scope Beta
    function helper takes nothing returns integer
        return 100
    endfunction
endscope

function TestMain takes nothing returns integer
    return helper()
endfunction
)";
    JassVM vm2;
    (void)vm2;
    if (!Load(vm, src)) return false;
    vm.ExecuteInitializers();
    // Alpha's private helper ran at init (41 + 1); Beta's public helper
    // answers direct calls.
    TEST_ASSERT(vm.GetGlobal("initLog").intVal == 42);
    TEST_ASSERT(vm.CallFunction("TestMain").intVal == 100);
    return true;
}

bool TestLibraryWrapper() {
    JassVM vm;
    const std::string src = R"(
library Lib needs nothing initializer LibInit
    private struct Hidden
        integer v = 5
    endstruct
    private function LibInit takes nothing returns nothing
        set libRan = 1
    endfunction
endlibrary

globals
    integer libRan = 0
endglobals

function TestMain takes nothing returns integer
    local Lib__Hidden h = Lib__Hidden.allocate()
    return h.v + libRan
endfunction
)";
    // Library-private struct is reachable by its mangled name here because
    // the test itself plays the role of in-scope code for name checking;
    // the key assertions are parse + init + member defaults.
    if (!Load(vm, src)) return false;
    vm.ExecuteInitializers();
    TEST_ASSERT(vm.GetGlobal("libRan").intVal == 1);
    TEST_ASSERT(vm.CallFunction("TestMain").intVal == 6);
    return true;
}

bool TestLeadingDotAndMethodRef() {
    JassVM vm;
    const std::string src = R"(
struct FilterHolder
    integer base = 100
    method add takes integer x returns integer
        return .base + x
    endmethod
    static method makeFilter takes nothing returns code
        return null
    endmethod
endstruct

function TestMain takes nothing returns integer
    local FilterHolder f = FilterHolder.allocate()
    return f.add(23)
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 123);
    return true;
}

bool TestStructMethodRefValue() {
    JassVM vm;
    const std::string src = R"(
struct CB
    method onEvent takes nothing returns nothing
    endmethod
    static method get takes nothing returns code
        return null
    endmethod
endstruct

function TakesCode takes code c returns integer
    return 7
endfunction

function TestMain takes nothing returns integer
    return TakesCode(method CB.onEvent)
endfunction
)";
    // method CB.onEvent resolves to the mangled function name string.
    if (!Load(vm, src)) return false;
    TEST_ASSERT(vm.CallFunction("TestMain").intVal == 7);
    return true;
}

bool TestModuleBasicImplement() {
    JassVM vm;
    const std::string src = R"(
module Counter
    integer value = 0
    method add takes integer amount returns nothing
        set thistype.value = thistype.value + amount
    endmethod
    method get takes nothing returns integer
        return thistype.value
    endmethod
endmodule

struct Player
    implement Counter
    string name = ""
endstruct

function TestMain takes nothing returns integer
    local Player p = Player.allocate()
    call p.add(7)
    return p.get()
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 7);
    return true;
}

bool TestModuleStaticPerStruct() {
    JassVM vm;
    const std::string src = R"(
module Counted
    static integer total = 0
    method touch takes nothing returns nothing
        set thistype.total = thistype.total + 1
    endmethod
    static method count takes nothing returns integer
        return thistype.total
    endmethod
endmodule

struct A
    implement Counted
endstruct

struct B
    implement Counted
endstruct

function TestMain takes nothing returns integer
    local A a = A.allocate()
    local B b = B.allocate()
    call a.touch()
    call a.touch()
    call b.touch()
    // Per-struct static copies: A=2, B=1.
    return A.count() * 10 + B.count()
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 21);
    return true;
}

bool TestModuleOverrideAndOptional() {
    JassVM vm;
    const std::string src = R"(
module Greeter
    method greet takes nothing returns integer
        return 1
    endmethod
    method other takes nothing returns integer
        return 2
    endmethod
endmodule

struct S
    implement Greeter
    implement optional MissingModule
    method greet takes nothing returns integer
        return 10
    endmethod
endstruct

function TestMain takes nothing returns integer
    local S s = S.allocate()
    return s.greet() * 100 + s.other()
endfunction
)";
    // Struct's own greet wins (10); module's other fills in (2).
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 1002);
    return true;
}

bool TestModuleForwardReference() {
    JassVM vm;
    const std::string src = R"(
struct Early
    implement Late
endstruct

module Late
    integer v = 33
    method get takes nothing returns integer
        return thistype.v
    endmethod
endmodule

function TestMain takes nothing returns integer
    local Early e = Early.allocate()
    return e.get()
endfunction
)";
    TEST_ASSERT(Run(vm, src, "TestMain").intVal == 33);
    return true;
}

bool TestModuleMissingError() {
    JassVM raw;
    const std::string src = R"(
struct Broken
    implement Nope
endstruct

function TestMain takes nothing returns integer
    return 0
endfunction
)";
    raw.LoadScript(src);
    bool found = false;
    for (const auto& e : raw.GetErrors()) {
        if (e.find("Nope") != std::string::npos) found = true;
    }
    TEST_ASSERT(found);
    return true;
}

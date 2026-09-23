// Weather effect descriptors: Add/Enable/Remove natives keep rect, code
// and on/off state (ref leaves these NYI; no particle visuals here).
#include <iostream>

#include "ecs/csimulation.h"
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

JassValue CallNative(JassVM& vm, const std::string& name,
                     const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

} // namespace

bool TestWeatherEffectLifecycle() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    TEST_ASSERT(data != nullptr);

    const int rect = CallNative(vm, "Rect",
        {JassValue::Real(0.0f), JassValue::Real(0.0f),
         JassValue::Real(512.0f), JassValue::Real(512.0f)}).intVal;
    TEST_ASSERT(rect != 0);

    // 'RAlr' Ashenvale rain, passed as an integer FourCC like JASS does.
    const int code = ((int)'R' << 24) | ((int)'A' << 16) | ((int)'l' << 8) | (int)'r';
    const int fx = CallNative(vm, "AddWeatherEffect",
        {JassValue::Int(rect), JassValue::Int(code)}).intVal;
    TEST_ASSERT(fx != 0);
    TEST_ASSERT(data->weathers.size() == 1);

    const auto& stored = data->weathers.begin()->second;
    TEST_ASSERT(stored->code == War3ID((uint32_t)code));
    TEST_ASSERT(stored->minX == 0.0f && stored->minY == 0.0f);
    TEST_ASSERT(stored->maxX == 512.0f && stored->maxY == 512.0f);
    TEST_ASSERT(!stored->enabled);

    CallNative(vm, "EnableWeatherEffect", {JassValue::Int(fx), JassValue::Bool(true)});
    TEST_ASSERT(stored->enabled);
    CallNative(vm, "EnableWeatherEffect", {JassValue::Int(fx), JassValue::Bool(false)});
    TEST_ASSERT(!stored->enabled);

    CallNative(vm, "RemoveWeatherEffect", {JassValue::Int(fx)});
    TEST_ASSERT(data->weathers.empty());
    return true;
}

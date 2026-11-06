// Phase 1 Engine Modernization Tests:
// 1. Godot-style multicast Signal<Args...> & SimEventBus coexistence with legacy callbacks
// 2. C++17 template auto-binder (JassBinder) registration and script execution
// 3. Blz*Frame <-> UIFrame live widget tree bridge and frame click trigger dispatch
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/signal.h"
#include "ecs/sim_events.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "scripting/JassVM.h"
#include "scripting/JassNatives.h"
#include "scripting/jass_binder.h"
#include "ui/UIFrame.h"
#include "ui/SimpleFrame.h"
#include "ui/ButtonFrame.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

bool TestSignalAndSimEventBusMulticast() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    int legacyDeathCount = 0;
    int subADeathCount = 0;
    int subBDeathCount = 0;
    int lastDeadHandle = -1;

    // Install legacy callback (e.g. JASS WireSimulationEvents)
    sim.SetUnitDeathCallback([&](int unitH, int /*killerH*/, int /*owner*/) {
        legacyDeathCount++;
        lastDeadHandle = unitH;
    });

    // Subscribe two independent multicast listeners (e.g. Audio + Presentation Bridge)
    SignalConnectionId connA = sim.Events().onUnitDeath.Connect(
        [&](int /*unitH*/, int /*killerH*/, int /*owner*/) { subADeathCount++; });
    SignalConnectionId connB = sim.Events().onUnitDeath.Connect(
        [&](int /*unitH*/, int /*killerH*/, int /*owner*/) { subBDeathCount++; });

    TEST_ASSERT(sim.Events().onUnitDeath.SubscriberCount() == 3);
    TEST_ASSERT(static_cast<bool>(sim.GetUnitDeathCallback()));

    // Dispatch through the legacy GetUnitDeathCallback() path used by CUnit::Damage
    sim.GetUnitDeathCallback()(42, 7, 0);
    TEST_ASSERT(legacyDeathCount == 1);
    TEST_ASSERT(subADeathCount == 1);
    TEST_ASSERT(subBDeathCount == 1);
    TEST_ASSERT(lastDeadHandle == 42);

    // Disconnect subscriber A and verify subscriber B + legacy still fire
    TEST_ASSERT(sim.Events().onUnitDeath.Disconnect(connA));
    sim.Events().onUnitDeath.Emit(99, 7, 0);
    TEST_ASSERT(legacyDeathCount == 2);
    TEST_ASSERT(subADeathCount == 1);
    TEST_ASSERT(subBDeathCount == 2);
    TEST_ASSERT(lastDeadHandle == 99);

    (void)connB;
    return true;
}

bool TestJassBinderAutoRegistration() {
    JassVM vm;
    int sideEffect = 0;

    jass::RegisterNativeAuto(vm, "ModernAddInts", [](int a, int b) -> int {
        return a + b;
    });
    jass::RegisterNativeAuto(vm, "ModernScaleReal", [](float x, float factor) -> float {
        return x * factor;
    });
    jass::RegisterNativeAuto(vm, "ModernFormatTag", [&](const std::string& prefix, int id, bool active) -> std::string {
        sideEffect = id;
        return prefix + ":" + std::to_string(id) + (active ? ":ON" : ":OFF");
    });

    const std::string script = R"(
        native ModernAddInts takes integer a, integer b returns integer
        native ModernScaleReal takes real x, real factor returns real
        native ModernFormatTag takes string prefix, integer id, boolean active returns string

        globals
            integer g_sum = 0
            real g_scaled = 0.0
            string g_tag = ""
        endglobals

        function main takes nothing returns nothing
            set g_sum = ModernAddInts(17, 25)
            set g_scaled = ModernScaleReal(2.5, 4.0)
            set g_tag = ModernFormatTag("UNIT", g_sum, true)
        endfunction
    )";

    TEST_ASSERT(vm.LoadScript(script));
    vm.Execute("main");
    TEST_ASSERT(vm.GetGlobal("g_sum").AsInt() == 42);
    TEST_ASSERT(vm.GetGlobal("g_scaled").AsReal() == 10.0f);
    TEST_ASSERT(vm.GetGlobal("g_tag").strVal == "UNIT:42:ON");
    TEST_ASSERT(sideEffect == 42);
    return true;
}

bool TestBlzFrameLiveUIFrameBridgeAndEvents() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    natives.WireSimulationEvents(sim);

    GameUI gui;
    gui.screenW = 1280;
    gui.screenH = 720;
    SimpleFrame root("Root");
    gui.rootFrame = &root;
    natives.SetBoundGameUI(&gui);

    const std::string script = R"(
        globals
            integer g_btn = 0
            integer g_clickedFrame = 0
            integer g_clickedEvent = 0
            integer g_clickCount = 0
        endglobals

        function OnBtnClick takes nothing returns nothing
            set g_clickedFrame = BlzGetTriggerFrame()
            set g_clickedEvent = BlzGetTriggerFrameEvent()
            set g_clickCount = g_clickCount + 1
        endfunction

        function main takes nothing returns nothing
            local trigger t = CreateTrigger()
            set g_btn = BlzCreateFrameByType("GLUETEXTBUTTON", "ModernActionBtn", 0, "", 0)
            call BlzFrameSetText(g_btn, "Launch Modern Engine")
            call BlzFrameSetSize(g_btn, 0.2, 0.06)
            call BlzFrameSetVisible(g_btn, true)
            call BlzTriggerRegisterFrameEvent(t, g_btn, ConvertFrameEventType(1))
            call TriggerAddAction(t, function OnBtnClick)
            call BlzFrameClick(g_btn)
        endfunction
    )";

    TEST_ASSERT(vm.LoadScript(script));
    vm.Execute("main");
    natives.PumpQueuedThreads();

    const int btnHandle = vm.GetGlobal("g_btn").AsInt();
    TEST_ASSERT(btnHandle != 0);
    TEST_ASSERT(vm.GetGlobal("g_clickCount").AsInt() == 1);
    TEST_ASSERT(vm.GetGlobal("g_clickedEvent").AsInt() == 1);

    // Verify live UIFrame node exists in GameUI registry and reflects JASS state
    UIFrame* liveNode = natives.GetLiveUIFrame(btnHandle);
    TEST_ASSERT(liveNode != nullptr);
    TEST_ASSERT(gui.frameRegistry.count("ModernActionBtn") == 1);
    TEST_ASSERT(gui.frameRegistry["ModernActionBtn"] == liveNode);
    TEST_ASSERT(liveNode->IsVisible());
    TEST_ASSERT(liveNode->GetWidth() > 300.0f);

    auto* btnNode = dynamic_cast<ButtonFrame*>(liveNode);
    TEST_ASSERT(btnNode != nullptr);
    TEST_ASSERT(btnNode->GetText() == "Launch Modern Engine");

    // Simulate real UI mouse click on ButtonFrame and verify JASS trigger fires again
    liveNode->SetPosition(100.0f, 100.0f);
    btnNode->OnMouseDown(110.0f, 110.0f, 0);
    btnNode->OnMouseUp(110.0f, 110.0f, 0);
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("g_clickCount").AsInt() == 2);

    return true;
}

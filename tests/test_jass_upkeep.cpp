// Sim-alignment tests: upkeep tax levels, upkeep-rate player states,
// race-preference query, timer-dialog speed (ref CPlayer/Jass2/timers).
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/player.h"
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
    if (it == natives.end()) {
        std::cerr << "missing native " << name << "\n";
        return JassValue::Null();
    }
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

bool TestUpkeepGoldTaxLevels() {
    Fixture f;
    CPlayer* p = f.sim.GetPlayer(0);
    TEST_ASSERT(p);
    // No upkeep at or below 50 food.
    p->SetFoodUsed(50);
    TEST_ASSERT(p->GetUpkeepLevel() == 0);
    TEST_ASSERT(p->ApplyUpkeepGoldTax(100) == 100);
    // Low upkeep 51-80: 30% gold tax, lumber untouched by upkeep path.
    p->SetFoodUsed(51);
    TEST_ASSERT(p->GetUpkeepLevel() == 1);
    TEST_ASSERT(p->ApplyUpkeepGoldTax(100) == 70);
    p->SetFoodUsed(80);
    TEST_ASSERT(p->ApplyUpkeepGoldTax(100) == 70);
    // High upkeep 81+: 60% gold tax.
    p->SetFoodUsed(81);
    TEST_ASSERT(p->GetUpkeepLevel() == 2);
    TEST_ASSERT(p->ApplyUpkeepGoldTax(100) == 40);
    // Explicit upkeep-rate override wins over the food table.
    p->SetGoldUpkeepRate(10);
    TEST_ASSERT(p->ApplyUpkeepGoldTax(100) == 90);
    p->SetGoldUpkeepRate(0);
    TEST_ASSERT(p->ApplyUpkeepGoldTax(100) == 40);

    // Player states 13/14 round-trip through natives (ref CPlayer fields).
    const int ph = f.data->AllocHandle(JassHandleInfo::PLAYER, 0);
    CallNative(f.vm, "SetPlayerState",
               {JassValue::Int(ph), JassValue::Int(13), JassValue::Int(25)});
    CallNative(f.vm, "SetPlayerState",
               {JassValue::Int(ph), JassValue::Int(14), JassValue::Int(5)});
    TEST_ASSERT(CallNative(f.vm, "GetPlayerState",
                           {JassValue::Int(ph), JassValue::Int(13)}).intVal == 25);
    TEST_ASSERT(CallNative(f.vm, "GetPlayerState",
                           {JassValue::Int(ph), JassValue::Int(14)}).intVal == 5);
    // 25% explicit gold rate now drives the deposit tax.
    TEST_ASSERT(p->ApplyUpkeepGoldTax(100) == 75);
    return true;
}

bool TestRacePrefSetQuery() {
    Fixture f;
    const int ph = f.data->AllocHandle(JassHandleInfo::PLAYER, 0);
    // Unset preference never matches (previous constant-false behavior kept
    // for the unset case, now backed by real stored state).
    TEST_ASSERT(!CallNative(f.vm, "IsPlayerRacePrefSet",
                            {JassValue::Int(ph), JassValue::Int(1)}).boolVal);
    CallNative(f.vm, "SetPlayerRacePreference", {JassValue::Int(ph), JassValue::Int(1)});
    TEST_ASSERT(CallNative(f.vm, "IsPlayerRacePrefSet",
                           {JassValue::Int(ph), JassValue::Int(1)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "IsPlayerRacePrefSet",
                            {JassValue::Int(ph), JassValue::Int(2)}).boolVal);
    // Re-setting replaces the single stored preference (ref setRacePref).
    CallNative(f.vm, "SetPlayerRacePreference", {JassValue::Int(ph), JassValue::Int(8)});
    TEST_ASSERT(!CallNative(f.vm, "IsPlayerRacePrefSet",
                            {JassValue::Int(ph), JassValue::Int(1)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "IsPlayerRacePrefSet",
                           {JassValue::Int(ph), JassValue::Int(8)}).boolVal);
    return true;
}

bool TestTimerDialogSpeedScalesDecay() {
    Fixture f;
    const int timer = CallNative(f.vm, "CreateTimer", {}).intVal;
    TEST_ASSERT(timer != 0);
    CallNative(f.vm, "TimerStart", {JassValue::Int(timer), JassValue::Real(10.0f),
                                    JassValue::Bool(false), JassValue::Str("")});
    const int dialog = CallNative(f.vm, "CreateTimerDialog", {JassValue::Int(timer)}).intVal;
    TEST_ASSERT(dialog != 0);
    // Hidden dialog: speed has no effect, real-time decay.
    CallNative(f.vm, "TimerDialogSetSpeed", {JassValue::Int(dialog), JassValue::Real(4.0f)});
    f.natives->UpdateTimers(1.0f);
    TEST_ASSERT(CallNative(f.vm, "TimerGetRemaining", {JassValue::Int(timer)}).realVal == 9.0f);
    // Displayed dialog: 4x countdown.
    CallNative(f.vm, "TimerDialogDisplay", {JassValue::Int(dialog), JassValue::Bool(true)});
    f.natives->UpdateTimers(1.0f);
    TEST_ASSERT(CallNative(f.vm, "TimerGetRemaining", {JassValue::Int(timer)}).realVal == 5.0f);
    // Zero speed freezes the countdown.
    CallNative(f.vm, "TimerDialogSetSpeed", {JassValue::Int(dialog), JassValue::Real(0.0f)});
    f.natives->UpdateTimers(1.0f);
    TEST_ASSERT(CallNative(f.vm, "TimerGetRemaining", {JassValue::Int(timer)}).realVal == 5.0f);
    return true;
}

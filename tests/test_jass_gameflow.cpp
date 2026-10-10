// Batch-5 native tests: camera state, game flow/cinematic, trigger
// extras, sound state, texttag velocity, counted group enums (ref game,
// camera, trigger, sound API).
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

CUnitType* MakeFlowType() {
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
        sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
    }
};

} // namespace

bool TestCameraFlowNatives() {
    Fixture f;
    // Noise retained for the renderer shake pass.
    CallNative(f.vm, "CameraSetSourceNoiseEx", {JassValue::Real(5), JassValue::Real(10), JassValue::Bool(true)});
    TEST_ASSERT(f.data->sourceNoise.magnitude == 5.0f);
    TEST_ASSERT(f.data->sourceNoise.vertOnly);
    CallNative(f.vm, "CameraSetTargetNoise", {JassValue::Real(2), JassValue::Real(4)});
    TEST_ASSERT(f.data->targetNoise.velocity == 4.0f);

    // Pan callbacks receive the final pose (x, z, y convention as twins).
    float tx = -1, ty = -1, tz = -1;
    f.data->camSetTarget = [&](float x, float y, float z) { tx = x; ty = y; tz = z; };
    CallNative(f.vm, "PanCameraToTimedWithZ", {JassValue::Real(100), JassValue::Real(200), JassValue::Real(50), JassValue::Real(2)});
    TEST_ASSERT(tx == 100.0f && ty == 50.0f && tz == 200.0f);
    float px = -1, py = -1, pz = -1;
    f.data->camSetPosition = [&](float x, float y, float z) { px = x; py = y; pz = z; };
    CallNative(f.vm, "SetCameraQuickPosition", {JassValue::Real(7), JassValue::Real(8)});
    TEST_ASSERT(px == 7.0f && pz == 8.0f);

    // Rotate + controller state retained.
    CallNative(f.vm, "SetCameraRotateMode", {JassValue::Real(1), JassValue::Real(2), JassValue::Real(3), JassValue::Real(4)});
    TEST_ASSERT(f.data->rotateMode.active && f.data->rotateMode.radians == 3.0f);
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8601, MakeFlowType(), 0, War3ID("hfoo"), 0, 0, 0));
    f.sim.Update();
    (void)u;
    const int uh = f.data->AllocHandle(JassHandleInfo::UNIT, 8601);
    CallNative(f.vm, "SetCameraTargetController", {JassValue::Int(uh), JassValue::Real(10), JassValue::Real(20), JassValue::Bool(true)});
    TEST_ASSERT(f.data->targetController.active);
    TEST_ASSERT(f.data->targetController.unitSimId == 8601);
    TEST_ASSERT(f.data->targetController.inheritOrientation);
    CallNative(f.vm, "SetCameraOrientController", {JassValue::Int(uh), JassValue::Real(1), JassValue::Real(2)});
    TEST_ASSERT(f.data->orientController.active);
    // Bad unit deactivates.
    CallNative(f.vm, "SetCameraTargetController", {JassValue::Int(999999), JassValue::Real(0), JassValue::Real(0), JassValue::Bool(false)});
    TEST_ASSERT(!f.data->targetController.active);
    return true;
}

bool TestGameFlowNatives() {
    Fixture f;
    // Pause freezes sim ticks.
    const int tick = f.sim.GetGameTurnTick();
    f.sim.Update();
    TEST_ASSERT(f.sim.GetGameTurnTick() == tick + 1);
    CallNative(f.vm, "PauseGame", {JassValue::Bool(true)});
    TEST_ASSERT(f.sim.IsPaused());
    f.sim.Update();
    TEST_ASSERT(f.sim.GetGameTurnTick() == tick + 1);
    CallNative(f.vm, "PauseGame", {JassValue::Bool(false)});
    TEST_ASSERT(!f.sim.IsPaused());

    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    CallNative(f.vm, "PauseCompAI", {JassValue::Int(p0), JassValue::Bool(true)});
    TEST_ASSERT(f.data->compAIpaused[0]);
    TEST_ASSERT(CallNative(f.vm, "GetAIDifficulty", {JassValue::Int(p0)}).intVal == f.data->gameConfig.defaultDifficulty);

    // Game over / cheats / restart.
    TEST_ASSERT(!CallNative(f.vm, "IsNoVictoryCheat", {}).boolVal);
    CallNative(f.vm, "EndGame", {JassValue::Bool(true)});
    TEST_ASSERT(f.data->gameEnded && f.data->gameVictory);
    CallNative(f.vm, "CripplePlayer", {JassValue::Int(p0), JassValue::Int(0), JassValue::Bool(true)});
    TEST_ASSERT(f.data->crippledPlayers.count(0) == 1);
    CallNative(f.vm, "DisableRestartMission", {JassValue::Bool(true)});
    TEST_ASSERT(f.data->restartDisabled);
    CallNative(f.vm, "Cheat", {JassValue::Str("greedisgood")});

    // Cinematic state machine.
    CallNative(f.vm, "PlayCinematic", {JassValue::Str("Cinematic.mpq")});
    TEST_ASSERT(f.data->cinematic.playing);
    TEST_ASSERT(f.data->cinematic.movie == "Cinematic.mpq");
    CallNative(f.vm, "SetCinematicScene",
        {JassValue::Int(7), JassValue::Int(1), JassValue::Str("Speaker"), JassValue::Str("Hi"),
         JassValue::Real(5), JassValue::Real(3)});
    TEST_ASSERT(f.data->cinematic.speakerTitle == "Speaker");
    TEST_ASSERT(f.data->cinematic.sceneDuration == 5.0f);
    CallNative(f.vm, "ForceCinematicSubtitles", {JassValue::Bool(true)});
    CallNative(f.vm, "SetIntroShotModel", {JassValue::Str("intro.mdx")});
    CallNative(f.vm, "SetIntroShotText", {JassValue::Str("Intro")});
    CallNative(f.vm, "DisplayLoadDialog", {});
    TEST_ASSERT(f.data->cinematic.subtitles && f.data->cinematic.loadDialog);
    TEST_ASSERT(f.data->cinematic.introModel == "intro.mdx");
    CallNative(f.vm, "EndCinematicScene", {});
    TEST_ASSERT(!f.data->cinematic.playing);

    // Players: removal closes the slot; ownership transfer moves units.
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8602, MakeFlowType(), 0, War3ID("hfoo"), 10, 10, 0));
    f.sim.Update();
    (void)u;
    f.sim.GetPlayer(0)->AddUnit(f.sim.GetUnit(8602));
    CallNative(f.vm, "SetPlayerUnitsOwner", {JassValue::Int(p0), JassValue::Int(1)});
    TEST_ASSERT(f.sim.GetUnit(8602)->GetPlayerIndex() == 1);
    TEST_ASSERT(f.sim.GetPlayer(1)->GetUnits().size() == 1);
    CallNative(f.vm, "RemovePlayer", {JassValue::Int(p0), JassValue::Int(2)});
    TEST_ASSERT(!f.data->playerMapConfig[0].slotActive);
    TEST_ASSERT(f.data->removedPlayerResults[0] == 2);

    // Game-type support + versions + preload hints.
    CallNative(f.vm, "SetGameTypeSupported", {JassValue::Int(3), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "IsGameTypeSupported", {JassValue::Int(3)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "IsGameTypeSupported", {JassValue::Int(4)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "VersionCompatible", {JassValue::Int(0)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "VersionSupported", {JassValue::Int(0)}).boolVal);
    CallNative(f.vm, "Preload", {JassValue::Str("x.mdx")});
    CallNative(f.vm, "PreloadEnd", {JassValue::Real(1)});
    CallNative(f.vm, "Preloader", {JassValue::Str("y.txt")});
    return true;
}

bool TestTriggerExtraNatives() {
    Fixture f;
    const int trig = CallNative(f.vm, "CreateTrigger", {}).intVal;
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8611, MakeFlowType(), 0, War3ID("hfoo"), 0, 0, 0));
    f.sim.Update();
    (void)u;
    const int uh = f.data->AllocHandle(JassHandleInfo::UNIT, 8611);

    // Actions return indices; removal revokes by index.
    const int a0 = CallNative(f.vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("ActA")}).intVal;
    const int a1 = CallNative(f.vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("ActB")}).intVal;
    TEST_ASSERT(a0 == 0 && a1 == 1);
    CallNative(f.vm, "TriggerRemoveAction", {JassValue::Int(trig), JassValue::Int(a0)});
    {
        auto it = f.data->triggers.find(trig);
        TEST_ASSERT(it != f.data->triggers.end());
        TEST_ASSERT(it->second->actionFuncNames.size() == 1);
        TEST_ASSERT(it->second->actionFuncNames[0] == "ActB");
    }
    // Reset clears counters and re-enables.
    {
        auto it = f.data->triggers.find(trig);
        it->second->evalCount = 5;
        it->second->enabled = false;
    }
    CallNative(f.vm, "ResetTrigger", {JassValue::Int(trig)});
    {
        auto it = f.data->triggers.find(trig);
        TEST_ASSERT(it->second->evalCount == 0 && it->second->enabled);
    }
    // State + filter + dialog interest retained.
    CallNative(f.vm, "TriggerRegisterUnitStateEvent",
        {JassValue::Int(trig), JassValue::Int(uh), JassValue::Int(1), JassValue::Int(0), JassValue::Real(50)});
    CallNative(f.vm, "TriggerRegisterFilterUnitEvent",
        {JassValue::Int(trig), JassValue::Int(uh), JassValue::Int(60), JassValue::Str("Filt")});
    const int dlg = CallNative(f.vm, "DialogCreate", {}).intVal;
    CallNative(f.vm, "TriggerRegisterDialogEvent", {JassValue::Int(trig), JassValue::Int(dlg)});
    {
        auto it = f.data->triggers.find(trig);
        TEST_ASSERT(it->second->unitStateEvents.size() == 1);
        TEST_ASSERT(it->second->unitStateEvents[0].limit == 50.0f);
        TEST_ASSERT(!it->second->conditionFuncNames.empty());
        bool dialogCode = false;
        for (const auto& [handle, code] : it->second->registeredEvents) {
            if (code == 4201) dialogCode = true;
        }
        TEST_ASSERT(dialogCode);
    }
    // Wait-on-sleeps round-trips per trigger (retail signatures take
    // whichTrigger); unknown ids read false. Sync stays unready w/o net.
    TEST_ASSERT(CallNative(f.vm, "IsTriggerWaitOnSleeps", {JassValue::Int(trig)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "IsTriggerWaitOnSleeps", {JassValue::Int(999999)}).boolVal);
    CallNative(f.vm, "TriggerWaitOnSleeps", {JassValue::Int(trig), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "IsTriggerWaitOnSleeps", {JassValue::Int(trig)}).boolVal);
    CallNative(f.vm, "TriggerWaitOnSleeps", {JassValue::Int(trig), JassValue::Bool(true)});
    CallNative(f.vm, "TriggerSyncStart", {});
    TEST_ASSERT(!CallNative(f.vm, "TriggerSyncReady", {}).boolVal);

    // Sound wait sleeps for the known duration.
    const int snd = CallNative(f.vm, "CreateSound",
        {JassValue::Str("x.wav"), JassValue::Bool(false), JassValue::Bool(false), JassValue::Bool(false),
         JassValue::Int(0), JassValue::Int(0), JassValue::Str("")}).intVal;
    CallNative(f.vm, "SetSoundDuration", {JassValue::Int(snd), JassValue::Int(2000)});
    CallNative(f.vm, "TriggerWaitForSound", {JassValue::Int(snd), JassValue::Real(0.5f)});
    TEST_ASSERT(f.vm.HasPendingSleep());
    return true;
}

bool TestSoundStateNatives() {
    Fixture f;
    const int snd = CallNative(f.vm, "CreateSoundFilenameWithLabel",
        {JassValue::Str("x.wav"), JassValue::Bool(false), JassValue::Bool(true), JassValue::Bool(false),
         JassValue::Int(0), JassValue::Int(0), JassValue::Str("MyLabel")}).intVal;
    TEST_ASSERT(snd != 0);
    {
        const int id = f.data->GetSimHandle(snd, JassHandleInfo::SOUND);
        auto it = f.data->sounds.find(id);
        TEST_ASSERT(it != f.data->sounds.end());
        TEST_ASSERT(it->second->file == "x.wav" && it->second->label == "MyLabel");
    }
    TEST_ASSERT(!CallNative(f.vm, "GetSoundIsPlaying", {JassValue::Int(snd)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "GetSoundIsLoading", {JassValue::Int(snd)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "GetSoundFileDuration", {JassValue::Str("x.wav")}).intVal == 0);
    CallNative(f.vm, "SetSoundConeAngles", {JassValue::Int(snd), JassValue::Real(30), JassValue::Real(60), JassValue::Int(50)});
    CallNative(f.vm, "SetSoundConeOrientation", {JassValue::Int(snd), JassValue::Real(1), JassValue::Real(0), JassValue::Real(0)});
    CallNative(f.vm, "SetSoundPlayPosition", {JassValue::Int(snd), JassValue::Int(1500)});
    CallNative(f.vm, "SetSoundVelocity", {JassValue::Int(snd), JassValue::Real(3), JassValue::Real(4), JassValue::Real(5)});
    {
        const int id = f.data->GetSimHandle(snd, JassHandleInfo::SOUND);
        auto it = f.data->sounds.find(id);
        TEST_ASSERT(it->second->coneInside == 30.0f && it->second->coneOutsideVolume == 50);
        TEST_ASSERT(it->second->orientX == 1.0f && it->second->velY == 4.0f);
        TEST_ASSERT(it->second->playPositionMs == 1500);
    }
    CallNative(f.vm, "RegisterStackedSound", {JassValue::Int(snd), JassValue::Bool(true), JassValue::Real(10), JassValue::Real(20)});
    CallNative(f.vm, "UnregisterStackedSound", {JassValue::Int(snd), JassValue::Bool(true), JassValue::Real(10), JassValue::Real(20)});
    {
        const int id = f.data->GetSimHandle(snd, JassHandleInfo::SOUND);
        TEST_ASSERT(!f.data->sounds[id]->stackedByPosition);
    }
    CallNative(f.vm, "VolumeGroupSetVolume", {JassValue::Int(2), JassValue::Real(0.5f)});
    TEST_ASSERT(f.data->volumeGroups[2] == 63);

    // Texttag velocity retained.
    const int tag = CallNative(f.vm, "CreateTextTag", {}).intVal;
    CallNative(f.vm, "SetTextTagVelocity", {JassValue::Int(tag), JassValue::Real(10), JassValue::Real(-20)});
    {
        const int id = f.data->GetSimHandle(tag, JassHandleInfo::TEXT_TAG);
        auto it = f.data->textTags.find(id);
        TEST_ASSERT(it != f.data->textTags.end());
        TEST_ASSERT(it->second->vx == 10.0f && it->second->vy == -20.0f);
    }
    return true;
}

bool TestGroupEnumCountedNatives() {
    Fixture f;
    for (int i = 0; i < 3; i++) {
        f.sim.AdoptUnit(std::make_unique<CUnit>(8620 + i, MakeFlowType(), 0, War3ID("hfoo"), (float)(i * 100), 0, 0));
    }
    f.sim.Update();
    const int grp = CallNative(f.vm, "CreateGroup", {}).intVal;
    CallNative(f.vm, "GroupEnumUnitsInRangeCounted",
        {JassValue::Int(grp), JassValue::Real(0), JassValue::Real(0), JassValue::Real(10000),
         JassValue::Str(""), JassValue::Int(2)});
    TEST_ASSERT(CallNative(f.vm, "CountUnitsInGroup", {JassValue::Int(grp)}).intVal == 2);
    // Non-positive limit means uncapped.
    CallNative(f.vm, "GroupEnumUnitsInRangeCounted",
        {JassValue::Int(grp), JassValue::Real(0), JassValue::Real(0), JassValue::Real(10000),
         JassValue::Str(""), JassValue::Int(0)});
    TEST_ASSERT(CallNative(f.vm, "CountUnitsInGroup", {JassValue::Int(grp)}).intVal == 3);

    const int rect = CallNative(f.vm, "Rect",
        {JassValue::Real(-10), JassValue::Real(-10), JassValue::Real(10000), JassValue::Real(10000)}).intVal;
    CallNative(f.vm, "GroupEnumUnitsInRectCounted",
        {JassValue::Int(grp), JassValue::Int(rect), JassValue::Str(""), JassValue::Int(1)});
    TEST_ASSERT(CallNative(f.vm, "CountUnitsInGroup", {JassValue::Int(grp)}).intVal == 1);
    const int loc = CallNative(f.vm, "Location", {JassValue::Real(0), JassValue::Real(0)}).intVal;
    CallNative(f.vm, "GroupEnumUnitsInRangeOfLocCounted",
        {JassValue::Int(grp), JassValue::Int(loc), JassValue::Real(10000), JassValue::Str(""), JassValue::Int(5)});
    TEST_ASSERT(CallNative(f.vm, "CountUnitsInGroup", {JassValue::Int(grp)}).intVal == 3);
    CallNative(f.vm, "GroupEnumUnitsOfTypeCounted",
        {JassValue::Int(grp), JassValue::Str("Footman"), JassValue::Str(""), JassValue::Int(5)});
    {
        const int n = CallNative(f.vm, "CountUnitsInGroup", {JassValue::Int(grp)}).intVal;
        TEST_ASSERT(n == 3);
    }
    return true;
}

bool TestTriggerExecuteWait() {
    Fixture f;
    const char* script = R"JASS(
native TriggerSleepAction takes real timeout returns nothing
native TriggerExecuteWait takes trigger whichTrigger returns nothing

globals
    integer step = 0
    trigger calleeTrig = null
endglobals

function Callee takes nothing returns nothing
    set step = 1
    call TriggerSleepAction(1.0)
    set step = 2
endfunction

function Caller takes nothing returns nothing
    call TriggerExecuteWait(calleeTrig)
    set step = 3
endfunction
)JASS";
    TEST_ASSERT(f.vm.LoadScript(script));
    const int callee = CallNative(f.vm, "CreateTrigger", {}).intVal;
    CallNative(f.vm, "TriggerAddAction", {JassValue::Int(callee), JassValue::Str("Callee")});
    f.vm.SetGlobal("calleeTrig", JassValue::Int(callee));
    // ExecuteWait forces the callee's waitOnSleeps on (ref), even if cleared.
    CallNative(f.vm, "TriggerWaitOnSleeps", {JassValue::Int(callee), JassValue::Bool(false)});
    f.natives->QueueFunction("Caller");
    f.natives->PumpQueuedThreads();
    // Callee ran to its wait; the caller is suspended behind the join.
    TEST_ASSERT(f.vm.GetGlobal("step").intVal == 1);
    TEST_ASSERT(f.natives->HasQueuedThreads());
    f.natives->UpdateTimers(0.5f);
    f.natives->PumpQueuedThreads();
    TEST_ASSERT(f.vm.GetGlobal("step").intVal == 1);
    f.natives->UpdateTimers(0.6f);
    f.natives->PumpQueuedThreads();
    // Join finished the callee remainder before the caller resumed.
    TEST_ASSERT(f.vm.GetGlobal("step").intVal == 3);
    TEST_ASSERT(!f.natives->HasQueuedThreads());

    // waitOnSleeps=false on the executing trigger skips waits entirely.
    const int skipper = CallNative(f.vm, "CreateTrigger", {}).intVal;
    CallNative(f.vm, "TriggerAddAction", {JassValue::Int(skipper), JassValue::Str("Callee")});
    CallNative(f.vm, "TriggerWaitOnSleeps", {JassValue::Int(skipper), JassValue::Bool(false)});
    f.vm.SetGlobal("step", JassValue::Int(0));
    JassQueuedContext ctx;
    ctx.currentTriggerId = skipper;
    f.natives->QueueFunctionWithContext("Callee", ctx);
    f.natives->PumpQueuedThreads();
    TEST_ASSERT(f.vm.GetGlobal("step").intVal == 2);
    TEST_ASSERT(!f.natives->HasQueuedThreads());
    return true;
}

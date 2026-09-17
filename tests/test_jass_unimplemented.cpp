// Tests for previously-unimplemented JASS natives, ported from
// warsmash_ref (Jass2.java / CAbilityHero.java / CPlayer.java):
// hero XP/level/stats, unit race/id-type, player identity, GetLocationZ,
// unit visual state, and timer dialogs.
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/abilities/ability_hero.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
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

CUnitType* MakeHeroType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Paladin", "Paladin", "icon", War3ID("Hpal"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 2, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::HERO,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"human", War3ID("H")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, true, 20, 2.0f, 15, 1.5f, 10, 1.0f,
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

} // namespace

bool TestJassHeroXpAndStats() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(7001, MakeHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(unit != nullptr);
    unit->AddAbility(sim, std::make_unique<CAbilityHero>(9001));
    sim.Update();
    auto data = natives.GetData();
    const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, 7001);

    // XP starts at 0, AddHeroXP applies and levels up (200 XP -> level 2).
    TEST_ASSERT(CallNative(vm, "GetHeroXP", {JassValue::Int(unitHandle)}).intVal == 0);
    CallNative(vm, "AddHeroXP", {JassValue::Int(unitHandle), JassValue::Int(200), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "GetHeroXP", {JassValue::Int(unitHandle)}).intVal == 200);
    TEST_ASSERT(CallNative(vm, "GetHeroLevel", {JassValue::Int(unitHandle)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "GetHeroSkillPoints", {JassValue::Int(unitHandle)}).intVal == 2);

    // SetHeroXP only increases (ref CAbilityHero.setXp).
    CallNative(vm, "SetHeroXP", {JassValue::Int(unitHandle), JassValue::Int(50), JassValue::Bool(false)});
    TEST_ASSERT(CallNative(vm, "GetHeroXP", {JassValue::Int(unitHandle)}).intVal == 200);

    // Stats reflect level growth: base 20 STR + 2/lvl => 22 at level 2.
    TEST_ASSERT(CallNative(vm, "GetHeroStr", {JassValue::Int(unitHandle)}).intVal == 22);

    // ModifyHeroStat add/subtract flows through the hero ability.
    CallNative(vm, "ModifyHeroStat",
               {JassValue::Int(unitHandle), JassValue::Int(0), JassValue::Int(1), JassValue::Int(5)});
    TEST_ASSERT(CallNative(vm, "GetHeroStr", {JassValue::Int(unitHandle)}).intVal == 27);
    CallNative(vm, "UnitModifySkillPoints", {JassValue::Int(unitHandle), JassValue::Int(-1)});
    TEST_ASSERT(CallNative(vm, "GetHeroSkillPoints", {JassValue::Int(unitHandle)}).intVal == 1);
    return true;
}

bool TestJassUnitRaceAndIdType() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(7002, MakeHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(unit != nullptr);
    sim.Update();
    auto data = natives.GetData();
    const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, 7002);

    // human race name maps to JASS ordinal 1 (ConvertRace).
    TEST_ASSERT(CallNative(vm, "GetUnitRace", {JassValue::Int(unitHandle)}).intVal == 1);
    TEST_ASSERT(CallNative(vm, "IsUnitRace", {JassValue::Int(unitHandle), JassValue::Int(1)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsUnitRace", {JassValue::Int(unitHandle), JassValue::Int(2)}).boolVal);
    // UNIT_TYPE_HERO = 0.
    TEST_ASSERT(CallNative(vm, "IsUnitIdType", {JassValue::Int(unitHandle), JassValue::Int(0)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsUnitIdType", {JassValue::Int(unitHandle), JassValue::Int(2)}).boolVal);

    // Visual state persists on the sim unit.
    CallNative(vm, "SetUnitScale", {JassValue::Int(unitHandle), JassValue::Real(1.5f)});
    CallNative(vm, "SetUnitTimeScale", {JassValue::Int(unitHandle), JassValue::Real(2.0f)});
    CallNative(vm, "SetUnitVertexColor", {JassValue::Int(unitHandle), JassValue::Int(255),
                                          JassValue::Int(0), JassValue::Int(0), JassValue::Int(128)});
    CallNative(vm, "SetUnitAcquireRange", {JassValue::Int(unitHandle), JassValue::Real(900.0f)});
    TEST_ASSERT(unit->GetScale() == 1.5f);
    TEST_ASSERT(unit->GetTimeScale() == 2.0f);
    int r = 0, g = 0, b = 0, a = 0;
    unit->GetVertexColor(r, g, b, a);
    TEST_ASSERT(r == 255 && g == 0 && b == 0 && a == 128);
    TEST_ASSERT(CallNative(vm, "GetUnitAcquireRange", {JassValue::Int(unitHandle)}).realVal == 900.0f);
    return true;
}

bool TestJassPlayerIdentityAndLocationZ() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "Alice", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "Bob", CMapControl::COMPUTER));
    sim.GetPlayer(0)->SetRace(1);
    sim.GetPlayer(0)->SetColor(3);
    sim.GetPlayer(1)->SetObserver(true);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    data->SetLocalPlayerIndex(1);
    data->SetTerrainHeightCallback([](float x, float y) { return x + y; });

    const int p0 = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int p1 = data->AllocHandle(JassHandleInfo::PLAYER, 1);

    TEST_ASSERT(CallNative(vm, "GetPlayerName", {JassValue::Int(p0)}).strVal == "Alice");
    CallNative(vm, "SetPlayerName", {JassValue::Int(p0), JassValue::Str("Alicia")});
    TEST_ASSERT(sim.GetPlayer(0)->GetName() == "Alicia");
    TEST_ASSERT(CallNative(vm, "GetPlayerRace", {JassValue::Int(p0)}).intVal == 1);
    TEST_ASSERT(CallNative(vm, "GetPlayerColor", {JassValue::Int(p0)}).intVal == 3);
    TEST_ASSERT(CallNative(vm, "IsPlayerObserver", {JassValue::Int(p1)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsPlayerObserver", {JassValue::Int(p0)}).boolVal);

    // GetLocalPlayer honors the configured index.
    const int localHandle = CallNative(vm, "GetLocalPlayer", {}).intVal;
    TEST_ASSERT(data->GetSimHandle(localHandle, JassHandleInfo::PLAYER) == 1);

    // Handicap XP round-trips (used by AddHeroXP).
    CallNative(vm, "SetPlayerHandicapXP", {JassValue::Int(p0), JassValue::Real(2.0f)});
    TEST_ASSERT(CallNative(vm, "GetPlayerHandicapXP", {JassValue::Int(p0)}).realVal == 2.0f);

    // GetLocationZ delegates to the terrain height callback (ref GetLocationZ).
    const int loc = CallNative(vm, "Location", {JassValue::Real(3.0f), JassValue::Real(4.0f)}).intVal;
    TEST_ASSERT(loc != 0);
    TEST_ASSERT(CallNative(vm, "GetLocationZ", {JassValue::Int(loc)}).realVal == 7.0f);

    // Timer dialogs persist title/display state.
    const int timer = CallNative(vm, "CreateTimer", {}).intVal;
    const int dialog =
        CallNative(vm, "CreateTimerDialog", {JassValue::Int(timer)}).intVal;
    TEST_ASSERT(dialog != 0);
    CallNative(vm, "TimerDialogSetTitle", {JassValue::Int(dialog), JassValue::Str("Hello")});
    CallNative(vm, "TimerDialogDisplay", {JassValue::Int(dialog), JassValue::Bool(true)});
    const int dialogSimId = data->GetSimHandle(dialog, JassHandleInfo::TIMER_DIALOG);
    auto dit = data->timerDialogs.find(dialogSimId);
    TEST_ASSERT(dit != data->timerDialogs.end());
    TEST_ASSERT(dit->second->title == "Hello");
    TEST_ASSERT(dit->second->displayed);
    CallNative(vm, "DestroyTimerDialog", {JassValue::Int(dialog)});
    TEST_ASSERT(data->timerDialogs.find(dialogSimId) == data->timerDialogs.end());
    return true;
}

bool TestJassGameStateAndConfig() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // Float game state TIME_OF_DAY(2) round-trips through the sim clock
    // (ref GetFloatGameState/SetFloatGameState).
    sim.SetGameTimeOfDay(10.5f);
    TEST_ASSERT(CallNative(vm, "GetFloatGameState", {JassValue::Int(2)}).realVal == 10.5f);
    CallNative(vm, "SetFloatGameState", {JassValue::Int(2), JassValue::Real(6.0f)});
    TEST_ASSERT(sim.GetGameTimeOfDay() == 6.0f);
    // Integer states read 0 like the reference (DIVINE_INTERVENTION=0).
    TEST_ASSERT(CallNative(vm, "GetIntegerGameState", {JassValue::Int(0)}).intVal == 0);

    // Difficulty / speed / type / placement / teams / flags persist.
    CallNative(vm, "SetGameDifficulty", {JassValue::Int(2)});
    TEST_ASSERT(CallNative(vm, "GetGameDifficulty", {}).intVal == 2);
    CallNative(vm, "SetGameSpeed", {JassValue::Int(3)});
    TEST_ASSERT(CallNative(vm, "GetGameSpeed", {}).intVal == 3);
    CallNative(vm, "SetGameType", {JassValue::Int(4)});
    CallNative(vm, "SetGamePlacement", {JassValue::Int(1)});
    CallNative(vm, "SetTeams", {JassValue::Int(2)});
    TEST_ASSERT(data->gameConfig.gameType == 4);
    TEST_ASSERT(data->gameConfig.placement == 1);
    TEST_ASSERT(data->gameConfig.teams == 2);
    CallNative(vm, "SetMapFlags", {JassValue::Int(8)});
    TEST_ASSERT(CallNative(vm, "IsMapFlagSet", {JassValue::Int(8)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsMapFlagSet", {JassValue::Int(4)}).boolVal);
    CallNative(vm, "SetMissionAvailable", {JassValue::Int(0), JassValue::Bool(true)});
    TEST_ASSERT(data->gameConfig.missionAvailable[0]);

    // Melee / campaign AI flags + CommandAI counting per player.
    const int p0 = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    CallNative(vm, "StartMeleeAI", {JassValue::Int(p0)});
    CallNative(vm, "StartCampaignAI", {JassValue::Int(p0)});
    CallNative(vm, "CommandAI", {JassValue::Int(p0), JassValue::Int(1), JassValue::Int(2)});
    TEST_ASSERT(data->playerAI[0].meleeAI);
    TEST_ASSERT(data->playerAI[0].campaignAI);
    TEST_ASSERT(data->playerAI[0].commandCount == 1);

    // Fog enable/mask round-trips and backs IsFogEnabled/MaskEnabled.
    CallNative(vm, "FogEnable", {JassValue::Bool(false)});
    CallNative(vm, "FogMaskEnable", {JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(vm, "IsFogEnabled", {}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsFogMaskEnabled", {}).boolVal);
    CallNative(vm, "SetFogMaskStyle", {JassValue::Int(1)});
    CallNative(vm, "SetFogZHeight", {JassValue::Real(500.0f)});
    TEST_ASSERT(data->fog.maskStyle == 1);
    TEST_ASSERT(data->fog.zHeight == 500.0f);

    // Tech caps and team assignment land on the sim player.
    const War3ID tech(0x41524f68u); // 'ARoh' style code
    CallNative(vm, "SetPlayerTechMaxAllowed",
               {JassValue::Int(p0), JassValue::Int((int)tech.value), JassValue::Int(2)});
    TEST_ASSERT(CallNative(vm, "GetPlayerTechMaxAllowed",
                           {JassValue::Int(p0), JassValue::Int((int)tech.value)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "GetPlayerTechCount", {JassValue::Int(p0)}).intVal == 0);
    CallNative(vm, "SetPlayerTeam", {JassValue::Int(p0), JassValue::Int(1)});
    TEST_ASSERT(sim.GetPlayer(0)->GetTeam() == 1);
    TEST_ASSERT(data->playerMapConfig[0].team == 1);
    return true;
}

bool TestJassUnitMobilityAndWaygate() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(7003, MakeHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(unit != nullptr);
    sim.Update();
    auto data = natives.GetData();
    const int unitHandle = data->AllocHandle(JassHandleInfo::UNIT, 7003);

    // Turn speed / prop window / pathing persist on the unit (ref setters).
    const float baseTurn = CallNative(vm, "GetUnitTurnSpeed", {JassValue::Int(unitHandle)}).realVal;
    CallNative(vm, "SetUnitTurnSpeed", {JassValue::Int(unitHandle), JassValue::Real(baseTurn + 1.0f)});
    TEST_ASSERT(unit->GetTurnRate() == baseTurn + 1.0f);
    CallNative(vm, "SetUnitPropWindow", {JassValue::Int(unitHandle), JassValue::Real(45.0f)});
    TEST_ASSERT(CallNative(vm, "GetUnitPropWindow", {JassValue::Int(unitHandle)}).realVal == 45.0f);
    CallNative(vm, "SetUnitPathing", {JassValue::Int(unitHandle), JassValue::Bool(false)});
    TEST_ASSERT(!unit->IsPathingEnabled());

    // Rally point round-trips through a location handle.
    const int loc = CallNative(vm, "Location", {JassValue::Real(100.0f), JassValue::Real(200.0f)}).intVal;
    CallNative(vm, "SetUnitRallyPoint", {JassValue::Int(unitHandle), JassValue::Int(loc)});
    TEST_ASSERT(unit->GetRallyX() == 100.0f && unit->GetRallyY() == 200.0f);
    const int rallyLoc = CallNative(vm, "GetUnitRallyPoint", {JassValue::Int(unitHandle)}).intVal;
    TEST_ASSERT(rallyLoc != 0);
    const int rallySimId = data->GetSimHandle(rallyLoc, JassHandleInfo::LOCATION);
    TEST_ASSERT(data->locations[rallySimId]->x == 100.0f);

    // Waygate destination + activation (ref CAbilityWayGate state).
    CallNative(vm, "WaygateSetDestination",
               {JassValue::Int(unitHandle), JassValue::Real(1000.0f), JassValue::Real(2000.0f)});
    CallNative(vm, "WaygateActivate", {JassValue::Int(unitHandle), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "WaygateGetDestinationX", {JassValue::Int(unitHandle)}).realVal == 1000.0f);
    TEST_ASSERT(CallNative(vm, "WaygateGetDestinationY", {JassValue::Int(unitHandle)}).realVal == 2000.0f);
    TEST_ASSERT(unit->IsWaygateActive());
    return true;
}

bool TestJassDialogBoards() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // Dialog lifecycle: message + buttons persist, display + clear work.
    const int dialog = CallNative(vm, "DialogCreate", {}).intVal;
    TEST_ASSERT(dialog != 0);
    CallNative(vm, "DialogSetMessage", {JassValue::Int(dialog), JassValue::Str("Choose")});
    const int button = CallNative(vm, "DialogAddButton",
                                  {JassValue::Int(dialog), JassValue::Str("OK"), JassValue::Int(79)}).intVal;
    TEST_ASSERT(button != 0);
    const int quitButton =
        CallNative(vm, "DialogAddQuitButton", {JassValue::Int(dialog), JassValue::Bool(true),
                                               JassValue::Str("Quit"), JassValue::Int(81)}).intVal;
    TEST_ASSERT(quitButton != 0);
    const int dialogSimId = data->GetSimHandle(dialog, JassHandleInfo::DIALOG);
    TEST_ASSERT(data->dialogs[dialogSimId]->message == "Choose");
    TEST_ASSERT(data->dialogs[dialogSimId]->buttonIds.size() == 2);
    const int p0 = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    CallNative(vm, "DialogDisplay", {JassValue::Int(p0), JassValue::Int(dialog), JassValue::Bool(true)});
    TEST_ASSERT(data->dialogs[dialogSimId]->displayed);
    CallNative(vm, "DialogClear", {JassValue::Int(dialog)});
    TEST_ASSERT(data->dialogs[dialogSimId]->buttonIds.empty());
    CallNative(vm, "DialogDestroy", {JassValue::Int(dialog)});
    TEST_ASSERT(data->dialogs.find(dialogSimId) == data->dialogs.end());

    // Multiboard: title, sizing, cell items round-trip values.
    const int board = CallNative(vm, "CreateMultiboard", {}).intVal;
    TEST_ASSERT(board != 0);
    CallNative(vm, "MultiboardSetTitleText", {JassValue::Int(board), JassValue::Str("Stats")});
    CallNative(vm, "MultiboardSetRowCount", {JassValue::Int(board), JassValue::Int(3)});
    CallNative(vm, "MultiboardSetColumnCount", {JassValue::Int(board), JassValue::Int(2)});
    TEST_ASSERT(CallNative(vm, "MultiboardGetTitleText", {JassValue::Int(board)}).strVal == "Stats");
    TEST_ASSERT(CallNative(vm, "MultiboardGetRowCount", {JassValue::Int(board)}).intVal == 3);
    TEST_ASSERT(CallNative(vm, "MultiboardGetColumnCount", {JassValue::Int(board)}).intVal == 2);
    const int cell = CallNative(vm, "MultiboardGetItem",
                                {JassValue::Int(board), JassValue::Int(1), JassValue::Int(0)}).intVal;
    TEST_ASSERT(cell != 0);
    CallNative(vm, "MultiboardSetItemValue", {JassValue::Int(cell), JassValue::Str("Hero")});
    TEST_ASSERT(CallNative(vm, "MultiboardGetItemValue", {JassValue::Int(cell)}).strVal == "Hero");
    // Same cell returns the same item.
    const int cellAgain = CallNative(vm, "MultiboardGetItem",
                                     {JassValue::Int(board), JassValue::Int(1), JassValue::Int(0)}).intVal;
    TEST_ASSERT(data->GetSimHandle(cell, JassHandleInfo::MULTIBOARDITEM) ==
                data->GetSimHandle(cellAgain, JassHandleInfo::MULTIBOARDITEM));
    CallNative(vm, "MultiboardDisplay", {JassValue::Int(board), JassValue::Bool(true)});
    CallNative(vm, "MultiboardMinimize", {JassValue::Int(board), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "IsMultiboardDisplayed", {JassValue::Int(board)}).boolVal);
    TEST_ASSERT(CallNative(vm, "IsMultiboardMinimized", {JassValue::Int(board)}).boolVal);
    CallNative(vm, "MultiboardReleaseItem", {JassValue::Int(cell)});
    CallNative(vm, "DestroyMultiboard", {JassValue::Int(board)});
    const int boardSimId = data->GetSimHandle(board, JassHandleInfo::MULTIBOARD);
    TEST_ASSERT(boardSimId < 0 || data->multiboards.find(boardSimId) == data->multiboards.end());

    // Leaderboard: items track players, values update, removal works.
    const int lb = CallNative(vm, "CreateLeaderboard", {}).intVal;
    TEST_ASSERT(lb != 0);
    CallNative(vm, "LeaderboardSetLabel", {JassValue::Int(lb), JassValue::Str("Kills")});
    CallNative(vm, "LeaderboardAddItem",
               {JassValue::Int(lb), JassValue::Str("Alice"), JassValue::Int(5), JassValue::Int(p0)});
    TEST_ASSERT(CallNative(vm, "LeaderboardGetItemCount", {JassValue::Int(lb)}).intVal == 1);
    TEST_ASSERT(CallNative(vm, "LeaderboardHasPlayerItem", {JassValue::Int(lb), JassValue::Int(p0)}).boolVal);
    TEST_ASSERT(CallNative(vm, "LeaderboardGetPlayerIndex", {JassValue::Int(lb), JassValue::Int(p0)}).intVal == 0);
    CallNative(vm, "LeaderboardSetItemValue",
               {JassValue::Int(lb), JassValue::Int(0), JassValue::Int(7)});
    const int lbSimId = data->GetSimHandle(lb, JassHandleInfo::LEADERBOARD);
    TEST_ASSERT(data->leaderboards[lbSimId]->items[0].value == 7);
    CallNative(vm, "LeaderboardDisplay", {JassValue::Int(lb), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "IsLeaderboardDisplayed", {JassValue::Int(lb)}).boolVal);
    CallNative(vm, "LeaderboardRemovePlayerItem", {JassValue::Int(lb), JassValue::Int(p0)});
    TEST_ASSERT(CallNative(vm, "LeaderboardGetItemCount", {JassValue::Int(lb)}).intVal == 0);
    CallNative(vm, "DestroyLeaderboard", {JassValue::Int(lb)});
    TEST_ASSERT(data->leaderboards.find(lbSimId) == data->leaderboards.end());
    return true;
}

bool TestJassCameraSetup() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    data->SetLocalPlayerIndex(0);

    // Field + destination state round-trips.
    const int setup = CallNative(vm, "CreateCameraSetup", {}).intVal;
    TEST_ASSERT(setup != 0);
    CallNative(vm, "CameraSetupSetField",
               {JassValue::Int(setup), JassValue::Int(1), JassValue::Real(1500.0f), JassValue::Real(0.0f)});
    TEST_ASSERT(CallNative(vm, "CameraSetupGetField",
                           {JassValue::Int(setup), JassValue::Int(1)}).realVal == 1500.0f);
    CallNative(vm, "CameraSetupSetDestPosition",
               {JassValue::Int(setup), JassValue::Real(100.0f), JassValue::Real(200.0f), JassValue::Real(0.0f)});
    TEST_ASSERT(CallNative(vm, "CameraSetupGetDestPositionX", {JassValue::Int(setup)}).realVal == 100.0f);
    TEST_ASSERT(CallNative(vm, "CameraSetupGetDestPositionY", {JassValue::Int(setup)}).realVal == 200.0f);
    const int destLoc = CallNative(vm, "CameraSetupGetDestPositionLoc", {JassValue::Int(setup)}).intVal;
    TEST_ASSERT(destLoc != 0);
    const int destSimId = data->GetSimHandle(destLoc, JassHandleInfo::LOCATION);
    TEST_ASSERT(data->locations[destSimId]->x == 100.0f);

    // Apply routes through the engine camera callback.
    float camX = -1.0f, camY = -1.0f, camZ = -1.0f;
    data->camSetTarget = [&](float x, float y, float z) { camX = x; camY = y; camZ = z; };
    CallNative(vm, "CameraSetupApply",
               {JassValue::Int(setup), JassValue::Bool(true), JassValue::Bool(true)});
    TEST_ASSERT(camX == 100.0f && camZ == 200.0f);
    CallNative(vm, "CameraSetupApplyWithZ", {JassValue::Int(setup), JassValue::Real(50.0f)});
    TEST_ASSERT(camY == 50.0f);

    // ForPlayer applies only for the local player (common.j arg order:
    // doPan, setup, player, duration[, zOffset]).
    camX = -1.0f;
    const int p0 = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int p1 = data->AllocHandle(JassHandleInfo::PLAYER, 1);
    CallNative(vm, "CameraSetupApplyForPlayer",
               {JassValue::Bool(true), JassValue::Int(setup), JassValue::Int(p1), JassValue::Real(0.0f)});
    TEST_ASSERT(camX == -1.0f);
    CallNative(vm, "CameraSetupApplyForPlayer",
               {JassValue::Bool(true), JassValue::Int(setup), JassValue::Int(p0), JassValue::Real(0.0f)});
    TEST_ASSERT(camX == 100.0f);

    // Bounds collapse 8 coords to min/max like the reference.
    CallNative(vm, "SetCameraBounds", {JassValue::Real(0.0f), JassValue::Real(0.0f), JassValue::Real(100.0f),
                                       JassValue::Real(0.0f), JassValue::Real(100.0f), JassValue::Real(200.0f),
                                       JassValue::Real(0.0f), JassValue::Real(200.0f)});
    TEST_ASSERT(CallNative(vm, "GetCameraBoundMinX", {}).realVal == 0.0f);
    TEST_ASSERT(CallNative(vm, "GetCameraBoundMaxX", {}).realVal == 100.0f);
    TEST_ASSERT(CallNative(vm, "GetCameraBoundMaxY", {}).realVal == 200.0f);

    // Smoothing / cinematic / stop state.
    CallNative(vm, "CameraSetSmoothingFactor", {JassValue::Real(2.5f)});
    TEST_ASSERT(data->cameraSmoothingFactor == 2.5f);
    CallNative(vm, "SetCinematicCamera", {JassValue::Str("cam.mdl")});
    TEST_ASSERT(data->cinematicCameraModel == "cam.mdl");
    CallNative(vm, "StopCamera", {});
    TEST_ASSERT(data->cameraStopped);
    return true;
}

bool TestJassSoundMusic() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // Sound params persist on the handle (ref CSound fields).
    const int sound =
        CallNative(vm, "CreateSound", {JassValue::Str("Abilities\\Spells\\Human\\ThunderClap\\ThunderClap.wav"),
                                       JassValue::Bool(false), JassValue::Bool(false), JassValue::Bool(false),
                                       JassValue::Int(10), JassValue::Int(10), JassValue::Str("")}).intVal;
    TEST_ASSERT(sound != 0);
    const int sndSimId = data->GetSimHandle(sound, JassHandleInfo::SOUND);
    CallNative(vm, "SetSoundChannel", {JassValue::Int(sound), JassValue::Int(3)});
    CallNative(vm, "SetSoundDistances", {JassValue::Int(sound), JassValue::Real(100.0f), JassValue::Real(2000.0f)});
    CallNative(vm, "SetSoundDistanceCutoff", {JassValue::Int(sound), JassValue::Real(3000.0f)});
    CallNative(vm, "SetSoundDuration", {JassValue::Int(sound), JassValue::Int(1500)});
    CallNative(vm, "KillSoundWhenDone", {JassValue::Int(sound)});
    CallNative(vm, "RegisterStackedSound",
               {JassValue::Int(sound), JassValue::Bool(true), JassValue::Real(256.0f), JassValue::Real(256.0f)});
    TEST_ASSERT(data->sounds[sndSimId]->channel == 3);
    TEST_ASSERT(data->sounds[sndSimId]->minDistance == 100.0f);
    TEST_ASSERT(data->sounds[sndSimId]->maxDistance == 2000.0f);
    TEST_ASSERT(data->sounds[sndSimId]->distanceCutoff == 3000.0f);
    TEST_ASSERT(CallNative(vm, "GetSoundDuration", {JassValue::Int(sound)}).intVal == 1500);
    TEST_ASSERT(data->sounds[sndSimId]->killWhenDone);
    TEST_ASSERT(data->sounds[sndSimId]->stackedByPosition);

    // Playback state tracks start/stop (StopSound args: sound, killWhenDone, fadeOut).
    CallNative(vm, "StartSound", {JassValue::Int(sound)});
    TEST_ASSERT(data->sounds[sndSimId]->playing);
    CallNative(vm, "StopSound", {JassValue::Int(sound), JassValue::Bool(false), JassValue::Bool(true)});
    TEST_ASSERT(!data->sounds[sndSimId]->playing);

    // Music state machine mirrors the reference meleeUI flow.
    CallNative(vm, "SetMapMusic",
               {JassValue::Str("Music\\map.mp3"), JassValue::Bool(false), JassValue::Int(0)});
    TEST_ASSERT(data->music.mapMusic == "Music\\map.mp3");
    CallNative(vm, "PlayMusic", {JassValue::Str("Music\\battle.mp3")});
    TEST_ASSERT(data->music.playing && data->music.current == "Music\\battle.mp3");
    CallNative(vm, "SetMusicVolume", {JassValue::Int(80)});
    CallNative(vm, "SetMusicPlayPosition", {JassValue::Int(5000)});
    TEST_ASSERT(data->music.volume == 80);
    TEST_ASSERT(data->music.playPositionMs == 5000);
    CallNative(vm, "StopMusic", {JassValue::Bool(true)});
    TEST_ASSERT(!data->music.playing);
    CallNative(vm, "ResumeMusic", {});
    TEST_ASSERT(data->music.playing);
    CallNative(vm, "PlayThematicMusicEx", {JassValue::Str("Music\\theme.mp3"), JassValue::Int(1000)});
    TEST_ASSERT(data->music.current == "Music\\theme.mp3");
    TEST_ASSERT(data->music.playPositionMs == 1000);
    // EndThematicMusic falls back to the map music like the reference.
    CallNative(vm, "EndThematicMusic", {});
    TEST_ASSERT(data->music.current == "Music\\map.mp3");
    TEST_ASSERT(data->music.playing);
    CallNative(vm, "ClearMapMusic", {});
    TEST_ASSERT(data->music.mapMusic.empty());
    CallNative(vm, "NewSoundEnvironment", {JassValue::Str("Default")});
    TEST_ASSERT(data->music.soundEnvironment == "Default");
    CallNative(vm, "VolumeGroupReset", {});
    TEST_ASSERT(data->music.volume == 127);
    return true;
}

bool TestJassCinematicVisuals() {
    JassVM vm;
    CSimulation sim;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // Cine-filter state round-trips (common.j arg orders).
    CallNative(vm, "SetCineFilterTexture", {JassValue::Str("ReplaceableTextures\\CameraMasks\\Black_mask.blp")});
    CallNative(vm, "SetCineFilterBlendMode", {JassValue::Int(1)});
    CallNative(vm, "SetCineFilterTexMapFlags", {JassValue::Int(2)});
    CallNative(vm, "SetCineFilterStartUV", {JassValue::Real(0.0f), JassValue::Real(0.0f),
                                            JassValue::Real(0.5f), JassValue::Real(0.5f)});
    CallNative(vm, "SetCineFilterEndUV", {JassValue::Real(0.0f), JassValue::Real(0.0f),
                                          JassValue::Real(1.0f), JassValue::Real(1.0f)});
    CallNative(vm, "SetCineFilterStartColor", {JassValue::Int(255), JassValue::Int(0),
                                               JassValue::Int(0), JassValue::Int(128)});
    CallNative(vm, "SetCineFilterEndColor", {JassValue::Int(0), JassValue::Int(0),
                                             JassValue::Int(255), JassValue::Int(255)});
    CallNative(vm, "SetCineFilterDuration", {JassValue::Real(2.0f)});
    CallNative(vm, "DisplayCineFilter", {JassValue::Bool(true)});
    TEST_ASSERT(data->cineFilter.texture == "ReplaceableTextures\\CameraMasks\\Black_mask.blp");
    TEST_ASSERT(data->cineFilter.blendMode == 1);
    TEST_ASSERT(data->cineFilter.texMapFlags == 2);
    TEST_ASSERT(data->cineFilter.startUV[2] == 0.5f);
    TEST_ASSERT(data->cineFilter.endUV[2] == 1.0f);
    TEST_ASSERT(data->cineFilter.startColor[0] == 255 && data->cineFilter.startColor[3] == 128);
    TEST_ASSERT(data->cineFilter.endColor[2] == 255);
    TEST_ASSERT(data->cineFilter.duration == 2.0f);
    TEST_ASSERT(data->cineFilter.displayed);

    // Cinematic mode + day/night models.
    CallNative(vm, "SetCinematicMode", {JassValue::Bool(true)});
    TEST_ASSERT(data->cinematicMode);
    CallNative(vm, "SetDayNightModels",
               {JassValue::Str("terrain.mdl"), JassValue::Str("unit.mdl")});
    TEST_ASSERT(data->dayNightTerrainModel == "terrain.mdl");
    TEST_ASSERT(data->dayNightUnitModel == "unit.mdl");

    // Sky state, then SetSky resets to defaults.
    CallNative(vm, "SetSkyModel", {JassValue::Str("sky.mdl")});
    CallNative(vm, "SetSkyTexture", {JassValue::Str("sky.blp")});
    CallNative(vm, "SetSkyColor", {JassValue::Int(128), JassValue::Int(128), JassValue::Int(255)});
    CallNative(vm, "SetSkyFog", {JassValue::Real(100.0f), JassValue::Real(2000.0f)});
    TEST_ASSERT(data->sky.model == "sky.mdl");
    TEST_ASSERT(data->sky.texture == "sky.blp");
    TEST_ASSERT(data->sky.color[2] == 255);
    TEST_ASSERT(data->sky.fogFar == 2000.0f);
    CallNative(vm, "SetSky", {});
    TEST_ASSERT(data->sky.model.empty());

    // Water state.
    CallNative(vm, "SetWaterBaseColor", {JassValue::Int(0), JassValue::Int(64),
                                         JassValue::Int(128), JassValue::Int(255)});
    CallNative(vm, "SetWaterColor", {JassValue::Int(10), JassValue::Int(20),
                                     JassValue::Int(30), JassValue::Int(40)});
    CallNative(vm, "SetWaterTransparency", {JassValue::Real(0.5f)});
    CallNative(vm, "SetWaterZ", {JassValue::Real(-10.0f)});
    CallNative(vm, "SetWaterDeformStyle", {JassValue::Int(1)});
    TEST_ASSERT(data->water.baseColor[2] == 128);
    TEST_ASSERT(data->water.color[0] == 10);
    TEST_ASSERT(data->water.transparency == 0.5f);
    TEST_ASSERT(data->water.z == -10.0f);
    TEST_ASSERT(data->water.deformStyle == 1);
    return true;
}

bool TestJassSuspendDecayMisc() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // Time-of-day suspend freezes the clock; scale multiplies advancement.
    sim.SetGameTimeOfDay(10.0f);
    CallNative(vm, "SuspendTimeOfDay", {JassValue::Bool(true)});
    sim.AdvanceGameTime(5.0f);
    TEST_ASSERT(sim.GetGameTimeOfDay() == 10.0f);
    CallNative(vm, "SuspendTimeOfDay", {JassValue::Bool(false)});
    CallNative(vm, "SetTimeOfDayScale", {JassValue::Real(2.0f)});
    TEST_ASSERT(CallNative(vm, "GetTimeOfDayScale", {}).realVal == 2.0f);
    sim.AdvanceGameTime(5.0f);
    TEST_ASSERT(sim.GetGameTimeOfDay() == 20.0f);

    // Hero XP suspend blocks AddXP gains map-wide.
    auto* hero = sim.AdoptUnit(std::make_unique<CUnit>(7004, MakeHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(hero != nullptr);
    hero->AddAbility(sim, std::make_unique<CAbilityHero>(9002));
    sim.Update();
    const int heroHandle = data->AllocHandle(JassHandleInfo::UNIT, 7004);
    CallNative(vm, "SuspendHeroXP", {JassValue::Bool(true)});
    CallNative(vm, "AddHeroXP", {JassValue::Int(heroHandle), JassValue::Int(500), JassValue::Bool(false)});
    TEST_ASSERT(CallNative(vm, "GetHeroXP", {JassValue::Int(heroHandle)}).intVal == 0);
    CallNative(vm, "SuspendHeroXP", {JassValue::Bool(false)});
    CallNative(vm, "AddHeroXP", {JassValue::Int(heroHandle), JassValue::Int(500), JassValue::Bool(false)});
    TEST_ASSERT(CallNative(vm, "GetHeroXP", {JassValue::Int(heroHandle)}).intVal == 500);

    // Decay suspend freezes corpse progression on a dead unit.
    auto* grunt = sim.AdoptUnit(std::make_unique<CUnit>(7005, MakeHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(grunt != nullptr);
    sim.Update();
    grunt->SetLife(sim, 0.0f);
    grunt->Update(sim);
    TEST_ASSERT(grunt->IsDead());
    const int gruntHandle = data->AllocHandle(JassHandleInfo::UNIT, 7005);
    CallNative(vm, "UnitSuspendDecay", {JassValue::Int(gruntHandle), JassValue::Bool(true)});
    TEST_ASSERT(grunt->IsDecaySuspended());
    for (int i = 0; i < 600; ++i) sim.Update();
    TEST_ASSERT(!grunt->IsCorpse());
    CallNative(vm, "UnitSuspendDecay", {JassValue::Int(gruntHandle), JassValue::Bool(false)});
    for (int i = 0; i < 600; ++i) sim.Update();
    TEST_ASSERT(grunt->IsCorpse());

    // Integer game states round-trip except ids 0/1 (always 0, like ref).
    CallNative(vm, "SetIntegerGameState", {JassValue::Int(7), JassValue::Int(42)});
    TEST_ASSERT(CallNative(vm, "GetIntegerGameState", {JassValue::Int(7)}).intVal == 42);
    CallNative(vm, "SetIntegerGameState", {JassValue::Int(0), JassValue::Int(99)});
    TEST_ASSERT(CallNative(vm, "GetIntegerGameState", {JassValue::Int(0)}).intVal == 0);

    // Timer dialog remaining-time write reaches the underlying timer.
    const int timer = CallNative(vm, "CreateTimer", {}).intVal;
    CallNative(vm, "TimerStart", {JassValue::Int(timer), JassValue::Real(60.0f), JassValue::Bool(false),
                                  JassValue::Null()});
    const int dialog = CallNative(vm, "CreateTimerDialog", {JassValue::Int(timer)}).intVal;
    CallNative(vm, "TimerDialogSetRealTimeRemaining", {JassValue::Int(dialog), JassValue::Real(30.0f)});
    const int timerSimId = data->GetSimHandle(timer, JassHandleInfo::TIMER);
    TEST_ASSERT(data->timers[timerSimId]->remaining == 30.0f);

    // Destructable animation tags + life restore.
    auto* dest = sim.CreateDestructable(War3ID("ATtr"), 0.0f, 0.0f, 500.0f);
    TEST_ASSERT(dest != nullptr);
    const int destHandle = data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId());
    CallNative(vm, "SetDestructableAnimation", {JassValue::Int(destHandle), JassValue::Str("birth")});
    TEST_ASSERT(dest->GetAnimation() == "birth");
    CallNative(vm, "QueueDestructableAnimation", {JassValue::Int(destHandle), JassValue::Str("death")});
    TEST_ASSERT(dest->GetQueuedAnimation() == "death");
    dest->SetLife(sim, 100.0f);
    CallNative(vm, "DestructableRestoreLife",
               {JassValue::Int(destHandle), JassValue::Real(400.0f), JassValue::Bool(false)});
    TEST_ASSERT(dest->GetLife() == 400.0f);

    // Trigger eval/exec counters increment on fired triggers.
    TEST_ASSERT(vm.LoadScript("function CountedAction takes nothing returns nothing\nendfunction\n"));
    const int trig2 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterUnitEvent",
               {JassValue::Int(trig2), JassValue::Int(heroHandle), JassValue::Int(71)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig2), JassValue::Str("CountedAction")});
    TEST_ASSERT(CallNative(vm, "GetTriggerEvalCount", {JassValue::Int(trig2)}).intVal == 0);
    natives.FireTrainedEvent(7004, 7005);
    natives.PumpQueuedThreads(1024);
    // Note: CreateTrigger returns the raw trigger id (not a handle).
    TEST_ASSERT(data->triggers[trig2]->evalCount >= 1);
    TEST_ASSERT(data->triggers[trig2]->execCount >= 1);
    TEST_ASSERT(CallNative(vm, "GetTriggerEvalCount", {JassValue::Int(trig2)}).intVal >= 1);
    TEST_ASSERT(CallNative(vm, "GetTriggerExecCount", {JassValue::Int(trig2)}).intVal >= 1);
    return true;
}

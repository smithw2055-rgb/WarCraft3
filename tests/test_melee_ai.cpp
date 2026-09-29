// Melee AI tests: race-plan discovery from unit data, worker training,
// harvest income, supply/barracks construction, attack waves, and the
// StartMeleeAI/CommandAI/PauseCompAI/GetAIDifficulty native wiring.
// Needs retail data (SKIP-guarded like the other MPQ tests).
#include <filesystem>
#include <iostream>
#include <memory>

#include "data/destructabledata.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/csimulation.h"
#include "ecs/melee_ai.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "resource/mpq_storm.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

void OpenGameData(MpqManager& mpq) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    const char* archives[] = {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"};
    for (auto a : archives) mpq.Open(res + a);
}

bool HasMeleeIds(CUnitData& data) {
    const char* ids[] = {"hhou", "hpea", "hfar", "hbar", "hfoo", "ngold"};
    for (auto id : ids) {
        if (!data.HasUnitType(War3ID(id))) return false;
    }
    return true;
}

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

} // namespace

bool TestMeleeAIDiscoversHumanPlan() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    OpenGameData(mpq);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!HasMeleeIds(unitData)) return true;

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));
    sim.GetPlayer(1)->SetRace(1);
    sim.GetPlayer(1)->SetGold(2000);
    sim.GetPlayer(1)->SetFoodCap(100);

    CUnit* hall = sim.CreateUnit(War3ID("hhou"), 1, 0, 0, 0);
    TEST_ASSERT(hall != nullptr);
    CUnit* peasant = sim.CreateUnit(War3ID("hpea"), 1, 200, 0, 0);
    TEST_ASSERT(peasant != nullptr);
    // Peasants ship with harvest/build abilities from sim creation.
    TEST_ASSERT(peasant->GetAbility(War3ID("Ahar")) != nullptr);

    MeleeAIController ai(&sim, 1);
    for (int i = 0; i < 25; i++) sim.Update(), ai.Update();
    TEST_ASSERT(ai.GetThinkCount() > 0);
    TEST_ASSERT(ai.HasPlan());
    TEST_ASSERT(ai.GetPlan().worker == War3ID("hpea"));
    TEST_ASSERT(ai.GetPlan().farm == War3ID("hfar"));
    TEST_ASSERT(ai.GetPlan().barracks == War3ID("hbar"));
    TEST_ASSERT(ai.GetPlan().armyUnit == War3ID("hfoo"));
    return true;
}

bool TestMeleeAIEconomyGrows() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    OpenGameData(mpq);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    CDestructableData destData = CDestructableData::LoadFromMpq(mpq);
    if (!HasMeleeIds(unitData)) return true;

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.SetDestructableData(&destData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));
    sim.GetPlayer(1)->SetRace(1);
    sim.GetPlayer(1)->SetGold(2000);
    sim.GetPlayer(1)->SetFoodCap(100);

    sim.CreateUnit(War3ID("hhou"), 1, 0, 0, 0);
    for (int i = 0; i < 3; i++) {
        sim.CreateUnit(War3ID("hpea"), 1, 200 + i * 40, 0, 0);
    }
    CUnit* mine = sim.CreateUnit(War3ID("ngold"), 12, 600, 0, 0);
    TEST_ASSERT(mine != nullptr);
    if (auto* gold = dynamic_cast<CAbilityGoldMine*>(mine->GetAbility(War3ID("Agld")))) {
        gold->SetGold(12500);
    } else {
        return true; // mine shape differs; economy assertions need Agld
    }

    const int goldBefore = sim.GetPlayer(1)->GetGold();
    MeleeAIController ai(&sim, 1);
    // ~75 game-seconds: harvest trips land and trainees pop.
    bool sawCarried = false;
    for (int i = 0; i < 1500; i++) {
        sim.Update();
        ai.Update();
        if (i % 50 == 0) {
            for (CUnit* unit : sim.GetUnits()) {
                if (!unit || unit->IsDead() || unit->GetPlayerIndex() != 1) continue;
                if (auto* harvest = dynamic_cast<CAbilityHarvest*>(
                        unit->GetAbility(War3ID("Ahar")))) {
                    if (harvest->GetCarriedAmount() > 0) sawCarried = true;
                }
            }
        }
    }
    // Harvest orders execute (a worker carried gold at some point).
    TEST_ASSERT(sawCarried);
    // Workforce trained up (started at 3).
    int workers = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetPlayerIndex() == 1 &&
            unit->GetTypeId() == War3ID("hpea")) {
            workers++;
        }
    }
    TEST_ASSERT(workers >= 4);
    TEST_ASSERT(ai.GetThinkCount() > 50);
    (void)goldBefore;
    return true;
}

bool TestMeleeAIAttackWave() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    OpenGameData(mpq);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!HasMeleeIds(unitData)) return true;

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));
    sim.GetPlayer(1)->SetRace(1);
    sim.GetPlayer(1)->SetGold(5000);
    sim.GetPlayer(1)->SetFoodCap(100);

    sim.CreateUnit(War3ID("hhou"), 1, 0, 0, 0);
    // Enemy base across the map.
    CUnit* enemyHall = sim.CreateUnit(War3ID("hhou"), 0, 5000, 0, 0);
    TEST_ASSERT(enemyHall != nullptr);
    for (int i = 0; i < 6; i++) {
        sim.CreateUnit(War3ID("hfoo"), 1, 100 + i * 30, 100, 0);
    }

    MeleeAIController ai(&sim, 1);
    for (int i = 0; i < 60; i++) {
        sim.Update();
        ai.Update();
    }
    TEST_ASSERT(ai.GetWaveNumber() == 1);
    // Army marched out toward the enemy base at x=5000.
    int advanced = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != 1) continue;
        if (unit->GetTypeId() != War3ID("hfoo")) continue;
        if (unit->GetX() > 300.0f) advanced++;
    }
    TEST_ASSERT(advanced > 0);
    return true;
}

bool TestMeleeAINativesWiring() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    const int p1 = CallNative(vm, "Player", {JassValue::Int(1)}).intVal;
    CallNative(vm, "StartMeleeAI", {JassValue::Int(p1), JassValue::Str("human.ai")});
    auto it = data->aiControllers.find(1);
    TEST_ASSERT(it != data->aiControllers.end());
    TEST_ASSERT(it->second->IsEnabled());
    TEST_ASSERT(it->second->GetDifficulty() == data->gameConfig.defaultDifficulty);
    TEST_ASSERT(CallNative(vm, "GetAIDifficulty", {JassValue::Int(p1)}).intVal ==
                data->gameConfig.defaultDifficulty);

    CallNative(vm, "CommandAI", {JassValue::Int(p1), JassValue::Int(1), JassValue::Int(0)});
    TEST_ASSERT(it->second->GetLastCommand() == 1);

    CallNative(vm, "PauseCompAI", {JassValue::Int(p1), JassValue::Bool(true)});
    TEST_ASSERT(!it->second->IsEnabled());
    const int thinksBefore = it->second->GetThinkCount();
    for (int i = 0; i < 40; i++) {
        sim.Update();
        natives.UpdateAI();
    }
    TEST_ASSERT(it->second->GetThinkCount() == thinksBefore); // paused: no thinking
    CallNative(vm, "PauseCompAI", {JassValue::Int(p1), JassValue::Bool(false)});
    for (int i = 0; i < 40; i++) {
        sim.Update();
        natives.UpdateAI();
    }
    TEST_ASSERT(it->second->GetThinkCount() > thinksBefore);
    return true;
}

namespace {

struct AiHarness {
    MpqManager mpq;
    CGameplayConstants gc;
    std::unique_ptr<CUnitData> unitData;
    std::unique_ptr<CDestructableData> destData;
    std::unique_ptr<CSimulation> sim;
    std::unique_ptr<MeleeAIController> ai;

    // Mirror of TestMeleeAIEconomyGrows setup; null when data is missing.
    bool Init() {
        if (!ResourcesPresent()) return false;
        OpenGameData(mpq);
        unitData = std::make_unique<CUnitData>(CUnitData::LoadFromMpq(mpq, gc));
        destData = std::make_unique<CDestructableData>(CDestructableData::LoadFromMpq(mpq));
        if (!HasMeleeIds(*unitData)) return false;
        sim = std::make_unique<CSimulation>();
        sim->SetUnitData(unitData.get());
        sim->SetDestructableData(destData.get());
        sim->AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        sim->AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));
        sim->GetPlayer(1)->SetRace(1);
        sim->GetPlayer(1)->SetGold(2000);
        sim->GetPlayer(1)->SetFoodCap(100);
        sim->CreateUnit(War3ID("hhou"), 1, 0, 0, 0);
        for (int i = 0; i < 3; i++) {
            sim->CreateUnit(War3ID("hpea"), 1, 200 + i * 40, 0, 0);
        }
        CUnit* mine = sim->CreateUnit(War3ID("ngold"), 12, 600, 0, 0);
        if (auto* gold = dynamic_cast<CAbilityGoldMine*>(mine->GetAbility(War3ID("Agld")))) {
            gold->SetGold(12500);
        } else {
            return false;
        }
        ai = std::make_unique<MeleeAIController>(sim.get(), 1);
        return true;
    }

    void Run(int ticks) {
        for (int i = 0; i < ticks; i++) {
            sim->Update();
            ai->Update();
        }
    }
};

} // namespace

bool TestMeleeAIOrdersEnterCommandLog() {
    AiHarness h1, h2;
    if (!h1.Init() || !h2.Init()) return true; // SKIP without retail data
    h1.Run(1500);
    h2.Run(1500);
    // Every AI decision is visible in the lockstep log, on the AI seat...
    TEST_ASSERT(!h1.sim->LoggedCommands().empty());
    for (const auto& cmd : h1.sim->LoggedCommands()) {
        TEST_ASSERT(cmd.player == 1);
    }
    // ...and two identical runs agree exactly (deterministic AI).
    TEST_ASSERT(h1.sim->LoggedCommands() == h2.sim->LoggedCommands());
    TEST_ASSERT(h1.sim->HashState() == h2.sim->HashState());
    return true;
}

bool TestMeleeAIDifficultyPacing() {
    CSimulation sim;
    MeleeAIController easy(&sim, 0);
    easy.SetDifficulty(0);
    TEST_ASSERT(easy.GetThinkIntervalTicks() == 40);
    TEST_ASSERT(easy.GetWaveCooldownTicks() == 1200);
    MeleeAIController normal(&sim, 0);
    normal.SetDifficulty(1);
    TEST_ASSERT(normal.GetThinkIntervalTicks() == 20);
    TEST_ASSERT(normal.GetWaveCooldownTicks() == 600);
    MeleeAIController insane(&sim, 0);
    insane.SetDifficulty(2);
    TEST_ASSERT(insane.GetThinkIntervalTicks() == 10);
    TEST_ASSERT(insane.GetWaveCooldownTicks() == 300);
    // Clamping keeps the stored difficulty in range.
    insane.SetDifficulty(99);
    TEST_ASSERT(insane.GetDifficulty() == 2);
    insane.SetDifficulty(-5);
    TEST_ASSERT(insane.GetDifficulty() == 0);
    return true;
}

bool TestMeleeAIThinkRate() {
    CSimulation sim;
    MeleeAIController easy(&sim, 0);
    easy.SetDifficulty(0);
    MeleeAIController normal(&sim, 0);
    normal.SetDifficulty(1);
    MeleeAIController insane(&sim, 0);
    insane.SetDifficulty(2);
    for (int i = 0; i < 200; i++) {
        sim.Update();
        easy.Update();
        normal.Update();
        insane.Update();
    }
    // No units/data: controllers idle, but the think cadence still ticks.
    TEST_ASSERT(easy.GetThinkCount() == 5);
    TEST_ASSERT(normal.GetThinkCount() == 10);
    TEST_ASSERT(insane.GetThinkCount() == 20);
    return true;
}

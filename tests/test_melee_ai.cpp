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
#include "data/upgradedata.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_queue.h"
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

// Synthetic lab type (no MPQ): bare building shell for queue research.
CUnitType* MakeLabType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Lab", "Lab", "icon", War3ID("hbla"),
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
    TEST_ASSERT(ai.GetPlan().altar == War3ID("haltar"));
    TEST_ASSERT(ai.GetPlan().hero == War3ID("Hamg"));
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

bool TestMeleeAIResearchesUpgrades() {
    // Synthetic lab (no MPQ): rich player + upgrade data + idle queue.
    CSimulation sim;
    CUnitData unitData;
    sim.SetUnitData(&unitData);
    CUpgradeData upgradeData;
    auto up = std::make_unique<CUpgradeType>();
    up->id = War3ID("R001");
    up->goldCost = 100;
    up->lumberCost = 50;
    up->researchTime = 5;
    up->maxLevel = 2;
    upgradeData.RegisterUpgrade(War3ID("R001"), std::move(up));
    sim.SetUpgradeData(&upgradeData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));
    sim.GetPlayer(1)->SetGold(2000);
    sim.GetPlayer(1)->AddLumber(500);

    auto* lab = sim.AdoptUnit(std::make_unique<CUnit>(7101, MakeLabType(), 1, War3ID("hbla"), 0, 0, 0));
    TEST_ASSERT(lab != nullptr);
    lab->AddAbility(sim, std::make_unique<CAbilityQueue>(7102));
    // Queue ability rawcode varies; find it by scan.
    CAbilityQueue* q = nullptr;
    for (CAbility* ab : lab->GetAbilities()) {
        q = dynamic_cast<CAbilityQueue*>(ab);
        if (q) break;
    }
    TEST_ASSERT(q != nullptr);
    q->AddResearch(War3ID("R001"));

    MeleeAIController ai(&sim, 1);
    for (int i = 0; i < 400; i++) {
        sim.Update();
        ai.Update();
    }
    // Both levels researched to max (100g/50l each); no third starts.
    TEST_ASSERT(sim.GetPlayer(1)->GetTechLevel(War3ID("R001")) == 2);
    TEST_ASSERT(sim.GetPlayer(1)->GetGold() == 1800);
    return true;
}

bool TestMeleeAITrainsHero() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    OpenGameData(mpq);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!HasMeleeIds(unitData)) return true;
    if (!unitData.HasUnitType(War3ID("haltar")) || !unitData.HasUnitType(War3ID("Hamg"))) return true;

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));
    sim.GetPlayer(1)->SetRace(1);
    sim.GetPlayer(1)->SetGold(4000);
    sim.GetPlayer(1)->SetFoodCap(100);

    sim.CreateUnit(War3ID("hhou"), 1, 0, 0, 0);
    CUnit* altar = sim.CreateUnit(War3ID("haltar"), 1, 400, 0, 0);
    TEST_ASSERT(altar != nullptr);
    for (int i = 0; i < 6; i++) {
        sim.CreateUnit(War3ID("hpea"), 1, 200 + i * 40, 0, 0);
    }
    for (int i = 0; i < 3; i++) {
        sim.CreateUnit(War3ID("hfoo"), 1, 100 + i * 30, 100, 0);
    }

    MeleeAIController ai(&sim, 1);
    for (int i = 0; i < 1200; i++) {
        sim.Update();
        ai.Update();
    }
    // A hero mustered at the altar (economy/army guards passed).
    int heroes = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != 1) continue;
        const CUnitType* type = unit->GetUnitType();
        if (type && type->IsHero()) heroes++;
    }
    TEST_ASSERT(heroes >= 1);
    return true;
}

bool TestMeleeAIUpkeepThrottlesArmy() {
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
    sim.GetPlayer(1)->SetFoodCap(100);

    sim.CreateUnit(War3ID("hhou"), 1, 0, 0, 0);
    CUnit* barracks = sim.CreateUnit(War3ID("hbar"), 1, 400, 0, 0);
    TEST_ASSERT(barracks != nullptr);
    for (int i = 0; i < 6; i++) {
        sim.CreateUnit(War3ID("hpea"), 1, 200 + i * 40, 0, 0);
    }
    const CUnitType* foot = unitData.GetUnitType(War3ID("hfoo"));
    TEST_ASSERT(foot != nullptr);
    const int cost = foot->GetGoldCost();

    // High upkeep (level 2) with a thin wallet: training must wait.
    sim.GetPlayer(1)->SetFoodUsed(85);
    sim.GetPlayer(1)->SetGold(cost + 100);
    MeleeAIController ai(&sim, 1);
    for (int i = 0; i < 120; i++) {
        sim.Update();
        ai.Update();
    }
    auto* queue = dynamic_cast<CAbilityQueue*>(barracks->GetAbility(War3ID("Aque")));
    TEST_ASSERT(queue == nullptr || queue->GetQueueCount() == 0);
    // Rich enough to absorb the tax: training proceeds.
    sim.GetPlayer(1)->SetGold(cost + 500);
    for (int i = 0; i < 120; i++) {
        sim.Update();
        ai.Update();
    }
    queue = dynamic_cast<CAbilityQueue*>(barracks->GetAbility(War3ID("Aque")));
    bool trained = queue && queue->GetQueueCount() > 0;
    if (!trained) {
        // Already popped out of the queue: any player-1 footman proves it.
        for (CUnit* unit : sim.GetUnits()) {
            if (unit && !unit->IsDead() && unit->GetPlayerIndex() == 1 &&
                unit->GetTypeId() == War3ID("hfoo")) {
                trained = true;
            }
        }
    }
    TEST_ASSERT(trained);
    return true;
}

bool TestMeleeAIExpandsToSecondMine() {
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
    sim.GetPlayer(1)->SetGold(4000);
    sim.GetPlayer(1)->SetFoodCap(100);

    sim.CreateUnit(War3ID("hhou"), 1, 0, 0, 0);
    sim.CreateUnit(War3ID("hbar"), 1, 500, 0, 0);
    for (int i = 0; i < 6; i++) {
        sim.CreateUnit(War3ID("hpea"), 1, 200 + i * 40, 0, 0);
    }
    // Main mine nearly dry, rich mine across the map.
    CUnit* mainMine = sim.CreateUnit(War3ID("ngold"), 12, 700, 0, 0);
    CUnit* expoMine = sim.CreateUnit(War3ID("ngold"), 12, 4000, 0, 0);
    TEST_ASSERT(mainMine != nullptr && expoMine != nullptr);
    auto* mainGold = dynamic_cast<CAbilityGoldMine*>(mainMine->GetAbility(War3ID("Agld")));
    auto* expoGold = dynamic_cast<CAbilityGoldMine*>(expoMine->GetAbility(War3ID("Agld")));
    if (!mainGold || !expoGold) return true; // mine shape differs; skip
    mainGold->SetGold(500);
    expoGold->SetGold(12500);

    MeleeAIController ai(&sim, 1);
    bool expoBuilt = false;
    for (int i = 0; i < 8000 && !expoBuilt; i++) {
        sim.Update();
        ai.Update();
        if (i % 100 != 0) continue;
        int halls = 0;
        for (CUnit* unit : sim.GetUnits()) {
            if (unit && !unit->IsDead() && unit->GetPlayerIndex() == 1 &&
                unit->GetTypeId() == War3ID("hhou")) {
                halls++;
            }
        }
        expoBuilt = halls >= 2;
    }
    TEST_ASSERT(expoBuilt);
    // Transferred crew works the expo mine (not just the building).
    int expoCrew = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (!unit || unit->IsDead() || unit->GetPlayerIndex() != 1) continue;
        if (unit->GetTypeId() != War3ID("hpea")) continue;
        const float dx = unit->GetX() - 4000.0f;
        const float dy = unit->GetY() - 0.0f;
        if (dx * dx + dy * dy < 1500.0f * 1500.0f) expoCrew++;
    }
    TEST_ASSERT(expoCrew >= 1);
    return true;
}

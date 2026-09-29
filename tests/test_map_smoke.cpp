// Retail map smoke test: parse + config + main + pump + sim ticks on
// real melee and campaign maps, headless. Catches crashes and censuses
// unknown-function calls from genuine map scripts.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "data/abilitydata.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "resource/mpq_storm.h"
#include "scripting/JassVM.h"
#include "scripting/jass_common_env.h"
#include "scripting/jass_config_env.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace fs = std::filesystem;

namespace {

bool OpenGameMpqs(MpqManager& mpq) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    bool opened = false;
    for (auto a : {"War3x.mpq", "war3x.mpq", "War3Patch.mpq", "War3xLocal.mpq", "war3.mpq"}) {
        if (mpq.Open(res + a)) opened = true;
    }
    return opened;
}

std::map<std::string, int> CensusUnknown(const std::vector<std::string>& errors) {
    std::map<std::string, int> counts;
    const std::string prefix = "Unknown function: ";
    for (const auto& e : errors) {
        if (e.rfind(prefix, 0) == 0) counts[e.substr(prefix.size())]++;
    }
    return counts;
}

void PrintTopUnknown(const std::map<std::string, int>& counts, int top = 15) {
    std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cerr << "--- top unknown functions ---" << std::endl;
    for (size_t i = 0; i < sorted.size() && i < (size_t)top; i++) {
        std::cerr << "  " << sorted[i].first << " x" << sorted[i].second << std::endl;
    }
}

} // namespace

bool RunMapSmoke(MpqManager& mpq, const std::string& mapPath, bool expectCinematic, int localPlayer);

bool TestRetailMapSmoke() {
    const std::vector<std::pair<std::string, int>> maps = {
        {"/resources/Maps/FrozenThrone/(2)EchoIsles.w3x", 0},
        {"/resources/Maps/FrozenThrone/(2)SecretValley.w3x", 0},
        {"/resources/Maps/FrozenThrone/(4)TurtleRock.w3x", 0},
    };
    for (const auto& [rel, localPlayer] : maps) {
        const std::string mapPath = std::string(PROJECT_SOURCE_DIR) + rel;
        if (!fs::exists(mapPath)) {
            std::cerr << "map missing, skipping: " << rel << std::endl;
            continue;
        }
        MpqManager mpq;
        if (!OpenGameMpqs(mpq)) {
            std::cerr << "no game mpqs, skipping" << std::endl;
            return true;
        }
        if (!RunMapSmoke(mpq, mapPath, false, localPlayer)) return false;
    }
    // Campaign maps live inside the MPQs: extract to temp first. The local
    // player must own the cinematic force (Human01: player 1, NE01: 0).
    const std::vector<std::pair<std::string, int>> campaignMaps = {
        {"Maps\\Campaign\\Human01.w3m", 1},
        {"Maps\\FrozenThrone\\Campaign\\NightElfX01.w3x", 0},
    };
    if (std::getenv("WARSMASH_DUMP_SCRIPT")) {
        MpqManager mpqDbg;
        OpenGameMpqs(mpqDbg);
        for (const auto& bj : {"Scripts\\Blizzard.j", "Scripts\\common.j"}) {
            std::vector<uint8_t> bytes;
            if (mpqDbg.ReadFile(bj, bytes) && !bytes.empty()) {
                const fs::path outJ = fs::temp_directory_path() /
                    (std::string(bj) == "Scripts\\common.j" ? "common.j" : "Blizzard.j");
                std::ofstream out(outJ, std::ios::binary);
                out.write(reinterpret_cast<const char*>(bytes.data()),
                          static_cast<std::streamsize>(bytes.size()));
                std::cerr << "dumped " << outJ << " (" << bytes.size() << " bytes)" << std::endl;
            }
        }
        for (const auto& [inner, localPlayer] : campaignMaps) {
            (void)localPlayer;
            std::vector<uint8_t> bytes;
            if (!mpqDbg.ReadFile(inner, bytes) || bytes.empty()) continue;
            const fs::path tempPath = fs::temp_directory_path() / fs::path(inner).filename();
            {
                std::ofstream out(tempPath, std::ios::binary);
                out.write(reinterpret_cast<const char*>(bytes.data()),
                          static_cast<std::streamsize>(bytes.size()));
            }
            // Extract war3map.j via StormLib through ReadMapFile-style open.
            std::vector<uint8_t> jbytes;
            {
                // Reuse MpqManager by opening the temp map as an archive overlay.
                MpqManager tmp;
                if (tmp.Open(tempPath.string()) && tmp.ReadFile("war3map.j", jbytes)) {
                    const fs::path outJ = fs::temp_directory_path() /
                        (fs::path(inner).filename().string() + ".j");
                    std::ofstream out(outJ, std::ios::binary);
                    out.write(reinterpret_cast<const char*>(jbytes.data()),
                              static_cast<std::streamsize>(jbytes.size()));
                    std::cerr << "dumped " << outJ << " (" << jbytes.size() << " bytes)" << std::endl;
                }
            }
            fs::remove(tempPath);
        }
    }
    for (const auto& [inner, localPlayer] : campaignMaps) {
        MpqManager mpq;
        if (!OpenGameMpqs(mpq)) return true;
        std::vector<uint8_t> bytes;
        if (!mpq.ReadFile(inner, bytes) || bytes.empty()) {
            std::cerr << "campaign map missing in mpqs, skipping: " << inner << std::endl;
            continue;
        }
        const fs::path tempPath = fs::temp_directory_path() / fs::path(inner).filename();
        {
            std::ofstream out(tempPath, std::ios::binary);
            if (!out.is_open()) {
                std::cerr << "cannot write temp map, skipping" << std::endl;
                continue;
            }
            out.write(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
        }
        const bool ok = RunMapSmoke(mpq, tempPath.string(), true, localPlayer);
        fs::remove(tempPath);
        if (!ok) return false;
    }
    return true;
}

bool RunMapSmoke(MpqManager& mpq, const std::string& mapPath, bool expectCinematic, int localPlayer) {
    const std::vector<std::string> files = {"Scripts\\common.j", "Scripts\\Blizzard.j", "Scripts\\war3map.j"};

    // Config phase (player slots).
    JassConfigEnv configEnv;
    TEST_ASSERT(configEnv.Init(mpq, mapPath, files));
    configEnv.Config();

    // Game data so main() spawns real units.
    CGameplayConstants gameplayConstants;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gameplayConstants);
    CAbilityData abilityData = CAbilityData::LoadFromMpq(mpq);
    TEST_ASSERT(unitData.GetUnitType(War3ID("hpea")) != nullptr);

    CSimulation sim;
    sim.SetUnitData(&unitData);
    sim.SetAbilityData(&abilityData);
    for (int i = 0; i < 12; i++) {
        sim.AddPlayer(std::make_unique<CPlayer>(i, "P" + std::to_string(i), CMapControl::USER));
    }

    JassCommonEnv commonEnv;
    TEST_ASSERT(commonEnv.Init(mpq, mapPath, &sim, files, configEnv.GetData()));
    if (std::getenv("WARSMASH_DUMP_TRIGGERS")) {
        // Bisect: does an array write stick in this exact env?
        commonEnv.GetVM().LoadScript("function ProbeArr takes nothing returns nothing\n"
                                     "set bj_FORCE_PLAYER[1] = CreateForce()\n"
                                     "endfunction\n");
        commonEnv.GetVM().CallFunction("ProbeArr");
        const JassValue probe = commonEnv.GetVM().GetGlobal("bj_FORCE_PLAYER[1]");
        std::cerr << "  probe-arr-write=" << probe.intVal << std::endl;
        // Bisect 2: pre-populate via InitBlizzard, then Main must preserve it.
        if (commonEnv.GetVM().HasFunction("InitBlizzard")) {
            commonEnv.GetVM().CallFunction("InitBlizzard");
            const JassValue pre = commonEnv.GetVM().GetGlobal("bj_FORCE_PLAYER[1]");
            std::cerr << "  premain-initblizzard arr1=" << pre.intVal << std::endl;
        }
    // Bisect 3: run InitBlizzard's callees one by one to find the wiper.
    }
    if (auto* natives = commonEnv.GetNatives()) {
        if (auto data = natives->GetData()) data->SetLocalPlayerIndex(localPlayer);
    }
    commonEnv.Main();
    auto* natives = commonEnv.GetNatives();
    if (std::getenv("WARSMASH_DUMP_TRIGGERS")) {
        commonEnv.PumpQueuedThreads(4096);
        const JassValue early = commonEnv.GetVM().GetGlobal("bj_FORCE_PLAYER[1]");
        std::cerr << "  post-main-first-pump arr1=" << early.intVal << std::endl;
    }
    bool sawCinematic = false;
    for (int i = 0; i < 600; i++) {
        commonEnv.PumpQueuedThreads(4096);
        if (natives) natives->UpdateTimers(CSimulation::SIM_STEP_TIME);
        sim.Update();
        // Retail cinematics hide the interface (CinematicModeBJ), so the
        // interface flag is the observable signal (not cinematicMode,
        // which only the SetCinematicMode extension sets).
        if (natives && natives->GetData() && !natives->GetData()->uiConfig.showInterface) {
            sawCinematic = true;
        }
    }
    commonEnv.PumpQueuedThreads(4096);

    const auto& errors = commonEnv.GetVM().GetErrors();
    const auto unknown = CensusUnknown(errors);
    int execTriggers = 0;
    int totalTriggers = 0;
    if (std::getenv("WARSMASH_DUMP_TRIGGERS")) {
        const JassValue g = commonEnv.GetVM().GetGlobal("gg_trg_Map_Initialization");
        std::cerr << "  gg_trg_Map_Initialization=" << g.intVal << std::endl;
        if (natives && natives->GetData()) {
            for (const auto& [id, trig] : natives->GetData()->triggers) {
                if (!trig) continue;
                for (const auto& a : trig->actionFuncNames) {
                    if (a.find("Map_Initialization") != std::string::npos) {
                        std::cerr << "  init-trigger id=" << id << " exec=" << trig->execCount
                                  << " enabled=" << trig->enabled << " actions=" << trig->actionFuncNames.size()
                                  << " conds=" << trig->conditionFuncNames.size() << std::endl;
                    }
                }
                if (id == 33554442) {
                    std::cerr << "  trig33554442 actions:";
                    for (const auto& a : trig->actionFuncNames) std::cerr << " [" << a << "]";
                    std::cerr << " hasFunc=" << commonEnv.GetVM().HasFunction("Trig_Map_Initialization_Actions")
                              << std::endl;
                }
            }
        }
        // Bisect: call InitBlizzardGlobals directly and re-read.
        if (commonEnv.GetVM().HasFunction("InitBlizzardGlobals")) {
            commonEnv.GetVM().CallFunction("InitBlizzardGlobals");
            const JassValue arr1b = commonEnv.GetVM().GetGlobal("bj_FORCE_PLAYER[1]");
            std::cerr << " directInitBlizzardGlobals arr1=" << arr1b.intVal << std::endl;
        }
        // Bisect 2: does the full InitBlizzard stick?
        if (commonEnv.GetVM().HasFunction("InitBlizzard")) {
            commonEnv.GetVM().CallFunction("InitBlizzard");
            const JassValue arr1c = commonEnv.GetVM().GetGlobal("bj_FORCE_PLAYER[1]");
            std::cerr << " directInitBlizzard arr1=" << arr1c.intVal << std::endl;
        }
        if (natives && natives->GetData()) {
            const JassValue g2 = commonEnv.GetVM().GetGlobal("gg_trg_Map_Initialization");
            const auto& nativesMap = commonEnv.GetVM().GetRegisteredNatives();
            auto it = nativesMap.find("TriggerExecute");
            if (it != nativesMap.end() && g2.intVal != 0) {
                it->second({JassValue::Int(g2.intVal)});
                natives->PumpQueuedThreads(4096);
                auto dit = natives->GetData()->triggers.find(g2.intVal);
                std::cerr << "  manual-exec count="
                          << (dit != natives->GetData()->triggers.end() && dit->second ? dit->second->execCount : -1)
                          << " showInterface=" << natives->GetData()->uiConfig.showInterface << std::endl;
            }
            // Probe the flag plumbing itself.
            auto si = nativesMap.find("ShowInterface");
            if (si != nativesMap.end()) {
                si->second({JassValue::Bool(false), JassValue::Real(0)});
                std::cerr << "  direct-ShowInterface flag=" << natives->GetData()->uiConfig.showInterface << std::endl;
                si->second({JassValue::Bool(true), JassValue::Real(0)});
            }
            // Probe force membership + local player resolution.
            auto gf = nativesMap.find("GetForceOfPlayer");
            const JassValue arthasForce = commonEnv.GetVM().GetGlobal("udg_AAAPG_Arthas");
            std::cerr << "  arthasForce=" << arthasForce.intVal;
            const JassValue arthasPlayer = commonEnv.GetVM().GetGlobal("udg_AAAP_Arthas");
            std::cerr << " arthasPlayer=" << arthasPlayer.intVal;
            const JassValue force1 = commonEnv.GetVM().GetGlobal("bj_FORCE_PLAYER");
            std::cerr << " forceArray=" << (int)force1.type;
            const JassValue maxSlots = commonEnv.GetVM().GetGlobal("bj_MAX_PLAYER_SLOTS");
            std::cerr << " maxSlots=" << maxSlots.intVal;
            const JassValue arr1 = commonEnv.GetVM().GetGlobal("bj_FORCE_PLAYER[1]");
            std::cerr << " arr1=" << arr1.intVal;
            if (natives && natives->GetData()) {
                std::cerr << " forces=" << natives->GetData()->forces.size();
            }
            const JassValue started = commonEnv.GetVM().GetGlobal("bj_gameStarted");
            std::cerr << " gameStarted=" << started.boolVal;
            const JassValue filt = commonEnv.GetVM().GetGlobal("filterIssueHauntOrderAtLocBJ");
            std::cerr << " filtHaunt=" << filt.intVal;
            auto ipf = nativesMap.find("IsPlayerInForce");
            auto glp = nativesMap.find("GetLocalPlayer");
            if (ipf != nativesMap.end() && glp != nativesMap.end()) {
                const JassValue lp = glp->second({});
                std::cerr << " localPlayer=" << lp.intVal
                          << " inForce=" << ipf->second({lp, arthasForce}).boolVal;
            }
            std::cerr << std::endl;
        }
    }
    if (natives && natives->GetData()) {
        for (const auto& [id, trig] : natives->GetData()->triggers) {
            (void)id;
            totalTriggers++;
            if (trig && trig->execCount > 0) {
                execTriggers++;
                if (std::getenv("WARSMASH_DUMP_TRIGGERS")) {
                    std::cerr << "  fired trigger actions:";
                    for (const auto& a : trig->actionFuncNames) std::cerr << " " << a;
                    std::cerr << std::endl;
                }
            }
        }
    }
    std::cerr << "smoke: errors=" << errors.size() << " unknown-names=" << unknown.size()
              << " units=" << sim.GetUnits().size() << " cinematic=" << sawCinematic
              << " triggers=" << totalTriggers << " fired=" << execTriggers << std::endl;
    PrintTopUnknown(unknown);
    // Non-unknown errors (parse/runtime faults) must be empty.
    size_t other = 0;
    for (const auto& e : errors) {
        if (e.rfind("Unknown function: ", 0) != 0) {
            if (other < 10) std::cerr << "  ERR: " << e << std::endl;
            other++;
        }
    }
    TEST_ASSERT(other == 0);
    TEST_ASSERT(!sim.GetUnits().empty());
    if (expectCinematic) TEST_ASSERT(sawCinematic);
    return true;
}

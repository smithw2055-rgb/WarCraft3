#include "jass_common_env.h"
#include <iostream>
#include <algorithm>
#include <cctype>

#ifndef WITHOUT_STORMLIB
#include <StormLib.h>
#endif

namespace {

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

std::string BaseName(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool ReadFileFromMapArchive(const std::string& mapPath, const std::string& fileName,
                            std::vector<uint8_t>& out) {
#ifndef WITHOUT_STORMLIB
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return false;
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, fileName.c_str(), SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq);
        return false;
    }
    DWORD sz = SFileGetFileSize(hFile, nullptr);
    out.resize(sz);
    DWORD read = 0;
    const bool ok = SFileReadFile(hFile, out.data(), sz, &read, nullptr) && read == sz;
    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);
    return ok;
#else
    (void)mapPath; (void)fileName; (void)out;
    return false;
#endif
}

}

JassCommonEnv::JassCommonEnv()
    : data_(std::make_shared<JassNativesData>())
{
}

JassCommonEnv::~JassCommonEnv() = default;

bool JassCommonEnv::ReadFileFromMpq(MpqManager& mpq, const std::string& fileName,
                                     std::vector<uint8_t>& out) {
#ifndef WITHOUT_STORMLIB
    for (int i = 0; i < mpq.ArchiveCount(); i++) {
        HANDLE hMpq = mpq.GetArchiveHandle(i);
        if (!hMpq) continue;
        HANDLE hFile = nullptr;
        if (SFileOpenFileEx(hMpq, fileName.c_str(), SFILE_OPEN_FROM_MPQ, &hFile)) {
            DWORD sz = SFileGetFileSize(hFile, nullptr);
            out.resize(sz);
            DWORD read = 0;
            SFileReadFile(hFile, out.data(), sz, &read, nullptr);
            SFileCloseFile(hFile);
            return true;
        }
    }
#else
    (void)mpq; (void)fileName; (void)out;
#endif
    return false;
}

bool JassCommonEnv::ReadJassFile(MpqManager& mpq, const std::string& mapPath,
                                 const std::string& fileName, std::vector<uint8_t>& out) {
    out.clear();
    const std::string base = BaseName(fileName);
    const bool isMapScript = ToLowerCopy(base) == "war3map.j";

    if (isMapScript) {
        if (ReadFileFromMapArchive(mapPath, base, out)) return true;
        if (base != fileName && ReadFileFromMapArchive(mapPath, fileName, out)) return true;
    }

    if (ReadFileFromMpq(mpq, fileName, out)) return true;
    if (base != fileName && ReadFileFromMpq(mpq, base, out)) return true;

    if (!isMapScript) {
        if (ReadFileFromMapArchive(mapPath, fileName, out)) return true;
        if (base != fileName && ReadFileFromMapArchive(mapPath, base, out)) return true;
    }

    return false;
}

void JassCommonEnv::LoadJassFiles(MpqManager& mpq, const std::string& mapPath, const std::vector<std::string>& fileList) {
    for (const auto& file : fileList) {
        std::vector<uint8_t> data;
        if (ReadJassFile(mpq, mapPath, file, data) && !data.empty()) {
            std::string source((const char*)data.data(), data.size());
            std::cout << "JASS Common: loading " << file << " (" << source.size() << " bytes)" << std::endl;
            vm_.LoadScript(source);
        } else {
            std::cerr << "JASS Common: could not find " << file << " in MPQs" << std::endl;
        }
    }
}

bool JassCommonEnv::Init(MpqManager& mpq, const std::string& mapPath, CSimulation* sim,
                         const std::vector<std::string>& fileList,
                         const std::shared_ptr<JassNativesData>& configData) {
    // If config phase ran before, carry over player/slot data
    if (configData) {
        data_->configuredPlayerCount = configData->configuredPlayerCount;
        data_->startLocations = configData->startLocations;
        data_->playerMapConfig = configData->playerMapConfig;
        data_->items = configData->items;
        if (!data_->items.empty()) {
            std::cout << "JASS Common: carried over " << data_->items.size()
                      << " map items" << std::endl;
        }
    }

    // Register common (full game) natives
    natives_ = std::make_unique<JassNatives>(vm_, sim);
    natives_->SetData(data_);
    natives_->RegisterCommonNatives();

    LoadJassFiles(mpq, mapPath, fileList);

    if (!vm_.IsValid()) {
        for (auto& e : vm_.GetErrors())
            std::cerr << "JASS Common parse error: " << e << std::endl;
        return false;
    }

    return true;
}

void JassCommonEnv::Main() {
    if (!vm_.IsValid()) return;
    // vJASS scope/library initializers run before everything else.
    vm_.ExecuteInitializers();
    if (natives_ && vm_.HasFunction("abilities_main")) {
        std::cout << "JASS Common: queue abilities_main" << std::endl;
        natives_->QueueFunction("abilities_main");
    } else {
        std::cout << "JASS Common: abilities_main not found" << std::endl;
    }
    if (natives_ && vm_.HasFunction("main")) {
        std::cout << "JASS Common: queue main" << std::endl;
        natives_->QueueFunction("main");
    } else {
        std::cerr << "JASS Common: main() not found" << std::endl;
    }
}

int JassCommonEnv::PumpQueuedThreads(int maxCount) {
    return natives_ ? natives_->PumpQueuedThreads(maxCount) : 0;
}

void JassCommonEnv::WireCallbacks(CSimulation& sim) {
    if (!natives_) return;

    // Unit death events → JASS trigger system
    sim.SetUnitDeathCallback(
        [this, &sim](int unitHandle, int killerHandle, int) {
            if (natives_) {
                natives_->FireDeathEvent(unitHandle, killerHandle, JASS_EVENT_UNIT_DEATH);
                if (CUnit* dead = sim.GetUnit(unitHandle)) {
                    if (dead->IsHero()) natives_->FireRevivableEvent(unitHandle);
                }
            }
        }
    );

    // Unit damage events → JASS trigger system
    sim.SetUnitDamageCallback(
        [this](int unitHandle, int sourceHandle, float damage) {
            if (natives_)
                natives_->FireDamageEvent(unitHandle, sourceHandle, damage, JASS_EVENT_UNIT_DAMAGED);
        }
    );

    // Unit order events → JASS trigger system
    sim.SetUnitOrderCallback(
        [this](int unitHandle, int orderId, int targetHandle, float x, float y) {
            if (natives_)
                natives_->FireOrderEvent(unitHandle, orderId, targetHandle, x, y);
        }
    );

    // Transport boarding → JASS LOADED triggers
    sim.SetUnitLoadedCallback(
        [this](int transportHandle, int passengerHandle) {
            if (natives_)
                natives_->FireLoadedEvent(transportHandle, passengerHandle);
        }
    );

    // Corpse begins decaying → JASS DECAY triggers
    sim.SetUnitDecayCallback(
        [this](int unitHandle) {
            if (natives_)
                natives_->FireDecayEvent(unitHandle);
        }
    );

    // Unit item manipulation events -> JASS trigger system
    sim.SetItemEventCallback(
        [this](int unitHandle, int itemHandle, int eventType) {
            if (natives_)
                natives_->FireItemEvent(unitHandle, itemHandle, eventType);
        }
    );

    // Shop sales (buyer/seller/item) -> JASS SELL_ITEM triggers
    sim.SetShopEventCallback(
        [this](int buyerHandle, int sellerHandle, int itemHandle) {
            if (natives_)
                natives_->FireSellItemEvent(buyerHandle, sellerHandle, itemHandle);
        }
    );

    // Mercenary hires (buyer may be -1; seller/hired) -> SELL triggers
    sim.SetShopUnitEventCallback(
        [this](int buyerHandle, int sellerHandle, int hiredHandle) {
            if (natives_)
                natives_->FireSellUnitEvent(buyerHandle, sellerHandle, hiredHandle);
        }
    );

    // Pawn-back sales (buyer shop, seller pawner) -> PAWN triggers
    sim.SetPawnEventCallback(
        [this](int buyerHandle, int sellerHandle, int itemHandle) {
            if (natives_)
                natives_->FirePawnItemEvent(sellerHandle, buyerHandle, itemHandle);
        }
    );

    // Hero skill learning -> HERO_SKILL triggers
    sim.SetHeroSkillCallback(
        [this](int unitHandle, int skillId, int newLevel) {
            if (natives_)
                natives_->FireHeroSkillEvent(unitHandle, skillId, newLevel);
        }
    );

    // Queue lifecycle (kind 0 = train, 1 = research, 2 = upgrade morph,
    // 3 = hero revive).
    sim.SetQueueEventCallback(
        [this](int buildingHandle, int typeId, int kind, bool started) {
            if (!natives_) return;
            if (kind == 0) {
                if (started) {
                    natives_->FireTrainStartEvent(buildingHandle, typeId);
                } else {
                    natives_->FireTrainCancelEvent(buildingHandle, typeId);
                }
            } else if (kind == 1) {
                if (started) {
                    natives_->FireResearchStartEvent(buildingHandle, typeId);
                } else {
                    natives_->FireResearchCancelEvent(buildingHandle, typeId);
                }
            } else if (kind == 3) {
                // typeId carries the hero sim handle for revives (not a
                // rawcode like train/research).
                if (started) {
                    natives_->FireReviveStartEvent(buildingHandle, typeId);
                } else {
                    natives_->FireReviveCancelEvent(buildingHandle, typeId);
                }
            } else {
                if (started) {
                    natives_->FireUpgradeStartEvent(buildingHandle, typeId);
                } else {
                    natives_->FireUpgradeFinishEvent(buildingHandle, typeId);
                }
            }
        }
    );

    // Construction finish/cancel -> CONSTRUCT triggers
    sim.SetConstructEventCallback(
        [this](int buildingHandle, bool finished) {
            if (!natives_) return;
            if (finished) {
                natives_->FireConstructFinishEvent(buildingHandle);
            } else {
                natives_->FireConstructCancelEvent(buildingHandle);
            }
        }
    );

    // Sim-side ownership conversions (charm/possession) -> CHANGE_OWNER
    sim.SetOwnerChangedCallback(
        [this](int unitHandle, int prevOwnerIndex) {
            if (natives_)
                natives_->FireChangeOwnerEvent(unitHandle, prevOwnerIndex);
        }
    );

    // Unit trained / summoned / research / revive events -> JASS triggers
    sim.SetUnitTrainedCallback(
        [this](int buildingHandle, int trainedHandle) {
            if (natives_)
                natives_->FireTrainedEvent(buildingHandle, trainedHandle);
        }
    );
    sim.SetUnitSummonedCallback(
        [this](int summonerHandle, int summonedHandle) {
            if (natives_)
                natives_->FireSummonEvent(summonerHandle, summonedHandle);
        }
    );
    sim.SetResearchFinishedCallback(
        [this](int unitHandle, int upgradeId) {
            if (natives_)
                natives_->FireResearchEvent(unitHandle, upgradeId);
        }
    );
    sim.SetHeroRevivedCallback(
        [this](int reviverHandle, int revivedHandle) {
            if (natives_)
                natives_->FireReviveEvent(reviverHandle, revivedHandle);
        }
    );
    sim.SetSpellEventCallback(
        [this](int casterHandle, int stage, int targetSimHandle, float x, float y) {
            if (natives_)
                natives_->FireSpellEvent(casterHandle, stage, targetSimHandle, x, y);
        }
    );
}

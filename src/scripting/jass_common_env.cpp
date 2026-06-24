#include "jass_common_env.h"
#include <iostream>
#include <algorithm>

#ifndef WITHOUT_STORMLIB
#include <StormLib.h>
#endif

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

void JassCommonEnv::LoadJassFiles(MpqManager& mpq, const std::vector<std::string>& fileList) {
    for (const auto& file : fileList) {
        std::vector<uint8_t> data;
        if (ReadFileFromMpq(mpq, file, data) && !data.empty()) {
            std::string source((const char*)data.data(), data.size());
            std::cout << "JASS Common: loading " << file << " (" << source.size() << " bytes)" << std::endl;
            vm_.LoadScript(source);
        } else {
            std::cerr << "JASS Common: could not find " << file << " in MPQs" << std::endl;
        }
    }
}

bool JassCommonEnv::Init(MpqManager& mpq, CSimulation* sim,
                         const std::vector<std::string>& fileList,
                         const std::shared_ptr<JassNativesData>& configData) {
    LoadJassFiles(mpq, fileList);

    if (!vm_.IsValid()) {
        for (auto& e : vm_.GetErrors())
            std::cerr << "JASS Common parse error: " << e << std::endl;
        return false;
    }

    // If config phase ran before, carry over player/slot data
    if (configData) {
        data_->configuredPlayerCount = configData->configuredPlayerCount;
        data_->startLocations = configData->startLocations;
        data_->playerMapConfig = configData->playerMapConfig;
    }

    // Register common (full game) natives
    natives_ = std::make_unique<JassNatives>(vm_, sim);
    natives_->SetData(data_);
    natives_->RegisterCommonNatives();

    return true;
}

void JassCommonEnv::Main() {
    if (!vm_.IsValid()) return;
    std::cout << "JASS Common: executing main()" << std::endl;
    vm_.Execute("main");
}

void JassCommonEnv::WireCallbacks(CSimulation& sim) {
    if (!natives_) return;

    // Unit death events → JASS trigger system
    sim.SetUnitDeathCallback(
        [this](int unitHandle, int killerHandle, int) {
            if (natives_)
                natives_->FireDeathEvent(unitHandle, killerHandle, JASS_EVENT_UNIT_DEATH);
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
}

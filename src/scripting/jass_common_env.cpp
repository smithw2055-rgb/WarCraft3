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
    natives_->WireSimulationEvents(sim);
}

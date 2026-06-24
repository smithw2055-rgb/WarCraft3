#include "jass_config_env.h"
#include <iostream>
#include <fstream>
#include <cstring>
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

JassConfigEnv::JassConfigEnv()
    : data_(std::make_shared<JassNativesData>())
{
}

JassConfigEnv::~JassConfigEnv() = default;

bool JassConfigEnv::ReadFileFromMpq(MpqManager& mpq, const std::string& fileName,
                                     std::vector<uint8_t>& out) {
#ifndef WITHOUT_STORMLIB
    // Try to locate the file in any open MPQ
    std::string lowerName = fileName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

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

bool JassConfigEnv::ReadJassFile(MpqManager& mpq, const std::string& mapPath,
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

void JassConfigEnv::LoadJassFiles(MpqManager& mpq, const std::string& mapPath, const std::vector<std::string>& fileList) {
    for (const auto& file : fileList) {
        std::vector<uint8_t> data;
        if (ReadJassFile(mpq, mapPath, file, data) && !data.empty()) {
            std::string source((const char*)data.data(), data.size());
            std::cout << "JASS Config: loading " << file << " (" << source.size() << " bytes)" << std::endl;
            vm_.LoadScript(source);
        } else {
            std::cerr << "JASS Config: could not find " << file << " in MPQs" << std::endl;
        }
    }
}

bool JassConfigEnv::Init(MpqManager& mpq, const std::string& mapPath, const std::vector<std::string>& fileList) {
    natives_ = std::make_unique<JassNatives>(vm_, nullptr);
    natives_->SetData(data_);
    natives_->RegisterConfigNatives();

    LoadJassFiles(mpq, mapPath, fileList);

    if (!vm_.IsValid()) {
        for (auto& e : vm_.GetErrors())
            std::cerr << "JASS Config parse error: " << e << std::endl;
        return false;
    }

    return true;
}

void JassConfigEnv::Config() {
    if (!vm_.IsValid()) return;
    std::cout << "JASS Config: executing config()" << std::endl;
    if (natives_) {
        natives_->QueueFunction("config");
        natives_->PumpQueuedThreads(1024);
    } else {
        vm_.Execute("config");
    }
    configuredPlayerCount_ = data_->configuredPlayerCount;
}

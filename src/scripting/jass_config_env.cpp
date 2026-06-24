#include "jass_config_env.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>

#ifndef WITHOUT_STORMLIB
#include <StormLib.h>
#endif

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

void JassConfigEnv::LoadJassFiles(MpqManager& mpq, const std::vector<std::string>& fileList) {
    for (const auto& file : fileList) {
        // Try loading from MPQ first
        std::vector<uint8_t> data;
        if (ReadFileFromMpq(mpq, file, data) && !data.empty()) {
            std::string source((const char*)data.data(), data.size());
            std::cout << "JASS Config: loading " << file << " (" << source.size() << " bytes)" << std::endl;
            vm_.LoadScript(source);
        } else {
            std::cerr << "JASS Config: could not find " << file << " in MPQs" << std::endl;
        }
    }
}

bool JassConfigEnv::Init(MpqManager& mpq, const std::vector<std::string>& fileList) {
    LoadJassFiles(mpq, fileList);

    if (!vm_.IsValid()) {
        for (auto& e : vm_.GetErrors())
            std::cerr << "JASS Config parse error: " << e << std::endl;
        return false;
    }

    // Register config-phase natives only
    natives_ = std::make_unique<JassNatives>(vm_, nullptr);
    natives_->SetData(data_);
    natives_->RegisterConfigNatives();

    return true;
}

void JassConfigEnv::Config() {
    if (!vm_.IsValid()) return;
    std::cout << "JASS Config: executing config()" << std::endl;
    vm_.Execute("config");
    configuredPlayerCount_ = data_->configuredPlayerCount;
}

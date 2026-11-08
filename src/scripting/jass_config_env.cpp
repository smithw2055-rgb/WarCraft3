#include "jass_config_env.h"
#include <iostream>

JassConfigEnv::JassConfigEnv()
    : data_(std::make_shared<JassNativesData>()) {}

JassConfigEnv::~JassConfigEnv() = default;

bool JassConfigEnv::ReadFileFromMpq(MpqManager& mpq, const std::string& fileName,
                                    std::vector<uint8_t>& out) {
    return mpq.ReadFromGameArchivesOnly(fileName, out);
}

bool JassConfigEnv::ReadJassFile(MpqManager& mpq, const std::string& mapPath,
                                 const std::string& fileName, std::vector<uint8_t>& out) {
    return mpq.ReadJassScriptFile(mapPath, fileName, out);
}

void JassConfigEnv::LoadJassFiles(MpqManager& mpq, const std::string& mapPath,
                                  const std::vector<std::string>& fileList) {
    for (const auto& file : fileList) {
        std::vector<uint8_t> data;
        if (ReadJassFile(mpq, mapPath, file, data) && !data.empty()) {
            std::string source(reinterpret_cast<const char*>(data.data()), data.size());
            std::cout << "JASS Config: loading " << file << " (" << source.size() << " bytes)" << std::endl;
            vm_.LoadScript(source);
        } else {
            std::cerr << "JASS Config: could not find " << file << " in MPQs" << std::endl;
        }
    }
}

bool JassConfigEnv::Init(MpqManager& mpq, const std::string& mapPath,
                         const std::vector<std::string>& fileList) {
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

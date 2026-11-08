#include "jass_common_env.h"
#include <iostream>

JassCommonEnv::JassCommonEnv()
    : data_(std::make_shared<JassNativesData>()) {}

JassCommonEnv::~JassCommonEnv() = default;

bool JassCommonEnv::ReadFileFromMpq(MpqManager& mpq, const std::string& fileName,
                                    std::vector<uint8_t>& out) {
    return mpq.ReadFromGameArchivesOnly(fileName, out);
}

bool JassCommonEnv::ReadJassFile(MpqManager& mpq, const std::string& mapPath,
                                 const std::string& fileName, std::vector<uint8_t>& out) {
    return mpq.ReadJassScriptFile(mapPath, fileName, out);
}

void JassCommonEnv::LoadJassFiles(MpqManager& mpq, const std::string& mapPath,
                                  const std::vector<std::string>& fileList) {
    for (const auto& file : fileList) {
        std::vector<uint8_t> data;
        if (ReadJassFile(mpq, mapPath, file, data) && !data.empty()) {
            std::string source(reinterpret_cast<const char*>(data.data()), data.size());
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

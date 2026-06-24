#pragma once
#include "JassVM.h"
#include "JassNatives.h"
#include "../resource/mpq_storm.h"
#include <string>
#include <vector>
#include <memory>

class CSimulation;

class JassConfigEnv {
public:
    JassConfigEnv();
    ~JassConfigEnv();

    bool Init(MpqManager& mpq, const std::vector<std::string>& fileList);
    void Config();

    JassVM& GetVM() { return vm_; }
    std::shared_ptr<JassNativesData> GetData() const { return data_; }

    int GetConfiguredPlayerCount() const { return configuredPlayerCount_; }
    const std::array<JassStartLocationConfig, 24>& GetStartLocations() const { return data_->startLocations; }
    const std::array<JassPlayerMapConfig, 24>& GetPlayerMapConfig() const { return data_->playerMapConfig; }

private:
    void LoadJassFiles(MpqManager& mpq, const std::vector<std::string>& fileList);
    bool ReadFileFromMpq(MpqManager& mpq, const std::string& fileName, std::vector<uint8_t>& out);

    JassVM vm_;
    std::shared_ptr<JassNativesData> data_;
    std::unique_ptr<JassNatives> natives_;
    int configuredPlayerCount_ = 0;
};

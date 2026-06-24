#pragma once
#include "JassVM.h"
#include "JassNatives.h"
#include "../resource/mpq_storm.h"
#include "../ecs/csimulation.h"
#include <string>
#include <vector>
#include <memory>

class JassCommonEnv {
public:
    JassCommonEnv();
    ~JassCommonEnv();

    bool Init(MpqManager& mpq, CSimulation* sim, const std::vector<std::string>& fileList,
              const std::shared_ptr<JassNativesData>& configData = nullptr);
    void Main();
    void WireCallbacks(CSimulation& sim);

    JassVM& GetVM() { return vm_; }
    JassNatives* GetNatives() { return natives_.get(); }
    std::unique_ptr<JassNatives> ReleaseNatives() { return std::move(natives_); }
    std::shared_ptr<JassNativesData> GetData() const { return data_; }

private:
    void LoadJassFiles(MpqManager& mpq, const std::vector<std::string>& fileList);
    bool ReadFileFromMpq(MpqManager& mpq, const std::string& fileName, std::vector<uint8_t>& out);

    JassVM vm_;
    std::shared_ptr<JassNativesData> data_;
    std::unique_ptr<JassNatives> natives_;
};

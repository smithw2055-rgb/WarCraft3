#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "resource/slk.h"

struct CUpgradeType {
    War3ID id;
    std::string name;
    std::string iconPath;
    int goldCost = 0;
    int lumberCost = 0;
    int researchTime = 0;
    int maxLevel = 1;
    int race = 0;
    std::vector<int> goldPerLevel;
    std::vector<int> lumberPerLevel;
    std::vector<int> timePerLevel;
    std::vector<std::string> effectPerLevel;
};

class CUpgradeData {
public:
    CUpgradeData() = default;

    bool HasUpgrade(War3ID id) const;
    const CUpgradeType* GetUpgrade(War3ID id) const;

    static CUpgradeData LoadFromMpq(const class MpqManager& mpq);
    static CUpgradeData LoadFromMpq(const class MpqManager& mpq, const std::string& mapPath);

private:
    std::unordered_map<War3ID, std::unique_ptr<CUpgradeType>> upgrades_;
};

#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "resource/slk.h"

struct CUpgradeEffectSpec {
    std::string effectCode; // SLK effect1..4 (e.g., "rarm", "ratx", "ratd", "rhpx", "rhpo", "rmnx", "rmno", "rmvx", "rats", "rlev", "rsig")
    float base = 0.0f;      // SLK base1..4
    float mod = 0.0f;       // SLK mod1..4
    std::string code;       // SLK code1..4 (e.g., ability rawcode for rlev)

    float EvalAtLevel(int level) const {
        if (level <= 0) return 0.0f;
        return base + mod * static_cast<float>(level - 1);
    }

    float EvalCumulativeToLevel(int level) const {
        if (level <= 0) return 0.0f;
        float total = 0.0f;
        for (int l = 1; l <= level; ++l) {
            total += EvalAtLevel(l);
        }
        return total;
    }
};

struct CUpgradeType {
    War3ID id;
    std::string name;
    std::string iconPath;
    std::string upgradeClass; // SLK "class" (e.g., "armor", "melee", "ranged", "artillery", "caster")
    int goldCost = 0;
    int lumberCost = 0;
    int researchTime = 0;
    int maxLevel = 1;
    int race = 0;
    std::vector<int> goldPerLevel;
    std::vector<int> lumberPerLevel;
    std::vector<int> timePerLevel;
    std::vector<std::string> effectPerLevel;
    std::vector<CUpgradeEffectSpec> effects;
};

class CUpgradeData {
public:
    CUpgradeData() = default;

    bool HasUpgrade(War3ID id) const;
    const CUpgradeType* GetUpgrade(War3ID id) const;
    const std::unordered_map<War3ID, std::unique_ptr<CUpgradeType>>& GetAllUpgrades() const { return upgrades_; }
    // Test and script registration. Replaces an existing row for the same id.
    void RegisterUpgrade(War3ID id, std::unique_ptr<CUpgradeType> type) { upgrades_[id] = std::move(type); }

    static CUpgradeData LoadFromMpq(const class MpqManager& mpq);
    static CUpgradeData LoadFromMpq(const class MpqManager& mpq, const std::string& mapPath);

private:
    std::unordered_map<War3ID, std::unique_ptr<CUpgradeType>> upgrades_;
};

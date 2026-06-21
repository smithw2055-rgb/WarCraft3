#include "upgradedata.h"
#include "resource/mpq_storm.h"
#include <iostream>
#include <sstream>

CUpgradeData CUpgradeData::LoadFromMpq(const MpqManager& mpq) {
    CUpgradeData data;

    TxtTable tbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\UpgradeData.slk", buf)) {
            tbl.Load(buf.data(), buf.size());
        }
    }

    TxtTable skinTbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\UpgradeFunc.txt", buf)) {
            skinTbl.Load(buf.data(), buf.size());
        }
        std::vector<uint8_t> buf2;
        if (mpq.ReadFile("Units\\UpgradeStrings.txt", buf2)) {
            skinTbl.Load(buf2.data(), buf2.size());
        }
    }

    auto sections = tbl.Sections();
    for (auto& sid : sections) {
        if (sid.size() != 4) continue;
        War3ID id = War3ID::FromString(sid);

        auto upg = std::make_unique<CUpgradeType>();
        upg->id = id;
        upg->name = tbl.GetString(sid, "Name", sid);
        upg->iconPath = skinTbl.GetString(sid, "Art", "");
        upg->goldCost = tbl.GetInt(sid, "goldCost", 0);
        upg->lumberCost = tbl.GetInt(sid, "lumberCost", 0);
        upg->researchTime = tbl.GetInt(sid, "researchTime", 0);
        upg->maxLevel = std::max(1, tbl.GetInt(sid, "maxLevel", 1));
        upg->race = tbl.GetInt(sid, "race", 0);

        // Per-level costs
        for (int l = 1; l <= upg->maxLevel; l++) {
            std::string suffix = std::to_string(l);
            upg->goldPerLevel.push_back(tbl.GetInt(sid, "goldCost" + suffix, upg->goldCost));
            upg->lumberPerLevel.push_back(tbl.GetInt(sid, "lumberCost" + suffix, upg->lumberCost));
            upg->timePerLevel.push_back(tbl.GetInt(sid, "researchTime" + suffix, upg->researchTime));
            std::string eff = tbl.GetString(sid, "Effect" + suffix);
            if (!eff.empty()) upg->effectPerLevel.push_back(eff);
        }

        data.upgrades_[id] = std::move(upg);
    }

    std::cout << "CUpgradeData: loaded " << data.upgrades_.size() << " upgrades" << std::endl;
    return data;
}

bool CUpgradeData::HasUpgrade(War3ID id) const {
    return upgrades_.find(id) != upgrades_.end();
}

const CUpgradeType* CUpgradeData::GetUpgrade(War3ID id) const {
    auto it = upgrades_.find(id);
    return (it != upgrades_.end()) ? it->second.get() : nullptr;
}

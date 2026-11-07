#include "upgradedata.h"
#include "object_mod_meta.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <cctype>
#include <iostream>
#include <sstream>

namespace {

bool EqualsNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return true;
}

void ApplyUpgradeModSet(TxtTable& tbl, TxtTable& skinTbl, const ObjectMetaTable& meta,
                        const ObjectModificationSet& set,
                        int& applied, int& unknown, int& customCloned, int& customSkipped) {
    for (const auto& entry : set.custom) {
        const std::string oldId = entry.oldId.ToString();
        const std::string newId = entry.newId.ToString();
        if (oldId.size() != 4 || newId.size() != 4 || !tbl.HasSection(oldId)) {
            customSkipped++;
            continue;
        }
        for (const auto& [key, value] : tbl.GetSection(oldId)) {
            tbl.SetString(newId, key, value);
        }
        customCloned++;
        for (const auto& mod : entry.modifications) {
            std::string table, key;
            if (!ResolveObjectModField(meta, mod, table, key)) {
                unknown++;
                continue;
            }
            const std::string value = FormatObjectModValue(mod);
            if (EqualsNoCase(table, "Profile")) {
                skinTbl.SetString(newId, key, value);
            } else {
                tbl.SetString(newId, key, value);
            }
            applied++;
        }
    }
    for (const auto& entry : set.original) {
        const std::string section = entry.oldId.ToString();
        if (!tbl.HasSection(section)) {
            unknown += (int)entry.modifications.size();
            continue;
        }
        for (const auto& mod : entry.modifications) {
            std::string table, key;
            if (!ResolveObjectModField(meta, mod, table, key)) {
                unknown++;
                continue;
            }
            const std::string value = FormatObjectModValue(mod);
            if (EqualsNoCase(table, "Profile")) {
                skinTbl.SetString(section, key, value);
            } else {
                tbl.SetString(section, key, value);
            }
            applied++;
        }
    }
}

} // namespace

CUpgradeData CUpgradeData::LoadFromMpq(const MpqManager& mpq) {
    return LoadFromMpq(mpq, "");
}

CUpgradeData CUpgradeData::LoadFromMpq(const MpqManager& mpq, const std::string& mapPath) {
    CUpgradeData data;

    // UpgradeData.slk is SLK spreadsheet format (not TXT profile): convert
    // rows into sections keyed by upgradeid so the overlay + parse below
    // share one representation.
    TxtTable tbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\UpgradeData.slk", buf)) {
            SlkTable slk;
            if (slk.Load(buf.data(), buf.size())) {
                const int idCol = slk.FindColumn("upgradeid");
                if (idCol >= 0) {
                    for (int r = 0; r < slk.Rows(); ++r) {
                        const std::string sid = slk.GetString(r, idCol, "");
                        if (sid.size() != 4) continue;
                        for (int c = 0; c < slk.Cols() && c < (int)slk.colNames_.size(); ++c) {
                            const std::string& key = slk.colNames_[c];
                            if (key.empty()) continue;
                            const std::string value = slk.GetString(r, c, "");
                            if (value.empty() || value == "_") continue;
                            tbl.SetString(sid, key, value);
                        }
                    }
                }
            }
        }
    }

    TxtTable skinTbl;
    {
        const char* upgradeFiles[] = {
            "Units\\UpgradeFunc.txt",
            "Units\\UpgradeStrings.txt",
            "Units\\CommonUpgradeFunc.txt",
            "Units\\CommonUpgradeStrings.txt",
            "Units\\HumanUpgradeFunc.txt",
            "Units\\HumanUpgradeStrings.txt",
            "Units\\OrcUpgradeFunc.txt",
            "Units\\OrcUpgradeStrings.txt",
            "Units\\NightElfUpgradeFunc.txt",
            "Units\\NightElfUpgradeStrings.txt",
            "Units\\UndeadUpgradeFunc.txt",
            "Units\\UndeadUpgradeStrings.txt",
            "Units\\NeutralUpgradeFunc.txt",
            "Units\\NeutralUpgradeStrings.txt",
            "Units\\CampaignUpgradeFunc.txt",
            "Units\\CampaignUpgradeStrings.txt",
        };
        for (const char* f : upgradeFiles) {
            std::vector<uint8_t> buf;
            if (mpq.ReadFile(f, buf)) {
                skinTbl.Load(buf.data(), buf.size());
            }
        }
    }

    // Map-specific custom upgrades (war3map.w3q) before parsing.
    if (!mapPath.empty()) {
        ObjectModificationSet set;
        if (LoadWar3MapUpgradeMods(mapPath, set)) {
            std::vector<uint8_t> metaBuf;
            SlkTable metaSlk;
            ObjectMetaTable meta;
            if (mpq.ReadFile("Units\\UpgradeMetaData.slk", metaBuf) &&
                metaSlk.Load(metaBuf.data(), metaBuf.size()) && meta.Load(metaSlk)) {
                int applied = 0, unknown = 0, cloned = 0, skipped = 0;
                ApplyUpgradeModSet(tbl, skinTbl, meta, set, applied, unknown, cloned, skipped);
                std::cout << "ObjectMods: war3map.w3q applied=" << applied
                          << " unknown=" << unknown << " customCloned=" << cloned
                          << " customSkipped=" << skipped << std::endl;
            } else {
                std::cerr << "ObjectMods: UpgradeMetaData.slk unavailable; w3q skipped" << std::endl;
            }
        } else {
            std::cout << "ObjectMods: no war3map.w3q upgrade modifications" << std::endl;
        }
    }

    auto sections = tbl.Sections();
    auto raceIndex = [](const std::string& s) {
        if (s == "human") return 1;
        if (s == "orc") return 2;
        if (s == "nightelf") return 3;
        if (s == "undead") return 4;
        return 0;
    };
    for (auto& sid : sections) {
        if (sid.size() != 4) continue;
        War3ID id = War3ID::FromString(sid);

        auto upg = std::make_unique<CUpgradeType>();
        upg->id = id;
        // Display name lives in the skin strings (Profile "Name"); fall back
        // to the SLK comments column, then the raw id.
        upg->name = skinTbl.GetString(sid, "Name", "");
        if (upg->name.empty()) upg->name = tbl.GetString(sid, "comments", sid);
        upg->iconPath = skinTbl.GetString(sid, "Art", "");
        const int goldBase = tbl.GetInt(sid, "goldbase", 0);
        const int goldMod = tbl.GetInt(sid, "goldmod", 0);
        const int lumberBase = tbl.GetInt(sid, "lumberbase", 0);
        const int lumberMod = tbl.GetInt(sid, "lumbermod", 0);
        const int timeBase = tbl.GetInt(sid, "timebase", 0);
        const int timeMod = tbl.GetInt(sid, "timemod", 0);
        upg->maxLevel = std::max(1, tbl.GetInt(sid, "maxlevel", 1));
        upg->race = raceIndex(tbl.GetString(sid, "race", ""));
        upg->upgradeClass = tbl.GetString(sid, "class", "");

        // Per-level costs follow base + mod*(level-1) (ref upgrade cost formula).
        for (int l = 1; l <= upg->maxLevel; l++) {
            const int gold = goldBase + goldMod * (l - 1);
            const int lumber = lumberBase + lumberMod * (l - 1);
            const int time = timeBase + timeMod * (l - 1);
            upg->goldPerLevel.push_back(gold);
            upg->lumberPerLevel.push_back(lumber);
            upg->timePerLevel.push_back(time);
            if (l == 1) {
                upg->goldCost = gold;
                upg->lumberCost = lumber;
                upg->researchTime = time;
            }
            const std::string eff = tbl.GetString(sid, "effect" + std::to_string(l), "");
            if (!eff.empty()) upg->effectPerLevel.push_back(eff);
        }
        // Parse SLK effect slots effect1..4, base1..4, mod1..4, code1..4
        for (int slot = 1; slot <= 4; ++slot) {
            const std::string suffix = std::to_string(slot);
            const std::string effCode = tbl.GetString(sid, "effect" + suffix, "");
            if (effCode.empty() || effCode == "_") continue;
            CUpgradeEffectSpec spec;
            spec.effectCode = effCode;
            spec.base = tbl.GetFloat(sid, "base" + suffix, 0.0f);
            spec.mod = tbl.GetFloat(sid, "mod" + suffix, 0.0f);
            spec.code = tbl.GetString(sid, "code" + suffix, "");
            upg->effects.push_back(std::move(spec));
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

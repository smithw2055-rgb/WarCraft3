#include "abilitydata.h"
#include "object_mod_meta.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include "resource/slk.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

int FindRowByAlias(SlkTable& slk, int aliasCol, const std::string& alias) {
    if (aliasCol < 0) return -1;
    for (int r = 0; r < slk.Rows(); ++r) {
        if (slk.GetString(r, aliasCol, "") == alias) return r;
    }
    return -1;
}

// Clone a base SLK row for a custom object id; returns the new row index.
int CloneSlkRow(SlkTable& slk, int baseRow, const std::string& newAlias) {
    if (baseRow < 0 || baseRow >= slk.Rows()) return -1;
    std::vector<std::string> row = slk.data_[baseRow];
    const int aliasCol = slk.FindColumn("alias");
    if (aliasCol >= 0 && aliasCol < (int)row.size()) row[aliasCol] = newAlias;
    const int codeCol = slk.FindColumn("code");
    if (codeCol >= 0 && codeCol < (int)row.size()) row[codeCol] = newAlias;
    slk.data_.push_back(std::move(row));
    return (int)slk.data_.size() - 1;
}

void SetSlkCell(SlkTable& slk, int row, const std::string& colName, const std::string& value,
                int& applied, int& unknown) {
    const int col = slk.FindColumn(colName);
    if (col < 0 || row < 0 || row >= slk.Rows()) {
        unknown++;
        return;
    }
    if (col >= (int)slk.data_[row].size()) {
        slk.data_[row].resize(col + 1);
    }
    slk.data_[row][col] = value;
    applied++;
}

bool EqualsNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return true;
}

// Apply one w3a/w3h modification set to SLK rows (+ skin profile table).
void ApplyAbilityModSet(SlkTable& slk, TxtTable& skinTbl, const ObjectMetaTable& meta,
                        const ObjectModificationSet& set, int& applied, int& unknown,
                        int& customCloned, int& customSkipped) {
    const int aliasCol = slk.FindColumn("alias");
    for (const auto& entry : set.custom) {
        const std::string oldId = entry.oldId.ToString();
        const std::string newId = entry.newId.ToString();
        if (oldId.size() != 4 || newId.size() != 4) {
            customSkipped++;
            continue;
        }
        const int newRow = CloneSlkRow(slk, FindRowByAlias(slk, aliasCol, oldId), newId);
        if (newRow < 0) {
            customSkipped++;
            continue;
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
                applied++;
            } else {
                SetSlkCell(slk, newRow, key, value, applied, unknown);
            }
        }
    }
    for (const auto& entry : set.original) {
        const std::string section = entry.oldId.ToString();
        const int row = FindRowByAlias(slk, aliasCol, section);
        if (row < 0) {
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
                applied++;
            } else {
                SetSlkCell(slk, row, key, value, applied, unknown);
            }
        }
    }
}

void ApplyMapAbilityMods(const MpqManager& mpq, const std::string& mapPath,
                         SlkTable& abilitySlk, TxtTable& skinTbl, SlkTable& buffSlk) {
    if (mapPath.empty()) return;

    ObjectModificationSet abilitySet;
    const bool hasW3a = LoadWar3MapAbilityMods(mapPath, abilitySet);
    ObjectModificationSet buffSet;
    const bool hasW3h = LoadWar3MapBuffMods(mapPath, buffSet);
    if (!hasW3a && !hasW3h) {
        std::cout << "ObjectMods: no war3map.w3a/w3h ability modifications" << std::endl;
        return;
    }

    if (hasW3a) {
        std::vector<uint8_t> metaBuf;
        SlkTable metaSlk;
        ObjectMetaTable meta;
        if (mpq.ReadFile("Units\\AbilityMetaData.slk", metaBuf) &&
            metaSlk.Load(metaBuf.data(), metaBuf.size()) && meta.Load(metaSlk)) {
            int applied = 0, unknown = 0, cloned = 0, skipped = 0;
            ApplyAbilityModSet(abilitySlk, skinTbl, meta, abilitySet,
                               applied, unknown, cloned, skipped);
            std::cout << "ObjectMods: war3map.w3a applied=" << applied
                      << " unknown=" << unknown << " customCloned=" << cloned
                      << " customSkipped=" << skipped << std::endl;
        } else {
            std::cerr << "ObjectMods: AbilityMetaData.slk unavailable; w3a skipped" << std::endl;
        }
    }
    if (hasW3h) {
        std::vector<uint8_t> metaBuf;
        SlkTable metaSlk;
        ObjectMetaTable meta;
        if (mpq.ReadFile("Units\\AbilityBuffMetaData.slk", metaBuf) &&
            metaSlk.Load(metaBuf.data(), metaBuf.size()) && meta.Load(metaSlk)) {
            int applied = 0, unknown = 0, cloned = 0, skipped = 0;
            ApplyAbilityModSet(buffSlk, skinTbl, meta, buffSet,
                               applied, unknown, cloned, skipped);
            std::cout << "ObjectMods: war3map.w3h applied=" << applied
                      << " unknown=" << unknown << " customCloned=" << cloned
                      << " customSkipped=" << skipped << std::endl;
        } else {
            std::cerr << "ObjectMods: AbilityBuffMetaData.slk unavailable; w3h skipped" << std::endl;
        }
    }
}

} // namespace

CAbilityData CAbilityData::LoadFromMpq(const MpqManager& mpq) {
    return LoadFromMpq(mpq, "");
}

CAbilityData CAbilityData::LoadFromMpq(const MpqManager& mpq, const std::string& mapPath) {
    CAbilityData data;

    // Load AbilityData.slk (actual SLK spreadsheet format)
    SlkTable abilitySlk;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\AbilityData.slk", buf)) {
            abilitySlk.Load(buf.data(), buf.size());
        }
    }

    // Load skin/string files for additional fields (TXT profile format)
    TxtTable skinTbl;
    {
        std::vector<std::string> skinFiles = {
            "Units\\AbilitySkin.txt",
            "Units\\AbilityFunc.txt",
            "Units\\AbilityStrings.txt",
            "Units\\CommonAbilityFunc.txt",
            "Units\\CommonAbilityStrings.txt",
            "Units\\HumanAbilityFunc.txt",
            "Units\\HumanAbilityStrings.txt",
            "Units\\OrcAbilityFunc.txt",
            "Units\\OrcAbilityStrings.txt",
            "Units\\NightElfAbilityFunc.txt",
            "Units\\NightElfAbilityStrings.txt",
            "Units\\UndeadAbilityFunc.txt",
            "Units\\UndeadAbilityStrings.txt",
            "Units\\NeutralAbilityFunc.txt",
            "Units\\NeutralAbilityStrings.txt",
            "Units\\CampaignAbilityFunc.txt",
            "Units\\CampaignAbilityStrings.txt",
            "Units\\ItemAbilityFunc.txt",
            "Units\\ItemAbilityStrings.txt",
        };
        for (auto& f : skinFiles) {
            std::vector<uint8_t> sbuf;
            if (mpq.ReadFile(f, sbuf)) {
                skinTbl.Load(sbuf.data(), sbuf.size());
            }
        }
    }

    if (abilitySlk.Rows() == 0) {
        std::cout << "CAbilityData: no rows found" << std::endl;
        return data;
    }

    SlkTable buffSlk;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\AbilityBuffData.slk", buf)) {
            buffSlk.Load(buf.data(), buf.size());
        }
    }

    // Map-specific custom abilities/buffs (war3map.w3a/.w3h) before parsing.
    ApplyMapAbilityMods(mpq, mapPath, abilitySlk, skinTbl, buffSlk);

    // Map column names to indices
    auto col = [&](const std::string& name) -> int { return abilitySlk.FindColumn(name); };
    // Level 1 columns
    int colAlias = col("alias");
    int colCode = col("code");
    int colName = col("alias"); // alias column is the display name
    int colRace = col("race");
    int colLevels = col("levels");
    int colTargs1 = col("targs1");
    int colCast1 = col("Cast1");
    int colDur1 = col("Dur1");
    int colCool1 = col("Cool1");
    int colCost1 = col("Cost1");
    int colArea1 = col("Area1");
    int colRng1 = col("Rng1");
    int colDataA1 = col("DataA1");
    int colDataB1 = col("DataB1");
    int colDataC1 = col("DataC1");
    int colDataD1 = col("DataD1");
    int colDataE1 = col("DataE1");
    int colDataF1 = col("DataF1");
    int colDataG1 = col("DataG1");
    int colDataH1 = col("DataH1");
    int colDataI1 = col("DataI1");

    for (int r = 0; r < abilitySlk.Rows(); r++) {
        std::string alias = (colAlias >= 0) ? abilitySlk.GetString(r, colAlias, "") : "";
        if (alias.empty()) continue;

        // The ability code (section key for skin files) is either "code" or "alias"
        std::string code = (colCode >= 0) ? abilitySlk.GetString(r, colCode, alias) : alias;

        War3ID id = War3ID::FromString(alias);
        auto def = std::make_unique<CAbilityDefFull>();
        def->id = id;
        def->name = (colName >= 0) ? abilitySlk.GetString(r, colName, alias) : alias;
        def->legacyName = def->name;

        // UI fields from skin files (keyed by the "code" value)
        def->tooltip = skinTbl.GetString(code, "TooltipNormal", "");
        def->uberTip = skinTbl.GetString(code, "UberTip", "");
        def->hotkey = skinTbl.GetString(code, "Hotkey", "");

        // Art from skin file, try case variations
        def->iconArt = skinTbl.GetString(code, "Art", "");
        if (def->iconArt.empty()) {
            std::string lc = code;
            for (auto& c : lc) c = (char)std::tolower(c);
            def->iconArt = skinTbl.GetString(lc, "Art", "");
        }

        // Button position from skin file
        std::string btnPos = skinTbl.GetString(code, "Buttonpos", "");
        if (btnPos.empty()) {
            std::string lc = code;
            for (auto& c : lc) c = (char)std::tolower(c);
            btnPos = skinTbl.GetString(lc, "Buttonpos", "");
        }
        if (!btnPos.empty()) {
            size_t comma = btnPos.find(',');
            if (comma != std::string::npos) {
                def->buttonPosX = std::stoi(btnPos.substr(0, comma));
                def->buttonPosY = std::stoi(btnPos.substr(comma + 1));
            } else {
                def->buttonPosX = std::stoi(btnPos);
                def->buttonPosY = 0;
            }
        }

        // Hardcoded icon/button overrides for common abilities (skin files missing in this WC3 version)
        {
            auto setIfMissing = [&](const std::string& art, int bx, int by) {
                if (def->iconArt.empty()) def->iconArt = art;
                if (def->buttonPosX < 0) def->buttonPosX = bx;
                if (def->buttonPosY < 0) def->buttonPosY = by;
            };
            if (code == "Adef") setIfMissing("ReplaceableTextures\\CommandButtons\\BTNDefend.blp", 1, 1);
            else if (code == "AInv") setIfMissing("ReplaceableTextures\\CommandButtons\\BTNInventory.blp", 0, 1);
            else if (code == "AInf") setIfMissing("ReplaceableTextures\\CommandButtons\\BTNInvisibility.blp", 0, 2);
            // For abilities without matching hardcoded override, keep defaults (empty art, -1,-1 pos)
        }

        // Effect/sound art per effecttype from the Func profile skin tables
        // (ref GetAbilityEffect/Sound; single value per type, no index list).
        {
            static const char* kEffectKeys[7] = {
                "Effectart", "TargetArt", "Casterart", "Specialart",
                "Areaeffectart", "Missileart", "LightningEffect",
            };
            std::string lc = code;
            for (auto& c : lc) c = (char)std::tolower(c);
            for (int t = 0; t < 7; t++) {
                std::string art = skinTbl.GetString(code, kEffectKeys[t], "");
                if (art.empty()) art = skinTbl.GetString(lc, kEffectKeys[t], "");
                def->effectArtByType[(size_t)t] = art;
            }
            def->effectSound = skinTbl.GetString(code, "Effectsound", "");
            if (def->effectSound.empty()) def->effectSound = skinTbl.GetString(lc, "Effectsound", "");
        }

        // Combat fields (level 1)
        if (colTargs1 >= 0) def->targetsAllowed = abilitySlk.GetString(r, colTargs1, "");
        if (colCast1 >= 0) def->castTime = (float)std::atof(abilitySlk.GetString(r, colCast1, "0").c_str());
        if (colDur1 >= 0) def->duration = (float)std::atof(abilitySlk.GetString(r, colDur1, "0").c_str());
        if (colCool1 >= 0) def->cooldown = (float)std::atof(abilitySlk.GetString(r, colCool1, "0").c_str());
        if (colCost1 >= 0) def->manaCost = abilitySlk.GetInt(r, colCost1, 0);
        if (colArea1 >= 0) def->area = (float)std::atof(abilitySlk.GetString(r, colArea1, "0").c_str());
        if (colRng1 >= 0) def->range = (float)std::atof(abilitySlk.GetString(r, colRng1, "0").c_str());
        if (colLevels >= 0) def->levels = std::max(1, abilitySlk.GetInt(r, colLevels, 1));

        // Per-level combat fields (Cast/Dur/Cool/Cost/Area/Rng blocks 1..N;
        // retail pads past `levels`, so over-long vectors are normal).
        for (int level = 1; level <= 12; level++) {
            const std::string n = std::to_string(level);
            const int colCool = col("Cool" + n);
            if (colCool < 0) break;
            const int colCost = col("Cost" + n);
            const int colCast = col("Cast" + n);
            const int colDur = col("Dur" + n);
            const int colArea = col("Area" + n);
            const int colRng = col("Rng" + n);
            def->cooldowns.push_back((float)std::atof(abilitySlk.GetString(r, colCool, "0").c_str()));
            def->manaCosts.push_back((colCost >= 0) ? abilitySlk.GetInt(r, colCost, 0) : 0);
            def->castTimes.push_back((colCast >= 0)
                ? (float)std::atof(abilitySlk.GetString(r, colCast, "0").c_str()) : 0.0f);
            def->durations.push_back((colDur >= 0)
                ? (float)std::atof(abilitySlk.GetString(r, colDur, "0").c_str()) : 0.0f);
            def->areas.push_back((colArea >= 0)
                ? (float)std::atof(abilitySlk.GetString(r, colArea, "0").c_str()) : 0.0f);
            def->ranges.push_back((colRng >= 0)
                ? (float)std::atof(abilitySlk.GetString(r, colRng, "0").c_str()) : 0.0f);
            // Raw effect fields DataA-I for the same level.
            std::array<std::string, 9> dataRow;
            dataRow.fill("");
            static const char* kDataCols[9] = {
                "DataA", "DataB", "DataC", "DataD", "DataE",
                "DataF", "DataG", "DataH", "DataI",
            };
            for (int f = 0; f < 9; f++) {
                const int colData = col(std::string(kDataCols[f]) + n);
                if (colData < 0) break;
                std::string val = abilitySlk.GetString(r, colData, "");
                if (val != "-") dataRow[(size_t)f] = val;
            }
            def->dataLevels.push_back(dataRow);
            const int colUnit = col("UnitID" + n);
            if (colUnit >= 0) {
                std::string idStr = abilitySlk.GetString(r, colUnit, "");
                def->unitIds.push_back(idStr.empty() || idStr == "-"
                    ? War3ID() : War3ID::FromString(idStr));
            }
        }
        if (colRace >= 0) {
            std::string raceStr = abilitySlk.GetString(r, colRace, "");
            if (raceStr == "human") def->race = 1;
            else if (raceStr == "orc") def->race = 2;
            else if (raceStr == "nightelf") def->race = 3;
            else if (raceStr == "undead") def->race = 4;
        }

        // Data fields A-I (level 1)
        int ds[] = { colDataA1, colDataB1, colDataC1, colDataD1, colDataE1,
                     colDataF1, colDataG1, colDataH1, colDataI1 };
        for (int di = 0; di < 9; di++) {
            if (ds[di] >= 0) {
                std::string val = abilitySlk.GetString(r, ds[di], "");
                if (!val.empty() && val != "-") def->dataFields.push_back(val);
            }
        }

        data.abilities_[id] = std::move(def);
    }

    if (buffSlk.Rows() > 0) {
        auto buffCol = [&](const std::string& name) -> int { return buffSlk.FindColumn(name); };
        const int colAliasBuff = buffCol("alias");
        const int colCodeBuff = buffCol("code");
        const int colBuffArt = buffCol("Buffart");
        const int colBuffTip = buffCol("Bufftip");
        const int colBuffUberTip = buffCol("Buffubertip");

        for (int r = 0; r < buffSlk.Rows(); ++r) {
            std::string alias = (colAliasBuff >= 0) ? buffSlk.GetString(r, colAliasBuff, "") : "";
            if (alias.empty()) continue;

            std::string code = (colCodeBuff >= 0) ? buffSlk.GetString(r, colCodeBuff, alias) : alias;
            if (code.empty()) code = alias;

            auto def = std::make_unique<CBuffDefFull>();
            def->id = War3ID::FromString(alias);
            def->name = alias;
            def->tooltip = (colBuffTip >= 0) ? buffSlk.GetString(r, colBuffTip, "") : "";
            def->uberTip = (colBuffUberTip >= 0) ? buffSlk.GetString(r, colBuffUberTip, "") : "";
            def->iconArt = (colBuffArt >= 0) ? buffSlk.GetString(r, colBuffArt, "") : "";

            if (def->iconArt.empty()) {
                def->iconArt = skinTbl.GetString(code, "Buffart", "");
            }
            if (def->tooltip.empty()) {
                def->tooltip = skinTbl.GetString(code, "Bufftip", "");
            }
            if (def->uberTip.empty()) {
                def->uberTip = skinTbl.GetString(code, "Buffubertip", "");
            }

            data.buffs_[def->id] = std::move(def);
        }
    }

    std::cout << "CAbilityData: loaded " << data.abilities_.size() << " abilities" << std::endl;
    std::cout << "CAbilityData: loaded " << data.buffs_.size() << " buff UI defs" << std::endl;
    return data;
}

bool CAbilityData::HasAbility(War3ID id) const {
    return abilities_.find(id) != abilities_.end();
}

namespace {

int ClampAbilityLevel(const CAbilityDefFull* def, int level) {
    if (!def || def->levels <= 0) return 1;
    if (level < 1) return 1;
    if (level > def->levels) return def->levels;
    return level;
}

template <typename T>
T LevelOr(const std::vector<T>& values, int level, T fallback) {
    if (values.empty()) return fallback;
    int index = level - 1;
    if (index < 0) index = 0;
    if (index >= (int)values.size()) index = (int)values.size() - 1;
    return values[(size_t)index];
}

} // namespace

float CAbilityDefFull::CooldownAt(int level) const {
    return LevelOr(cooldowns, ClampAbilityLevel(this, level), cooldown);
}

int CAbilityDefFull::ManaCostAt(int level) const {
    return LevelOr(manaCosts, ClampAbilityLevel(this, level), manaCost);
}

float CAbilityDefFull::CastTimeAt(int level) const {
    return LevelOr(castTimes, ClampAbilityLevel(this, level), castTime);
}

float CAbilityDefFull::DurationAt(int level) const {
    return LevelOr(durations, ClampAbilityLevel(this, level), duration);
}

float CAbilityDefFull::AreaAt(int level) const {
    return LevelOr(areas, ClampAbilityLevel(this, level), area);
}

float CAbilityDefFull::RangeAt(int level) const {
    return LevelOr(ranges, ClampAbilityLevel(this, level), range);
}

std::string CAbilityDefFull::DataAt(int level, int field) const {
    if (field < 0 || field > 8 || dataLevels.empty()) return "";
    int index = ClampAbilityLevel(this, level) - 1;
    if (index < 0) index = 0;
    if (index >= (int)dataLevels.size()) index = (int)dataLevels.size() - 1;
    return dataLevels[(size_t)index][(size_t)field];
}

float CAbilityDefFull::DataFloatAt(int level, int field, float fallback) const {
    const std::string val = DataAt(level, field);
    if (val.empty()) return fallback;
    return (float)std::atof(val.c_str());
}

int CAbilityDefFull::DataIntAt(int level, int field, int fallback) const {
    const std::string val = DataAt(level, field);
    if (val.empty()) return fallback;
    return std::atoi(val.c_str());
}

War3ID CAbilityDefFull::UnitIdAt(int level, War3ID fallback) const {
    if (unitIds.empty()) return fallback;
    int index = ClampAbilityLevel(this, level) - 1;
    if (index < 0) index = 0;
    if (index >= (int)unitIds.size()) index = (int)unitIds.size() - 1;
    const War3ID id = unitIds[(size_t)index];
    if (id == War3ID()) return fallback;
    return id;
}

const CAbilityDefFull* CAbilityData::GetAbility(War3ID id) const {
    auto it = abilities_.find(id);
    return (it != abilities_.end()) ? it->second.get() : nullptr;
}

const CBuffDefFull* CAbilityData::GetBuff(War3ID id) const {
    auto it = buffs_.find(id);
    return (it != buffs_.end()) ? it->second.get() : nullptr;
}

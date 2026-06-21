#include "abilitydata.h"
#include "resource/mpq_storm.h"
#include "resource/slk.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>

CAbilityData CAbilityData::LoadFromMpq(const MpqManager& mpq) {
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

        // Combat fields (level 1)
        if (colTargs1 >= 0) def->targetsAllowed = abilitySlk.GetString(r, colTargs1, "");
        if (colCast1 >= 0) def->castTime = (float)std::atof(abilitySlk.GetString(r, colCast1, "0").c_str());
        if (colDur1 >= 0) def->duration = (float)std::atof(abilitySlk.GetString(r, colDur1, "0").c_str());
        if (colCool1 >= 0) def->cooldown = (float)std::atof(abilitySlk.GetString(r, colCool1, "0").c_str());
        if (colCost1 >= 0) def->manaCost = abilitySlk.GetInt(r, colCost1, 0);
        if (colArea1 >= 0) def->area = (float)std::atof(abilitySlk.GetString(r, colArea1, "0").c_str());
        if (colRng1 >= 0) def->range = (float)std::atof(abilitySlk.GetString(r, colRng1, "0").c_str());
        if (colLevels >= 0) def->levels = std::max(1, abilitySlk.GetInt(r, colLevels, 1));
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

    std::cout << "CAbilityData: loaded " << data.abilities_.size() << " abilities" << std::endl;
    return data;
}

bool CAbilityData::HasAbility(War3ID id) const {
    return abilities_.find(id) != abilities_.end();
}

const CAbilityDefFull* CAbilityData::GetAbility(War3ID id) const {
    auto it = abilities_.find(id);
    return (it != abilities_.end()) ? it->second.get() : nullptr;
}

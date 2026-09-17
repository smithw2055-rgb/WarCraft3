// Shared helpers for applying war3map object modifications (w3a/w3q/w3h and
// friends) through the Units\*MetaData.slk field mapping, mirroring the
// w3u path in unitdata.cpp (ref Warcraft3MapObjectData + level/dataPointer
// merge rules).
#pragma once
#include <cstdlib>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "resource/slk.h"
#include "map/object_mods.h"

struct ObjectMetaTable {
    // metaId (e.g. "acoo") -> {column -> value} for the *MetaData.slk rows.
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> rows;

    bool Load(const SlkTable& slk) {
        rows.clear();
        const int idCol = slk.FindColumn("ID");
        if (idCol < 0) return false;
        for (int r = 0; r < slk.Rows(); ++r) {
            const std::string id = slk.GetString(r, idCol, "");
            if (id.empty()) continue;
            for (int c = 0; c < slk.Cols() && c < (int)slk.colNames_.size(); ++c) {
                const std::string& name = slk.colNames_[c];
                if (name.empty()) continue;
                rows[id][name] = slk.GetString(r, c, "");
            }
        }
        return !rows.empty();
    }

    bool TryGet(const std::string& id, const std::string& key, std::string& out) const {
        auto it = rows.find(id);
        if (it == rows.end()) return false;
        auto jt = it->second.find(key);
        if (jt == it->second.end() || jt->second.empty()) return false;
        out = jt->second;
        return true;
    }
};

inline std::string FormatObjectModValue(const ObjectModification& mod) {
    switch (mod.varType) {
    case ObjectModification::INT:
    case ObjectModification::BOOLEAN:
        return std::to_string(mod.intValue);
    case ObjectModification::STRING:
        return mod.stringValue;
    case ObjectModification::REAL:
    case ObjectModification::UNREAL:
    default: {
        std::ostringstream ss;
        ss << mod.realValue;
        return ss.str();
    }
    }
}

// Resolve a modification to (table, key), expanding metadata data/index/
// repeat suffixes with the modification level (ported from unitdata.cpp).
inline bool ResolveObjectModField(const ObjectMetaTable& meta, const ObjectModification& mod,
                                  std::string& outTable, std::string& outKey) {
    const std::string metaId = mod.id.ToString();
    if (!meta.TryGet(metaId, "slk", outTable) || outTable.empty()) return false;
    if (!meta.TryGet(metaId, "field", outKey) || outKey.empty()) return false;

    int data = 0;
    std::string dataValue;
    if (meta.TryGet(metaId, "data", dataValue) && !dataValue.empty()) {
        data = std::atoi(dataValue.c_str());
    }
    if (data > 0) {
        outKey.push_back(static_cast<char>('A' + data - 1));
    }

    int index = -1;
    std::string indexValue;
    if (meta.TryGet(metaId, "index", indexValue) && !indexValue.empty()) {
        index = std::atoi(indexValue.c_str());
    }

    int repeat = 0;
    std::string repeatValue;
    if (meta.TryGet(metaId, "repeat", repeatValue) && !repeatValue.empty()) {
        repeat = std::atoi(repeatValue.c_str());
    }

    std::string appendIndexMode;
    meta.TryGet(metaId, "appendIndex", appendIndexMode);
    int level = mod.level;
    if (repeat > 0) {
        if (appendIndexMode == "1") {
            const int upgradeExtensionLevel = level - 1;
            if (upgradeExtensionLevel > 0) {
                outKey += std::to_string(upgradeExtensionLevel);
            }
        } else if (appendIndexMode == "0") {
            // Indexed list storage is collapsed into one comma field in this C++ data path.
        } else if (index == -1 || repeat >= 10) {
            if (level == 0) level = 1;
            if (repeat >= 10 && level >= 0 && level < 100) {
                if (level < 10) outKey.push_back('0');
                outKey += std::to_string(level);
            } else {
                outKey += std::to_string(level);
            }
        }
    }
    return true;
}

#pragma once
// ============================================================================
// Phase 12 Unified Data Table & ObjectModification Merge Engine (slk_merge.h)
//
// Consolidates the 4x duplicated SLK/TXT/ObjectMod merge implementations
// previously scattered across slk_merge.cpp, itemdata.cpp, unitdata.cpp,
// destructabledata.cpp, doodaddata.cpp, and object_mod_meta.h.
// ============================================================================
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "map/object_mods.h"
#include "resource/slk.h"

class MpqManager;

inline int FindFirstColumn(const SlkTable& slk,
                           std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const int column = slk.FindColumn(name);
        if (column >= 0) {
            return column;
        }
    }
    return -1;
}

inline std::string TrimCopy(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline void ParseWar3IdList(const std::string& str, std::vector<War3ID>& out) {
    if (str.empty()) return;
    size_t pos = 0;
    while (pos < str.size()) {
        const size_t comma = str.find(',', pos);
        const std::string code = TrimCopy(str.substr(pos, comma - pos));
        if (code.size() == 4) {
            out.push_back(War3ID::FromString(code));
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}

inline bool TryGetTxtValueCaseInsensitive(const TxtTable& tbl,
                                          const std::string& section,
                                          const std::string& key,
                                          std::string& outValue) {
    const auto values = tbl.GetSection(section);
    const auto it = values.find(key);
    if (it != values.end()) {
        outValue = it->second;
        return true;
    }
    const std::string targetKey = ToLowerCopy(key);
    for (const auto& [candidateKey, candidateValue] : values) {
        if (ToLowerCopy(candidateKey) == targetKey) {
            outValue = candidateValue;
            return true;
        }
    }
    return false;
}

struct SlkAccessor {
    const SlkTable* table = nullptr;
    int idColumn = -1;
    std::unordered_map<std::string, int> columns;
    std::unordered_map<std::string, int> rowsById;

    SlkAccessor() = default;

    SlkAccessor(const SlkTable& slk,
                std::initializer_list<const char*> idColumnCandidates) {
        Init(slk, idColumnCandidates);
    }

    void Init(const SlkTable& slk,
              std::initializer_list<const char*> idColumnCandidates) {
        table = &slk;
        columns.clear();
        rowsById.clear();
        idColumn = -1;

        for (int i = 0; i < slk.Cols(); ++i) {
            columns[slk.colNames_[i]] = i;
        }

        for (const char* candidate : idColumnCandidates) {
            auto it = columns.find(candidate);
            if (it != columns.end()) {
                idColumn = it->second;
                break;
            }
        }
        if (idColumn < 0 && slk.Cols() > 0) {
            idColumn = 0;
        }
        if (idColumn < 0) {
            return;
        }

        for (int row = 0; row < slk.Rows(); ++row) {
            const std::string id = TrimCopy(slk.GetString(row, idColumn));
            if (!id.empty()) {
                rowsById[id] = row;
            }
        }
    }

    bool TryGetValue(const std::string& id, const std::string& key,
                     std::string& outValue) const {
        if (!table) {
            return false;
        }
        const auto rowIt = rowsById.find(id);
        if (rowIt == rowsById.end()) {
            return false;
        }
        const auto colIt = columns.find(key);
        if (colIt == columns.end()) {
            return false;
        }
        outValue = table->GetString(rowIt->second, colIt->second);
        return !outValue.empty();
    }
};

// Overload 1: Row/Col indexed SLK + TXT overlay (used by destructables, doodads, items)
inline std::string GetMergedString(const SlkTable& slk, int row, int column,
                                   const TxtTable& txt, const std::string& section,
                                   const std::string& key,
                                   const std::string& def = "") {
    const std::string txtValue = txt.GetString(section, key, "");
    if (!txtValue.empty()) {
        return txtValue;
    }
    if (row >= 0 && column >= 0) {
        const std::string slkValue = slk.GetString(row, column, "");
        if (!slkValue.empty()) {
            return slkValue;
        }
    }
    return def;
}

inline int GetMergedInt(const SlkTable& slk, int row, int column,
                        const TxtTable& txt, const std::string& section,
                        const std::string& key, int def = 0) {
    const std::string txtValue = txt.GetString(section, key, "");
    if (!txtValue.empty()) {
        return txt.GetInt(section, key, def);
    }
    if (row >= 0 && column >= 0) {
        return slk.GetInt(row, column, def);
    }
    return def;
}

// Overload 2: SlkAccessor ID-keyed + case-insensitive TXT overlay (used by unitdata)
inline std::string GetMergedString(const TxtTable& txt, const SlkAccessor& slk,
                                   const std::string& section, const std::string& key,
                                   const std::string& def = "") {
    std::string value;
    if (TryGetTxtValueCaseInsensitive(txt, section, key, value)) {
        return value;
    }
    if (slk.TryGetValue(section, key, value)) {
        return value;
    }
    return def;
}

inline int GetMergedInt(const TxtTable& txt, const SlkAccessor& slk,
                        const std::string& section, const std::string& key, int def = 0) {
    const std::string value = GetMergedString(txt, slk, section, key);
    if (value.empty()) {
        return def;
    }
    return std::atoi(value.c_str());
}

inline float GetMergedFloat(const TxtTable& txt, const SlkAccessor& slk,
                            const std::string& section, const std::string& key, float def = 0.0f) {
    const std::string value = GetMergedString(txt, slk, section, key);
    if (value.empty()) {
        return def;
    }
    return std::strtof(value.c_str(), nullptr);
}

// ---- Map object-mod overlay helpers (shared across all object-data loaders) ----
std::string FormatObjectModificationValue(const ObjectModification& mod);

// 2-output overload: resolves both target SLK name and expanded field key
bool ResolveObjectModField(const SlkAccessor& meta, const ObjectModification& mod,
                           std::string& outSlk, std::string& outField);

// 1-output overload: resolves expanded field key while verifying target SLK is non-empty
bool ResolveObjectModField(const SlkAccessor& meta, const ObjectModification& mod,
                           std::string& outField);

int CopyObjectDataSection(TxtTable& txt, const SlkAccessor& slk,
                          const std::string& oldSection,
                          const std::string& newSection);

bool ApplyObjectModificationEntry(const ObjectModificationEntry& entry,
                                  const std::string& section,
                                  const SlkAccessor& meta,
                                  TxtTable& overlay,
                                  int& applied,
                                  int& unknown);

// Generic single-table object modification applier (shared by w3b, w3d, w3t)
void ApplySingleTableObjectMods(const MpqManager& mpq,
                                const ObjectModificationSet& mods,
                                std::initializer_list<const char*> metaSlkCandidates,
                                const SlkTable& baseSlk,
                                std::initializer_list<const char*> idColumnCandidates,
                                TxtTable& overlay,
                                const char* debugLabel = nullptr);

void ApplyMapDestructableObjectMods(const MpqManager& mpq, const std::string& mapPath,
                                    const SlkTable& destructableSlk,
                                    TxtTable& overlay);

void ApplyMapDoodadObjectMods(const MpqManager& mpq, const std::string& mapPath,
                              const SlkTable& doodadSlk,
                              TxtTable& overlay);

void ApplyMapItemObjectMods(const MpqManager& mpq, const std::string& mapPath,
                            const SlkTable& itemSlk,
                            TxtTable& overlay);

void CollectDestructableIds(const SlkAccessor& slk, const TxtTable& txt,
                            std::set<std::string>& outIds);

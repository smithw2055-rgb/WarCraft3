#pragma once
// Shared SLK + map-mod overlay helpers for object-data tables
// (units, destructables/doodads, items, ...). Extracted verbatim from
// destructabledata.cpp so CDoodadData reuses the same merge semantics:
// fuzzy column lookup, id-keyed row index, TXT overlay with SLK fallback.
#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <unordered_map>

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

// TXT overlay wins; SLK row/column second; default last. `txt` carries
// map w3b/w3u modifications, `slk` the base retail table.
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

// ---- Map object-mod overlay (war3map.w3b over the retail SLK) ----
// Implemented in slk_merge.cpp. Shared by every object-data loader so
// custom map objects merge with identical semantics.
std::string FormatObjectModificationValue(const ObjectModification& mod);

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

void ApplyMapDestructableObjectMods(const MpqManager& mpq, const std::string& mapPath,
                                    const SlkTable& destructableSlk,
                                    TxtTable& overlay);

// Doodad custom data (war3map.w3d + DoodadMetaData.slk): same merge
// semantics as destructables but sourced from the Doodad object changeset
// (ref Warcraft3MapObjectData doodadChangeset).
void ApplyMapDoodadObjectMods(const MpqManager& mpq, const std::string& mapPath,
                              const SlkTable& doodadSlk,
                              TxtTable& overlay);

void CollectDestructableIds(const SlkAccessor& slk, const TxtTable& txt,
                            std::set<std::string>& outIds);

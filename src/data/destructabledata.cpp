#include "destructabledata.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {

int FindFirstColumn(const SlkTable& slk, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const int column = slk.FindColumn(name);
        if (column >= 0) {
            return column;
        }
    }
    return -1;
}

std::string TrimCopy(std::string value) {
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

    SlkAccessor(const SlkTable& slk, std::initializer_list<const char*> idColumnCandidates) {
        Init(slk, idColumnCandidates);
    }

    void Init(const SlkTable& slk, std::initializer_list<const char*> idColumnCandidates) {
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

    bool TryGetValue(const std::string& id, const std::string& key, std::string& outValue) const {
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

std::string GetMergedString(const SlkTable& slk, int row, int column,
                            const TxtTable& txt, const std::string& section,
                            const std::string& key, const std::string& def = "") {
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

int GetMergedInt(const SlkTable& slk, int row, int column,
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

std::string FormatObjectModificationValue(const ObjectModification& mod) {
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

bool ResolveObjectModField(const SlkAccessor& meta, const ObjectModification& mod,
                           std::string& outField) {
    const std::string metaId = mod.id.ToString();
    std::string slkName;
    if (!meta.TryGetValue(metaId, "slk", slkName) || slkName.empty()) return false;
    if (!meta.TryGetValue(metaId, "field", outField) || outField.empty()) return false;

    int data = 0;
    std::string dataValue;
    if (meta.TryGetValue(metaId, "data", dataValue) && !dataValue.empty()) {
        data = std::atoi(dataValue.c_str());
    }
    if (data > 0) {
        outField.push_back(static_cast<char>('A' + data - 1));
    }

    int index = -1;
    std::string indexValue;
    if (meta.TryGetValue(metaId, "index", indexValue) && !indexValue.empty()) {
        index = std::atoi(indexValue.c_str());
    }

    int repeat = 0;
    std::string repeatValue;
    if (meta.TryGetValue(metaId, "repeat", repeatValue) && !repeatValue.empty()) {
        repeat = std::atoi(repeatValue.c_str());
    }

    std::string appendIndexMode;
    meta.TryGetValue(metaId, "appendIndex", appendIndexMode);
    int level = mod.level;
    if (repeat > 0) {
        if (appendIndexMode == "1") {
            const int upgradeExtensionLevel = level - 1;
            if (upgradeExtensionLevel > 0) {
                outField += std::to_string(upgradeExtensionLevel);
            }
        } else if (appendIndexMode == "0") {
        } else if (index == -1 || repeat >= 10) {
            if (level == 0) level = 1;
            if (repeat >= 10 && level >= 0 && level < 100) {
                if (level < 10) outField.push_back('0');
                outField += std::to_string(level);
            } else {
                outField += std::to_string(level);
            }
        }
    }
    return true;
}

int CopyObjectDataSection(TxtTable& txt, const SlkAccessor& slk,
                          const std::string& oldSection,
                          const std::string& newSection) {
    if (oldSection.empty() || newSection.empty()) return 0;

    int copied = 0;
    const auto rowIt = slk.rowsById.find(oldSection);
    if (slk.table && rowIt != slk.rowsById.end()) {
        const int row = rowIt->second;
        for (int col = 0; col < slk.table->Cols(); ++col) {
            if (col >= (int)slk.table->colNames_.size()) continue;
            const std::string& key = slk.table->colNames_[col];
            if (key.empty()) continue;
            const std::string value = slk.table->GetString(row, col);
            if (value.empty()) continue;
            txt.SetString(newSection, key, value);
            copied++;
        }
    }

    const auto oldTxtValues = txt.GetSection(oldSection);
    for (const auto& [key, value] : oldTxtValues) {
        txt.SetString(newSection, key, value);
        copied++;
    }
    return copied;
}

bool ApplyObjectModificationEntry(const ObjectModificationEntry& entry,
                                  const std::string& section,
                                  const SlkAccessor& meta,
                                  TxtTable& overlay,
                                  int& applied,
                                  int& unknown) {
    if (section.size() != 4) return false;
    bool touched = false;
    for (const auto& mod : entry.modifications) {
        std::string fieldName;
        if (!ResolveObjectModField(meta, mod, fieldName)) {
            unknown++;
            continue;
        }
        overlay.SetString(section, fieldName, FormatObjectModificationValue(mod));
        applied++;
        touched = true;
    }
    return touched;
}

void ApplyMapDestructableObjectMods(const MpqManager& mpq, const std::string& mapPath,
                                    const SlkTable& destructableSlk,
                                    TxtTable& overlay) {
    if (mapPath.empty()) return;

    ObjectModificationSet mods;
    if (!LoadWar3MapDestructableMods(mapPath, mods)) {
        std::cout << "ObjectMods: no war3map.w3b destructable modifications" << std::endl;
        return;
    }

    SlkTable metaSlk;
    std::vector<uint8_t> metaBuf;
    if (!mpq.ReadFile("Units\\DestructableMetaData.slk", metaBuf) ||
        !metaSlk.Load(metaBuf.data(), metaBuf.size())) {
        std::cerr << "ObjectMods: DestructableMetaData.slk unavailable; destructable modifications skipped" << std::endl;
        return;
    }

    const SlkAccessor meta(metaSlk, {"ID"});
    const SlkAccessor destructableData(destructableSlk, {"DestructableID", "destructableID", "doodID", "ID"});
    int applied = 0;
    int unknown = 0;
    int customCloned = 0;
    int customSkipped = 0;

    for (const auto& entry : mods.custom) {
        const std::string oldSection = entry.oldId.ToString();
        const std::string newSection = entry.newId.ToString();
        if (oldSection.size() != 4 || newSection.size() != 4) {
            customSkipped++;
            continue;
        }
        if (CopyObjectDataSection(overlay, destructableData, oldSection, newSection) == 0) {
            customSkipped++;
            continue;
        }
        customCloned++;
        ApplyObjectModificationEntry(entry, newSection, meta, overlay, applied, unknown);
    }

    for (const auto& entry : mods.original) {
        const std::string section = entry.oldId.ToString();
        ApplyObjectModificationEntry(entry, section, meta, overlay, applied, unknown);
    }

    std::cout << "ObjectMods: war3map.w3b original=" << mods.original.size()
              << " custom=" << mods.custom.size()
              << " applied=" << applied
              << " unknown=" << unknown
              << " customCloned=" << customCloned
              << " customSkipped=" << customSkipped << std::endl;
}

void CollectDestructableIds(const SlkAccessor& slk, const TxtTable& txt, std::set<std::string>& outIds) {
    for (const auto& [id, row] : slk.rowsById) {
        (void)row;
        if (id.size() == 4) {
            outIds.insert(id);
        }
    }
    for (const auto& section : txt.Sections()) {
        if (section.size() == 4) {
            outIds.insert(section);
        }
    }
}

}

CDestructableData CDestructableData::LoadFromMpq(const MpqManager& mpq,
                                                 const std::string& mapPath) {
    CDestructableData data;

    SlkTable slk;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\DestructableData.slk", buf)) {
            slk.Load(buf.data(), buf.size());
        }
    }

    TxtTable skinTbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\DestructableSkin.txt", buf)) {
            skinTbl.Load(buf.data(), buf.size());
        }
        std::vector<uint8_t> buf2;
        if (mpq.ReadFile("Units\\DestructableFunc.txt", buf2)) {
            skinTbl.Load(buf2.data(), buf2.size());
        }
    }

    ApplyMapDestructableObjectMods(mpq, mapPath, slk, skinTbl);

    int idColumn = FindFirstColumn(slk, {"unitID", "destructableID", "doodID"});
    const int nameColumn = FindFirstColumn(slk, {"Name"});
    const int fileColumn = FindFirstColumn(slk, {"file"});
    const int maxLifeColumn = FindFirstColumn(slk, {"HP"});
    const int armorColumn = FindFirstColumn(slk, {"def"});
    const int armorTypeColumn = FindFirstColumn(slk, {"armor", "defType"});
    const int canDropItemsColumn = FindFirstColumn(slk, {"canDropItems"});
    const int isTreeColumn = FindFirstColumn(slk, {"isTree"});
    const int isWallColumn = FindFirstColumn(slk, {"isWall"});
    const int isBlightColumn = FindFirstColumn(slk, {"isBlight"});
    const int isVisibleColumn = FindFirstColumn(slk, {"isVisible"});
    const int goldContentColumn = FindFirstColumn(slk, {"goldContent"});
    const int deathEffectColumn = FindFirstColumn(slk, {"deathEffect"});
    const int deathSoundColumn = FindFirstColumn(slk, {"deathSnd"});
    const int deathModelColumn = FindFirstColumn(slk, {"deathModel"});

    if (idColumn < 0 && slk.Cols() > 0) {
        idColumn = 0;
    }

    const SlkAccessor destructableData(slk, {"unitID", "destructableID", "doodID"});
    std::set<std::string> ids;
    CollectDestructableIds(destructableData, skinTbl, ids);

    for (const std::string& sid : ids) {
        if (sid.size() != 4) continue;
        const auto rowIt = destructableData.rowsById.find(sid);
        const int row = (rowIt != destructableData.rowsById.end()) ? rowIt->second : -1;
        War3ID id = War3ID::FromString(sid);

        auto d = std::make_unique<CDestructableType>();
        d->id = id;
        d->name = GetMergedString(slk, row, nameColumn, skinTbl, sid, "Name", sid);
        d->modelPath = GetMergedString(slk, row, fileColumn, skinTbl, sid, "file");
        d->iconPath = skinTbl.GetString(sid, "Art", "");
        d->maxLife = GetMergedInt(slk, row, maxLifeColumn, skinTbl, sid, "HP", 100);
        d->armor = GetMergedInt(slk, row, armorColumn, skinTbl, sid, "def", 0);
        d->armorType = GetMergedString(slk, row, armorTypeColumn, skinTbl, sid, "armor",
                                       GetMergedString(slk, row, armorTypeColumn, skinTbl, sid, "defType", "medium"));
        d->canDropItems = GetMergedInt(slk, row, canDropItemsColumn, skinTbl, sid, "canDropItems", 0) != 0;
        d->isTree = GetMergedInt(slk, row, isTreeColumn, skinTbl, sid, "isTree", 0) != 0;
        d->isWall = GetMergedInt(slk, row, isWallColumn, skinTbl, sid, "isWall", 0) != 0;
        d->isBlight = GetMergedInt(slk, row, isBlightColumn, skinTbl, sid, "isBlight", 0) != 0;
        d->isVisible = GetMergedInt(slk, row, isVisibleColumn, skinTbl, sid, "isVisible", 1) != 0;
        d->goldContent = GetMergedInt(slk, row, goldContentColumn, skinTbl, sid, "goldContent", 0);
        d->deathEffect = GetMergedString(slk, row, deathEffectColumn, skinTbl, sid, "deathEffect", "");
        d->deathSound = GetMergedString(slk, row, deathSoundColumn, skinTbl, sid, "deathSnd", "");
        d->deathModel = GetMergedString(slk, row, deathModelColumn, skinTbl, sid, "deathModel", "");

        data.destructables_[id] = std::move(d);
    }

    std::cout << "CDestructableData: loaded " << data.destructables_.size() << " destructables" << std::endl;
    return data;
}

bool CDestructableData::HasDestructable(War3ID id) const {
    return destructables_.find(id) != destructables_.end();
}

const CDestructableType* CDestructableData::GetDestructable(War3ID id) const {
    auto it = destructables_.find(id);
    return (it != destructables_.end()) ? it->second.get() : nullptr;
}

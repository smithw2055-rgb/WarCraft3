#include "itemdata.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <random>
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

std::string TrimCopy(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string LowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

CItemClass ParseItemClass(const std::string& classification) {
    const std::string value = LowerCopy(TrimCopy(classification));
    if (value == "permanent") return CItemClass::PERMANENT;
    if (value == "charged") return CItemClass::CHARGED;
    if (value == "powerup") return CItemClass::POWERUP;
    if (value == "artifact") return CItemClass::ARTIFACT;
    if (value == "purchasable") return CItemClass::PURCHASABLE;
    if (value == "campaign") return CItemClass::CAMPAIGN;
    if (value == "miscellaneous") return CItemClass::MISCELLANEOUS;
    if (value == "unknown") return CItemClass::UNKNOWN;
    if (value == "any") return CItemClass::ANY;
    return CItemClass::UNKNOWN;
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

void ApplyMapItemObjectMods(const MpqManager& mpq, const std::string& mapPath,
                            const SlkTable& itemSlk,
                            TxtTable& overlay) {
    if (mapPath.empty()) return;

    ObjectModificationSet mods;
    if (!LoadWar3MapItemMods(mapPath, mods)) {
        std::cout << "ObjectMods: no war3map.w3t item modifications" << std::endl;
        return;
    }

    SlkTable metaSlk;
    std::vector<uint8_t> metaBuf;
    if (!mpq.ReadFile("Units\\UnitMetaData.slk", metaBuf) ||
        !metaSlk.Load(metaBuf.data(), metaBuf.size())) {
        std::cerr << "ObjectMods: UnitMetaData.slk unavailable; item modifications skipped" << std::endl;
        return;
    }

    const SlkAccessor meta(metaSlk, {"ID"});
    const SlkAccessor itemData(itemSlk, {"itemID", "unitID", "ID"});
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
        if (CopyObjectDataSection(overlay, itemData, oldSection, newSection) == 0) {
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

    std::cout << "ObjectMods: war3map.w3t original=" << mods.original.size()
              << " custom=" << mods.custom.size()
              << " applied=" << applied
              << " unknown=" << unknown
              << " customCloned=" << customCloned
              << " customSkipped=" << customSkipped << std::endl;
}

void CollectItemIds(const SlkAccessor& slk, const TxtTable& txt, std::set<std::string>& outIds) {
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

void ParseWar3IdList(const std::string& str, std::vector<War3ID>& out) {
    size_t start = 0;
    while (start < str.size()) {
        size_t end = str.find(',', start);
        if (end == std::string::npos) {
            end = str.size();
        }
        std::string token = str.substr(start, end - start);
        token = TrimCopy(token);
        if (token.size() == 4) {
            out.push_back(War3ID::FromString(token));
        }
        start = end + 1;
    }
}

}

CItemData CItemData::LoadFromMpq(const MpqManager& mpq) {
    return LoadFromMpq(mpq, std::string());
}

CItemData CItemData::LoadFromMpq(const MpqManager& mpq, const std::string& mapPath) {
    CItemData data;

    SlkTable itemSlk;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\ItemData.slk", buf)) {
            itemSlk.Load(buf.data(), buf.size());
        }
    }

    TxtTable skinTbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\ItemFunc.txt", buf)) {
            skinTbl.Load(buf.data(), buf.size());
        }
        std::vector<uint8_t> buf2;
        if (mpq.ReadFile("Units\\ItemStrings.txt", buf2)) {
            skinTbl.Load(buf2.data(), buf2.size());
        }
        std::vector<uint8_t> buf3;
        if (mpq.ReadFile("Units\\ItemSkin.txt", buf3)) {
            skinTbl.Load(buf3.data(), buf3.size());
        }
    }

    ApplyMapItemObjectMods(mpq, mapPath, itemSlk, skinTbl);

    const int nameColumn = FindFirstColumn(itemSlk, {"Name"});
    const int fileColumn = FindFirstColumn(itemSlk, {"file"});
    const int artColumn = FindFirstColumn(itemSlk, {"Art"});
    const int abilityListColumn = FindFirstColumn(itemSlk, {"abilList"});
    const int goldColumn = FindFirstColumn(itemSlk, {"goldcost", "goldCost"});
    const int lumberColumn = FindFirstColumn(itemSlk, {"lumbercost", "lumberCost"});
    const int maxLifeColumn = FindFirstColumn(itemSlk, {"HP"});
    const int levelColumn = FindFirstColumn(itemSlk, {"Level", "level"});
    const int classColumn = FindFirstColumn(itemSlk, {"class"});
    const int pickRandomColumn = FindFirstColumn(itemSlk, {"pickRandom"});
    const int chargesColumn = FindFirstColumn(itemSlk, {"uses", "charges"});
    const int perishableColumn = FindFirstColumn(itemSlk, {"perishable"});
    const int powerupColumn = FindFirstColumn(itemSlk, {"powerup"});
    const int useOnlyColumn = FindFirstColumn(itemSlk, {"usable", "useOnly"});
    const int ignoreCooldownColumn = FindFirstColumn(itemSlk, {"ignoreCD"});
    const int droppableColumn = FindFirstColumn(itemSlk, {"droppable"});
    const int pawnableColumn = FindFirstColumn(itemSlk, {"pawnable"});
    const int dropOnDeathColumn = FindFirstColumn(itemSlk, {"drop"});
    const int sellableColumn = FindFirstColumn(itemSlk, {"sellable"});
    const int stockColumn = FindFirstColumn(itemSlk, {"stock"});
    const int stockRegenColumn = FindFirstColumn(itemSlk, {"stockRegen"});
    const int stockMaxColumn = FindFirstColumn(itemSlk, {"stockMax"});
    const int cooldownColumn = FindFirstColumn(itemSlk, {"cooldownID", "coolID"});

    const SlkAccessor itemData(itemSlk, {"itemID", "unitID", "ID"});
    std::set<std::string> ids;
    CollectItemIds(itemData, skinTbl, ids);

    for (const std::string& sid : ids) {
        if (sid.size() != 4) continue;
        const auto rowIt = itemData.rowsById.find(sid);
        const int row = (rowIt != itemData.rowsById.end()) ? rowIt->second : -1;
        War3ID id = War3ID::FromString(sid);

        auto item = std::make_unique<CItemType>();
        item->id = id;
        item->name = GetMergedString(itemSlk, row, nameColumn, skinTbl, sid, "Name", sid);
        item->legacyName = skinTbl.GetString(sid, "LegacyName");
        if (item->legacyName.empty()) item->legacyName = item->name;
        item->modelPath = GetMergedString(itemSlk, row, fileColumn, skinTbl, sid, "file");
        item->iconPath = GetMergedString(itemSlk, row, artColumn, skinTbl, sid, "Art");
        ParseWar3IdList(GetMergedString(itemSlk, row, abilityListColumn, skinTbl, sid, "abilList"),
                        item->abilityList);
        item->goldCost = GetMergedInt(itemSlk, row, goldColumn, skinTbl, sid, "goldCost", 0);
        item->lumberCost = GetMergedInt(itemSlk, row, lumberColumn, skinTbl, sid, "lumberCost", 0);
        item->maxLife = GetMergedInt(itemSlk, row, maxLifeColumn, skinTbl, sid, "HP", 50);
        item->level = GetMergedInt(itemSlk, row, levelColumn, skinTbl, sid, "Level", 0);
        item->itemClass = ParseItemClass(GetMergedString(itemSlk, row, classColumn, skinTbl, sid, "class"));
        item->includeAsRandomChoice =
            GetMergedInt(itemSlk, row, pickRandomColumn, skinTbl, sid, "pickRandom", 0) != 0;
        item->charges = GetMergedInt(itemSlk, row, chargesColumn, skinTbl, sid, "charges", 0);
        item->perishable = GetMergedInt(itemSlk, row, perishableColumn, skinTbl, sid, "perishable", 0) != 0;
        item->powerup = GetMergedInt(itemSlk, row, powerupColumn, skinTbl, sid, "powerup", 0) != 0;
        item->useOnly = GetMergedInt(itemSlk, row, useOnlyColumn, skinTbl, sid, "useOnly", 0) != 0;
        item->ignoreCooldown = GetMergedInt(itemSlk, row, ignoreCooldownColumn, skinTbl, sid, "ignoreCD", 0) != 0;
        item->droppable = GetMergedInt(itemSlk, row, droppableColumn, skinTbl, sid, "droppable", 0) != 0;
        item->pawnable = GetMergedInt(itemSlk, row, pawnableColumn, skinTbl, sid, "pawnable", 0) != 0;
        item->dropOnDeath = GetMergedInt(itemSlk, row, dropOnDeathColumn, skinTbl, sid, "drop", 0) != 0;
        item->sellable = GetMergedInt(itemSlk, row, sellableColumn, skinTbl, sid, "sellable", 0) != 0;
        item->stockable = GetMergedInt(itemSlk, row, stockColumn, skinTbl, sid, "stock", 0) != 0;
        item->stockRegen = GetMergedInt(itemSlk, row, stockRegenColumn, skinTbl, sid, "stockRegen", 0);
        item->stockMax = GetMergedInt(itemSlk, row, stockMaxColumn, skinTbl, sid, "stockMax", 0);
        item->cooldownID = GetMergedInt(itemSlk, row, cooldownColumn, skinTbl, sid, "coolID", 0);

        if (item->includeAsRandomChoice) {
            auto& choices = data.levelToRandomChoices_[item->level];
            choices.unclassifiedItems.push_back(id);
            choices.classificationToItems[(int)item->itemClass].push_back(id);
        }

        data.items_[id] = std::move(item);
    }

    std::cout << "CItemData: loaded " << data.items_.size() << " items" << std::endl;
    return data;
}

bool CItemData::HasItem(War3ID id) const {
    return items_.find(id) != items_.end();
}

const CItemType* CItemData::GetItem(War3ID id) const {
    auto it = items_.find(id);
    return (it != items_.end()) ? it->second.get() : nullptr;
}

War3ID CItemData::ChooseRandomItem(int level, std::mt19937& rng) const {
    auto it = levelToRandomChoices_.find(level);
    if (it == levelToRandomChoices_.end() || it->second.unclassifiedItems.empty()) {
        return War3ID();
    }
    const auto& items = it->second.unclassifiedItems;
    const size_t index = std::uniform_int_distribution<size_t>(0, items.size() - 1)(rng);
    return items[index];
}

War3ID CItemData::ChooseRandomItem(CItemClass itemClass, int level, std::mt19937& rng) const {
    auto it = levelToRandomChoices_.find(level);
    if (it == levelToRandomChoices_.end()) {
        std::cerr << "chooseRandomItem: no item set: " << level << std::endl;
        return War3ID();
    }
    auto classIt = it->second.classificationToItems.find((int)itemClass);
    if (classIt == it->second.classificationToItems.end() || classIt->second.empty()) {
        std::cerr << "chooseRandomItem: no items of class: " << (int)itemClass
                  << " " << level << std::endl;
        return War3ID();
    }
    const auto& items = classIt->second;
    const size_t index = std::uniform_int_distribution<size_t>(0, items.size() - 1)(rng);
    return items[index];
}

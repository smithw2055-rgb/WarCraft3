#include "itemdata.h"
#include "resource/mpq_storm.h"
#include <iostream>

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

}

CItemData CItemData::LoadFromMpq(const MpqManager& mpq) {
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

    int idColumn = FindFirstColumn(itemSlk, {"itemID", "unitID", "ID"});
    if (idColumn < 0 && itemSlk.Cols() > 0) {
        idColumn = 0;
    }
    const int nameColumn = FindFirstColumn(itemSlk, {"Name"});
    const int fileColumn = FindFirstColumn(itemSlk, {"file"});
    const int artColumn = FindFirstColumn(itemSlk, {"Art"});
    const int goldColumn = FindFirstColumn(itemSlk, {"goldcost", "goldCost"});
    const int lumberColumn = FindFirstColumn(itemSlk, {"lumbercost", "lumberCost"});
    const int levelColumn = FindFirstColumn(itemSlk, {"Level", "level"});
    const int chargesColumn = FindFirstColumn(itemSlk, {"uses", "charges"});
    const int perishableColumn = FindFirstColumn(itemSlk, {"perishable"});
    const int powerupColumn = FindFirstColumn(itemSlk, {"powerup"});
    const int useOnlyColumn = FindFirstColumn(itemSlk, {"usable", "useOnly"});
    const int stockColumn = FindFirstColumn(itemSlk, {"stock"});
    const int stockRegenColumn = FindFirstColumn(itemSlk, {"stockRegen"});
    const int stockMaxColumn = FindFirstColumn(itemSlk, {"stockMax"});
    const int cooldownColumn = FindFirstColumn(itemSlk, {"cooldownID", "coolID"});

    for (int row = 0; row < itemSlk.Rows(); ++row) {
        const std::string sid = itemSlk.GetString(row, idColumn, "");
        if (sid.size() != 4) continue;
        War3ID id = War3ID::FromString(sid);

        auto item = std::make_unique<CItemType>();
        item->id = id;
        item->name = GetMergedString(itemSlk, row, nameColumn, skinTbl, sid, "Name", sid);
        item->legacyName = skinTbl.GetString(sid, "LegacyName");
        if (item->legacyName.empty()) item->legacyName = item->name;
        item->modelPath = GetMergedString(itemSlk, row, fileColumn, skinTbl, sid, "file");
        item->iconPath = GetMergedString(itemSlk, row, artColumn, skinTbl, sid, "Art");
        item->goldCost = GetMergedInt(itemSlk, row, goldColumn, skinTbl, sid, "goldCost", 0);
        item->lumberCost = GetMergedInt(itemSlk, row, lumberColumn, skinTbl, sid, "lumberCost", 0);
        item->level = GetMergedInt(itemSlk, row, levelColumn, skinTbl, sid, "level", 0);
        item->charges = GetMergedInt(itemSlk, row, chargesColumn, skinTbl, sid, "charges", 0);
        item->perishable = GetMergedInt(itemSlk, row, perishableColumn, skinTbl, sid, "perishable", 0) != 0;
        item->powerup = GetMergedInt(itemSlk, row, powerupColumn, skinTbl, sid, "powerup", 0) != 0;
        item->useOnly = GetMergedInt(itemSlk, row, useOnlyColumn, skinTbl, sid, "useOnly", 0) != 0;
        item->stockable = GetMergedInt(itemSlk, row, stockColumn, skinTbl, sid, "stock", 0) != 0;
        item->stockRegen = GetMergedInt(itemSlk, row, stockRegenColumn, skinTbl, sid, "stockRegen", 0);
        item->stockMax = GetMergedInt(itemSlk, row, stockMaxColumn, skinTbl, sid, "stockMax", 0);
        item->cooldownID = GetMergedInt(itemSlk, row, cooldownColumn, skinTbl, sid, "coolID", 0);

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

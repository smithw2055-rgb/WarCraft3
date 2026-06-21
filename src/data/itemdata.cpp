#include "itemdata.h"
#include "resource/mpq_storm.h"
#include <iostream>

CItemData CItemData::LoadFromMpq(const MpqManager& mpq) {
    CItemData data;

    TxtTable itemTbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\ItemData.slk", buf)) {
            itemTbl.Load(buf.data(), buf.size());
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

    auto sections = itemTbl.Sections();
    for (auto& sid : sections) {
        if (sid.size() != 4) continue;
        War3ID id = War3ID::FromString(sid);

        auto item = std::make_unique<CItemType>();
        item->id = id;
        item->name = itemTbl.GetString(sid, "Name", sid);
        item->legacyName = itemTbl.GetString(sid, "LegacyName");
        if (item->legacyName.empty()) item->legacyName = item->name;
        item->iconPath = itemTbl.GetString(sid, "Art");
        item->goldCost = itemTbl.GetInt(sid, "goldCost", 0);
        item->lumberCost = itemTbl.GetInt(sid, "lumberCost", 0);
        item->level = itemTbl.GetInt(sid, "level", 0);
        item->charges = itemTbl.GetInt(sid, "charges", 0);
        item->perishable = itemTbl.GetInt(sid, "perishable", 0) != 0;
        item->powerup = itemTbl.GetInt(sid, "powerup", 0) != 0;
        item->useOnly = itemTbl.GetInt(sid, "useOnly", 0) != 0;
        item->stockable = itemTbl.GetInt(sid, "stock", 0) != 0;
        item->stockRegen = itemTbl.GetInt(sid, "stockRegen", 0);
        item->stockMax = itemTbl.GetInt(sid, "stockMax", 0);
        item->cooldownID = itemTbl.GetInt(sid, "coolID", 0);

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

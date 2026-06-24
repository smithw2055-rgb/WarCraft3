#include "destructabledata.h"
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

CDestructableData CDestructableData::LoadFromMpq(const MpqManager& mpq) {
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

    for (int row = 0; row < slk.Rows(); ++row) {
        const std::string sid = slk.GetString(row, idColumn, "");
        if (sid.size() != 4) continue;
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

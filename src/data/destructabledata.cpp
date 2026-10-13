#include "destructabledata.h"
#include "data/slk_merge.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map>

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
        const int pathTexColumn = FindFirstColumn(slk, {"pathTex"});
        d->pathTex = GetMergedString(slk, row, pathTexColumn, skinTbl, sid, "pathTex", "");
        const int pathTexDeathColumn = FindFirstColumn(slk, {"pathTexDeath"});
        d->pathTexDeath = GetMergedString(slk, row, pathTexDeathColumn, skinTbl, sid, "pathTexDeath", "");

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

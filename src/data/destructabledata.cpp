#include "destructabledata.h"
#include "data/slk_merge.h"
#include "map/object_mods.h"
#include "map/path_tex.h"
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
    const int targTypeColumn = FindFirstColumn(slk, {"targType", "btar"});
    const int canDropItemsColumn = FindFirstColumn(slk, {"canDropItems"});
    const int isTreeColumn = FindFirstColumn(slk, {"isTree"});
    const int isWallColumn = FindFirstColumn(slk, {"isWall"});
    const int isBlightColumn = FindFirstColumn(slk, {"isBlight"});
    const int isVisibleColumn = FindFirstColumn(slk, {"isVisible"});
    const int goldContentColumn = FindFirstColumn(slk, {"goldContent"});
    const int deathEffectColumn = FindFirstColumn(slk, {"deathEffect"});
    const int deathSoundColumn = FindFirstColumn(slk, {"deathSnd"});
    const int deathModelColumn = FindFirstColumn(slk, {"deathModel"});
    const int occHColumn = FindFirstColumn(slk, {"occH", "occlusionHeight"});

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
        // Retail targType (ref CDestructableData TARGETED_AS): comma-separated
        // target names, mixed case ("tree", "debris,vulnerable"). Unknown
        // tokens are skipped so custom object data never breaks the load.
        {
            const std::string raw = GetMergedString(slk, row, targTypeColumn, skinTbl, sid, "targType",
                                                    GetMergedString(slk, row, targTypeColumn, skinTbl, sid, "btar", ""));
            std::string token;
            for (size_t i = 0; i <= raw.size(); ++i) {
                const char c = (i < raw.size()) ? raw[i] : ',';
                if (c == ',') {
                    const std::string key = TrimCopy(token);
                    token.clear();
                    if (key.empty()) continue;
                    CTargetType parsed = CTargetType::GROUND;
                    if (CTargetTypeInfo::FromString(key, parsed)) d->targetedAs.insert(parsed);
                } else {
                    token.push_back(c);
                }
            }
        }
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
        {
            const std::string occ = GetMergedString(slk, row, occHColumn, skinTbl, sid, "occH", "0");
            try { d->occlusionHeight = occ.empty() ? 0.0f : std::stof(occ); }
            catch (...) { d->occlusionHeight = 0.0f; }
        }

        data.destructables_[id] = std::move(d);
    }

    // Decode footprint textures once (ref War3MapViewer destructable
    // pathing): placement and WPM stamps never touch the MPQ at runtime.
    for (const auto& [id, type] : data.destructables_) {
        (void)id;
        if (type) {
            if (!type->pathTex.empty()) type->pathTexImage = LoadPathTex(mpq, type->pathTex);
            if (!type->pathTexDeath.empty()) type->pathTexDeathImage = LoadPathTex(mpq, type->pathTexDeath);
        }
    }

    std::cout << "CDestructableData: loaded " << data.destructables_.size() << " destructables" << std::endl;
    return data;
}

bool CDestructableData::HasDestructable(War3ID id) const {
    return destructables_.find(id) != destructables_.end();
}

void CDestructableData::RegisterDestructable(War3ID id, std::unique_ptr<CDestructableType> type) {
    if (!type) return;
    destructables_[id] = std::move(type);
}

const CDestructableType* CDestructableData::GetDestructable(War3ID id) const {
    auto it = destructables_.find(id);
    return (it != destructables_.end()) ? it->second.get() : nullptr;
}

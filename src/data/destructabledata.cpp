#include "destructabledata.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map>

#include "data/slk_merge.h"
#include "engine/resource/table_schema_binder.h"
#include "map/object_mods.h"
#include "map/path_tex.h"
#include "resource/mpq_storm.h"

namespace {

const engine::resource::TableSchemaBinder<CDestructableType>& GetDestructableSchemaBinder() {
    static const auto kSchema = []() {
        engine::resource::TableSchemaBinder<CDestructableType> binder;
        binder.BindString("file", &CDestructableType::modelPath, "");
        binder.BindInt("HP", &CDestructableType::maxLife, 100);
        binder.BindInt("def", &CDestructableType::armor, 0);
        binder.BindString("armor", &CDestructableType::armorType, "medium", "defType");
        binder.BindBool("canDropItems", &CDestructableType::canDropItems, false);
        binder.BindBool("isTree", &CDestructableType::isTree, false);
        binder.BindBool("isWall", &CDestructableType::isWall, false);
        binder.BindBool("isBlight", &CDestructableType::isBlight, false);
        binder.BindBool("isVisible", &CDestructableType::isVisible, true);
        binder.BindInt("goldContent", &CDestructableType::goldContent, 0);
        binder.BindString("deathEffect", &CDestructableType::deathEffect, "");
        binder.BindString("deathSnd", &CDestructableType::deathSound, "");
        binder.BindString("deathModel", &CDestructableType::deathModel, "");
        binder.BindString("pathTex", &CDestructableType::pathTex, "");
        binder.BindString("pathTexDeath", &CDestructableType::pathTexDeath, "");
        binder.BindFloat("occH", &CDestructableType::occlusionHeight, 0.0f, "occlusionHeight");
        return binder;
    }();
    return kSchema;
}

} // namespace

CDestructableData CDestructableData::LoadFromMpq(const MpqManager& mpq,
                                                 const std::string& mapPath) {
    CDestructableData data;

    SlkTable slk;
    LoadSlkTableFromMpq(mpq, "Units\\DestructableData.slk", slk);

    TxtTable skinTbl;
    LoadTxtOverlaysFromMpq(mpq, {
        "Units\\DestructableSkin.txt",
        "Units\\DestructableFunc.txt",
    }, skinTbl);

    ApplyMapDestructableObjectMods(mpq, mapPath, slk, skinTbl);

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
    const int pathTexColumn = FindFirstColumn(slk, {"pathTex"});
    const int pathTexDeathColumn = FindFirstColumn(slk, {"pathTexDeath"});
    const int occHColumn = FindFirstColumn(slk, {"occH", "occlusionHeight"});

    const SlkAccessor destructableData(slk, {"unitID", "destructableID", "doodID"});
    std::set<std::string> ids;
    CollectDestructableIds(destructableData, skinTbl, ids);

    const auto& schema = GetDestructableSchemaBinder();
    struct DestructableRowScratch {
        std::string file;
        std::string hp;
        std::string def;
        std::string armor;
        std::string defType;
        std::string canDropItems;
        std::string isTree;
        std::string isWall;
        std::string isBlight;
        std::string isVisible;
        std::string goldContent;
        std::string deathEffect;
        std::string deathSnd;
        std::string deathModel;
        std::string pathTex;
        std::string pathTexDeath;
        std::string occH;
    } rowScratch;

    for (const std::string& sid : ids) {
        if (sid.size() != 4) continue;
        const auto rowIt = destructableData.rowsById.find(sid);
        const int row = (rowIt != destructableData.rowsById.end()) ? rowIt->second : -1;
        const War3ID id = War3ID::FromString(sid);

        rowScratch.file = GetMergedString(slk, row, fileColumn, skinTbl, sid, "file");
        rowScratch.hp = GetMergedString(slk, row, maxLifeColumn, skinTbl, sid, "HP", "100");
        rowScratch.def = GetMergedString(slk, row, armorColumn, skinTbl, sid, "def", "0");
        rowScratch.armor = GetMergedString(slk, row, armorTypeColumn, skinTbl, sid, "armor");
        rowScratch.defType = GetMergedString(slk, row, armorTypeColumn, skinTbl, sid, "defType");
        rowScratch.canDropItems = GetMergedString(slk, row, canDropItemsColumn, skinTbl, sid, "canDropItems", "0");
        rowScratch.isTree = GetMergedString(slk, row, isTreeColumn, skinTbl, sid, "isTree", "0");
        rowScratch.isWall = GetMergedString(slk, row, isWallColumn, skinTbl, sid, "isWall", "0");
        rowScratch.isBlight = GetMergedString(slk, row, isBlightColumn, skinTbl, sid, "isBlight", "0");
        rowScratch.isVisible = GetMergedString(slk, row, isVisibleColumn, skinTbl, sid, "isVisible", "1");
        rowScratch.goldContent = GetMergedString(slk, row, goldContentColumn, skinTbl, sid, "goldContent", "0");
        rowScratch.deathEffect = GetMergedString(slk, row, deathEffectColumn, skinTbl, sid, "deathEffect", "");
        rowScratch.deathSnd = GetMergedString(slk, row, deathSoundColumn, skinTbl, sid, "deathSnd", "");
        rowScratch.deathModel = GetMergedString(slk, row, deathModelColumn, skinTbl, sid, "deathModel", "");
        rowScratch.pathTex = GetMergedString(slk, row, pathTexColumn, skinTbl, sid, "pathTex", "");
        rowScratch.pathTexDeath = GetMergedString(slk, row, pathTexDeathColumn, skinTbl, sid, "pathTexDeath", "");
        rowScratch.occH = GetMergedString(slk, row, occHColumn, skinTbl, sid, "occH", "0");

        auto d = std::make_unique<CDestructableType>();
        d->id = id;
        d->name = GetMergedString(slk, row, nameColumn, skinTbl, sid, "Name", sid);
        d->iconPath = skinTbl.GetString(sid, "Art", "");

        schema.HydrateRow(*d, [&](std::string_view col) -> std::string_view {
            if (col == "file") return rowScratch.file;
            if (col == "HP") return rowScratch.hp;
            if (col == "def") return rowScratch.def;
            if (col == "armor") return rowScratch.armor;
            if (col == "defType") return rowScratch.defType;
            if (col == "canDropItems") return rowScratch.canDropItems;
            if (col == "isTree") return rowScratch.isTree;
            if (col == "isWall") return rowScratch.isWall;
            if (col == "isBlight") return rowScratch.isBlight;
            if (col == "isVisible") return rowScratch.isVisible;
            if (col == "goldContent") return rowScratch.goldContent;
            if (col == "deathEffect") return rowScratch.deathEffect;
            if (col == "deathSnd") return rowScratch.deathSnd;
            if (col == "deathModel") return rowScratch.deathModel;
            if (col == "pathTex") return rowScratch.pathTex;
            if (col == "pathTexDeath") return rowScratch.pathTexDeath;
            if (col == "occH" || col == "occlusionHeight") return rowScratch.occH;
            return std::string_view{};
        });

        // Retail targType (ref CDestructableData TARGETED_AS): comma-separated
        // target names, mixed case ("tree", "debris,vulnerable"). Unknown
        // tokens are skipped so custom object data never breaks the load.
        const std::string raw = GetMergedString(
            slk, row, targTypeColumn, skinTbl, sid, "targType",
            GetMergedString(slk, row, targTypeColumn, skinTbl, sid, "btar", ""));
        std::string token;
        for (size_t i = 0; i <= raw.size(); ++i) {
            const char c = (i < raw.size()) ? raw[i] : ',';
            if (c == ',') {
                const std::string key = TrimCopy(token);
                token.clear();
                if (key.empty()) continue;
                CTargetType parsed = CTargetType::GROUND;
                if (CTargetTypeInfo::FromString(key, parsed)) {
                    d->targetedAs.insert(parsed);
                }
            } else {
                token.push_back(c);
            }
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

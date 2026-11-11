#include "doodaddata.h"

#include <iostream>
#include <set>
#include <string>
#include <unordered_map>

#include "data/slk_merge.h"
#include "engine/resource/table_schema_binder.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"

namespace {

const engine::resource::TableSchemaBinder<CDoodadType>& GetDoodadSchemaBinder() {
    static const auto kSchema = []() {
        engine::resource::TableSchemaBinder<CDoodadType> binder;
        binder.BindString("file", &CDoodadType::file, "");
        binder.BindBool("walkable", &CDoodadType::walkable, false);
        binder.BindString("pathTex", &CDoodadType::pathTex, "");
        binder.BindBool("selectable", &CDoodadType::selectable, false);
        binder.BindString("doodClass", &CDoodadType::doodClass, "");
        return binder;
    }();
    return kSchema;
}

} // namespace

CDoodadData CDoodadData::LoadFromMpq(const MpqManager& mpq,
                                     const std::string& mapPath) {
    CDoodadData data;

    SlkTable doodadSlk;
    LoadSlkTableFromMpq(mpq, "Doodads\\Doodads.slk", doodadSlk);
    SlkTable destSlk;
    LoadSlkTableFromMpq(mpq, "Units\\DestructableData.slk", destSlk);

    TxtTable skinTbl;
    LoadTxtOverlaysFromMpq(mpq, {
        "Doodads\\DoodadSkins.txt",
        "Units\\DestructableSkin.txt",
        "Units\\DestructableFunc.txt",
    }, skinTbl);

    // Doodad object changeset (war3map.w3d); destructable mods (w3b) also
    // apply since doodads share the destructable namespace in-game.
    // Overlays apply to both base tables' accessor below.
    ApplyMapDoodadObjectMods(mpq, mapPath, doodadSlk.Rows() > 0 ? doodadSlk : destSlk, skinTbl);
    ApplyMapDestructableObjectMods(mpq, mapPath, destSlk.Rows() > 0 ? destSlk : doodadSlk, skinTbl);

    const SlkAccessor doodadData(doodadSlk, {"doodID", "unitID", "destructableID"});
    const SlkAccessor destData(destSlk, {"unitID", "destructableID", "doodID"});
    std::set<std::string> ids;
    CollectDestructableIds(doodadData, skinTbl, ids);
    CollectDestructableIds(destData, skinTbl, ids);

    auto colOf = [](const SlkTable& slk, const char* name) -> int {
        return slk.FindColumn(name);
    };

    const auto& schema = GetDoodadSchemaBinder();
    struct DoodadRowScratch {
        std::string file;
        std::string walkable;
        std::string pathTex;
        std::string selectable;
        std::string doodClass;
    } rowScratch;

    for (const std::string& sid : ids) {
        if (sid.size() != 4) continue;
        // Prefer the dedicated doodad row; fall back to destructable row.
        const SlkTable* srcSlk = nullptr;
        int row = -1;
        auto dit = doodadData.rowsById.find(sid);
        if (dit != doodadData.rowsById.end()) {
            srcSlk = &doodadSlk;
            row = dit->second;
        } else {
            auto eit = destData.rowsById.find(sid);
            if (eit != destData.rowsById.end()) {
                srcSlk = &destSlk;
                row = eit->second;
            }
        }
        // Skin-only sections (custom mods with no base row): read overlay.
        if (!srcSlk) {
            srcSlk = doodadSlk.Rows() > 0 ? &doodadSlk : &destSlk;
            row = -1;
        }

        rowScratch.file = GetMergedString(*srcSlk, row, colOf(*srcSlk, "file"), skinTbl, sid, "file");
        rowScratch.walkable = GetMergedString(*srcSlk, row, colOf(*srcSlk, "walkable"), skinTbl, sid, "walkable");
        rowScratch.pathTex = GetMergedString(*srcSlk, row, colOf(*srcSlk, "pathTex"), skinTbl, sid, "pathTex");
        rowScratch.selectable = GetMergedString(*srcSlk, row, colOf(*srcSlk, "selectable"), skinTbl, sid, "selectable");
        rowScratch.doodClass = GetMergedString(*srcSlk, row, colOf(*srcSlk, "doodClass"), skinTbl, sid, "doodClass");

        auto d = std::make_unique<CDoodadType>();
        d->id = War3ID::FromString(sid);
        d->name = GetMergedString(*srcSlk, row, colOf(*srcSlk, "Name"), skinTbl, sid, "Name", sid);
        schema.HydrateRow(*d, [&](std::string_view col) -> std::string_view {
            if (col == "file") return rowScratch.file;
            if (col == "walkable") return rowScratch.walkable;
            if (col == "pathTex") return rowScratch.pathTex;
            if (col == "selectable") return rowScratch.selectable;
            if (col == "doodClass") return rowScratch.doodClass;
            return std::string_view{};
        });

        data.doodads_[d->id] = std::move(d);
    }

    std::cout << "CDoodadData: loaded " << data.doodads_.size() << " doodads" << std::endl;
    return data;
}

bool CDoodadData::HasDoodad(War3ID id) const {
    return doodads_.find(id) != doodads_.end();
}

const CDoodadType* CDoodadData::GetDoodad(War3ID id) const {
    auto it = doodads_.find(id);
    return (it != doodads_.end()) ? it->second.get() : nullptr;
}

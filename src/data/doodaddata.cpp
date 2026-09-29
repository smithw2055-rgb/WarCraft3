#include "doodaddata.h"
#include "data/slk_merge.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <iostream>
#include <set>

CDoodadData CDoodadData::LoadFromMpq(const MpqManager& mpq,
                                     const std::string& mapPath) {
    CDoodadData data;

    SlkTable doodadSlk;
    {
        // Dedicated doodad table (ref StandardObjectData Doodads.slk).
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Doodads\\Doodads.slk", buf)) {
            doodadSlk.Load(buf.data(), buf.size());
        }
    }
    SlkTable destSlk;
    {
        // Destructable table supplements: trees/bridges live here
        // (ATtr summer tree, ATt0 bridge), not in Doodads.slk.
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\DestructableData.slk", buf)) {
            destSlk.Load(buf.data(), buf.size());
        }
    }

    TxtTable skinTbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Doodads\\DoodadSkins.txt", buf)) {
            skinTbl.Load(buf.data(), buf.size());
        }
        std::vector<uint8_t> buf2;
        if (mpq.ReadFile("Units\\DestructableSkin.txt", buf2)) {
            skinTbl.Load(buf2.data(), buf2.size());
        }
        std::vector<uint8_t> buf3;
        if (mpq.ReadFile("Units\\DestructableFunc.txt", buf3)) {
            skinTbl.Load(buf3.data(), buf3.size());
        }
    }

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

        auto d = std::make_unique<CDoodadType>();
        d->id = War3ID::FromString(sid);
        d->name = GetMergedString(*srcSlk, row, colOf(*srcSlk, "Name"), skinTbl, sid, "Name", sid);
        d->file = GetMergedString(*srcSlk, row, colOf(*srcSlk, "file"), skinTbl, sid, "file");
        d->walkable = GetMergedInt(*srcSlk, row, colOf(*srcSlk, "walkable"), skinTbl, sid, "walkable", 0) != 0;
        d->pathTex = GetMergedString(*srcSlk, row, colOf(*srcSlk, "pathTex"), skinTbl, sid, "pathTex");
        d->selectable = GetMergedInt(*srcSlk, row, colOf(*srcSlk, "selectable"), skinTbl, sid, "selectable", 0) != 0;
        d->doodClass = GetMergedString(*srcSlk, row, colOf(*srcSlk, "doodClass"), skinTbl, sid, "doodClass");

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

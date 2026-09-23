#include "doodaddata.h"
#include "data/slk_merge.h"
#include "map/object_mods.h"
#include "resource/mpq_storm.h"
#include <iostream>
#include <set>

CDoodadData CDoodadData::LoadFromMpq(const MpqManager& mpq,
                                     const std::string& mapPath) {
    CDoodadData data;

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

    // Same w3b overlay as destructables: custom doodads clone retail rows.
    ApplyMapDestructableObjectMods(mpq, mapPath, slk, skinTbl);

    const int nameColumn = FindFirstColumn(slk, {"Name"});
    const int fileColumn = FindFirstColumn(slk, {"file"});
    const int walkableColumn = FindFirstColumn(slk, {"walkable"});
    const int pathTexColumn = FindFirstColumn(slk, {"pathTex"});
    const int selectableColumn = FindFirstColumn(slk, {"selectable"});
    const int doodClassColumn = FindFirstColumn(slk, {"doodClass"});

    const SlkAccessor doodadData(slk, {"unitID", "destructableID", "doodID"});
    std::set<std::string> ids;
    CollectDestructableIds(doodadData, skinTbl, ids);

    for (const std::string& sid : ids) {
        if (sid.size() != 4) continue;
        const auto rowIt = doodadData.rowsById.find(sid);
        const int row = (rowIt != doodadData.rowsById.end()) ? rowIt->second : -1;

        auto d = std::make_unique<CDoodadType>();
        d->id = War3ID::FromString(sid);
        d->name = GetMergedString(slk, row, nameColumn, skinTbl, sid, "Name", sid);
        d->file = GetMergedString(slk, row, fileColumn, skinTbl, sid, "file");
        d->walkable = GetMergedInt(slk, row, walkableColumn, skinTbl, sid, "walkable", 0) != 0;
        d->pathTex = GetMergedString(slk, row, pathTexColumn, skinTbl, sid, "pathTex");
        d->selectable = GetMergedInt(slk, row, selectableColumn, skinTbl, sid, "selectable", 0) != 0;
        d->doodClass = GetMergedString(slk, row, doodClassColumn, skinTbl, sid, "doodClass");

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

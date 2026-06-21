#include "destructabledata.h"
#include "resource/mpq_storm.h"
#include <iostream>

CDestructableData CDestructableData::LoadFromMpq(const MpqManager& mpq) {
    CDestructableData data;

    TxtTable tbl;
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\DestructableData.slk", buf)) {
            tbl.Load(buf.data(), buf.size());
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

    auto sections = tbl.Sections();
    for (auto& sid : sections) {
        if (sid.size() != 4) continue;
        War3ID id = War3ID::FromString(sid);

        auto d = std::make_unique<CDestructableType>();
        d->id = id;
        d->name = tbl.GetString(sid, "Name", sid);
        d->modelPath = tbl.GetString(sid, "file");
        d->iconPath = skinTbl.GetString(sid, "Art", "");
        d->maxLife = tbl.GetInt(sid, "HP", 100);
        d->armor = tbl.GetInt(sid, "def", 0);
        d->armorType = tbl.GetString(sid, "defType", "medium");
        d->canDropItems = tbl.GetInt(sid, "canDropItems", 0) != 0;
        d->isTree = tbl.GetInt(sid, "isTree", 0) != 0;
        d->isWall = tbl.GetInt(sid, "isWall", 0) != 0;
        d->isBlight = tbl.GetInt(sid, "isBlight", 0) != 0;
        d->isVisible = tbl.GetInt(sid, "isVisible", 1) != 0;
        d->goldContent = tbl.GetInt(sid, "goldContent", 0);
        d->deathEffect = tbl.GetString(sid, "deathEffect", "");
        d->deathSound = tbl.GetString(sid, "deathSnd", "");
        d->deathModel = tbl.GetString(sid, "deathModel", "");

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

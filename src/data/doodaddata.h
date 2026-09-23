#pragma once
// Doodad static data: the decorative half of Units\DestructableData.slk.
// Retail keeps no separate DoodadData.slk; doodads, trees, walls and
// bridges share the destructable table, distinguished by doodClass and
// by the interactive flags CDestructableData reads. This view exposes
// the doodad-relevant columns (model file, walkability, pathing texture,
// selectability) with the same map-mod merge semantics.
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include "resource/slk.h"

struct CDoodadType {
    War3ID id;
    std::string name;
    std::string file;
    bool walkable = false;
    std::string pathTex;
    bool selectable = false;
    std::string doodClass;
};

class CDoodadData {
public:
    CDoodadData() = default;

    bool HasDoodad(War3ID id) const;
    const CDoodadType* GetDoodad(War3ID id) const;
    size_t Count() const { return doodads_.size(); }

    static CDoodadData LoadFromMpq(const class MpqManager& mpq,
                                   const std::string& mapPath = "");

private:
    std::unordered_map<War3ID, std::unique_ptr<CDoodadType>> doodads_;
};

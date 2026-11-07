// Shared helpers for applying war3map object modifications (w3a/w3q/w3h)
// backed by the unified SlkAccessor & ResolveObjectModField in slk_merge.h.
#pragma once
#include <string>
#include <utility>

#include "data/slk_merge.h"
#include "map/object_mods.h"
#include "resource/slk.h"

struct ObjectMetaTable {
    SlkTable ownedSlk;
    SlkAccessor accessor;

    ObjectMetaTable() = default;

    ObjectMetaTable(const ObjectMetaTable& other)
        : ownedSlk(other.ownedSlk)
        , accessor(other.accessor) {
        RebindAccessor();
    }

    ObjectMetaTable& operator=(const ObjectMetaTable& other) {
        if (this != &other) {
            ownedSlk = other.ownedSlk;
            accessor = other.accessor;
            RebindAccessor();
        }
        return *this;
    }

    ObjectMetaTable(ObjectMetaTable&& other) noexcept
        : ownedSlk(std::move(other.ownedSlk))
        , accessor(std::move(other.accessor)) {
        RebindAccessor();
        other.accessor.table = nullptr;
    }

    ObjectMetaTable& operator=(ObjectMetaTable&& other) noexcept {
        if (this != &other) {
            ownedSlk = std::move(other.ownedSlk);
            accessor = std::move(other.accessor);
            RebindAccessor();
            other.accessor.table = nullptr;
        }
        return *this;
    }

    bool Load(const SlkTable& slk) {
        if (slk.FindColumn("ID") < 0) return false;
        ownedSlk = slk;
        accessor.Init(ownedSlk, {"ID"});
        return !accessor.rowsById.empty();
    }

    bool TryGet(const std::string& id, const std::string& key, std::string& out) const {
        return accessor.TryGetValue(id, key, out);
    }

private:
    void RebindAccessor() {
        accessor.table = (ownedSlk.Cols() > 0 || ownedSlk.Rows() > 0) ? &ownedSlk : nullptr;
    }
};

inline std::string FormatObjectModValue(const ObjectModification& mod) {
    return FormatObjectModificationValue(mod);
}

inline bool ResolveObjectModField(const ObjectMetaTable& meta, const ObjectModification& mod,
                                  std::string& outTable, std::string& outKey) {
    return ::ResolveObjectModField(meta.accessor, mod, outTable, outKey);
}

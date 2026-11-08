#include "data/slk_merge.h"
#include "resource/mpq_storm.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

std::string FormatObjectModificationValue(const ObjectModification& mod) {
    switch (mod.varType) {
    case ObjectModification::INT:
    case ObjectModification::BOOLEAN:
        return std::to_string(mod.intValue);
    case ObjectModification::STRING:
        return mod.stringValue;
    case ObjectModification::REAL:
    case ObjectModification::UNREAL:
    default: {
        std::ostringstream ss;
        ss << mod.realValue;
        return ss.str();
    }
    }
}

bool ResolveObjectModField(const SlkAccessor& meta, const ObjectModification& mod,
                           std::string& outSlk, std::string& outField) {
    const std::string metaId = mod.id.ToString();
    if (!meta.TryGetValue(metaId, "slk", outSlk) || outSlk.empty()) return false;
    if (!meta.TryGetValue(metaId, "field", outField) || outField.empty()) return false;

    int data = 0;
    std::string dataValue;
    if (meta.TryGetValue(metaId, "data", dataValue) && !dataValue.empty()) {
        data = std::atoi(dataValue.c_str());
    }
    if (data > 0) {
        outField.push_back(static_cast<char>('A' + data - 1));
    }

    int index = -1;
    std::string indexValue;
    if (meta.TryGetValue(metaId, "index", indexValue) && !indexValue.empty()) {
        index = std::atoi(indexValue.c_str());
    }

    int repeat = 0;
    std::string repeatValue;
    if (meta.TryGetValue(metaId, "repeat", repeatValue) && !repeatValue.empty()) {
        repeat = std::atoi(repeatValue.c_str());
    }

    std::string appendIndexMode;
    meta.TryGetValue(metaId, "appendIndex", appendIndexMode);
    int level = mod.level;
    if (repeat > 0) {
        if (appendIndexMode == "1") {
            const int upgradeExtensionLevel = level - 1;
            if (upgradeExtensionLevel > 0) {
                outField += std::to_string(upgradeExtensionLevel);
            }
        } else if (appendIndexMode == "0") {
            // Indexed list storage is collapsed into one comma field in this C++ data path.
        } else if (index == -1 || repeat >= 10) {
            if (level == 0) level = 1;
            if (repeat >= 10 && level >= 0 && level < 100) {
                if (level < 10) outField.push_back('0');
                outField += std::to_string(level);
            } else {
                outField += std::to_string(level);
            }
        }
    }
    return true;
}

bool ResolveObjectModField(const SlkAccessor& meta, const ObjectModification& mod,
                           std::string& outField) {
    std::string slkName;
    return ResolveObjectModField(meta, mod, slkName, outField);
}

int CopyObjectDataSection(TxtTable& txt, const SlkAccessor& slk,
                          const std::string& oldSection,
                          const std::string& newSection) {
    if (oldSection.empty() || newSection.empty()) return 0;

    int copied = 0;
    const auto rowIt = slk.rowsById.find(oldSection);
    if (slk.table && rowIt != slk.rowsById.end()) {
        const int row = rowIt->second;
        for (int col = 0; col < slk.table->Cols(); ++col) {
            if (col >= (int)slk.table->colNames_.size()) continue;
            const std::string& key = slk.table->colNames_[col];
            if (key.empty()) continue;
            const std::string value = slk.table->GetString(row, col);
            if (value.empty()) continue;
            txt.SetString(newSection, key, value);
            copied++;
        }
    }

    const auto oldTxtValues = txt.GetSection(oldSection);
    for (const auto& [key, value] : oldTxtValues) {
        txt.SetString(newSection, key, value);
        copied++;
    }
    return copied;
}

bool ApplyObjectModificationEntry(const ObjectModificationEntry& entry,
                                  const std::string& section,
                                  const SlkAccessor& meta,
                                  TxtTable& overlay,
                                  int& applied,
                                  int& unknown) {
    if (section.size() != 4) return false;
    bool touched = false;
    for (const auto& mod : entry.modifications) {
        std::string fieldName;
        if (!ResolveObjectModField(meta, mod, fieldName)) {
            unknown++;
            continue;
        }
        overlay.SetString(section, fieldName, FormatObjectModificationValue(mod));
        applied++;
        touched = true;
    }
    return touched;
}

void ApplySingleTableObjectMods(const MpqManager& mpq,
                                const ObjectModificationSet& mods,
                                std::initializer_list<const char*> metaSlkCandidates,
                                const SlkTable& baseSlk,
                                std::initializer_list<const char*> idColumnCandidates,
                                TxtTable& overlay,
                                const char* debugLabel) {
    SlkTable metaSlk;
    bool metaLoaded = false;
    std::vector<uint8_t> metaBuf;
    for (const char* candidatePath : metaSlkCandidates) {
        if (mpq.ReadFile(candidatePath, metaBuf) &&
            metaSlk.Load(metaBuf.data(), metaBuf.size())) {
            metaLoaded = true;
            break;
        }
    }
    if (!metaLoaded) {
        if (debugLabel) {
            std::cerr << "ObjectMods: metadata SLK unavailable for " << debugLabel
                      << "; modifications skipped" << std::endl;
        }
        return;
    }

    const SlkAccessor meta(metaSlk, {"ID"});
    const SlkAccessor baseData(baseSlk, idColumnCandidates);
    int applied = 0;
    int unknown = 0;
    int customCloned = 0;
    int customSkipped = 0;

    for (const auto& entry : mods.custom) {
        const std::string oldSection = entry.oldId.ToString();
        const std::string newSection = entry.newId.ToString();
        if (oldSection.size() != 4 || newSection.size() != 4) {
            customSkipped++;
            continue;
        }
        if (CopyObjectDataSection(overlay, baseData, oldSection, newSection) == 0) {
            customSkipped++;
            continue;
        }
        customCloned++;
        ApplyObjectModificationEntry(entry, newSection, meta, overlay, applied, unknown);
    }

    for (const auto& entry : mods.original) {
        const std::string section = entry.oldId.ToString();
        ApplyObjectModificationEntry(entry, section, meta, overlay, applied, unknown);
    }

    if (debugLabel) {
        std::cout << "ObjectMods: " << debugLabel
                  << " original=" << mods.original.size()
                  << " custom=" << mods.custom.size()
                  << " applied=" << applied
                  << " unknown=" << unknown
                  << " customCloned=" << customCloned
                  << " customSkipped=" << customSkipped << std::endl;
    }
}

void ApplyMapDestructableObjectMods(const MpqManager& mpq, const std::string& mapPath,
                                    const SlkTable& destructableSlk,
                                    TxtTable& overlay) {
    if (mapPath.empty()) return;
    ObjectModificationSet mods;
    if (!LoadWar3MapDestructableMods(mapPath, mods)) {
        std::cout << "ObjectMods: no war3map.w3b destructable modifications" << std::endl;
        return;
    }
    ApplySingleTableObjectMods(
        mpq, mods, {"Units\\DestructableMetaData.slk"},
        destructableSlk, {"DestructableID", "destructableID", "doodID", "ID"},
        overlay, "war3map.w3b");
}

void ApplyMapDoodadObjectMods(const MpqManager& mpq, const std::string& mapPath,
                              const SlkTable& doodadSlk,
                              TxtTable& overlay) {
    if (mapPath.empty()) return;
    ObjectModificationSet mods;
    if (!LoadWar3MapDoodadMods(mapPath, mods)) {
        return;
    }
    ApplySingleTableObjectMods(
        mpq, mods, {"Doodads\\DoodadMetaData.slk", "Units\\DestructableMetaData.slk"},
        doodadSlk, {"doodID", "DestructableID", "destructableID", "ID"},
        overlay, "war3map.w3d");
}

void ApplyMapItemObjectMods(const MpqManager& mpq, const std::string& mapPath,
                            const SlkTable& itemSlk,
                            TxtTable& overlay) {
    if (mapPath.empty()) return;
    ObjectModificationSet mods;
    if (!LoadWar3MapItemMods(mapPath, mods)) {
        std::cout << "ObjectMods: no war3map.w3t item modifications" << std::endl;
        return;
    }
    ApplySingleTableObjectMods(
        mpq, mods, {"Units\\UnitMetaData.slk"},
        itemSlk, {"itemID", "unitID", "ID"},
        overlay, "war3map.w3t");
}

void CollectDestructableIds(const SlkAccessor& slk, const TxtTable& txt, std::set<std::string>& outIds) {
    for (const auto& [id, row] : slk.rowsById) {
        (void)row;
        if (id.size() == 4) {
            outIds.insert(id);
        }
    }
    for (const auto& section : txt.Sections()) {
        if (section.size() == 4) {
            outIds.insert(section);
        }
    }
}

bool LoadSlkTableFromMpq(const MpqManager& mpq, const std::string& path, SlkTable& outSlk) {
    std::vector<uint8_t> buf;
    if (!mpq.ReadFile(path, buf) || buf.empty()) {
        return false;
    }
    return outSlk.Load(buf.data(), buf.size());
}

int LoadTxtOverlaysFromMpq(const MpqManager& mpq,
                           std::initializer_list<const char*> paths,
                           TxtTable& outTxt) {
    int loadedCount = 0;
    std::vector<uint8_t> buf;
    for (const char* path : paths) {
        if (!path) continue;
        if (mpq.ReadFile(path, buf) && !buf.empty()) {
            if (outTxt.Load(buf.data(), buf.size())) {
                ++loadedCount;
            }
        }
    }
    return loadedCount;
}

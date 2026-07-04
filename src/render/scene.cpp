#include "scene.h"
#include "map/object_mods.h"
#include "resource/slk.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <unordered_set>

static std::string War3IDToString(uint32_t id) {
    char s[5] = {(char)(id&0xFF), (char)((id>>8)&0xFF),
                 (char)((id>>16)&0xFF), (char)((id>>24)&0xFF), 0};
    return std::string(s);
}

static uint32_t StringToWar3ID(const std::string& id) {
    uint32_t war3ID = 0;
    for (int i = 0; i < 4 && i < (int)id.size(); ++i) {
        war3ID |= ((uint32_t)(unsigned char)id[i]) << ((3 - i) * 8);
    }
    return war3ID;
}

static std::string NormalizePath(const std::string& p) {
    std::string r = p;
    for (auto& c : r) if (c == '/') c = '\\';
    // Remove trailing model extension if present; callers append the desired .mdx.
    if (r.size() > 4) {
        std::string tail = r.substr(r.size() - 4);
        std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) {
            return (char)std::tolower(c);
        });
        if (tail == ".mdx" || tail == ".mdl") r.resize(r.size() - 4);
    }
    return r;
}

static void PushUniquePath(std::vector<std::string>& paths, const std::string& path) {
    if (path.empty()) return;
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

static std::string NormalizeModelPathWithExtension(const std::string& path) {
    std::string normalized = path;
    if (normalized.empty()) {
        return normalized;
    }
    for (char& c : normalized) {
        if (c == '/') {
            c = '\\';
        }
    }
    if (normalized.size() <= 4) {
        return normalized + ".mdx";
    }
    std::string tail = normalized.substr(normalized.size() - 4);
    std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    if (tail == ".mdl") {
        normalized.resize(normalized.size() - 4);
        normalized += ".mdx";
    }
    else if (tail != ".mdx") {
        normalized += ".mdx";
    }
    return normalized;
}

static Vec3 TransformInstanceLocalPoint(const PlacedModel& inst, const Vec3& localPosition) {
    const float c = std::cos(inst.renderAngle);
    const float s = std::sin(inst.renderAngle);
    const float lx = localPosition.x * inst.scale[0];
    const float ly = localPosition.y * inst.scale[1];
    const float lz = localPosition.z * inst.scale[2];
    return {
        inst.renderPos[0] + c * lx + s * lz,
        inst.renderPos[1] + ly,
        inst.renderPos[2] - s * lx + c * lz
    };
}

static Mat4 BuildInstanceModelMatrix(const PlacedModel& inst) {
    Mat4 model = Mat4::Identity();
    const float c = std::cos(inst.renderAngle);
    const float s = std::sin(inst.renderAngle);
    model.m[0] = c * inst.scale[0];
    model.m[2] = -s * inst.scale[0];
    model.m[5] = inst.scale[1];
    model.m[8] = s * inst.scale[2];
    model.m[10] = c * inst.scale[2];
    model.m[12] = inst.renderPos[0];
    model.m[13] = inst.renderPos[1];
    model.m[14] = inst.renderPos[2];
    return model;
}

namespace {

static std::string TrimCopy(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

struct SlkAccessor {
    const SlkTable* table = nullptr;
    int idColumn = -1;
    std::unordered_map<std::string, int> columns;
    std::unordered_map<std::string, int> rowsById;

    SlkAccessor() = default;

    SlkAccessor(const SlkTable& slk, std::initializer_list<const char*> idColumnCandidates) {
        Init(slk, idColumnCandidates);
    }

    void Init(const SlkTable& slk, std::initializer_list<const char*> idColumnCandidates) {
        table = &slk;
        columns.clear();
        rowsById.clear();
        idColumn = -1;

        for (int i = 0; i < slk.Cols(); ++i) {
            columns[slk.colNames_[i]] = i;
        }

        for (const char* candidate : idColumnCandidates) {
            auto it = columns.find(candidate);
            if (it != columns.end()) {
                idColumn = it->second;
                break;
            }
        }
        if (idColumn < 0 && slk.Cols() > 0) {
            idColumn = 0;
        }
        if (idColumn < 0) {
            return;
        }

        for (int row = 0; row < slk.Rows(); ++row) {
            const std::string id = TrimCopy(slk.GetString(row, idColumn));
            if (!id.empty()) {
                rowsById[id] = row;
            }
        }
    }

    bool TryGetValue(const std::string& id, const std::string& key, std::string& outValue) const {
        if (!table) {
            return false;
        }
        const auto rowIt = rowsById.find(id);
        if (rowIt == rowsById.end()) {
            return false;
        }
        const auto colIt = columns.find(key);
        if (colIt == columns.end()) {
            return false;
        }
        outValue = table->GetString(rowIt->second, colIt->second);
        return !outValue.empty();
    }
};

static std::string FormatObjectModificationValue(const ObjectModification& mod) {
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

static bool ResolveObjectModField(const SlkAccessor& meta, const ObjectModification& mod,
                                  std::string& outField) {
    const std::string metaId = mod.id.ToString();
    std::string slkName;
    if (!meta.TryGetValue(metaId, "slk", slkName) || slkName.empty()) return false;
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

} // namespace

bool SceneManager::Init(const MpqManager& mpq) {
    mpq_ = const_cast<MpqManager*>(&mpq);
    ResetLoadStats();

    // Load UnitUI.slk: unit War3ID to model path.
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\unitUI.slk", buf)) {
            SlkTable slk;
            if (slk.Load(buf.data(), buf.size())) {
                int colID = -1, colFile = -1;
                for (int i = 0; i < slk.Cols(); i++) {
                    if (slk.colNames_[i] == "unitUIID") colID = i;
                    if (slk.colNames_[i] == "file") colFile = i;
                }
                if (colID >= 0 && colFile >= 0) {
                    for (int r = 0; r < slk.Rows(); r++) {
                        std::string id = slk.GetString(r, colID);
                        std::string file = slk.GetString(r, colFile);
                        if (!id.empty() && !file.empty()) {
                            uint32_t war3ID = StringToWar3ID(id);
                            std::string modelPath = NormalizePath(file) + ".mdx";
                            unitPaths_[war3ID] = modelPath;
                            unitFiles_[war3ID] = file;
                        }
                    }
                    std::cout << "Scene: " << unitPaths_.size() << " unit paths loaded" << std::endl;
                }
            }
        }
    }

    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\ItemData.slk", buf)) {
            SlkTable slk;
            if (slk.Load(buf.data(), buf.size())) {
                int colID = -1, colFile = -1;
                for (int i = 0; i < slk.Cols(); ++i) {
                    const std::string& name = slk.colNames_[i];
                    if (name == "itemID" || name == "unitID" || name == "ID") colID = i;
                    if (name == "file") colFile = i;
                }
                if (colID < 0 && slk.Cols() > 0) {
                    colID = 0;
                }
                if (colID >= 0 && colFile >= 0) {
                    for (int r = 0; r < slk.Rows(); ++r) {
                        std::string id = slk.GetString(r, colID);
                        std::string file = slk.GetString(r, colFile);
                        if (id.size() == 4 && !file.empty()) {
                            uint32_t war3ID = StringToWar3ID(id);
                            itemPaths_[war3ID] = NormalizePath(file) + ".mdx";
                            itemFiles_[war3ID] = file;
                        }
                    }
                    std::cout << "Scene: " << itemPaths_.size() << " item paths loaded" << std::endl;
                }
                else {
                    std::cout << "Scene: ItemData.slk missing columns"
                              << " itemID=" << colID
                              << " file=" << colFile << std::endl;
                }
            }
        }
    }
    // Load Doodads.slk: doodad War3ID to model path.
    {
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Doodads\\Doodads.slk", buf)) {
            SlkTable slk;
            if (slk.Load(buf.data(), buf.size())) {
                int colID = -1, colDir = -1, colFile = -1;
                for (int i = 0; i < slk.Cols(); i++) {
                    if (slk.colNames_[i] == "doodID") colID = i;
                    if (slk.colNames_[i] == "dir") colDir = i;
                    if (slk.colNames_[i] == "file") colFile = i;
                }
                if (colID >= 0 && colFile >= 0) {
                    for (int r = 0; r < slk.Rows(); r++) {
                        std::string id = slk.GetString(r, colID);
                        std::string dir = slk.GetString(r, colDir);
                        std::string file = slk.GetString(r, colFile);
                        if (!id.empty() && !file.empty()) {
                            uint32_t war3ID = StringToWar3ID(id);
                            std::string basePath = file;
                            if (!dir.empty()) {
                                basePath = dir + "\\" + file;
                            }
                            // Store the base file path; ResolveAndLoad will try variations
                            doodadPaths_[war3ID] = NormalizePath(basePath);
                            doodadDirs_[war3ID] = dir;
                            doodadFiles_[war3ID] = file;
                        }
                    }
                    std::cout << "Scene: " << doodadPaths_.size() << " doodad paths loaded" << std::endl;
                }
                else {
                    std::cout << "Scene: Doodads.slk missing columns"
                              << " doodID=" << colID
                              << " dir=" << colDir
                              << " file=" << colFile << std::endl;
                }
            }
        }
    }

    {
        SlkTable slk;
        std::vector<uint8_t> buf;
        if (mpq.ReadFile("Units\\DestructableData.slk", buf)) {
            if (slk.Load(buf.data(), buf.size())) {
                int colID = -1, colFile = -1;
                for (int i = 0; i < slk.Cols(); i++) {
                    const std::string& name = slk.colNames_[i];
                    if (name == "unitID" || name == "destructableID" || name == "doodID") colID = i;
                    if (name == "file" || name == "dir") {
                        if (name == "file") colFile = i;
                    }
                }

                if (colID < 0 && slk.Cols() > 0) {
                    colID = 0;
                }

                int destructableCount = 0;
                if (colID >= 0 && colFile >= 0) {
                    for (int r = 0; r < slk.Rows(); ++r) {
                        std::string sid = slk.GetString(r, colID);
                        std::string file = slk.GetString(r, colFile);
                        if (sid.size() != 4 || file.empty()) continue;

                        uint32_t war3ID = StringToWar3ID(sid);
                        doodadPaths_[war3ID] = NormalizePath(file);
                        doodadDirs_[war3ID].clear();
                        doodadFiles_[war3ID] = file;
                        destructableCount++;
                    }
                    std::cout << "Scene: " << destructableCount << " destructable paths loaded" << std::endl;
                }
                else {
                    std::cout << "Scene: DestructableData.slk missing columns"
                              << " id=" << colID
                              << " file=" << colFile << std::endl;
                }
            }
        }
    }

    return true;
}

void SceneManager::ApplyMapObjectModifications(const std::string& mapPath) {
    if (!mpq_ || mapPath.empty()) return;

    auto updateUnitPath = [this](uint32_t id) {
        auto fileIt = unitFiles_.find(id);
        if (fileIt == unitFiles_.end() || fileIt->second.empty()) {
            return false;
        }
        unitPaths_[id] = NormalizePath(fileIt->second) + ".mdx";
        return true;
    };

    auto updateItemPath = [this](uint32_t id) {
        auto fileIt = itemFiles_.find(id);
        if (fileIt == itemFiles_.end() || fileIt->second.empty()) {
            return false;
        }
        itemPaths_[id] = NormalizePath(fileIt->second) + ".mdx";
        return true;
    };

    auto updateDoodadPath = [this](uint32_t id) {
        auto fileIt = doodadFiles_.find(id);
        if (fileIt == doodadFiles_.end() || fileIt->second.empty()) {
            return false;
        }
        const auto dirIt = doodadDirs_.find(id);
        std::string basePath = fileIt->second;
        if (dirIt != doodadDirs_.end() && !dirIt->second.empty()) {
            basePath = dirIt->second + "\\" + fileIt->second;
        }
        doodadPaths_[id] = NormalizePath(basePath);
        return true;
    };

    auto applyUnitMods = [this, &mapPath, &updateUnitPath]() {
        ObjectModificationSet mods;
        if (!LoadWar3MapObjectMods(mapPath, "war3map.w3u", false, mods)) {
            return;
        }

        SlkTable metaSlk;
        std::vector<uint8_t> metaBuf;
        if (!mpq_->ReadFile("Units\\UnitMetaData.slk", metaBuf) ||
            !metaSlk.Load(metaBuf.data(), metaBuf.size())) {
            std::cerr << "SceneObjectMods: Units\\UnitMetaData.slk unavailable; render path modifications skipped for war3map.w3u" << std::endl;
            return;
        }

        const SlkAccessor meta(metaSlk, {"ID"});
        int applied = 0;
        int unknown = 0;
        int customCloned = 0;
        int customSkipped = 0;

        auto applyEntry = [&](const ObjectModificationEntry& entry, uint32_t id) {
            bool touchedPath = false;
            for (const auto& mod : entry.modifications) {
                std::string fieldName;
                if (!ResolveObjectModField(meta, mod, fieldName)) {
                    unknown++;
                    continue;
                }
                std::transform(fieldName.begin(), fieldName.end(), fieldName.begin(), [](unsigned char c) {
                    return (char)std::tolower(c);
                });
                if (fieldName == "file") {
                    unitFiles_[id] = FormatObjectModificationValue(mod);
                    applied++;
                    touchedPath = true;
                }
            }
            if (touchedPath) {
                updateUnitPath(id);
            }
        };

        for (const auto& entry : mods.custom) {
            const uint32_t oldId = StringToWar3ID(entry.oldId.ToString());
            const uint32_t newId = StringToWar3ID(entry.newId.ToString());
            auto oldFile = unitFiles_.find(oldId);
            if (oldFile == unitFiles_.end()) {
                customSkipped++;
                continue;
            }
            unitFiles_[newId] = oldFile->second;
            updateUnitPath(newId);
            customCloned++;
            applyEntry(entry, newId);
        }

        for (const auto& entry : mods.original) {
            const uint32_t id = StringToWar3ID(entry.oldId.ToString());
            applyEntry(entry, id);
        }

        std::cout << "SceneObjectMods: war3map.w3u"
                  << " original=" << mods.original.size()
                  << " custom=" << mods.custom.size()
                  << " applied=" << applied
                  << " unknown=" << unknown
                  << " customCloned=" << customCloned
                  << " customSkipped=" << customSkipped << std::endl;
    };

    auto applyItemMods = [this, &mapPath, &updateItemPath]() {
        ObjectModificationSet mods;
        if (!LoadWar3MapObjectMods(mapPath, "war3map.w3t", false, mods)) {
            return;
        }

        SlkTable metaSlk;
        std::vector<uint8_t> metaBuf;
        if (!mpq_->ReadFile("Units\\UnitMetaData.slk", metaBuf) ||
            !metaSlk.Load(metaBuf.data(), metaBuf.size())) {
            std::cerr << "SceneObjectMods: Units\\UnitMetaData.slk unavailable; render path modifications skipped for war3map.w3t" << std::endl;
            return;
        }

        const SlkAccessor meta(metaSlk, {"ID"});
        int applied = 0;
        int unknown = 0;
        int customCloned = 0;
        int customSkipped = 0;

        auto applyEntry = [&](const ObjectModificationEntry& entry, uint32_t id) {
            bool touchedPath = false;
            for (const auto& mod : entry.modifications) {
                std::string fieldName;
                if (!ResolveObjectModField(meta, mod, fieldName)) {
                    unknown++;
                    continue;
                }
                std::transform(fieldName.begin(), fieldName.end(), fieldName.begin(), [](unsigned char c) {
                    return (char)std::tolower(c);
                });
                if (fieldName == "file") {
                    itemFiles_[id] = FormatObjectModificationValue(mod);
                    applied++;
                    touchedPath = true;
                }
            }
            if (touchedPath) {
                updateItemPath(id);
            }
        };

        for (const auto& entry : mods.custom) {
            const uint32_t oldId = StringToWar3ID(entry.oldId.ToString());
            const uint32_t newId = StringToWar3ID(entry.newId.ToString());
            auto oldFile = itemFiles_.find(oldId);
            if (oldFile == itemFiles_.end()) {
                customSkipped++;
                continue;
            }
            itemFiles_[newId] = oldFile->second;
            updateItemPath(newId);
            customCloned++;
            applyEntry(entry, newId);
        }

        for (const auto& entry : mods.original) {
            const uint32_t id = StringToWar3ID(entry.oldId.ToString());
            applyEntry(entry, id);
        }

        std::cout << "SceneObjectMods: war3map.w3t"
                  << " original=" << mods.original.size()
                  << " custom=" << mods.custom.size()
                  << " applied=" << applied
                  << " unknown=" << unknown
                  << " customCloned=" << customCloned
                  << " customSkipped=" << customSkipped << std::endl;
    };

    auto applyDoodadMods = [this, &mapPath, &updateDoodadPath](const char* fileName, bool extended,
                                                               const char* metaPath) {
        ObjectModificationSet mods;
        if (!LoadWar3MapObjectMods(mapPath, fileName, extended, mods)) {
            return;
        }

        SlkTable metaSlk;
        std::vector<uint8_t> metaBuf;
        if (!mpq_->ReadFile(metaPath, metaBuf) || !metaSlk.Load(metaBuf.data(), metaBuf.size())) {
            std::cerr << "SceneObjectMods: " << metaPath
                      << " unavailable; render path modifications skipped for "
                      << fileName << std::endl;
            return;
        }

        const SlkAccessor meta(metaSlk, {"ID"});
        int applied = 0;
        int unknown = 0;
        int customCloned = 0;
        int customSkipped = 0;

        auto applyEntry = [&](const ObjectModificationEntry& entry, uint32_t id) {
            bool touchedPath = false;
            for (const auto& mod : entry.modifications) {
                std::string fieldName;
                if (!ResolveObjectModField(meta, mod, fieldName)) {
                    unknown++;
                    continue;
                }
                const std::string fieldLower = [&fieldName]() {
                    std::string lower = fieldName;
                    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                        return (char)std::tolower(c);
                    });
                    return lower;
                }();

                if (fieldLower == "file") {
                    doodadFiles_[id] = FormatObjectModificationValue(mod);
                    applied++;
                    touchedPath = true;
                } else if (fieldLower == "dir") {
                    doodadDirs_[id] = FormatObjectModificationValue(mod);
                    applied++;
                    touchedPath = true;
                }
            }
            if (touchedPath) {
                updateDoodadPath(id);
            }
        };

        for (const auto& entry : mods.custom) {
            const uint32_t oldId = StringToWar3ID(entry.oldId.ToString());
            const uint32_t newId = StringToWar3ID(entry.newId.ToString());
            auto oldFile = doodadFiles_.find(oldId);
            if (oldFile == doodadFiles_.end()) {
                customSkipped++;
                continue;
            }
            doodadFiles_[newId] = oldFile->second;
            auto oldDir = doodadDirs_.find(oldId);
            if (oldDir != doodadDirs_.end()) {
                doodadDirs_[newId] = oldDir->second;
            } else {
                doodadDirs_[newId].clear();
            }
            updateDoodadPath(newId);
            customCloned++;
            applyEntry(entry, newId);
        }

        for (const auto& entry : mods.original) {
            const uint32_t id = StringToWar3ID(entry.oldId.ToString());
            applyEntry(entry, id);
        }

        std::cout << "SceneObjectMods: " << fileName
                  << " original=" << mods.original.size()
                  << " custom=" << mods.custom.size()
                  << " applied=" << applied
                  << " unknown=" << unknown
                  << " customCloned=" << customCloned
                  << " customSkipped=" << customSkipped << std::endl;
    };

    applyUnitMods();
    applyItemMods();
    applyDoodadMods("war3map.w3d", true, "Doodads\\DoodadMetaData.slk");
    applyDoodadMods("war3map.w3b", false, "Units\\DestructableMetaData.slk");
}

void SceneManager::Clear() {
    for (auto* inst : instances_) delete inst;
    instances_.clear();
    cache_.clear();
    ResetLoadStats();
}

void SceneManager::ResetLoadStats() {
    loadStats_ = LoadStats{};
}

void SceneManager::PrintLoadStats() const {
    std::cout << "SceneStats: units " << loadStats_.unitSuccess << "/" << loadStats_.unitAttempts
              << ", items " << loadStats_.itemSuccess << "/" << loadStats_.itemAttempts
              << ", doodads " << loadStats_.doodadSuccess << "/" << loadStats_.doodadAttempts
              << std::endl;
    if (loadStats_.missReasons.empty()) return;

    std::vector<std::pair<std::string, int>> misses(loadStats_.missReasons.begin(), loadStats_.missReasons.end());
    std::sort(misses.begin(), misses.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    for (const auto& miss : misses) {
        std::cout << "  SceneMiss: " << miss.first << " x" << miss.second << std::endl;
    }
}

MdxViewer* SceneManager::GetOrLoadModel(const std::string& path) {
    auto it = cache_.find(path);
    if (it == cache_.end()) {
        auto [insertIt, inserted] = cache_.try_emplace(path);
        insertIt->second.path = path;
        it = insertIt;
    }

    MdxEntry& entry = it->second;
    if (!EnsureModelCached(path, entry)) {
        return nullptr;
    }
    if (entry.sharedViewer) {
        return entry.sharedViewer.get();
    }

    entry.sharedViewer = std::make_unique<MdxViewer>();
    entry.sharedViewer->DebugLogState("scene-shared-after-ctor");
    std::cout << "SceneModelLoad: viewer loading " << path << std::endl;
    if (!entry.sharedViewer->Load(entry.model, mpq_)) {
        std::cout << "SceneModelLoad: viewer load failed for " << path << std::endl;
        entry.sharedViewer.reset();
        return nullptr;
    }

    return entry.sharedViewer.get();
}

bool SceneManager::EnsureModelCached(const std::string& path, MdxEntry& entry) {
    if (entry.modelLoaded) {
        return true;
    }

    std::vector<uint8_t> data;
    if (!mpq_ || !mpq_->ReadFile(path, data)) {
        return false;
    }

    MdxModel model;
    if (!MdxLoad(data.data(), data.size(), model)) {
        return false;
    }
    entry.path = path;
    entry.model = std::move(model);
    entry.modelLoaded = true;
    return true;
}

std::unique_ptr<MdxViewer> SceneManager::CreateUnitViewer(const std::string& path) {
    MdxViewer* sharedViewer = GetOrLoadModel(path);
    if (!sharedViewer) {
        return nullptr;
    }

    auto viewer = std::make_unique<MdxViewer>();
    if (!viewer->ShareModelResourcesFrom(*sharedViewer)) {
        return nullptr;
    }
    return viewer;
}

MdxViewer* SceneManager::ResolveAndLoad(uint32_t war3ID, uint32_t skinId, int variation, bool doodad, const char** missReason) {
    std::vector<uint32_t> ids;
    if (skinId != 0 && skinId != war3ID) ids.push_back(skinId);
    ids.push_back(war3ID);

    if (!doodad) {
        bool sawUnitPath = false;
        for (uint32_t id : ids) {
            auto unitIt = unitPaths_.find(id);
            if (unitIt == unitPaths_.end()) continue;
            sawUnitPath = true;
            MdxViewer* viewer = GetOrLoadModel(unitIt->second);
            if (viewer) return viewer;
        }
        if (missReason) *missReason = sawUnitPath ? "unit-model-miss" : "unit-path-miss";
        return nullptr;
    }

    bool sawDoodadPath = false;
    int safeVariation = std::max(0, variation);
    for (uint32_t id : ids) {
        auto doodIt = doodadPaths_.find(id);
        if (doodIt == doodadPaths_.end()) continue;

        sawDoodadPath = true;
        std::string basePath = NormalizePath(doodIt->second);
        size_t slash = basePath.find_last_of('\\');
        std::string leaf = (slash == std::string::npos) ? basePath : basePath.substr(slash + 1);

        std::vector<std::string> candidates;
        PushUniquePath(candidates, basePath + ".mdx");
        PushUniquePath(candidates, basePath + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "\\" + leaf + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "\\" + leaf + "0.mdx");
        PushUniquePath(candidates, basePath + "_" + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "_v" + std::to_string(safeVariation) + ".mdx");
        PushUniquePath(candidates, basePath + "_V" + std::to_string(safeVariation) + ".mdx");

        for (const std::string& candidate : candidates) {
            MdxViewer* viewer = GetOrLoadModel(candidate);
            if (viewer) return viewer;
        }
    }

    if (missReason) *missReason = sawDoodadPath ? "doodad-model-miss" : "doodad-path-miss";
    return nullptr;
}

bool SceneManager::AddDoodad(uint32_t war3ID, const float pos[3], float angle,
                              const float scale[3], int variation, int flags, int life, uint32_t skinId) {
    (void)flags;
    (void)life;
    loadStats_.doodadAttempts++;

    const char* missReason = nullptr;
    MdxViewer* viewer = ResolveAndLoad(war3ID, skinId, variation, true, &missReason);
    if (!viewer) {
        loadStats_.missReasons[missReason ? missReason : "doodad-unknown-miss"]++;
        return false;
    }

    auto* inst = new PlacedModel();
    inst->pos[0] = pos[0]; inst->pos[1] = pos[2]; inst->pos[2] = pos[1];
    inst->renderPos[0] = inst->pos[0]; inst->renderPos[1] = inst->pos[1]; inst->renderPos[2] = inst->pos[2];
    inst->angle = angle;
    inst->renderAngle = angle;
    inst->scale[0] = scale[0]; inst->scale[1] = scale[2]; inst->scale[2] = scale[1];
    inst->variation = variation;
    inst->debugWar3ID = war3ID;
    inst->debugSkinID = skinId;
    inst->debugKind = "doodad";
    inst->visible = true;
    inst->viewer = viewer;
    instances_.push_back(inst);
    static int doodadDiagCount = 0;
    if (doodadDiagCount < 8) {
        char s[5] = {(char)(war3ID & 0xFF), (char)((war3ID >> 8) & 0xFF),
                     (char)((war3ID >> 16) & 0xFF), (char)((war3ID >> 24) & 0xFF), 0};
        std::cout << "DoodadDiag[" << doodadDiagCount << "]: id=" << s
                  << " rawPos=(" << pos[0] << "," << pos[1] << "," << pos[2] << ")"
                  << " mappedPos=(" << inst->pos[0] << "," << inst->pos[1] << "," << inst->pos[2] << ")"
                  << " scale=(" << inst->scale[0] << "," << inst->scale[1] << "," << inst->scale[2] << ")"
                  << " angle=" << angle
                  << " variation=" << variation
                  << std::endl;
        doodadDiagCount++;
    }
    loadStats_.doodadSuccess++;
    return true;
}

bool SceneManager::HasItemPath(uint32_t war3ID, uint32_t skinId) const {
    if (skinId != 0 && skinId != war3ID && itemPaths_.find(skinId) != itemPaths_.end()) {
        return true;
    }
    return itemPaths_.find(war3ID) != itemPaths_.end();
}

int SceneManager::AddItem(uint32_t war3ID, const float pos[3], float angle,
                           const float scale[3], int flags, uint32_t skinId) {
    (void)flags;
    loadStats_.itemAttempts++;

    std::vector<uint32_t> ids;
    if (skinId != 0 && skinId != war3ID) ids.push_back(skinId);
    ids.push_back(war3ID);

    bool sawItemPath = false;
    MdxViewer* viewer = nullptr;
    for (uint32_t id : ids) {
        auto itemIt = itemPaths_.find(id);
        if (itemIt == itemPaths_.end()) continue;
        sawItemPath = true;
        viewer = GetOrLoadModel(itemIt->second);
        if (viewer) break;
    }

    if (!viewer) {
        loadStats_.missReasons[sawItemPath ? "item-model-miss" : "item-path-miss"]++;
        return -1;
    }

    auto* inst = new PlacedModel();
    inst->pos[0] = pos[0]; inst->pos[1] = pos[2]; inst->pos[2] = pos[1];
    inst->renderPos[0] = inst->pos[0]; inst->renderPos[1] = inst->pos[1]; inst->renderPos[2] = inst->pos[2];
    inst->angle = angle;
    inst->renderAngle = angle;
    inst->scale[0] = scale[0]; inst->scale[1] = scale[2]; inst->scale[2] = scale[1];
    inst->debugWar3ID = war3ID;
    inst->debugSkinID = skinId;
    inst->debugKind = "item";
    inst->visible = true;
    inst->viewer = viewer;
    const int instanceIndex = (int)instances_.size();
    instances_.push_back(inst);
    loadStats_.itemSuccess++;
    return instanceIndex;
}

int SceneManager::AddUnit(uint32_t war3ID, const float pos[3], float angle,
                            const float scale[3], int player, int hitpoints, int flags, uint32_t skinId,
                            bool playBirthAnimation) {
    (void)hitpoints;
    (void)flags;
    loadStats_.unitAttempts++;

    const char* missReason = nullptr;
    std::unique_ptr<MdxViewer> uniqueViewer;
    MdxViewer* viewer = nullptr;

    // Units need per-instance animation/geoset state like warsmash_ref. We still
    // share immutable GPU/model resources under the hood to avoid buffer-pool exhaustion.
    std::vector<uint32_t> ids;
    if (skinId != 0 && skinId != war3ID) ids.push_back(skinId);
    ids.push_back(war3ID);

    bool sawUnitPath = false;
    for (uint32_t id : ids) {
        auto unitIt = unitPaths_.find(id);
        if (unitIt == unitPaths_.end()) continue;
        sawUnitPath = true;
        uniqueViewer = CreateUnitViewer(unitIt->second);
        if (uniqueViewer) {
            break;
        }
    }

    viewer = uniqueViewer.get();
    if (!viewer && !sawUnitPath) {
        missReason = "unit-path-miss";
    }

    if (!viewer) {
        viewer = ResolveAndLoad(war3ID, skinId, 0, false, &missReason);
    }
    if (!viewer) {
        loadStats_.missReasons[missReason ? missReason : "unit-unknown-miss"]++;
        return -1;
    }

    auto* inst = new PlacedModel();
    inst->pos[0] = pos[0]; inst->pos[1] = pos[2]; inst->pos[2] = pos[1];
    inst->renderPos[0] = inst->pos[0]; inst->renderPos[1] = inst->pos[1]; inst->renderPos[2] = inst->pos[2];
    inst->angle = angle;
    inst->renderAngle = angle;
    inst->scale[0] = scale[0]; inst->scale[1] = scale[2]; inst->scale[2] = scale[1];
    inst->player = player;
    inst->debugWar3ID = war3ID;
    inst->debugSkinID = skinId;
    inst->debugKind = "unit";
    inst->visible = true;
    inst->smoothMotion = true;
    inst->ownedViewer = std::move(uniqueViewer);
    inst->viewer = viewer;
    if (playBirthAnimation) {
        inst->viewer->PlayBirthThenStandSequence();
    }
    const int instanceIndex = (int)instances_.size();
    instances_.push_back(inst);
    loadStats_.unitSuccess++;
    return instanceIndex;
}

int SceneManager::AddTransientModelAtScenePosition(const std::string& path, const float pos[3],
                                                   float angle, float scale, int player,
                                                   float fallbackLifeSeconds) {
    const std::string modelPath = NormalizeModelPathWithExtension(path);
    std::unique_ptr<MdxViewer> uniqueViewer = CreateUnitViewer(modelPath);
    if (!uniqueViewer) {
        loadStats_.missReasons["event-spawn-model-miss"]++;
        return -1;
    }

    auto* inst = new PlacedModel();
    inst->pos[0] = pos[0];
    inst->pos[1] = pos[1];
    inst->pos[2] = pos[2];
    inst->renderPos[0] = pos[0];
    inst->renderPos[1] = pos[1];
    inst->renderPos[2] = pos[2];
    inst->angle = angle;
    inst->renderAngle = angle;
    inst->scale[0] = scale;
    inst->scale[1] = scale;
    inst->scale[2] = scale;
    inst->player = player;
    inst->debugKind = "event-spawn";
    inst->visible = true;
    inst->smoothMotion = false;
    inst->transient = true;
    inst->transientAge = 0.0f;
    inst->transientMaxAge = std::max(0.1f, fallbackLifeSeconds);
    inst->ownedViewer = std::move(uniqueViewer);
    inst->viewer = inst->ownedViewer.get();
    if (inst->viewer) {
        inst->viewer->PlaySequenceByIndex(0, false);
    }

    const int instanceIndex = (int)instances_.size();
    instances_.push_back(inst);
    return instanceIndex;
}

int SceneManager::AddTransientModelWithSceneMatrix(const std::string& path, const Mat4& modelMatrix,
                                                   int player, float fallbackLifeSeconds) {
    const std::string modelPath = NormalizeModelPathWithExtension(path);
    std::unique_ptr<MdxViewer> uniqueViewer = CreateUnitViewer(modelPath);
    if (!uniqueViewer) {
        loadStats_.missReasons["event-spawn-model-miss"]++;
        return -1;
    }

    auto* inst = new PlacedModel();
    inst->pos[0] = modelMatrix.m[12];
    inst->pos[1] = modelMatrix.m[13];
    inst->pos[2] = modelMatrix.m[14];
    inst->renderPos[0] = inst->pos[0];
    inst->renderPos[1] = inst->pos[1];
    inst->renderPos[2] = inst->pos[2];
    inst->player = player;
    inst->debugKind = "event-spawn";
    inst->visible = true;
    inst->smoothMotion = false;
    inst->transient = true;
    inst->transientAge = 0.0f;
    inst->transientMaxAge = std::max(0.1f, fallbackLifeSeconds);
    inst->useCustomModelMatrix = true;
    inst->customModelMatrix = modelMatrix;
    inst->ownedViewer = std::move(uniqueViewer);
    inst->viewer = inst->ownedViewer.get();
    if (inst->viewer) {
        inst->viewer->PlaySequenceByIndex(0, false);
    }

    const int instanceIndex = (int)instances_.size();
    instances_.push_back(inst);
    return instanceIndex;
}

bool SceneManager::UpdateInstancePosition(int index, const float pos[3], float angle,
                                          float movementSpeed, float turnRate,
                                          bool smoothMotion) {
    if (index < 0 || index >= (int)instances_.size()) return false;
    auto* inst = instances_[index];
    inst->pos[0] = pos[0];
    inst->pos[1] = pos[1];
    inst->pos[2] = pos[2];
    inst->angle = angle;
    inst->movementSpeed = movementSpeed;
    inst->turnRate = turnRate;
    inst->smoothMotion = smoothMotion;
    return true;
}

bool SceneManager::SetInstanceVisible(int index, bool visible) {
    if (index < 0 || index >= (int)instances_.size()) return false;
    instances_[index]->visible = visible;
    return true;
}

void SceneManager::SetDebugRenderRange(int start, int count) {
    debugRenderStart_ = std::max(0, start);
    debugRenderCount_ = count;
}

void SceneManager::SetDebugRenderKind(const std::string& kind) {
    debugRenderKind_ = kind;
}

void SceneManager::RenderAll(const Mat4& view, const Mat4& proj, float dt) {
    frameEventEffects_.clear();

    auto normalizeAngleDegrees = [](float angle) {
        while (angle < 0.0f) angle += 360.0f;
        while (angle >= 360.0f) angle -= 360.0f;
        return angle;
    };
    auto normalizeAngleDelta = [](float delta) {
        while (delta < -180.0f) delta += 360.0f;
        while (delta > 180.0f) delta -= 360.0f;
        return delta;
    };

    std::unordered_set<MdxViewer*> updatedViewers;
    for (auto* inst : instances_) {
        if (inst->smoothMotion) {
            const float dx = inst->pos[0] - inst->renderPos[0];
            const float dz = inst->pos[2] - inst->renderPos[2];
            const float distance = std::sqrt((dx * dx) + (dz * dz));
            const float speedDelta = std::max(0.0f, inst->movementSpeed) * std::max(0.0f, dt);
            if ((distance > speedDelta) && (speedDelta > 0.0f) && (dt < 1.0f)) {
                inst->renderPos[0] += (speedDelta * dx) / distance;
                inst->renderPos[2] += (speedDelta * dz) / distance;
            }
            else {
                inst->renderPos[0] = inst->pos[0];
                inst->renderPos[2] = inst->pos[2];
            }
            inst->renderPos[1] = inst->pos[1];

            const float targetAngle = normalizeAngleDegrees(inst->angle);
            const float currentAngle = normalizeAngleDegrees(inst->renderAngle);
            const float angleDelta = normalizeAngleDelta(targetAngle - currentAngle);
            const float turnDegreesPerTick = std::max(0.1f, inst->turnRate * 180.0f / 3.1415926535f);
            const float turnDegreesPerSecond = turnDegreesPerTick / (1.0f / 20.0f);
            const float maxTurnStep = turnDegreesPerSecond * std::max(0.0f, dt);
            if (std::fabs(angleDelta) <= maxTurnStep) {
                inst->renderAngle = targetAngle;
            }
            else {
                inst->renderAngle = normalizeAngleDegrees(
                    currentAngle + ((angleDelta < 0.0f) ? -maxTurnStep : maxTurnStep));
            }
        }
        else {
            inst->renderPos[0] = inst->pos[0];
            inst->renderPos[1] = inst->pos[1];
            inst->renderPos[2] = inst->pos[2];
            inst->renderAngle = inst->angle;
        }

        if (inst->visible && inst->viewer) {
            if (updatedViewers.insert(inst->viewer).second) {
                inst->viewer->Update(dt, view);
                for (const MdxEventEffect& effect : inst->viewer->GetResolvedEventEffects()) {
                    SceneEventEffect eventEffect;
                    eventEffect.effect = effect;
                    const Vec3 eventPos = effect.hasLocalTransform
                        ? TransformInstanceLocalPoint(*inst, effect.localPosition)
                        : Vec3{inst->renderPos[0], inst->renderPos[1], inst->renderPos[2]};
                    eventEffect.x = eventPos.x;
                    eventEffect.y = eventPos.y;
                    eventEffect.z = eventPos.z;
                    if (effect.hasLocalTransform) {
                        eventEffect.hasWorldTransform = true;
                        eventEffect.worldTransform = BuildInstanceModelMatrix(*inst) * effect.localTransform;
                    }
                    frameEventEffects_.push_back(std::move(eventEffect));
                }
            }
            if (inst->transient) {
                inst->transientAge += std::max(0.0f, dt);
                const bool sequenceFinished = inst->viewer->HasAnimations()
                    && inst->viewer->IsCurrentSequenceFinished();
                if (sequenceFinished || inst->transientAge >= inst->transientMaxAge) {
                    inst->visible = false;
                    inst->ownedViewer.reset();
                    inst->viewer = nullptr;
                }
            }
        }
    }

    const int renderStart = std::max(0, debugRenderStart_);
    const int renderEnd = (debugRenderCount_ < 0)
        ? (int)instances_.size()
        : std::min((int)instances_.size(), renderStart + std::max(0, debugRenderCount_));
    static int loggedDebugStart = -1;
    static int loggedDebugCount = -2;
    if (debugRenderCount_ >= 0 && debugRenderCount_ <= 16 &&
        (loggedDebugStart != renderStart || loggedDebugCount != debugRenderCount_)) {
        loggedDebugStart = renderStart;
        loggedDebugCount = debugRenderCount_;
        auto fourCC = [](uint32_t id) {
            char s[5] = {(char)(id & 0xFF), (char)((id >> 8) & 0xFF),
                         (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF), 0};
            return std::string(s);
        };
        std::cout << "SceneDebugRange: start=" << renderStart
                  << " count=" << debugRenderCount_
                  << " end=" << renderEnd
                  << " total=" << instances_.size()
                  << std::endl;
        for (int index = renderStart; index < renderEnd; ++index) {
            auto* inst = instances_[index];
            if (!debugRenderKind_.empty() && inst->debugKind != debugRenderKind_) continue;
            std::cout << "SceneDebugInstance[" << index << "]: kind=" << inst->debugKind
                      << " id=" << fourCC(inst->debugWar3ID)
                      << " skin=" << fourCC(inst->debugSkinID)
                      << " model=" << (inst->viewer ? inst->viewer->GetName() : "<null>")
                      << " pos=(" << inst->renderPos[0] << "," << inst->renderPos[1] << "," << inst->renderPos[2] << ")"
                      << " scale=(" << inst->scale[0] << "," << inst->scale[1] << "," << inst->scale[2] << ")"
                      << " angle=" << inst->renderAngle
                      << " visible=" << (inst->visible ? 1 : 0)
                      << std::endl;
        }
    }
    for (int index = renderStart; index < renderEnd; ++index) {
        auto* inst = instances_[index];
        if (!debugRenderKind_.empty() && inst->debugKind != debugRenderKind_) continue;
        if (inst->visible && inst->viewer) {
            if (inst->useCustomModelMatrix) {
                inst->viewer->RenderWithModelMatrix(inst->customModelMatrix, view, proj, inst->player);
            } else {
                Vec3 p = {inst->renderPos[0], inst->renderPos[1], inst->renderPos[2]};
                float s = (inst->scale[0] + inst->scale[1] + inst->scale[2]) / 3.0f;
                inst->viewer->RenderAt(p, view, proj, s, inst->player, inst->renderAngle);
            }
        }
    }
}

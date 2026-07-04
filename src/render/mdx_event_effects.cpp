#include "mdx_event_effects.h"

#include "resource/mpq_storm.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <sstream>

namespace {

static std::string Trim(std::string value) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [&](char c) { return !isSpace((unsigned char)c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [&](char c) { return !isSpace((unsigned char)c); }).base(), value.end());
    return value;
}

static std::string NormalizeSlkPath(std::string path, const char* extension) {
    path = Trim(path);
    for (char& c : path) {
        if (c == '/') {
            c = '\\';
        }
    }
    if (!path.empty() && extension && extension[0]) {
        const size_t slash = path.find_last_of("\\/");
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
            path += extension;
        }
    }
    return path;
}

static std::string NormalizeMdxModelPath(std::string path) {
    path = NormalizeSlkPath(std::move(path), ".mdx");
    if (path.size() > 4) {
        std::string tail = path.substr(path.size() - 4);
        std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) {
            return (char)std::tolower(c);
        });
        if (tail == ".mdl") {
            path.resize(path.size() - 4);
            path += ".mdx";
        }
    }
    return path;
}

static bool LoadSlk(MpqManager& mpq, const char* path, SlkTable& table) {
    std::vector<uint8_t> bytes;
    if (!mpq.ReadFile(path, bytes)) {
        return false;
    }
    return table.Load(bytes.data(), bytes.size());
}

static int FindFirstColumn(const SlkTable& table, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const int col = table.FindColumn(name);
        if (col >= 0) {
            return col;
        }
    }
    return -1;
}

static int FindRowById(const SlkTable& table, const std::string& id,
                       std::initializer_list<const char*> idColumns) {
    const std::string trimmedId = Trim(id);
    for (const char* name : idColumns) {
        const int col = table.FindColumn(name);
        if (col >= 0) {
            const int row = table.FindRow(trimmedId, col);
            if (row >= 0) {
                return row;
            }
        }
    }
    if (table.Cols() > 0) {
        return table.FindRow(trimmedId, 0);
    }
    return -1;
}

static std::string GetStringAny(const SlkTable& table, int row,
                                std::initializer_list<const char*> names,
                                const std::string& def = "") {
    const int col = FindFirstColumn(table, names);
    return col >= 0 ? Trim(table.GetString(row, col, def)) : def;
}

static float GetFloatAny(const SlkTable& table, int row,
                         std::initializer_list<const char*> names,
                         float def = 0.0f) {
    const std::string value = GetStringAny(table, row, names);
    return value.empty() ? def : (float)std::strtod(value.c_str(), nullptr);
}

static int GetIntAny(const SlkTable& table, int row,
                     std::initializer_list<const char*> names,
                     int def = 0) {
    const std::string value = GetStringAny(table, row, names);
    return value.empty() ? def : std::atoi(value.c_str());
}

static std::array<float, 4> GetColorAny(const SlkTable& table, int row,
                                        const char* prefix,
                                        std::array<float, 4> def) {
    const std::string p(prefix ? prefix : "");
    return {{
        GetFloatAny(table, row, {(p + "R").c_str(), (p + "r").c_str()}, def[0]),
        GetFloatAny(table, row, {(p + "G").c_str(), (p + "g").c_str()}, def[1]),
        GetFloatAny(table, row, {(p + "B").c_str(), (p + "b").c_str()}, def[2]),
        GetFloatAny(table, row, {(p + "A").c_str(), (p + "a").c_str()}, def[3])
    }};
}

static std::vector<std::string> SplitCsv(const std::string& value) {
    std::vector<std::string> out;
    std::istringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = Trim(item);
        if (!item.empty()) {
            out.push_back(std::move(item));
        }
    }
    return out;
}

static void ResolveSplatRow(const SlkTable& table, int row, const std::string& type,
                            const std::string& id, MdxEventEffect& effect) {
    const std::string dir = GetStringAny(table, row, {"Dir", "dir"});
    const std::string file = GetStringAny(table, row, {"file", "File"});
    effect.type = type;
    effect.id = id;
    effect.texturePath = NormalizeSlkPath(dir.empty() ? file : dir + "\\" + file, ".blp");
    effect.scale = GetFloatAny(table, row, {"Scale", "scale"}, 0.0f);
    effect.columns = std::max(1, GetIntAny(table, row, {"Columns", "columns"}, 1));
    effect.rows = std::max(1, GetIntAny(table, row, {"Rows", "rows"}, 1));
    effect.blendMode = GetIntAny(table, row, {"BlendMode", "blendMode"}, 0);
    effect.footprint = type == "FPT";
    effect.colors[0] = GetColorAny(table, row, "Start", effect.colors[0]);
    effect.colors[1] = GetColorAny(table, row, "Middle", effect.colors[1]);
    effect.colors[2] = GetColorAny(table, row, "End", effect.colors[2]);
    if (type == "UBR") {
        effect.intervalTimes[0] = GetFloatAny(table, row, {"BirthTime"}, 0.0f);
        effect.intervalTimes[1] = GetFloatAny(table, row, {"PauseTime"}, 0.0f);
        effect.intervalTimes[2] = GetFloatAny(table, row, {"Decay"}, 0.0f);
        effect.intervalCount = 3;
    } else {
        effect.intervalTimes[0] = GetFloatAny(table, row, {"Lifespan"}, 0.0f);
        effect.intervalTimes[1] = GetFloatAny(table, row, {"Decay"}, 0.0f);
        effect.intervalTimes[2] = 0.0f;
        effect.uvIntervals[0] = {{
            GetFloatAny(table, row, {"UVLifespanStart"}, 0.0f),
            GetFloatAny(table, row, {"UVLifespanEnd"}, 0.0f),
            GetFloatAny(table, row, {"LifespanRepeat"}, 0.0f)
        }};
        effect.uvIntervals[1] = {{
            GetFloatAny(table, row, {"UVDecayStart"}, 0.0f),
            GetFloatAny(table, row, {"UVDecayEnd"}, 0.0f),
            GetFloatAny(table, row, {"DecayRepeat"}, 0.0f)
        }};
        effect.intervalCount = 2;
    }
    effect.lifeSpan = effect.intervalTimes[0] + effect.intervalTimes[1] + effect.intervalTimes[2];
    effect.resolved = !effect.texturePath.empty();
}

} // namespace

bool MdxEventEffectResolver::Load(MpqManager& mpq) {
    hasSplatData_ = LoadSlk(mpq, "Splats\\SplatData.slk", splatData_);
    hasUberSplatData_ = LoadSlk(mpq, "Splats\\UberSplatData.slk", uberSplatData_);
    hasSpawnData_ = LoadSlk(mpq, "Splats\\SpawnData.slk", spawnData_);
    hasAnimLookups_ = LoadSlk(mpq, "UI\\SoundInfo\\AnimLookups.slk", animLookups_);
    hasAnimSounds_ = LoadSlk(mpq, "UI\\SoundInfo\\AnimSounds.slk", animSounds_);
    return hasSplatData_ || hasUberSplatData_ || hasSpawnData_ || hasAnimSounds_;
}

MdxEventEffect MdxEventEffectResolver::Resolve(const MdxEventObject& eventObject) const {
    MdxEventEffect effect = ResolveByTypeAndId(eventObject.type, eventObject.id);
    effect.unshaded = (eventObject.flags & 0x8000u) != 0;
    return effect;
}

MdxEventEffect MdxEventEffectResolver::Resolve(const MdxEventTrigger& trigger) const {
    MdxEventEffect effect = ResolveByTypeAndId(trigger.type, trigger.id);
    effect.unshaded = (trigger.flags & 0x8000u) != 0;
    return effect;
}

MdxEventEffect MdxEventEffectResolver::ResolveByTypeAndId(const std::string& type,
                                                          const std::string& id) const {
    MdxEventEffect effect;
    effect.type = type;
    effect.id = id;

    if ((type == "SPL" || type == "FPT") && hasSplatData_) {
        const int row = FindRowById(splatData_, id, {"SplatID", "splatID", "ID", "id"});
        if (row >= 0) {
            ResolveSplatRow(splatData_, row, type, id, effect);
        }
    } else if (type == "UBR" && hasUberSplatData_) {
        const int row = FindRowById(uberSplatData_, id, {"UberSplatID", "splatID", "ID", "id"});
        if (row >= 0) {
            ResolveSplatRow(uberSplatData_, row, type, id, effect);
        }
    } else if (type == "SPN" && hasSpawnData_) {
        const int row = FindRowById(spawnData_, id, {"SpawnID", "spawnID", "ID", "id"});
        if (row >= 0) {
            effect.modelPath = NormalizeMdxModelPath(GetStringAny(spawnData_, row, {"Model", "model"}));
            effect.scale = GetFloatAny(spawnData_, row, {"Scale", "scale"}, 1.0f);
            effect.resolved = !effect.modelPath.empty();
        }
    } else if (type == "SND" && hasAnimSounds_) {
        std::string soundLabel = id;
        if (hasAnimLookups_) {
            const int lookupRow = FindRowById(animLookups_, id, {"Anim", "Name", "ID", "id"});
            if (lookupRow >= 0) {
                const std::string label = GetStringAny(animLookups_, lookupRow, {"SoundLabel", "Label"});
                if (!label.empty()) {
                    soundLabel = label;
                }
            }
        }
        const int soundRow = FindRowById(animSounds_, soundLabel, {"SoundLabel", "Label", "ID", "id"});
        if (soundRow >= 0) {
            const std::string directory = GetStringAny(animSounds_, soundRow, {"DirectoryBase", "Dir"});
            for (std::string file : SplitCsv(GetStringAny(animSounds_, soundRow, {"FileNames", "FileName", "File"}))) {
                effect.soundPaths.push_back(NormalizeSlkPath(directory + file, ""));
            }
            effect.volume = GetFloatAny(animSounds_, soundRow, {"Volume"}, 127.0f) / 127.0f;
            effect.resolved = !effect.soundPaths.empty();
        }
    }

    return effect;
}

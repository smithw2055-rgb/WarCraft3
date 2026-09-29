#include "UnitSoundset.h"
#include "resource/slk.h"

const char* UnitSoundset::Kinds[6] = {"What", "Pissed", "YesAttack", "Yes", "Ready", "Warcry"};

std::vector<std::string> UnitSoundset::SplitList(const std::string& text) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t comma = text.find(',', pos);
        std::string part = (comma == std::string::npos) ? text.substr(pos)
                                                        : text.substr(pos, comma - pos);
        // Trim spaces.
        size_t b = part.find_first_not_of(" \t\r\n");
        size_t e = part.find_last_not_of(" \t\r\n");
        if (b != std::string::npos) out.push_back(part.substr(b, e - b + 1));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

UnitAckSoundDesc UnitSoundset::Resolve(const SlkTable& slk,
                                       const std::string& soundName,
                                       const std::string& kind) {
    UnitAckSoundDesc desc;
    if (soundName.empty() || kind.empty()) return desc;
    const std::string key = soundName + kind;

    // Find the row whose any cell equals the key (retail key column varies).
    int hitRow = -1;
    for (int r = 0; r < slk.Rows(); r++) {
        for (int c = 0; c < slk.Cols(); c++) {
            if (slk.GetString(r, c) == key) {
                hitRow = r;
                break;
            }
        }
        if (hitRow >= 0) break;
    }
    if (hitRow < 0) return desc;

    auto field = [&](const char* name) -> std::string {
        const int col = slk.FindColumn(name);
        if (col < 0) return "";
        return slk.GetString(hitRow, col);
    };

    const std::string fileNames = field("FileNames");
    std::string dirBase = field("DirectoryBase");
    if (dirBase.size() > 1 && dirBase.back() != '\\' && dirBase.back() != '/') {
        dirBase += "\\";
    }
    for (const auto& f : SplitList(fileNames)) {
        if (!f.empty()) desc.files.push_back(dirBase + f);
    }
    if (!field("Volume").empty()) {
        desc.volume = (float)std::atof(field("Volume").c_str()) / 127.0f;
    }
    if (!field("Pitch").empty()) {
        desc.pitch = (float)std::atof(field("Pitch").c_str());
    }
    if (!field("PitchVariance").empty()) {
        desc.pitchVariance = (float)std::atof(field("PitchVariance").c_str());
        if (desc.pitchVariance == 1.0f) desc.pitchVariance = 0.0f; // ref sentinel
    }
    if (!field("MinDistance").empty()) {
        desc.minDistance = (float)std::atof(field("MinDistance").c_str());
    }
    if (!field("MaxDistance").empty()) {
        desc.maxDistance = (float)std::atof(field("MaxDistance").c_str());
    }
    if (!field("DistanceCutoff").empty()) {
        desc.distanceCutoff = (float)std::atof(field("DistanceCutoff").c_str());
    }
    for (const auto& flag : SplitList(field("Flags"))) {
        if (flag == "LOOPING") desc.looping = true;
    }
    desc.found = true;
    return desc;
}

std::vector<UnitAckSoundDesc> UnitSoundset::ResolveAll(const SlkTable& slk,
                                                       const std::string& soundName) {
    std::vector<UnitAckSoundDesc> out;
    for (const char* kind : Kinds) {
        out.push_back(Resolve(slk, soundName, kind));
    }
    return out;
}

std::string UnitSoundset::PickFile(const UnitAckSoundDesc& desc, int index) {
    if (desc.files.empty()) return "";
    int n = (int)desc.files.size();
    int i = ((index % n) + n) % n;
    return desc.files[(size_t)i];
}

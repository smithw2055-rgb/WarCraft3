#pragma once
// UI\war3skins.txt section-aware lookup.
// Ref warsmash_ref GameUI.getSkinField/trySkinField: DecorateFileNames keys
// (e.g. File "ConsoleTexture01") resolve through the race skin section
// (e.g. [Human] ConsoleTexture01=UI\Console\Human\HumanUITile01), with a
// "<key>_V<gameVersion>" fallback before giving up.
#include "../resource/mpq_storm.h"
#include <map>
#include <string>

struct SkinData {
    std::map<std::string, std::map<std::string, std::string>> sections;
    std::string section = "Human";
    int gameVersion = 0;

    static SkinData Load(MpqManager* mpq) {
        SkinData skin;
        if (!mpq) return skin;
        std::vector<uint8_t> buf;
        if (!mpq->ReadFile("UI\\war3skins.txt", buf) || buf.empty()) return skin;
        std::string content((const char*)buf.data(), buf.size());
        std::string current;
        size_t pos = 0;
        while (pos <= content.size()) {
            size_t nl = content.find('\n', pos);
            std::string line = content.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            pos = nl == std::string::npos ? content.size() + 1 : nl + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
                line.pop_back();
            size_t first = line.find_first_not_of(" \t");
            if (first == std::string::npos) continue;
            line = line.substr(first);
            if (line.empty() || line.rfind("//", 0) == 0) continue;
            if (line.front() == '[' && line.back() == ']') {
                current = line.substr(1, line.size() - 2);
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos || current.empty()) continue;
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            const size_t kend = key.find_last_not_of(" \t");
            if (kend != std::string::npos) key.resize(kend + 1);
            skin.sections[current][key] = value;
        }
        return skin;
    }

    // Non-throwing lookup (ref trySkinField over the merged race+Default
    // skin in GameUI.loadSkin: race keys win, Default fills the rest, each
    // with a "<key>_V<gameVersion>" fallback).
    std::string TryField(const std::string& key) const {
        if (key.empty()) return key;
        const std::string versioned = key + "_V" + std::to_string(gameVersion);
        auto lookup = [&](const std::string& sec, const std::string& k) -> const std::string* {
            auto sit = sections.find(sec);
            if (sit == sections.end()) return nullptr;
            auto it = sit->second.find(k);
            return it != sit->second.end() ? &it->second : nullptr;
        };
        if (auto* v = lookup(section, key)) return *v;
        if (auto* v = lookup("Default", key)) return *v;
        if (auto* v = lookup(section, versioned)) return *v;
        if (auto* v = lookup("Default", versioned)) return *v;

        // Fallback for "Neutral" suffix (e.g. InfoPanelIconDamageNormalNeutral -> InfoPanelIconDamageNormal)
        if (key.size() > 7 && key.compare(key.size() - 7, 7, "Neutral") == 0) {
            std::string baseKey = key.substr(0, key.size() - 7);
            const std::string baseVersioned = baseKey + "_V" + std::to_string(gameVersion);
            if (auto* v = lookup(section, baseKey)) return *v;
            if (auto* v = lookup("Default", baseKey)) return *v;
            if (auto* v = lookup(section, baseVersioned)) return *v;
            if (auto* v = lookup("Default", baseVersioned)) return *v;
        }

        return key;
    }
};

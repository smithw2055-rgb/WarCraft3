#include "warsmash_config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

bool WarsmashConfig::Load(const std::string& iniPath) {
    SetDefaults();

    std::ifstream file(iniPath);
    if (!file.is_open()) {
        std::cout << "Config: " << iniPath << " not found, using defaults" << std::endl;
        return false;
    }

    std::string line;
    std::string section;
    int sourceIdx = 0;
    bool explicitJassFiles = false;

    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
            return !std::isspace(ch);
        }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    };

    auto appendJassFile = [&](std::string value) {
        trim(value);
        if (!value.empty()) {
            if (!explicitJassFiles) {
                jassFileList.clear();
                explicitJassFiles = true;
            }
            jassFileList.push_back(value);
        }
    };

    while (std::getline(file, line)) {
        trim(line);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        // Section header
        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            sourceIdx = 0;
            continue;
        }

        // Key=Value
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        trim(key);
        trim(value);

        if (section == "Emulator") {
            if (key == "MaxPlayers") maxPlayers = std::stoi(value);
            else if (key == "GameVersion") gameVersion = std::stoi(value);
            else if (key == "JassFile") {
                appendJassFile(value);
            }
            else if (key == "JassFileList") {
                std::stringstream ss(value);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    appendJassFile(item);
                }
            }
            else if (key == "TilesetDefault") tilesetDefault = value;
        }
        else if (section == "DataSources") {
            // Parse Type00=..., Path00=... pattern
            std::string idxStr = key;
            // Remove non-digit prefix (e.g., "Type" from "Type00")
            size_t digitStart = idxStr.find_first_of("0123456789");
            if (digitStart == std::string::npos) continue;
            std::string typeOrPath = idxStr.substr(0, digitStart);
            int idx = std::stoi(idxStr.substr(digitStart));

            // Ensure dataSources vector is large enough
            while ((int)dataSources.size() <= idx)
                dataSources.emplace_back("", "");

            if (typeOrPath == "Type" || typeOrPath == "type")
                dataSources[idx].first = value;
            else if (typeOrPath == "Path" || typeOrPath == "path")
                dataSources[idx].second = value;
        }
        else if (section == "GamingNetwork") {
            if (key == "Server") server = value;
            else if (key == "MapDownloadDir") mapDownloadDir = value;
        }
    }

    // Remove empty datasources
    dataSources.erase(
        std::remove_if(dataSources.begin(), dataSources.end(),
            [](const auto& p) { return p.first.empty() && p.second.empty(); }),
        dataSources.end());

    std::cout << "Config loaded: " << dataSources.size() << " datasources, "
              << "maxPlayers=" << maxPlayers
              << ", gameVersion=" << gameVersion
              << ", jassFiles=" << jassFileList.size()
              << std::endl;

    return true;
}

void WarsmashConfig::SetDefaults() {
    maxPlayers = 28;
    gameVersion = 1;
    jassFileList = { "Scripts\\common.j", "Scripts\\Blizzard.j", "Scripts\\war3map.j" };
    dataSources.clear();
    server = "localhost";
    mapDownloadDir = "Maps/Download";
    tilesetDefault = "L";
    localPlayerIndex = -1;
}

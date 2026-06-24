#pragma once
#include <string>
#include <vector>
#include <utility>

struct WarsmashConfig {
    int maxPlayers = 28;
    int gameVersion = 1;
    std::vector<std::string> jassFileList;

    // [DataSources] entries: {type, path}
    std::vector<std::pair<std::string, std::string>> dataSources;

    // [GamingNetwork]
    std::string server = "localhost";
    std::string mapDownloadDir = "Maps/Download";

    // [Emulator] parsed fields (exposed for consumer use)
    std::string tilesetDefault = "L";
    int localPlayerIndex = -1;

    bool Load(const std::string& iniPath);
    void SetDefaults();
};

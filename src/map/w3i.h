#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct W3iPlayer {
    int id = 0;
    int type = 0; // 1 human, 2 computer, ...
    int race = 0; // 1 human, 2 orc, 3 undead, 4 nightelf
    int fixedStartPosition = 0;
    std::string name;
    float startX = 0.0f;
    float startY = 0.0f;
    uint32_t allyLowPrio = 0;
    uint32_t allyHighPrio = 0;
    uint32_t enemyLowFlags = 0;
    uint32_t enemyHighFlags = 0;
};

struct W3iForce {
    uint32_t flags = 0;
    uint32_t playerMask = 0;
    std::string name;
};

struct W3iUpgradeAvailability {
    uint32_t playerFlags = 0;
    uint32_t id = 0; // War3ID upgrade
    int levelAffected = 0;
    int availability = 0;
};

struct W3iTechAvailability {
    uint32_t playerFlags = 0;
    uint32_t id = 0; // War3ID tech
};

struct W3iRandomUnitEntry {
    int chance = 0;
    std::vector<uint32_t> ids; // War3ID per position
};

struct W3iRandomUnitTable {
    int id = 0;
    std::string name;
    int positions = 0;
    std::vector<int> columnTypes;
    std::vector<W3iRandomUnitEntry> units;
};

struct W3iRandomItemEntry {
    int chance = 0;
    uint32_t id = 0; // War3ID item
};

struct W3iRandomItemTable {
    uint32_t id = 0; // War3ID
    std::string name;
    std::vector<std::vector<W3iRandomItemEntry>> sets;
};

struct W3iInfo {
    int version = 0;
    int saves = 0;
    int gameVersionMajor = 0;
    int gameVersionMinor = 0;
    int gameVersionPatch = 0;
    int gameVersionCommit = 0;
    std::string name;
    std::string author;
    std::string description;
    std::string recommendedPlayers;
    float cameraBounds[8] = {};
    int cameraComplements[4] = {};
    int playableWidth = 0;
    int playableHeight = 0;
    char tileset = 'A';
    int gameDataSet = 0;
    std::string loadingScreenModel;
    std::string loadingScreenText;
    std::string loadingScreenTitle;
    std::string loadingScreenSubtitle;
    std::string prologueScreenModel;
    std::string prologueScreenText;
    std::string prologueScreenTitle;
    std::string prologueScreenSubtitle;
    int useTerrainFog = 0;
    float fogHeight[2] = {};
    float fogDensity = 0.0f;
    uint8_t fogColor[4] = {};
    int globalWeather = 0;
    std::string soundEnvironment;
    char lightEnvironmentTileset = 0;
    uint8_t waterVertexColor[4] = {};
    std::vector<W3iPlayer> players;
    std::vector<W3iForce> forces;
    std::vector<W3iUpgradeAvailability> upgradeAvailability;
    std::vector<W3iTechAvailability> techAvailability;
    std::vector<W3iRandomUnitTable> randomUnitTables;
    std::vector<W3iRandomItemTable> randomItemTables;
};

bool W3ILoad(const uint8_t* data, size_t size, W3iInfo& out);
bool W3ILoadFromMpq(const std::string& mapPath, W3iInfo& out);
void W3IPrint(const W3iInfo& info);

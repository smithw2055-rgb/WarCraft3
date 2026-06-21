#pragma once
#include <string>
#include <vector>
#include <cstdint>

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
};

bool W3ILoad(const uint8_t* data, size_t size, W3iInfo& out);
bool W3ILoadFromMpq(const std::string& mapPath, W3iInfo& out);
void W3IPrint(const W3iInfo& info);

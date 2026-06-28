#pragma once

#include <string>
#include <vector>

namespace War3UI {

struct GlueMapListItem {
    std::string path;
    std::string fileName;
    std::string displayName;
    std::string author;
    std::string description;
    std::string recommendedPlayers;
    std::string mapSize;
    std::string tileset;
    int playerSlots = 0;
};

struct GlueCampaignMission {
    std::string header;
    std::string name;
    std::string mapPath;
};

struct GlueCampaignListItem {
    std::string id;
    std::string header;
    std::string name;
    std::vector<GlueCampaignMission> missions;
};

} // namespace War3UI

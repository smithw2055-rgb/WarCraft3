#include "w3x_storm.h"
#include "w3e.h"
#include "resource/mpq_storm.h"
#include <iostream>
#include <vector>

bool W3XLoadTerrain(const std::string& mapPath, W3EMap& out) {
    std::vector<uint8_t> buffer;
    if (!MpqManager::ReadSingleArchiveFile(mapPath, "war3map.w3e", buffer)) {
        std::cerr << "W3X: war3map.w3e unavailable in " << mapPath << std::endl;
        return false;
    }
    return W3ELoad(buffer.data(), buffer.size(), out);
}

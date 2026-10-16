#pragma once

#include <filesystem>
#include <string>

class MpqManager;

bool ResolveMapPathForLoad(const std::filesystem::path& resourcesDir,
                           const MpqManager& mpq,
                           const std::string& requestedPath,
                           std::string& resolvedPath);

// Portrait model rule (ref RenderWidgetTypeData.getPortraitModel):
// "<base>_portrait.mdx" when the data source carries it, else the unit
// model itself. Pure string transform; existence probing stays with the
// caller (SceneManager::PortraitPathFor).
inline std::string PortraitPathForModel(const std::string& modelPath) {
    if (modelPath.empty()) return {};
    const size_t dot = modelPath.find_last_of('.');
    const std::string base = (dot == std::string::npos) ? modelPath : modelPath.substr(0, dot);
    return base + "_portrait.mdx";
}

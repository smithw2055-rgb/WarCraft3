#pragma once

#include <filesystem>
#include <string>

class MpqManager;

bool ResolveMapPathForLoad(const std::filesystem::path& resourcesDir,
                           const MpqManager& mpq,
                           const std::string& requestedPath,
                           std::string& resolvedPath);

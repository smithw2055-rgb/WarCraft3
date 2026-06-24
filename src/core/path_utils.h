#pragma once
#include <string>
#include <filesystem>

namespace path_utils {

// Resolve a relative configured path against project root and resources dir.
// Precedence: project/body -> resources/body -> resources/filename -> project/body.
std::filesystem::path ResolveConfiguredPath(const std::filesystem::path& projectRoot,
                                            const std::filesystem::path& resourcesDir,
                                            const std::string& configuredPath);

// Normalize a metadata path: forward slashes -> backslashes; append ".blp" if no extension.
std::string NormalizeMetadataPath(std::string path);

// Trim leading/trailing whitespace (space/tab/cr/lf).
std::string TrimMetadataValue(std::string value);

// Return the trailing path component (basename).
std::string BaseName(const std::string& path);

// Lowercase a copy of a string.
std::string ToLowerCopy(std::string value);

} // namespace path_utils
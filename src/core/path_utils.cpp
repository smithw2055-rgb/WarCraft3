#include "path_utils.h"
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace path_utils {

fs::path ResolveConfiguredPath(const fs::path& projectRoot,
                               const fs::path& resourcesDir,
                               const std::string& configuredPath) {
    fs::path path(configuredPath);
    if (path.is_absolute()) {
        return path;
    }

    const fs::path fromProject = projectRoot / path;
    if (fs::exists(fromProject)) {
        return fromProject;
    }

    const fs::path fromResources = resourcesDir / path;
    if (fs::exists(fromResources)) {
        return fromResources;
    }

    const fs::path fromResourceFileName = resourcesDir / path.filename();
    if (fs::exists(fromResourceFileName)) {
        return fromResourceFileName;
    }

    return fromProject;
}

std::string NormalizeMetadataPath(std::string path) {
    for (char& c : path) {
        if (c == '/') {
            c = '\\';
        }
    }
    if (!path.empty() && path.find('.') == std::string::npos) {
        path += ".blp";
    }
    return path;
}

std::string TrimMetadataValue(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string BaseName(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

} // namespace path_utils
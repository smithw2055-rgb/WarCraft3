#include "map_path_resolver.h"

#include "resource/mpq_storm.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

std::string NormalizeMpqArchivePath(std::string value) {
    for (char& c : value) {
        if (c == '/') c = '\\';
    }
    while (!value.empty() && (value.front() == '\\' || value.front() == '/')) {
        value.erase(value.begin());
    }
    return value;
}

bool LooksLikeMapArchivePath(const std::string& value) {
    const std::string lower = LowerAscii(value);
    return lower.size() >= 4
        && (lower.rfind(".w3m") == lower.size() - 4
            || lower.rfind(".w3x") == lower.size() - 4);
}

std::string TempMapFileNameForMpqPath(const std::string& mpqPath) {
    std::string name = "warsmash_mpq_map_";
    for (char c : mpqPath) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '.' || c == '-' || c == '_') {
            name.push_back(c);
        } else {
            name.push_back('_');
        }
    }
    return name;
}

bool MaterializeMapFromOpenMpqs(const MpqManager& mpq, const std::string& requestedPath,
                                std::string& outPath) {
    const std::string mpqPath = NormalizeMpqArchivePath(requestedPath);
    if (!LooksLikeMapArchivePath(mpqPath)) {
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!mpq.ReadFile(mpqPath, bytes) || bytes.empty()) {
        return false;
    }

    const fs::path tempPath = fs::temp_directory_path() / TempMapFileNameForMpqPath(mpqPath);
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Map path: failed to write temporary MPQ map " << tempPath.string() << std::endl;
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out.good()) {
        std::cerr << "Map path: failed while writing temporary MPQ map " << tempPath.string() << std::endl;
        return false;
    }

    outPath = tempPath.string();
    std::cout << "Map path: materialized MPQ map '" << mpqPath
              << "' -> " << outPath
              << " bytes=" << bytes.size() << std::endl;
    return true;
}

} // namespace

bool ResolveMapPathForLoad(const fs::path& resourcesDir,
                           const MpqManager& mpq,
                           const std::string& requestedPath,
                           std::string& resolvedPath) {
    resolvedPath.clear();
    if (requestedPath.empty()) {
        return false;
    }

    const fs::path requestedFsPath(requestedPath);
    if (fs::exists(requestedFsPath)) {
        resolvedPath = requestedFsPath.string();
        return true;
    }

    if (requestedFsPath.is_relative()) {
        const fs::path resourcesPath = resourcesDir / requestedFsPath;
        if (fs::exists(resourcesPath)) {
            resolvedPath = resourcesPath.string();
            return true;
        }

        return MaterializeMapFromOpenMpqs(mpq, requestedPath, resolvedPath);
    }

    return false;
}

#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>
#ifdef far
#undef far
#endif
#ifdef near
#undef near
#endif
#include "map/w3e.h"
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <cstring>
#include <cmath>
#include <regex>
#include "map/unitsdoo.h"
#include "map/w3i.h"
#include "map/object_mods.h"
#include "core/map_path_resolver.h"
#include "model/mdx.h"
#include "render/mdx_animator.h"
#include "render/mdx_event_effects.h"
#include "render/mdx_sequence_select.h"
#include "render/mdx_skinning.h"
#include "resource/mpq_storm.h"
#include "resource/slk.h"

namespace fs = std::filesystem;

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

static const char* G_PROJECT_SOURCE_DIR = PROJECT_SOURCE_DIR;

static fs::path ResourcePath(const char* filename) {
    return fs::path(G_PROJECT_SOURCE_DIR) / "resources" / filename;
}

static constexpr uint32_t MakeMdxTag(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16)
         | (uint32_t(uint8_t(c)) << 8) | uint32_t(uint8_t(d));
}

static const uint32_t TAG_KTAT = MakeMdxTag('K', 'T', 'A', 'T');
static const uint32_t TAG_KTAR = MakeMdxTag('K', 'T', 'A', 'R');
static const uint32_t TAG_KTAS = MakeMdxTag('K', 'T', 'A', 'S');
static const uint32_t TAG_KGTR = MakeMdxTag('K', 'G', 'T', 'R');
static const uint32_t TAG_KGRT = MakeMdxTag('K', 'G', 'R', 'T');
static const uint32_t TAG_KGSC = MakeMdxTag('K', 'G', 'S', 'C');
static const uint32_t TAG_KGAC = MakeMdxTag('K', 'G', 'A', 'C');
static const uint32_t TAG_KATV = MakeMdxTag('K', 'A', 'T', 'V');
static const uint32_t TAG_KEVT = MakeMdxTag('K', 'E', 'V', 'T');

static std::vector<std::string> CollectAllCampaignMissionMapPaths();

struct FileEntry {
    std::string filename;
    uint32_t fileSize;
    uint32_t packedSize;
    uint32_t flags;
    bool isEncrypted;
    bool isCompressed;
};

struct ArchiveStats {
    std::string archiveName;
    bool opened = false;
    size_t totalFiles = 0;
    size_t encryptedFiles = 0;
    size_t compressedFiles = 0;
    size_t totalSize = 0;
    size_t packedSize = 0;
    std::vector<FileEntry> entries;
};

static const char* GetArchiveType(const std::string& filename) {
    if (filename.find("war3.mpq") != std::string::npos) return "Base Game";
    if (filename.find("war3x.mpq") != std::string::npos) return "Expansion";
    if (filename.find("War3Patch.mpq") != std::string::npos) return "Patch";
    if (filename.find(".w3x") != std::string::npos) return "Map (W3X)";
    if (filename.find(".w3m") != std::string::npos) return "Map (W3M)";
    return "Unknown";
}

static std::string FormatSize(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        unit++;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %s", size, units[unit]);
    return std::string(buf);
}

bool TestStormLibEnumerateWar3Mpq() {
    std::cout << "\n=== StormLib MPQ Enumeration Test ===\n" << std::endl;
    
    const char* archiveNames[] = {
        "war3.mpq",
        "War3Patch.mpq",
    };
    
    for (const char* archiveName : archiveNames) {
        fs::path archivePath = ResourcePath(archiveName);
        if (!fs::exists(archivePath)) {
            std::cout << "[SKIP] " << archiveName << " not found" << std::endl;
            continue;
        }
        
        ArchiveStats stats;
        stats.archiveName = archiveName;
        
        HANDLE hMpq = nullptr;
        if (!SFileOpenArchive(archivePath.string().c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
            std::cerr << "[ERROR] Failed to open " << archiveName << std::endl;
            continue;
        }
        
        stats.opened = true;
        std::cout << "\n--- " << archiveName << " ---" << std::endl;
        std::cout << "Type: " << GetArchiveType(archiveName) << std::endl;
        
        // Enumerate files using SFileFindFirstFile/SFileFindNextFile
        SFILE_FIND_DATA findData;
        HANDLE hFind = SFileFindFirstFile(hMpq, "*", &findData, nullptr);
        if (hFind == nullptr) {
            std::cerr << "[ERROR] Failed to enumerate files in " << archiveName << std::endl;
            SFileCloseArchive(hMpq);
            continue;
        }
        
        do {
            FileEntry entry;
            entry.filename = findData.cFileName;
            entry.fileSize = findData.dwFileSize;
            entry.packedSize = findData.dwCompSize;
            entry.flags = findData.dwFileFlags;
            entry.isEncrypted = (entry.flags & MPQ_FILE_ENCRYPTED) != 0;
            entry.isCompressed = (entry.flags & MPQ_FILE_COMPRESS) != 0;
            
            stats.entries.push_back(entry);
            stats.totalFiles++;
            stats.totalSize += entry.fileSize;
            stats.packedSize += entry.packedSize;
            
            if (entry.isEncrypted) stats.encryptedFiles++;
            if (entry.isCompressed) stats.compressedFiles++;
            
        } while (SFileFindNextFile(hFind, &findData));
        
        SFileFindClose(hFind);
        SFileCloseArchive(hMpq);
        
        // Print summary
        std::cout << "Total files: " << stats.totalFiles << std::endl;
        std::cout << "Encrypted files: " << stats.encryptedFiles << std::endl;
        std::cout << "Compressed files: " << stats.compressedFiles << std::endl;
        std::cout << "Total size: " << FormatSize(stats.totalSize) << std::endl;
        std::cout << "Packed size: " << FormatSize(stats.packedSize) << std::endl;
        
        // Print first 30 files
        std::cout << "\nFirst 30 files:" << std::endl;
        size_t printCount = std::min(stats.entries.size(), size_t(30));
        for (size_t i = 0; i < printCount; ++i) {
            const auto& e = stats.entries[i];
            std::cout << "  [" << (e.isEncrypted ? "E" : " ") << (e.isCompressed ? "C" : " ") << "] "
                      << FormatSize(e.fileSize) << " " << e.filename << std::endl;
        }
        
        // Categorize files
        size_t jFileCount = 0;
        size_t slkFileCount = 0;
        size_t mdlxFileCount = 0;
        size_t blpFileCount = 0;
        size_t wavFileCount = 0;
        size_t w3eFileCount = 0;
        size_t w3uFileCount = 0;
        size_t w3dFileCount = 0;
        size_t otherCount = 0;
        
        for (const auto& e : stats.entries) {
            std::string ext = "";
            size_t dotPos = e.filename.rfind('.');
            if (dotPos != std::string::npos) {
                ext = e.filename.substr(dotPos);
            }
            
            if (ext == ".j") jFileCount++;
            else if (ext == ".slk") slkFileCount++;
            else if (ext == ".mdx" || ext == ".mdl") mdlxFileCount++;
            else if (ext == ".blp") blpFileCount++;
            else if (ext == ".wav") wavFileCount++;
            else if (ext == ".w3e") w3eFileCount++;
            else if (ext == ".w3u" || ext == ".w3t" || ext == ".w3a") w3uFileCount++;
            else if (ext == ".w3d" || ext == ".w3q" || ext == ".w3b") w3dFileCount++;
            else otherCount++;
        }
        
        std::cout << "\nFile categories:" << std::endl;
        std::cout << "  Scripts (.j): " << jFileCount << std::endl;
        std::cout << "  Data tables (.slk): " << slkFileCount << std::endl;
        std::cout << "  Models (.mdx/.mdl): " << mdlxFileCount << std::endl;
        std::cout << "  Textures (.blp): " << blpFileCount << std::endl;
        std::cout << "  Audio (.wav): " << wavFileCount << std::endl;
        std::cout << "  Terrain (.w3e): " << w3eFileCount << std::endl;
        std::cout << "  Units/Abilities (.w3u/.w3t/.w3a): " << w3uFileCount << std::endl;
        std::cout << "  Doodads (.w3d/.w3q/.w3b): " << w3dFileCount << std::endl;
        std::cout << "  Other: " << otherCount << std::endl;
    }
    
    return true;
}

bool TestStormLibExtractSamples() {
    std::cout << "\n=== StormLib File Extraction Test ===\n" << std::endl;
    
    const char* archiveName = "war3.mpq";
    fs::path archivePath = ResourcePath(archiveName);
    
    if (!fs::exists(archivePath)) {
        std::cout << "[SKIP] " << archiveName << " not found" << std::endl;
        return true;
    }
    
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(archivePath.string().c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        std::cerr << "[ERROR] Failed to open " << archiveName << std::endl;
        return false;
    }
    
    // Sample files to extract
    const char* sampleFiles[] = {
        "Scripts\\common.j",
        "Scripts\\Blizzard.j",
        "units\\unitdata.slk",
        "war3map.w3e",
    };
    
    for (const char* sampleFile : sampleFiles) {
        HANDLE hFile = nullptr;
        if (!SFileOpenFileEx(hMpq, sampleFile, SFILE_OPEN_BASE_FILE, &hFile)) {
            std::cout << "[SKIP] " << sampleFile << " not found in archive" << std::endl;
            continue;
        }
        
        DWORD fileSize = SFileGetFileSize(hFile, nullptr);
        std::vector<uint8_t> buffer(fileSize);
        DWORD bytesRead = 0;
        
        if (!SFileReadFile(hFile, buffer.data(), fileSize, &bytesRead, nullptr)) {
            std::cerr << "[ERROR] Failed to read " << sampleFile << std::endl;
            SFileCloseFile(hFile);
            continue;
        }
        
        SFileCloseFile(hFile);
        
        std::cout << "[OK] " << sampleFile << " - " << FormatSize(bytesRead);
        
        // Print first few bytes as hex
        std::cout << " - Header: ";
        for (int i = 0; i < std::min(16, (int)bytesRead); ++i) {
            printf("%02X ", buffer[i]);
        }
        
        // Try to detect file type
        if (bytesRead >= 4) {
            if (std::memcmp(buffer.data(), "W3E!", 4) == 0) {
                std::cout << "(W3E terrain)";
            } else if (std::memcmp(buffer.data(), "MDLX", 4) == 0) {
                std::cout << "(MDLX model)";
            } else if (buffer[0] == 0x89 && buffer[1] == 'P' && buffer[2] == 'N' && buffer[3] == 'G') {
                std::cout << "(PNG image)";
            } else if (buffer[0] == 'B' && buffer[1] == 'L' && buffer[2] == 'P' && buffer[3] == '1' ||
                       buffer[0] == 'B' && buffer[1] == 'L' && buffer[2] == 'P' && buffer[3] == '2') {
                std::cout << "(BLP texture)";
            }
        }
        
        std::cout << std::endl;
    }
    
    SFileCloseArchive(hMpq);
    return true;
}

bool TestStormLibW3xMaps() {
    std::cout << "\n=== StormLib W3X/W3M Map Test ===\n" << std::endl;
    
    std::vector<fs::path> mapFiles;
    fs::path resourcesDir = ResourcePath("");
    
    if (!fs::exists(resourcesDir)) {
        std::cout << "[SKIP] resources directory not found" << std::endl;
        return true;
    }
    
    // Find all .w3x and .w3m files
    for (const auto& entry : fs::recursive_directory_iterator(resourcesDir)) {
        std::string ext = entry.path().extension().string();
        if (ext == ".w3x" || ext == ".w3m") {
            mapFiles.push_back(entry.path());
        }
    }
    
    if (mapFiles.empty()) {
        std::cout << "[SKIP] No W3X/W3M map files found" << std::endl;
        return true;
    }
    
    for (const auto& mapPath : mapFiles) {
        HANDLE hMpq = nullptr;
        if (!SFileOpenArchive(mapPath.string().c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
            std::cerr << "[ERROR] Failed to open " << mapPath.filename().string() << std::endl;
            continue;
        }
        
        std::cout << "\n--- " << mapPath.filename().string() << " ---" << std::endl;
        
        // Try to open war3map.w3e
        HANDLE hFile = nullptr;
        if (SFileOpenFileEx(hMpq, "war3map.w3e", SFILE_OPEN_BASE_FILE, &hFile)) {
            DWORD fileSize = SFileGetFileSize(hFile, nullptr);
            std::vector<uint8_t> buffer(fileSize);
            DWORD bytesRead = 0;
            
            if (SFileReadFile(hFile, buffer.data(), buffer.size(), &bytesRead, nullptr)) {
                std::cout << "war3map.w3e: " << FormatSize(fileSize);
                
                W3EMap parsed{};
                if (bytesRead == fileSize && W3ELoad(buffer.data(), buffer.size(), parsed)) {
                    std::cout << " - Version " << parsed.version << ", Tileset '" << parsed.tileset << "', Size "
                              << parsed.tileWidth << "x" << parsed.tileHeight;
                } else {
                    std::cout << " - unable to parse header";
                }
                std::cout << std::endl;
            }
            
            SFileCloseFile(hFile);
        } else {
            std::cout << "war3map.w3e: not found" << std::endl;
        }
        
        // Count file types in map
        size_t jFileCount = 0;
        size_t w3uCount = 0;
        size_t w3dCount = 0;
        size_t w3rCount = 0;
        size_t w3cCount = 0;
        size_t w3sCount = 0;
        size_t w3iCount = 0;
        size_t impCount = 0;
        size_t otherCount = 0;
        
        SFILE_FIND_DATA findData;
        HANDLE hFind = SFileFindFirstFile(hMpq, "*", &findData, nullptr);
        if (hFind) {
            do {
                std::string filename = findData.cFileName;
                size_t dotPos = filename.rfind('.');
                std::string ext = (dotPos != std::string::npos) ? filename.substr(dotPos) : "";
                
                if (ext == ".j") jFileCount++;
                else if (ext == ".w3u" || ext == ".w3t" || ext == ".w3a") w3uCount++;
                else if (ext == ".w3d" || ext == ".w3q" || ext == ".w3b") w3dCount++;
                else if (ext == ".w3r") w3rCount++;
                else if (ext == ".w3c") w3cCount++;
                else if (ext == ".w3s") w3sCount++;
                else if (ext == ".w3i") w3iCount++;
                else if (ext == ".imp") impCount++;
                else otherCount++;
                
            } while (SFileFindNextFile(hFind, &findData));
            
            SFileFindClose(hFind);
        }
        
        std::cout << "Files: J=" << jFileCount << " Units=" << w3uCount << " Doodads=" << w3dCount
                  << " Regions=" << w3rCount << " Cameras=" << w3cCount << " Sounds=" << w3sCount
                  << " Info=" << w3iCount << " Imports=" << impCount << " Other=" << otherCount << std::endl;
        
        SFileCloseArchive(hMpq);
    }
    
    return true;
}

static bool ReadFileFromDefaultMpqs(const char* mpqPath, std::vector<uint8_t>& out) {
    const char* archiveNames[] = {
        "War3Patch.mpq",
        "War3xLocal.mpq",
        "war3x.mpq",
        "war3.mpq",
    };

    for (const char* archiveName : archiveNames) {
        const fs::path archivePath = ResourcePath(archiveName);
        if (!fs::exists(archivePath)) {
            continue;
        }

        HANDLE hMpq = nullptr;
        if (!SFileOpenArchive(archivePath.string().c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
            continue;
        }

        HANDLE hFile = nullptr;
        if (SFileOpenFileEx(hMpq, mpqPath, SFILE_OPEN_BASE_FILE, &hFile)) {
            const DWORD fileSize = SFileGetFileSize(hFile, nullptr);
            out.resize(fileSize);
            DWORD bytesRead = 0;
            const bool ok = SFileReadFile(hFile, out.data(), fileSize, &bytesRead, nullptr)
                && bytesRead == fileSize;
            SFileCloseFile(hFile);
            SFileCloseArchive(hMpq);
            if (ok) {
                std::cout << "[OK] " << mpqPath << " from " << archiveName
                          << " - " << FormatSize(fileSize) << std::endl;
                return true;
            }
            out.clear();
            return false;
        }

        SFileCloseArchive(hMpq);
    }

    return false;
}

static bool ReadFileFromMpqArchive(const fs::path& archivePath, const char* mpqPath, std::vector<uint8_t>& out) {
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(archivePath.string().c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) {
        return false;
    }

    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, mpqPath, SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq);
        return false;
    }

    const DWORD fileSize = SFileGetFileSize(hFile, nullptr);
    out.resize(fileSize);
    DWORD bytesRead = 0;
    const bool ok = SFileReadFile(hFile, out.data(), fileSize, &bytesRead, nullptr)
        && bytesRead == fileSize;
    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);
    if (!ok) {
        out.clear();
    }
    return ok;
}

static void ExtractJassNativeDeclarations(const std::string& source, std::unordered_set<std::string>& out) {
    static const std::regex pattern(R"((^|[\r\n])\s*(?:constant\s+)?native\s+([A-Za-z_][A-Za-z0-9_]*)\b)");
    for (std::sregex_iterator it(source.begin(), source.end(), pattern), end; it != end; ++it) {
        out.insert((*it)[2].str());
    }
}

static std::unordered_map<std::string, int> ExtractJassCallCounts(const std::string& source) {
    std::unordered_map<std::string, int> counts;
    static const std::regex pattern(R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*\()");
    for (std::sregex_iterator it(source.begin(), source.end(), pattern), end; it != end; ++it) {
        counts[(*it)[1].str()]++;
    }
    return counts;
}

static bool ExtractRegisteredJassNatives(std::unordered_set<std::string>& out) {
    const fs::path scriptingDir = fs::path(G_PROJECT_SOURCE_DIR) / "src" / "scripting";
    const std::vector<fs::path> sourcePaths = {
        scriptingDir / "JassNatives.cpp",
        scriptingDir / "natives" / "jass_natives_units.cpp",
        scriptingDir / "natives" / "jass_natives_triggers.cpp",
        scriptingDir / "natives" / "jass_natives_environment.cpp",
        scriptingDir / "natives" / "jass_natives_ui_data.cpp",
    };
    static const std::regex pattern(R"((?:RegisterNative(?:Auto)?|Bind)\s*\(\s*(?:[A-Za-z0-9_]+\s*,\s*)?\"([A-Za-z_][A-Za-z0-9_]*)\")");
    for (const fs::path& sourcePath : sourcePaths) {
        std::ifstream in(sourcePath, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "[ERROR] Failed to read " << sourcePath.string() << std::endl;
            return false;
        }
        const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        for (std::sregex_iterator it(source.begin(), source.end(), pattern), end; it != end; ++it) {
            out.insert((*it)[1].str());
        }
    }
    return !out.empty();
}

static bool OpenDefaultMpqs(MpqManager& mpq) {
    const char* archiveNames[] = {
        "War3Patch.mpq",
        "War3xLocal.mpq",
        "war3x.mpq",
        "war3.mpq",
    };

    bool openedAny = false;
    for (const char* archiveName : archiveNames) {
        const fs::path archivePath = ResourcePath(archiveName);
        if (fs::exists(archivePath) && mpq.Open(archivePath.string())) {
            openedAny = true;
        }
    }
    return openedAny;
}

static std::string War3IdHighString(uint32_t id) {
    char text[5] = {
        static_cast<char>((id >> 24) & 0xFF),
        static_cast<char>((id >> 16) & 0xFF),
        static_cast<char>((id >> 8) & 0xFF),
        static_cast<char>(id & 0xFF),
        0
    };
    return text;
}

static bool IsKnownNonRenderableUnitId(const std::string& id) {
    return id == "sloc";
}

static std::string NormalizeModelPath(std::string path) {
    for (char& c : path) {
        if (c == '/') c = '\\';
    }
    if (path.size() >= 4) {
        std::string tail = path.substr(path.size() - 4);
        for (char& c : tail) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (tail == ".mdx" || tail == ".mdl") {
            path.resize(path.size() - 4);
        }
    }
    return path + ".mdx";
}

static std::string TrimString(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::string ToLowerString(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

static std::string ObjectModificationValueString(const ObjectModification& mod) {
    switch (mod.varType) {
    case ObjectModification::STRING:
        return TrimString(mod.stringValue);
    case ObjectModification::INT:
    case ObjectModification::BOOLEAN:
        return std::to_string(mod.intValue);
    case ObjectModification::REAL:
    case ObjectModification::UNREAL:
    default:
        return std::to_string(mod.realValue);
    }
}

static bool WriteTempBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

static bool LoadUnitModelPathTable(std::unordered_map<std::string, std::string>& out) {
    std::vector<uint8_t> bytes;
    if (!ReadFileFromDefaultMpqs("Units\\unitUI.slk", bytes)) {
        std::cerr << "[ERROR] Failed to read Units\\unitUI.slk" << std::endl;
        return false;
    }

    SlkTable slk;
    if (!slk.Load(bytes.data(), bytes.size())) {
        std::cerr << "[ERROR] Failed to parse Units\\unitUI.slk" << std::endl;
        return false;
    }

    const int idCol = slk.FindColumn("unitUIID");
    const int fileCol = slk.FindColumn("file");
    if (idCol < 0 || fileCol < 0) {
        std::cerr << "[ERROR] Units\\unitUI.slk missing unitUIID/file columns" << std::endl;
        return false;
    }

    for (int row = 0; row < slk.Rows(); ++row) {
        const std::string id = slk.GetString(row, idCol);
        const std::string file = slk.GetString(row, fileCol);
        if (!id.empty() && !file.empty()) {
            out[id] = NormalizeModelPath(file);
        }
    }

    std::cout << "Unit model table entries=" << out.size() << std::endl;
    return !out.empty();
}

static bool ApplyCampaignUnitModelPathOverrides(
    const std::string& mapPath,
    std::unordered_map<std::string, std::string>& modelPaths) {
    ObjectModificationSet mods;
    if (!LoadWar3MapUnitMods(mapPath, mods)) {
        return false;
    }

    std::vector<uint8_t> metaBytes;
    if (!ReadFileFromDefaultMpqs("Units\\UnitMetaData.slk", metaBytes)) {
        std::cout << "Campaign unit model overrides: UnitMetaData read=no" << std::endl;
        return false;
    }

    SlkTable meta;
    if (!meta.Load(metaBytes.data(), metaBytes.size())) {
        std::cout << "Campaign unit model overrides: UnitMetaData parse=no" << std::endl;
        return false;
    }

    const int idCol = meta.FindColumn("ID");
    const int slkCol = meta.FindColumn("slk");
    const int fieldCol = meta.FindColumn("field");
    if (idCol < 0 || slkCol < 0 || fieldCol < 0) {
        std::cout << "Campaign unit model overrides: UnitMetaData columns missing" << std::endl;
        return false;
    }

    std::unordered_set<std::string> modelFileMetaIds;
    for (int row = 0; row < meta.Rows(); ++row) {
        const std::string id = TrimString(meta.GetString(row, idCol));
        const std::string slk = ToLowerString(TrimString(meta.GetString(row, slkCol)));
        const std::string field = ToLowerString(TrimString(meta.GetString(row, fieldCol)));
        if (!id.empty()
            && field == "file"
            && slk.find("unitui") != std::string::npos) {
            modelFileMetaIds.insert(id);
        }
    }

    int customCloned = 0;
    int customSkipped = 0;
    int modelOverrides = 0;

    auto applyEntry = [&](const ObjectModificationEntry& entry, const std::string& id) {
        for (const ObjectModification& mod : entry.modifications) {
            if (modelFileMetaIds.find(mod.id.ToString()) == modelFileMetaIds.end()) {
                continue;
            }
            const std::string file = ObjectModificationValueString(mod);
            if (file.empty()) {
                continue;
            }
            modelPaths[id] = NormalizeModelPath(file);
            ++modelOverrides;
        }
    };

    for (const ObjectModificationEntry& entry : mods.custom) {
        const std::string oldId = entry.oldId.ToString();
        const std::string newId = entry.newId.ToString();
        const auto oldPath = modelPaths.find(oldId);
        if (oldPath == modelPaths.end()) {
            ++customSkipped;
            continue;
        }
        modelPaths[newId] = oldPath->second;
        ++customCloned;
        applyEntry(entry, newId);
    }

    for (const ObjectModificationEntry& entry : mods.original) {
        applyEntry(entry, entry.oldId.ToString());
    }

    std::cout << "Campaign unit model overrides: original=" << mods.original.size()
              << " custom=" << mods.custom.size()
              << " cloned=" << customCloned
              << " skipped=" << customSkipped
              << " file_overrides=" << modelOverrides << std::endl;
    return customCloned > 0 || modelOverrides > 0;
}

struct CampaignUnitModelProbe {
    std::string mapPath;
    std::string unitId;
    std::string modelPath;
    MdxModel model;
};

static std::string TempSafeMapName(const std::string& mapPath) {
    std::string name = mapPath;
    for (char& c : name) {
        if (c == '\\' || c == '/' || c == ':' || c == ' ') {
            c = '_';
        }
    }
    return name;
}

static bool LoadCampaignInitialUnitModelsForMap(
    const std::string& campaignPath,
    const std::unordered_map<std::string, std::string>& modelPaths,
    std::vector<CampaignUnitModelProbe>& out,
    int maxModels) {
    std::vector<uint8_t> campaignBytes;
    if (!ReadFileFromDefaultMpqs(campaignPath.c_str(), campaignBytes)) {
        std::cerr << "[ERROR] Failed to read campaign map " << campaignPath << std::endl;
        return false;
    }

    const fs::path tempPath = fs::temp_directory_path() / ("warsmash_campaign_units_" + TempSafeMapName(campaignPath));
    if (!WriteTempBytes(tempPath, campaignBytes)) {
        std::cerr << "[ERROR] Failed to write temp campaign map " << tempPath.string() << std::endl;
        return false;
    }

    std::unordered_map<std::string, std::string> effectiveModelPaths = modelPaths;
    ApplyCampaignUnitModelPathOverrides(tempPath.string(), effectiveModelPaths);

    UnitsDooData unitsDoo;
    const bool unitsOk = UnitsDooLoadFromMpq(tempPath.string(), unitsDoo, nullptr);
    fs::remove(tempPath);
    if (!unitsOk) {
        std::cerr << "[ERROR] Failed to load campaign war3mapUnits.doo" << std::endl;
        return false;
    }

    std::cout << "Campaign initial units map=" << campaignPath
              << " units=" << unitsDoo.units.size() << std::endl;
    if (unitsDoo.units.empty()) {
        return false;
    }

    std::unordered_set<uint32_t> seenUnitIds;
    for (const UnitData& unit : unitsDoo.units) {
        if (!seenUnitIds.insert(unit.id).second) {
            continue;
        }

        const std::string unitId = War3IdHighString(unit.id);
        const auto pathIt = effectiveModelPaths.find(unitId);
        if (pathIt == effectiveModelPaths.end()) {
            if (IsKnownNonRenderableUnitId(unitId)) {
                std::cout << "Initial unit " << unitId << " model=known-nonrenderable" << std::endl;
                continue;
            }
            std::cout << "Initial unit " << unitId << " model=no-ui-row" << std::endl;
            continue;
        }

        std::vector<uint8_t> modelBytes;
        if (!ReadFileFromDefaultMpqs(pathIt->second.c_str(), modelBytes)) {
            std::cout << "Initial unit " << unitId << " model=" << pathIt->second << " read=fail" << std::endl;
            continue;
        }

        MdxModel model;
        if (!MdxLoad(modelBytes.data(), modelBytes.size(), model)) {
            std::cout << "Initial unit " << unitId << " model=" << pathIt->second << " parse=fail" << std::endl;
            continue;
        }

        out.push_back({campaignPath, unitId, pathIt->second, std::move(model)});
        if ((int)out.size() >= maxModels) {
            break;
        }
    }

    return !out.empty();
}

static bool LoadCampaignInitialUnitModels(std::vector<CampaignUnitModelProbe>& out, int maxModels) {
    std::unordered_map<std::string, std::string> modelPaths;
    if (!LoadUnitModelPathTable(modelPaths)) {
        return false;
    }

    return LoadCampaignInitialUnitModelsForMap(
        "Maps\\Campaign\\Prologue01.w3m",
        modelPaths,
        out,
        maxModels);
}

static std::vector<std::string> ExtractCampaignMissionMapPaths(const std::string& campaignStrings) {
    std::vector<std::string> paths;
    size_t lineStart = 0;
    while (lineStart < campaignStrings.size()) {
        size_t lineEnd = campaignStrings.find_first_of("\r\n", lineStart);
        if (lineEnd == std::string::npos) {
            lineEnd = campaignStrings.size();
        }
        const std::string line = campaignStrings.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 1;

        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line.compare(first, 7, "Mission") != 0) {
            continue;
        }

        std::vector<std::string> fields;
        size_t search = first;
        while (fields.size() < 3) {
            const size_t open = line.find('"', search);
            if (open == std::string::npos) {
                break;
            }
            const size_t close = line.find('"', open + 1);
            if (close == std::string::npos) {
                break;
            }
            fields.push_back(line.substr(open + 1, close - open - 1));
            search = close + 1;
        }

        if (fields.size() >= 3) {
            std::string path = fields[2];
            for (char& c : path) {
                if (c == '/') c = '\\';
            }
            paths.push_back(path);
        }
    }
    return paths;
}

bool TestStormLibCampaignStringsMissionMapsSmoke() {
    std::cout << "\n=== StormLib Campaign Strings Mission Map Smoke Test ===\n" << std::endl;

    std::vector<uint8_t> bytes;
    if (!ReadFileFromDefaultMpqs("UI\\CampaignStrings.txt", bytes)) {
        std::cerr << "[ERROR] Failed to read UI\\CampaignStrings.txt" << std::endl;
        return false;
    }

    const std::string source(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    const std::vector<std::string> paths = ExtractCampaignMissionMapPaths(source);
    std::unordered_set<std::string> uniquePaths(paths.begin(), paths.end());
    int readableMaps = 0;

    for (const std::string& path : uniquePaths) {
        std::vector<uint8_t> mapBytes;
        if (ReadFileFromDefaultMpqs(path.c_str(), mapBytes) && !mapBytes.empty()) {
            ++readableMaps;
        } else {
            std::cerr << "[ERROR] Campaign mission map not readable: " << path << std::endl;
        }
    }

    std::cout << "Campaign strings mission maps total=" << paths.size()
              << " unique=" << uniquePaths.size()
              << " readable=" << readableMaps << std::endl;

    return paths.size() >= 30
        && uniquePaths.size() >= 30
        && readableMaps == (int)uniquePaths.size();
}

bool TestStormLibCampaignMapPathResolverSmoke() {
    std::cout << "\n=== StormLib Campaign Map Path Resolver Smoke Test ===\n" << std::endl;

    MpqManager mpq;
    if (!OpenDefaultMpqs(mpq)) {
        std::cerr << "[ERROR] Failed to open default MPQs" << std::endl;
        return false;
    }

    std::string resolvedPath;
    if (!ResolveMapPathForLoad(ResourcePath(""), mpq, "Maps\\Campaign\\Prologue01.w3m", resolvedPath)) {
        std::cerr << "[ERROR] Failed to resolve campaign MPQ map path" << std::endl;
        return false;
    }

    W3iInfo info;
    const bool infoOk = W3ILoadFromMpq(resolvedPath, info);
    std::cout << "Campaign map resolver requested=Maps\\Campaign\\Prologue01.w3m"
              << " resolved=" << resolvedPath
              << " exists=" << (fs::exists(resolvedPath) ? "yes" : "no")
              << " w3i=" << (infoOk ? "yes" : "no")
              << " name=" << info.name
              << std::endl;

    return fs::exists(resolvedPath)
        && infoOk
        && info.tileset != '\0';
}

bool TestStormLibCampaignMapSmoke() {
    std::cout << "\n=== StormLib Campaign Map Smoke Test ===\n" << std::endl;

    const char* campaignPath = "Maps\\Campaign\\Prologue01.w3m";
    std::vector<uint8_t> bytes;
    if (!ReadFileFromDefaultMpqs(campaignPath, bytes)) {
        std::cerr << "[ERROR] Failed to read " << campaignPath << std::endl;
        return false;
    }

    const fs::path tempPath = fs::temp_directory_path() / "warsmash_campaign_Prologue01.w3m";
    {
        std::ofstream out(tempPath, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "[ERROR] Failed to write temp campaign map " << tempPath.string() << std::endl;
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out.good()) {
            std::cerr << "[ERROR] Failed while writing temp campaign map " << tempPath.string() << std::endl;
            return false;
        }
    }

    HANDLE hMap = nullptr;
    if (!SFileOpenArchive(tempPath.string().c_str(), 0, MPQ_OPEN_READ_ONLY, &hMap)) {
        std::cerr << "[ERROR] Failed to open campaign map archive " << tempPath.string() << std::endl;
        fs::remove(tempPath);
        return false;
    }

    bool ok = true;
    HANDLE hW3e = nullptr;
    if (SFileOpenFileEx(hMap, "war3map.w3e", SFILE_OPEN_BASE_FILE, &hW3e)) {
        const DWORD fileSize = SFileGetFileSize(hW3e, nullptr);
        std::vector<uint8_t> w3eBytes(fileSize);
        DWORD bytesRead = 0;
        if (SFileReadFile(hW3e, w3eBytes.data(), fileSize, &bytesRead, nullptr) && bytesRead == fileSize) {
            W3EMap parsed{};
            if (W3ELoad(w3eBytes.data(), w3eBytes.size(), parsed)) {
                std::cout << "Campaign terrain: version=" << parsed.version
                          << " tileset=" << parsed.tileset
                          << " size=" << parsed.tileWidth << "x" << parsed.tileHeight << std::endl;
            } else {
                std::cerr << "[ERROR] Campaign terrain failed W3E parse" << std::endl;
                ok = false;
            }
        } else {
            std::cerr << "[ERROR] Failed reading campaign war3map.w3e" << std::endl;
            ok = false;
        }
        SFileCloseFile(hW3e);
    } else {
        std::cerr << "[ERROR] Campaign war3map.w3e missing" << std::endl;
        ok = false;
    }

    const char* requiredFiles[] = {
        "war3map.w3i",
        "war3map.j",
        "war3mapUnits.doo",
        "war3map.doo",
        "war3map.wpm"
    };
    for (const char* required : requiredFiles) {
        const bool exists = SFileHasFile(hMap, required);
        std::cout << "Campaign file " << required << "=" << (exists ? "yes" : "no") << std::endl;
        if (!exists) {
            ok = false;
        }
    }

    SFileCloseArchive(hMap);
    fs::remove(tempPath);
    return ok;
}

bool TestStormLibClassicDoodadObjectModsSmoke() {
    std::cout << "\n=== StormLib Classic Doodad Object Mods Smoke Test ===\n" << std::endl;

    const char* campaignPath = "Maps\\FrozenThrone\\Campaign\\NightElfX01.w3x";
    std::vector<uint8_t> mapBytes;
    if (!ReadFileFromDefaultMpqs(campaignPath, mapBytes)) {
        std::cerr << "[ERROR] Failed to read " << campaignPath << std::endl;
        return false;
    }

    const fs::path tempPath = fs::temp_directory_path() / "warsmash_objectmods_NightElfX01.w3x";
    {
        std::ofstream out(tempPath, std::ios::binary);
        if (!out.is_open()) {
            std::cerr << "[ERROR] Failed to write temp campaign map " << tempPath.string() << std::endl;
            return false;
        }
        out.write(reinterpret_cast<const char*>(mapBytes.data()), static_cast<std::streamsize>(mapBytes.size()));
        if (!out.good()) {
            std::cerr << "[ERROR] Failed while writing temp campaign map " << tempPath.string() << std::endl;
            return false;
        }
    }

    ObjectModificationSet mods;
    const bool loaded = LoadWar3MapObjectMods(tempPath.string(), "war3map.w3d", true, mods);
    fs::remove(tempPath);

    if (!loaded) {
        std::cerr << "[ERROR] Failed to parse NightElfX01 war3map.w3d" << std::endl;
        return false;
    }

    std::unordered_set<std::string> fieldIds;
    for (const ObjectModificationEntry& entry : mods.original) {
        for (const ObjectModification& mod : entry.modifications) {
            fieldIds.insert(mod.id.ToString());
        }
    }

    std::cout << "NightElfX01 war3map.w3d version=" << mods.version
              << " original=" << mods.original.size()
              << " custom=" << mods.custom.size()
              << " fields=";
    for (const auto& field : fieldIds) {
        std::cout << field << ' ';
    }
    std::cout << std::endl;

    return mods.version == 1
        && mods.original.size() == 2
        && mods.custom.empty()
        && fieldIds.count("dmas") > 0
        && fieldIds.count("dmis") > 0
        && fieldIds.count("dvr1") > 0
        && fieldIds.count("dvg1") > 0;
}

bool TestStormLibCampaignObjectModsCoverageSmoke() {
    std::cout << "\n=== StormLib Campaign Object Mods Coverage Smoke Test ===\n" << std::endl;

    const std::vector<std::string> mapPaths = CollectAllCampaignMissionMapPaths();
    struct ObjectModFileSpec {
        const char* fileName;
        bool extended;
    };
    const ObjectModFileSpec specs[] = {
        {"war3map.w3u", false},
        {"war3map.w3t", false},
        {"war3map.w3b", false},
        {"war3map.w3d", true},
    };

    int mapsRead = 0;
    int filesPresent = 0;
    int filesParsed = 0;
    int parseFailures = 0;
    std::unordered_map<std::string, int> parsedByFile;

    for (const std::string& mapPath : mapPaths) {
        std::vector<uint8_t> mapBytes;
        if (!ReadFileFromDefaultMpqs(mapPath.c_str(), mapBytes) || mapBytes.empty()) {
            std::cerr << "[ERROR] Campaign object mods map unreadable path=" << mapPath << std::endl;
            continue;
        }
        ++mapsRead;

        const fs::path tempPath = fs::temp_directory_path() /
            ("warsmash_objectmods_coverage_" + TempSafeMapName(mapPath));
        if (!WriteTempBytes(tempPath, mapBytes)) {
            std::cerr << "[ERROR] Failed to write temp campaign map " << tempPath.string() << std::endl;
            continue;
        }

        for (const ObjectModFileSpec& spec : specs) {
            std::vector<uint8_t> objectModBytes;
            if (!ReadFileFromMpqArchive(tempPath, spec.fileName, objectModBytes)) {
                continue;
            }
            ++filesPresent;

            ObjectModificationSet mods;
            if (LoadWar3MapObjectMods(tempPath.string(), spec.fileName, spec.extended, mods)) {
                ++filesParsed;
                parsedByFile[spec.fileName]++;
            } else {
                ++parseFailures;
                std::cerr << "[ERROR] Campaign object mods parse failed map="
                          << mapPath << " file=" << spec.fileName << std::endl;
            }
        }

        fs::remove(tempPath);
    }

    std::cout << "Campaign object mods maps=" << mapPaths.size()
              << " maps_read=" << mapsRead
              << " files_present=" << filesPresent
              << " files_parsed=" << filesParsed
              << " parse_failures=" << parseFailures;
    for (const ObjectModFileSpec& spec : specs) {
        std::cout << " " << spec.fileName << "=" << parsedByFile[spec.fileName];
    }
    std::cout << std::endl;

    return mapsRead == (int)mapPaths.size()
        && filesPresent > 0
        && filesParsed == filesPresent
        && parseFailures == 0;
}

bool TestStormLibHuman01JassNativeCoverageSmoke() {
    std::cout << "\n=== StormLib Human01 JASS Native Coverage Smoke Test ===\n" << std::endl;

    std::vector<uint8_t> commonBytes;
    std::vector<uint8_t> blizzardBytes;
    std::vector<uint8_t> mapBytes;
    if (!ReadFileFromDefaultMpqs("Scripts\\common.j", commonBytes)
        || !ReadFileFromDefaultMpqs("Scripts\\Blizzard.j", blizzardBytes)
        || !ReadFileFromDefaultMpqs("Maps\\Campaign\\Human01.w3m", mapBytes)) {
        std::cerr << "[ERROR] Failed to read Human01 JASS coverage inputs" << std::endl;
        return false;
    }

    const fs::path tempPath = fs::temp_directory_path() / "warsmash_jass_native_coverage_Human01.w3m";
    if (!WriteTempBytes(tempPath, mapBytes)) {
        std::cerr << "[ERROR] Failed to write temp Human01 map " << tempPath.string() << std::endl;
        return false;
    }

    std::vector<uint8_t> war3mapBytes;
    const bool scriptOk = ReadFileFromMpqArchive(tempPath, "war3map.j", war3mapBytes);
    fs::remove(tempPath);
    if (!scriptOk) {
        std::cerr << "[ERROR] Failed to read Human01 war3map.j" << std::endl;
        return false;
    }

    const std::string commonSource(reinterpret_cast<const char*>(commonBytes.data()), commonBytes.size());
    const std::string blizzardSource(reinterpret_cast<const char*>(blizzardBytes.data()), blizzardBytes.size());
    const std::string mapSource(reinterpret_cast<const char*>(war3mapBytes.data()), war3mapBytes.size());

    std::unordered_set<std::string> declaredNatives;
    ExtractJassNativeDeclarations(commonSource, declaredNatives);
    ExtractJassNativeDeclarations(blizzardSource, declaredNatives);
    ExtractJassNativeDeclarations(mapSource, declaredNatives);

    std::unordered_set<std::string> registeredNatives;
    if (!ExtractRegisteredJassNatives(registeredNatives)) {
        return false;
    }

    const auto callCounts = ExtractJassCallCounts(mapSource);
    std::vector<std::pair<std::string, int>> missing;
    int calledDeclaredNativeNames = 0;
    int calledDeclaredNativeCalls = 0;
    for (const auto& [name, count] : callCounts) {
        if (declaredNatives.find(name) == declaredNatives.end()) {
            continue;
        }
        ++calledDeclaredNativeNames;
        calledDeclaredNativeCalls += count;
        if (registeredNatives.find(name) == registeredNatives.end()) {
            missing.push_back({name, count});
        }
    }
    std::sort(missing.begin(), missing.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::cout << "Human01 JASS native coverage declared=" << declaredNatives.size()
              << " registered=" << registeredNatives.size()
              << " called_declared_native_names=" << calledDeclaredNativeNames
              << " called_declared_native_calls=" << calledDeclaredNativeCalls
              << " missing_registered=" << missing.size()
              << std::endl;

    for (size_t i = 0; i < missing.size() && i < 20; ++i) {
        std::cerr << "[ERROR] Human01 native call missing registration name="
                  << missing[i].first << " count=" << missing[i].second << std::endl;
    }

    return calledDeclaredNativeNames >= 10
        && calledDeclaredNativeCalls >= 20
        && missing.empty();
}

bool TestStormLibCampaignInitialUnitMdxSmoke() {
    std::cout << "\n=== StormLib Campaign Initial Unit MDX Smoke Test ===\n" << std::endl;

    std::vector<CampaignUnitModelProbe> probes;
    if (!LoadCampaignInitialUnitModels(probes, 6)) {
        return false;
    }

    int animatedOrRiggedModels = 0;

    for (const CampaignUnitModelProbe& probe : probes) {
        const MdxModel& model = probe.model;
        const bool hasRigOrAnimation = !model.bones.empty() || !model.helpers.empty() || !model.sequences.empty();
        if (hasRigOrAnimation) {
            ++animatedOrRiggedModels;
        }
        std::cout << "Initial unit " << probe.unitId
                  << " model=" << probe.modelPath
                  << " geosets=" << model.geosets.size()
                  << " sequences=" << model.sequences.size()
                  << " bones=" << model.bones.size()
                  << " helpers=" << model.helpers.size()
                  << std::endl;
    }

    std::cout << "Campaign unit MDX resolved=" << probes.size()
              << " parsed=" << probes.size()
              << " rigged_or_animated=" << animatedOrRiggedModels << std::endl;

    return probes.size() >= 3 && animatedOrRiggedModels >= 3;
}

static bool MatrixIsFinite(const Mat4& matrix) {
    for (float value : matrix.m) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

static bool MatricesDiffer(const std::vector<Mat4>& a, const std::vector<Mat4>& b, float epsilon = 0.0001f) {
    if (a.size() != b.size()) {
        return true;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        for (int j = 0; j < 16; ++j) {
            if (std::fabs(a[i].m[j] - b[i].m[j]) > epsilon) {
                return true;
            }
        }
    }
    return false;
}

static int CountGlobalNodeTransformTimelines(const MdxModel& model) {
    int count = 0;
    auto countTimelines = [&](const std::vector<MdxTimeline>& timelines) {
        for (const MdxTimeline& timeline : timelines) {
            if (timeline.globalSequenceId < 0) {
                continue;
            }
            if (timeline.tag == TAG_KGTR || timeline.tag == TAG_KGRT || timeline.tag == TAG_KGSC) {
                ++count;
            }
        }
    };
    for (const MdxNode& node : model.bones) countTimelines(node.timelines);
    for (const MdxNode& node : model.helpers) countTimelines(node.timelines);
    for (const MdxAttachment& attachment : model.attachments) countTimelines(attachment.timelines);
    for (const MdxLight& light : model.lights) countTimelines(light.timelines);
    for (const MdxParticleEmitter1& emitter : model.particleEmitters1) countTimelines(emitter.timelines);
    for (const MdxParticleEmitter2& emitter : model.particleEmitters2) countTimelines(emitter.timelines);
    for (const MdxParticleEmitterPopcorn& emitter : model.particleEmittersPopcorn) countTimelines(emitter.timelines);
    for (const MdxRibbonEmitter& ribbon : model.ribbonEmitters) countTimelines(ribbon.timelines);
    for (const MdxEventObject& eventObject : model.eventObjects) countTimelines(eventObject.timelines);
    for (const MdxCollisionShape& shape : model.collisionShapes) countTimelines(shape.timelines);
    return count;
}

static void AddAnimatorObjectId(std::vector<int>& objectIds, int objectId) {
    if (objectId >= 0) {
        objectIds.push_back(objectId);
    }
}

static std::vector<int> CollectAnimatorObjectIds(const MdxModel& model) {
    std::vector<int> objectIds;
    for (const MdxNode& node : model.bones) AddAnimatorObjectId(objectIds, node.objectId);
    for (const MdxNode& node : model.helpers) AddAnimatorObjectId(objectIds, node.objectId);
    for (const MdxAttachment& attachment : model.attachments) AddAnimatorObjectId(objectIds, attachment.objectId);
    for (const MdxLight& light : model.lights) AddAnimatorObjectId(objectIds, light.objectId);
    for (const MdxParticleEmitter1& emitter : model.particleEmitters1) AddAnimatorObjectId(objectIds, emitter.objectId);
    for (const MdxParticleEmitter2& emitter : model.particleEmitters2) AddAnimatorObjectId(objectIds, emitter.objectId);
    for (const MdxParticleEmitterPopcorn& emitter : model.particleEmittersPopcorn) AddAnimatorObjectId(objectIds, emitter.objectId);
    for (const MdxRibbonEmitter& ribbon : model.ribbonEmitters) AddAnimatorObjectId(objectIds, ribbon.objectId);
    for (const MdxEventObject& eventObject : model.eventObjects) AddAnimatorObjectId(objectIds, eventObject.objectId);
    for (const MdxCollisionShape& shape : model.collisionShapes) AddAnimatorObjectId(objectIds, shape.objectId);
    return objectIds;
}

static int CountSpecialAnimatorObjectIds(const MdxModel& model) {
    return (int)model.attachments.size()
        + (int)model.lights.size()
        + (int)model.particleEmitters1.size()
        + (int)model.particleEmitters2.size()
        + (int)model.particleEmittersPopcorn.size()
        + (int)model.ribbonEmitters.size()
        + (int)model.eventObjects.size()
        + (int)model.collisionShapes.size();
}

static int CountMissingAnimatorObjectIds(const MdxModel& model,
                                         const MdxAnimator& animator,
                                         const std::string& label) {
    int missing = 0;
    std::unordered_set<int> seen;
    for (int objectId : CollectAnimatorObjectIds(model)) {
        if (!seen.insert(objectId).second) {
            continue;
        }
        if (animator.GetBoneIndexForObjectId(objectId) < 0) {
            ++missing;
            std::cout << "Animator object missing label=" << label
                      << " object_id=" << objectId << std::endl;
        }
    }
    return missing;
}

static int FindFirstPlayableSequence(const MdxModel& model) {
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const MdxSequence& sequence = model.sequences[(size_t)i];
        if (sequence.endFrame > sequence.startFrame) {
            return i;
        }
    }
    return -1;
}

static bool AllMatricesFinite(const std::vector<Mat4>& matrices) {
    if (matrices.empty()) {
        return false;
    }
    for (const Mat4& matrix : matrices) {
        if (!MatrixIsFinite(matrix)) {
            return false;
        }
    }
    return true;
}

static bool ModelTriggersAnyEvent(const MdxModel& model) {
    if (model.eventObjects.empty() || model.sequences.empty()) {
        return false;
    }

    auto updateToEventFrame = [&](int sequenceIndex, const MdxTimeline& timeline,
                                  uint32_t targetFrame, uint32_t startFrame) -> bool {
        MdxAnimator animator;
        animator.Init(model);
        if (!animator.SetSequence(sequenceIndex, false, 1.0f)) {
            return false;
        }
        const uint32_t delta = targetFrame >= startFrame ? targetFrame - startFrame : 0;
        animator.Update((float)delta / 1000.0f);
        for (const MdxEventTrigger& trigger : animator.GetTriggeredEvents()) {
            if (trigger.keyframeIndex >= 0
                && trigger.keyframeIndex < (int)timeline.frames.size()
                && trigger.frame == timeline.frames[(size_t)trigger.keyframeIndex]) {
                return true;
            }
        }
        return false;
    };

    for (const MdxEventObject& eventObject : model.eventObjects) {
        for (const MdxTimeline& timeline : eventObject.timelines) {
            if (timeline.tag != TAG_KEVT || timeline.frames.empty()) {
                continue;
            }
            if (timeline.globalSequenceId >= 0
                && timeline.globalSequenceId < (int)model.globalSequences.size()
                && model.globalSequences[(size_t)timeline.globalSequenceId] > 0) {
                const uint32_t duration = model.globalSequences[(size_t)timeline.globalSequenceId];
                for (uint32_t keyFrame : timeline.frames) {
                    if (keyFrame <= duration && updateToEventFrame(0, timeline, keyFrame, 0)) {
                        return true;
                    }
                }
                continue;
            }

            for (int sequenceIndex = 0; sequenceIndex < (int)model.sequences.size(); ++sequenceIndex) {
                const MdxSequence& sequence = model.sequences[(size_t)sequenceIndex];
                if (sequence.endFrame <= sequence.startFrame) {
                    continue;
                }
                for (uint32_t keyFrame : timeline.frames) {
                    if (keyFrame >= sequence.startFrame && keyFrame <= sequence.endFrame
                        && updateToEventFrame(sequenceIndex, timeline, keyFrame, sequence.startFrame)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static int CountResolvableTriggeredEventEffects(const MdxModel& model,
                                                const MdxEventEffectResolver& resolver,
                                                const MpqManager* mpq,
                                                int& outSplatEffects,
                                                int& outTimedSplatEffects,
                                                int& outColoredSplatEffects,
                                                int& outSpawnEffects,
                                                int& outReadableSpawnEffects,
                                                int& outSoundEffects,
                                                int& outReadableSoundEffects) {
    if (model.eventObjects.empty() || model.sequences.empty()) {
        return 0;
    }

    auto countAtEventFrame = [&](int sequenceIndex, const MdxTimeline& timeline,
                                 uint32_t targetFrame, uint32_t startFrame) -> int {
        MdxAnimator animator;
        animator.Init(model);
        if (!animator.SetSequence(sequenceIndex, false, 1.0f)) {
            return 0;
        }
        const uint32_t delta = targetFrame >= startFrame ? targetFrame - startFrame : 0;
        animator.Update((float)delta / 1000.0f);

        int resolved = 0;
        for (const MdxEventTrigger& trigger : animator.GetTriggeredEvents()) {
            if (trigger.keyframeIndex < 0
                || trigger.keyframeIndex >= (int)timeline.frames.size()
                || trigger.frame != timeline.frames[(size_t)trigger.keyframeIndex]) {
                continue;
            }
            const MdxEventEffect effect = resolver.Resolve(trigger);
            if (!effect.resolved) {
                continue;
            }
            ++resolved;
            if (!effect.texturePath.empty()) {
                ++outSplatEffects;
                float intervalSum = 0.0f;
                for (int i = 0; i < effect.intervalCount && i < (int)effect.intervalTimes.size(); ++i) {
                    intervalSum += effect.intervalTimes[(size_t)i];
                }
                if (effect.intervalCount >= 2
                    && std::isfinite(intervalSum)
                    && intervalSum > 0.0f
                    && std::fabs(intervalSum - effect.lifeSpan) <= 0.001f) {
                    ++outTimedSplatEffects;
                }
                bool colorsFinite = true;
                for (const auto& color : effect.colors) {
                    for (float component : color) {
                        colorsFinite = colorsFinite && std::isfinite(component);
                    }
                }
                if (colorsFinite) {
                    ++outColoredSplatEffects;
                }
            }
            if (!effect.modelPath.empty()) {
                ++outSpawnEffects;
                if (mpq) {
                    std::vector<uint8_t> bytes;
                    MdxModel spawnedModel;
                    if (mpq->ReadFile(effect.modelPath, bytes)
                        && !bytes.empty()
                        && MdxLoad(bytes.data(), bytes.size(), spawnedModel)) {
                        ++outReadableSpawnEffects;
                    }
                }
            }
            if (!effect.soundPaths.empty()) {
                ++outSoundEffects;
                if (mpq) {
                    std::vector<uint8_t> bytes;
                    if (mpq->ReadFile(effect.soundPaths.front(), bytes) && !bytes.empty()) {
                        ++outReadableSoundEffects;
                    }
                }
            }
        }
        return resolved;
    };

    int resolved = 0;
    for (const MdxEventObject& eventObject : model.eventObjects) {
        for (const MdxTimeline& timeline : eventObject.timelines) {
            if (timeline.tag != TAG_KEVT || timeline.frames.empty()) {
                continue;
            }
            if (timeline.globalSequenceId >= 0
                && timeline.globalSequenceId < (int)model.globalSequences.size()
                && model.globalSequences[(size_t)timeline.globalSequenceId] > 0) {
                const uint32_t duration = model.globalSequences[(size_t)timeline.globalSequenceId];
                for (uint32_t keyFrame : timeline.frames) {
                    if (keyFrame <= duration) {
                        resolved += countAtEventFrame(0, timeline, keyFrame, 0);
                    }
                }
                continue;
            }

            for (int sequenceIndex = 0; sequenceIndex < (int)model.sequences.size(); ++sequenceIndex) {
                const MdxSequence& sequence = model.sequences[(size_t)sequenceIndex];
                if (sequence.endFrame <= sequence.startFrame) {
                    continue;
                }
                for (uint32_t keyFrame : timeline.frames) {
                    if (keyFrame >= sequence.startFrame && keyFrame <= sequence.endFrame) {
                        resolved += countAtEventFrame(sequenceIndex, timeline, keyFrame, sequence.startFrame);
                    }
                }
            }
        }
    }
    return resolved;
}

static std::vector<int> CollectAttachmentVisibilityObjectIds(const MdxModel& model) {
    std::vector<int> objectIds;
    for (const MdxAttachment& attachment : model.attachments) {
        for (const MdxTimeline& timeline : attachment.timelines) {
            if (timeline.tag == TAG_KATV) {
                objectIds.push_back(attachment.objectId);
                break;
            }
        }
    }
    return objectIds;
}

static int CountFiniteAnimatedGeosetColorSamples(const MdxModel& model, int& outTimelineCount) {
    int finiteSamples = 0;
    for (const MdxGeosetAnim& geosetAnim : model.geosetAnims) {
        for (const MdxTimeline& timeline : geosetAnim.timelines) {
            if (timeline.tag != TAG_KGAC) {
                continue;
            }
            ++outTimelineCount;
            const uint32_t sampleFrame = timeline.frames.empty() ? 0 : timeline.frames[0];
            float sampledBgr[3] = {};
            float sampledRgb[3] = {};
            if (!MdxSampleTimeline(timeline, sampleFrame, sampledBgr, 3, 1.0f)) {
                continue;
            }
            MdxConvertAnimatedColorBgrToRgb(sampledBgr, sampledRgb);
            if (std::isfinite(sampledRgb[0])
                && std::isfinite(sampledRgb[1])
                && std::isfinite(sampledRgb[2])) {
                ++finiteSamples;
            }
        }
    }
    return finiteSamples;
}

bool TestStormLibCampaignInitialUnitAnimatorSmoke() {
    std::cout << "\n=== StormLib Campaign Initial Unit Animator Smoke Test ===\n" << std::endl;

    std::vector<CampaignUnitModelProbe> probes;
    if (!LoadCampaignInitialUnitModels(probes, 8)) {
        return false;
    }

    int initializedAnimators = 0;
    int finiteMatrixSets = 0;
    int changingMatrixSets = 0;
    int animatorObjectRefs = 0;
    int missingAnimatorObjectRefs = 0;
    int specialAnimatorObjectModels = 0;
    int specialAnimatorObjectRefs = 0;

    for (const CampaignUnitModelProbe& probe : probes) {
        const std::vector<int> animatorObjectIds = CollectAnimatorObjectIds(probe.model);
        if (animatorObjectIds.empty()) {
            continue;
        }
        animatorObjectRefs += (int)animatorObjectIds.size();
        const int specialObjectRefs = CountSpecialAnimatorObjectIds(probe.model);
        if (specialObjectRefs > 0) {
            ++specialAnimatorObjectModels;
            specialAnimatorObjectRefs += specialObjectRefs;
        }

        MdxAnimator animator;
        animator.Init(probe.model);
        if (animator.GetBoneCount() <= 0 || animator.GetBoneMatrices().empty()) {
            std::cout << "Animator unit " << probe.unitId
                      << " model=" << probe.modelPath
                      << " bone_matrices=empty" << std::endl;
            continue;
        }
        ++initializedAnimators;
        missingAnimatorObjectRefs += CountMissingAnimatorObjectIds(
            probe.model, animator, probe.unitId + ":" + probe.modelPath);

        bool allFinite = true;
        for (const Mat4& matrix : animator.GetBoneMatrices()) {
            if (!MatrixIsFinite(matrix)) {
                allFinite = false;
                break;
            }
        }
        if (allFinite) {
            ++finiteMatrixSets;
        }

        bool changed = false;
        for (int seqIndex = 0; seqIndex < (int)probe.model.sequences.size(); ++seqIndex) {
            const MdxSequence& sequence = probe.model.sequences[(size_t)seqIndex];
            if (sequence.endFrame <= sequence.startFrame) {
                continue;
            }
            if (!animator.SetSequence(seqIndex, true, 1.0f)) {
                continue;
            }
            const std::vector<Mat4> startMatrices = animator.GetBoneMatrices();
            animator.SetFrameByRatio(0.5f);
            const std::vector<Mat4> middleMatrices = animator.GetBoneMatrices();
            if (MatricesDiffer(startMatrices, middleMatrices)) {
                changed = true;
                break;
            }
        }
        if (changed) {
            ++changingMatrixSets;
        }

        std::cout << "Animator unit " << probe.unitId
                  << " model=" << probe.modelPath
                  << " bone_count=" << animator.GetBoneCount()
                  << " sequences=" << animator.GetSequenceCount()
                  << " animator_objects=" << animatorObjectIds.size()
                  << " special_objects=" << specialObjectRefs
                  << " finite=" << (allFinite ? "yes" : "no")
                  << " changes=" << (changed ? "yes" : "no")
                  << std::endl;
    }

    std::cout << "Campaign animator initialized=" << initializedAnimators
              << " finite=" << finiteMatrixSets
              << " changing=" << changingMatrixSets
              << " animator_objects=" << animatorObjectRefs
              << " missing_animator_objects=" << missingAnimatorObjectRefs
              << " special_object_models=" << specialAnimatorObjectModels
              << " special_objects=" << specialAnimatorObjectRefs << std::endl;

    return initializedAnimators >= 3
        && finiteMatrixSets >= 3
        && changingMatrixSets >= 1
        && animatorObjectRefs >= initializedAnimators
        && missingAnimatorObjectRefs == 0;
}

bool TestStormLibCampaignInitialUnitSkinningSmoke() {
    std::cout << "\n=== StormLib Campaign Initial Unit Skinning Smoke Test ===\n" << std::endl;

    std::vector<CampaignUnitModelProbe> probes;
    if (!LoadCampaignInitialUnitModels(probes, 8)) {
        return false;
    }

    int modelsWithSkinning = 0;
    int totalSkinnedGeosets = 0;
    int totalVertices = 0;
    int totalInfluencedVertices = 0;
    int totalInvalidGroups = 0;
    int totalInvalidBones = 0;
    int totalBadWeightSums = 0;

    for (const CampaignUnitModelProbe& probe : probes) {
        MdxAnimator animator;
        animator.Init(probe.model);

        MdxSkinningBuildStats stats;
        for (const MdxGeoset& geo : probe.model.geosets) {
            std::vector<MdxSkinnedVertex> vertices;
            MdxBuildSkinnedVertices(
                geo,
                geo.uvs.empty() ? nullptr : &geo.uvs,
                [&animator](int objectId) { return animator.GetBoneIndexForObjectId(objectId); },
                vertices,
                &stats);
        }
        if (stats.skinnedGeosets <= 0) {
            continue;
        }

        ++modelsWithSkinning;
        totalSkinnedGeosets += stats.skinnedGeosets;
        totalVertices += stats.vertices;
        totalInfluencedVertices += stats.verticesWithInfluence;
        totalInvalidGroups += stats.invalidGroupReferences;
        totalInvalidBones += stats.invalidBoneReferences;
        totalBadWeightSums += stats.badWeightSums;

        std::cout << "Skinning unit " << probe.unitId
                  << " model=" << probe.modelPath
                  << " geosets=" << stats.skinnedGeosets
                  << " vertices=" << stats.vertices
                  << " influenced=" << stats.verticesWithInfluence
                  << " invalid_groups=" << stats.invalidGroupReferences
                  << " invalid_bones=" << stats.invalidBoneReferences
                  << " bad_weights=" << stats.badWeightSums
                  << std::endl;
    }

    std::cout << "Campaign skinning models=" << modelsWithSkinning
              << " geosets=" << totalSkinnedGeosets
              << " vertices=" << totalVertices
              << " influenced=" << totalInfluencedVertices
              << " invalid_groups=" << totalInvalidGroups
              << " invalid_bones=" << totalInvalidBones
              << " bad_weights=" << totalBadWeightSums
              << std::endl;

    return modelsWithSkinning >= 3
        && totalSkinnedGeosets >= 3
        && totalVertices > 0
        && totalInfluencedVertices == totalVertices
        && totalInvalidGroups == 0
        && totalInvalidBones == 0
        && totalBadWeightSums == 0;
}

static void AppendCampaignMissionPathsFromResource(const char* resourcePath, std::vector<std::string>& out) {
    std::vector<uint8_t> bytes;
    if (!ReadFileFromDefaultMpqs(resourcePath, bytes)) {
        std::cout << "Campaign strings source " << resourcePath << " readable=no" << std::endl;
        return;
    }

    const std::string source(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    const std::vector<std::string> paths = ExtractCampaignMissionMapPaths(source);
    out.insert(out.end(), paths.begin(), paths.end());
    std::cout << "Campaign strings source " << resourcePath
              << " missions=" << paths.size() << std::endl;
}

static bool AddUniqueCampaignPath(
    const std::string& path,
    std::vector<std::string>& out,
    std::unordered_set<std::string>& seen) {
    const std::string lower = ToLowerString(path);
    const bool isMap = lower.size() >= 4
        && (lower.rfind(".w3m") == lower.size() - 4
            || lower.rfind(".w3x") == lower.size() - 4);
    if (path.empty() || !isMap || !seen.insert(path).second) {
        return false;
    }
    out.push_back(path);
    return true;
}

static std::vector<std::string> CollectRepresentativeCampaignMapPaths() {
    std::vector<std::string> discovered;
    AppendCampaignMissionPathsFromResource("UI\\CampaignStrings.txt", discovered);
    AppendCampaignMissionPathsFromResource("UI\\CampaignStrings_exp.txt", discovered);

    std::vector<std::string> selected;
    std::unordered_set<std::string> seen;
    const char* preferred[] = {
        "Maps\\Campaign\\Prologue01.w3m",
        "Maps\\Campaign\\Human01.w3m",
        "Maps\\Campaign\\Undead01.w3m",
        "Maps\\Campaign\\Orc01.w3m",
        "Maps\\Campaign\\NightElf01.w3m",
        "Maps\\FrozenThrone\\Campaign\\NightElfX01.w3x",
        "Maps\\FrozenThrone\\Campaign\\HumanX01.w3x",
        "Maps\\FrozenThrone\\Campaign\\UndeadX01.w3x"
    };

    for (const char* path : preferred) {
        AddUniqueCampaignPath(path, selected, seen);
    }

    for (const std::string& path : discovered) {
        if (selected.size() >= 8) {
            break;
        }
        AddUniqueCampaignPath(path, selected, seen);
    }

    return selected;
}

static std::vector<std::string> CollectAllCampaignMissionMapPaths() {
    std::vector<std::string> discovered;
    AppendCampaignMissionPathsFromResource("UI\\CampaignStrings.txt", discovered);
    AppendCampaignMissionPathsFromResource("UI\\CampaignStrings_exp.txt", discovered);

    std::vector<std::string> selected;
    std::unordered_set<std::string> seen;
    for (const std::string& path : discovered) {
        AddUniqueCampaignPath(path, selected, seen);
    }
    return selected;
}

bool TestStormLibCampaignJassNativeCoverageSmoke() {
    std::cout << "\n=== StormLib Campaign JASS Native Coverage Smoke Test ===\n" << std::endl;

    std::vector<uint8_t> commonBytes;
    std::vector<uint8_t> blizzardBytes;
    if (!ReadFileFromDefaultMpqs("Scripts\\common.j", commonBytes)
        || !ReadFileFromDefaultMpqs("Scripts\\Blizzard.j", blizzardBytes)) {
        std::cerr << "[ERROR] Failed to read campaign JASS native declaration inputs" << std::endl;
        return false;
    }

    const std::string commonSource(reinterpret_cast<const char*>(commonBytes.data()), commonBytes.size());
    const std::string blizzardSource(reinterpret_cast<const char*>(blizzardBytes.data()), blizzardBytes.size());

    std::unordered_set<std::string> declaredNatives;
    ExtractJassNativeDeclarations(commonSource, declaredNatives);
    ExtractJassNativeDeclarations(blizzardSource, declaredNatives);

    std::unordered_set<std::string> registeredNatives;
    if (!ExtractRegisteredJassNatives(registeredNatives)) {
        return false;
    }

    const std::vector<std::string> mapPaths = CollectAllCampaignMissionMapPaths();
    std::unordered_map<std::string, int> missingTotals;
    int mapsRead = 0;
    int scriptsRead = 0;
    int calledDeclaredNativeCalls = 0;
    int calledDeclaredNativeNamesTotal = 0;

    for (const std::string& mapPath : mapPaths) {
        std::vector<uint8_t> mapBytes;
        if (!ReadFileFromDefaultMpqs(mapPath.c_str(), mapBytes) || mapBytes.empty()) {
            std::cerr << "[ERROR] Campaign native coverage map unreadable path=" << mapPath << std::endl;
            continue;
        }
        ++mapsRead;

        const fs::path tempPath = fs::temp_directory_path() /
            ("warsmash_jass_native_coverage_" + TempSafeMapName(mapPath));
        if (!WriteTempBytes(tempPath, mapBytes)) {
            std::cerr << "[ERROR] Failed to write temp campaign map " << tempPath.string() << std::endl;
            continue;
        }

        std::vector<uint8_t> war3mapBytes;
        const bool scriptOk = ReadFileFromMpqArchive(tempPath, "war3map.j", war3mapBytes);
        fs::remove(tempPath);
        if (!scriptOk) {
            std::cerr << "[ERROR] Failed to read campaign war3map.j path=" << mapPath << std::endl;
            continue;
        }
        ++scriptsRead;

        const std::string mapSource(reinterpret_cast<const char*>(war3mapBytes.data()), war3mapBytes.size());
        const auto callCounts = ExtractJassCallCounts(mapSource);
        int mapCalledDeclaredNativeNames = 0;
        int mapCalledDeclaredNativeCalls = 0;
        int mapMissingNames = 0;
        for (const auto& [name, count] : callCounts) {
            if (declaredNatives.find(name) == declaredNatives.end()) {
                continue;
            }
            ++mapCalledDeclaredNativeNames;
            mapCalledDeclaredNativeCalls += count;
            if (registeredNatives.find(name) == registeredNatives.end()) {
                missingTotals[name] += count;
                ++mapMissingNames;
            }
        }
        calledDeclaredNativeNamesTotal += mapCalledDeclaredNativeNames;
        calledDeclaredNativeCalls += mapCalledDeclaredNativeCalls;
        std::cout << "Campaign native coverage map=" << mapPath
                  << " native_names=" << mapCalledDeclaredNativeNames
                  << " native_calls=" << mapCalledDeclaredNativeCalls
                  << " missing_names=" << mapMissingNames
                  << std::endl;
    }

    std::vector<std::pair<std::string, int>> missing(missingTotals.begin(), missingTotals.end());
    std::sort(missing.begin(), missing.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::cout << "Campaign JASS native coverage maps=" << mapPaths.size()
              << " maps_read=" << mapsRead
              << " scripts_read=" << scriptsRead
              << " declared=" << declaredNatives.size()
              << " registered=" << registeredNatives.size()
              << " called_declared_native_names_total=" << calledDeclaredNativeNamesTotal
              << " called_declared_native_calls=" << calledDeclaredNativeCalls
              << " missing_registered=" << missing.size()
              << std::endl;

    for (size_t i = 0; i < missing.size() && i < 30; ++i) {
        std::cerr << "[ERROR] Campaign native call missing registration name="
                  << missing[i].first << " count=" << missing[i].second << std::endl;
    }

    return mapPaths.size() >= 30
        && mapsRead == (int)mapPaths.size()
        && scriptsRead == (int)mapPaths.size()
        && calledDeclaredNativeCalls >= 1000
        && missing.empty();
}

bool TestStormLibRepresentativeCampaignMdxAnimationSkinningSmoke() {
    std::cout << "\n=== StormLib Representative Campaign MDX Animation Skinning Smoke Test ===\n" << std::endl;

    std::unordered_map<std::string, std::string> modelPaths;
    if (!LoadUnitModelPathTable(modelPaths)) {
        return false;
    }

    const std::vector<std::string> mapPaths = CollectRepresentativeCampaignMapPaths();
    int mapsWithModels = 0;
    int mapsSkipped = 0;
    int totalModels = 0;
    int initializedAnimators = 0;
    int finiteMatrixSets = 0;
    int changingMatrixSets = 0;
    int modelsWithSkinning = 0;
    int totalSkinnedGeosets = 0;
    int totalVertices = 0;
    int totalInfluencedVertices = 0;
    int totalInvalidGroups = 0;
    int totalInvalidBones = 0;
    int totalBadWeightSums = 0;
    int textureAnimModels = 0;
    int textureAnimTimelines = 0;
    int textureAnimTranslateTimelines = 0;
    int textureAnimRotateTimelines = 0;
    int textureAnimScaleTimelines = 0;
    int textureAnimFiniteSamples = 0;
    int textureAnimBadSamples = 0;
    int textureAnimAnisotropicScaleSamples = 0;
    int animatorObjectRefs = 0;
    int missingAnimatorObjectRefs = 0;
    int specialAnimatorObjectModels = 0;
    int specialAnimatorObjectRefs = 0;
    bool loadedFrozenThroneMap = false;

    for (const std::string& mapPath : mapPaths) {
        std::vector<CampaignUnitModelProbe> probes;
        if (!LoadCampaignInitialUnitModelsForMap(mapPath, modelPaths, probes, 4)) {
            ++mapsSkipped;
            std::cout << "Representative campaign map skipped path=" << mapPath << std::endl;
            continue;
        }

        ++mapsWithModels;
        totalModels += (int)probes.size();
        if (mapPath.find("FrozenThrone") != std::string::npos) {
            loadedFrozenThroneMap = true;
        }

        for (const CampaignUnitModelProbe& probe : probes) {
            MdxAnimator animator;
            animator.Init(probe.model);
            const std::vector<int> animatorObjectIds = CollectAnimatorObjectIds(probe.model);
            animatorObjectRefs += (int)animatorObjectIds.size();
            const int specialObjectRefs = CountSpecialAnimatorObjectIds(probe.model);
            if (specialObjectRefs > 0) {
                ++specialAnimatorObjectModels;
                specialAnimatorObjectRefs += specialObjectRefs;
            }
            missingAnimatorObjectRefs += CountMissingAnimatorObjectIds(
                probe.model, animator, probe.mapPath + ":" + probe.unitId + ":" + probe.modelPath);

            bool finite = false;
            bool changed = false;
            if (animator.GetBoneCount() > 0 && !animator.GetBoneMatrices().empty()) {
                ++initializedAnimators;
                finite = true;
                for (const Mat4& matrix : animator.GetBoneMatrices()) {
                    if (!MatrixIsFinite(matrix)) {
                        finite = false;
                        break;
                    }
                }
                if (finite) {
                    ++finiteMatrixSets;
                }

                for (int seqIndex = 0; seqIndex < (int)probe.model.sequences.size(); ++seqIndex) {
                    const MdxSequence& sequence = probe.model.sequences[(size_t)seqIndex];
                    if (sequence.endFrame <= sequence.startFrame) {
                        continue;
                    }
                    if (!animator.SetSequence(seqIndex, true, 1.0f)) {
                        continue;
                    }
                    const std::vector<Mat4> startMatrices = animator.GetBoneMatrices();
                    animator.SetFrameByRatio(0.5f);
                    const std::vector<Mat4> middleMatrices = animator.GetBoneMatrices();
                    if (MatricesDiffer(startMatrices, middleMatrices)) {
                        changed = true;
                        break;
                    }
                }
                if (changed) {
                    ++changingMatrixSets;
                }
            }

            MdxSkinningBuildStats stats;
            for (const MdxGeoset& geo : probe.model.geosets) {
                std::vector<MdxSkinnedVertex> vertices;
                MdxBuildSkinnedVertices(
                    geo,
                    geo.uvs.empty() ? nullptr : &geo.uvs,
                    [&animator](int objectId) { return animator.GetBoneIndexForObjectId(objectId); },
                    vertices,
                    &stats);
            }

            if (stats.skinnedGeosets > 0) {
                ++modelsWithSkinning;
                totalSkinnedGeosets += stats.skinnedGeosets;
                totalVertices += stats.vertices;
                totalInfluencedVertices += stats.verticesWithInfluence;
                totalInvalidGroups += stats.invalidGroupReferences;
                totalInvalidBones += stats.invalidBoneReferences;
                totalBadWeightSums += stats.badWeightSums;
            }

            int modelTexAnimTimelines = 0;
            int modelTexAnimScaleTimelines = 0;
            for (const MdxTexAnim& texAnim : probe.model.texAnims) {
                for (const MdxTimeline& timeline : texAnim.timelines) {
                    if (timeline.tag != TAG_KTAT && timeline.tag != TAG_KTAR && timeline.tag != TAG_KTAS) {
                        continue;
                    }
                    ++modelTexAnimTimelines;
                    ++textureAnimTimelines;
                    if (timeline.tag == TAG_KTAT) {
                        ++textureAnimTranslateTimelines;
                    } else if (timeline.tag == TAG_KTAR) {
                        ++textureAnimRotateTimelines;
                    } else if (timeline.tag == TAG_KTAS) {
                        ++textureAnimScaleTimelines;
                        ++modelTexAnimScaleTimelines;
                    }

                    const int elemCount = (timeline.tag == TAG_KTAR) ? 4 : 3;
                    float sampled[4] = {};
                    const uint32_t sampleFrame = timeline.frames.empty() ? 0 : timeline.frames[0];
                    if (MdxSampleTimeline(timeline, sampleFrame, sampled, elemCount, timeline.tag == TAG_KTAS ? 1.0f : 0.0f)) {
                        bool finite = true;
                        for (int i = 0; i < elemCount; ++i) {
                            if (!std::isfinite(sampled[i])) {
                                finite = false;
                                break;
                            }
                        }
                        if (finite) {
                            ++textureAnimFiniteSamples;
                            if (timeline.tag == TAG_KTAS && std::fabs(sampled[0] - sampled[1]) > 0.0001f) {
                                ++textureAnimAnisotropicScaleSamples;
                            }
                        } else {
                            ++textureAnimBadSamples;
                        }
                    } else {
                        ++textureAnimBadSamples;
                    }
                }
            }
            if (modelTexAnimTimelines > 0) {
                ++textureAnimModels;
            }

            std::cout << "Representative campaign model map=" << probe.mapPath
                      << " unit=" << probe.unitId
                      << " model=" << probe.modelPath
                      << " sequences=" << probe.model.sequences.size()
                      << " bones=" << animator.GetBoneCount()
                      << " animator_objects=" << animatorObjectIds.size()
                      << " special_objects=" << specialObjectRefs
                      << " finite=" << (finite ? "yes" : "no")
                      << " changes=" << (changed ? "yes" : "no")
                      << " skinned_geosets=" << stats.skinnedGeosets
                      << " vertices=" << stats.vertices
                      << " invalid_groups=" << stats.invalidGroupReferences
                      << " invalid_bones=" << stats.invalidBoneReferences
                      << " bad_weights=" << stats.badWeightSums
                      << " tex_anim_timelines=" << modelTexAnimTimelines
                      << " tex_anim_scale=" << modelTexAnimScaleTimelines
                      << std::endl;
        }
    }

    std::cout << "Representative campaign MDX maps=" << mapsWithModels
              << " skipped=" << mapsSkipped
              << " models=" << totalModels
              << " animators=" << initializedAnimators
              << " finite=" << finiteMatrixSets
              << " changing=" << changingMatrixSets
              << " skinning_models=" << modelsWithSkinning
              << " skinned_geosets=" << totalSkinnedGeosets
              << " vertices=" << totalVertices
              << " influenced=" << totalInfluencedVertices
              << " invalid_groups=" << totalInvalidGroups
              << " invalid_bones=" << totalInvalidBones
              << " bad_weights=" << totalBadWeightSums
              << " tex_anim_models=" << textureAnimModels
              << " tex_anim_timelines=" << textureAnimTimelines
              << " ktat=" << textureAnimTranslateTimelines
              << " ktar=" << textureAnimRotateTimelines
              << " ktas=" << textureAnimScaleTimelines
              << " tex_anim_finite=" << textureAnimFiniteSamples
              << " tex_anim_bad=" << textureAnimBadSamples
              << " ktas_anisotropic=" << textureAnimAnisotropicScaleSamples
              << " animator_objects=" << animatorObjectRefs
              << " missing_animator_objects=" << missingAnimatorObjectRefs
              << " special_object_models=" << specialAnimatorObjectModels
              << " special_objects=" << specialAnimatorObjectRefs
              << " frozen_throne=" << (loadedFrozenThroneMap ? "yes" : "no")
              << std::endl;

    return mapsWithModels >= 5
        && totalModels >= 12
        && initializedAnimators >= 8
        && finiteMatrixSets == initializedAnimators
        && changingMatrixSets >= 3
        && modelsWithSkinning >= 8
        && totalSkinnedGeosets >= 8
        && totalVertices > 0
        && totalInfluencedVertices == totalVertices
        && totalInvalidGroups == 0
        && totalInvalidBones == 0
        && totalBadWeightSums == 0
        && animatorObjectRefs >= initializedAnimators
        && missingAnimatorObjectRefs == 0
        && loadedFrozenThroneMap;
}

bool TestStormLibMdxTextureAnimationSmoke() {
    std::cout << "\n=== StormLib MDX Texture Animation Smoke Test ===\n" << std::endl;

    const char* candidatePaths[] = {
        "UI\\Glues\\SinglePlayer\\NightElf_Exp\\NightElf_Exp.mdx",
        "UI\\Glues\\SinglePlayer\\Human_Exp\\Human_Exp.mdx",
        "UI\\Glues\\SinglePlayer\\Undead_Exp\\Undead_Exp.mdx",
        "UI\\Glues\\SinglePlayer\\Orc_Exp\\Orc_Exp.mdx",
        "UI\\Glues\\MainMenu\\MainMenu3d_exp\\MainMenu3d_exp.mdx",
        "Doodads\\Cinematic\\ArthasIllidanFight\\ArthasIllidanFight.mdx",
        "Abilities\\Spells\\Orc\\FeralSpirit\\feralspirittarget.mdx",
        "Abilities\\Spells\\Undead\\AnimateDead\\AnimateDeadTarget.mdx",
        "Abilities\\Spells\\Human\\MassTeleport\\MassTeleportTarget.mdx",
        "Abilities\\Spells\\NightElf\\Tranquility\\TranquilityTarget.mdx",
        "Abilities\\Spells\\Other\\Doom\\DoomDeath.mdx",
        "Abilities\\Spells\\Other\\Monsoon\\MonsoonBoltTarget.mdx"
    };

    int readable = 0;
    int parsed = 0;
    int modelsWithTextureAnimations = 0;
    int textureAnimTimelines = 0;
    int translateTimelines = 0;
    int rotateTimelines = 0;
    int scaleTimelines = 0;
    int finiteSamples = 0;
    int badSamples = 0;
    int anisotropicScaleSamples = 0;
    int globalNodeModels = 0;
    int globalNodeTimelines = 0;
    int globalNodeFiniteModels = 0;
    int globalNodeChangingModels = 0;
    int attachmentVisibilityModels = 0;
    int attachmentVisibilityTimelines = 0;
    int attachmentVisibilityQueryableModels = 0;
    int geosetColorModels = 0;
    int geosetColorTimelines = 0;
    int geosetColorFiniteSamples = 0;
    int eventObjectModels = 0;
    int eventObjects = 0;
    int eventTimelines = 0;
    int eventFrames = 0;
    int eventTriggeredModels = 0;

    for (const char* path : candidatePaths) {
        std::vector<uint8_t> bytes;
        if (!ReadFileFromDefaultMpqs(path, bytes)) {
            std::cout << "TextureAnim candidate path=" << path << " read=no" << std::endl;
            continue;
        }
        ++readable;

        MdxModel model;
        if (!MdxLoad(bytes.data(), bytes.size(), model)) {
            std::cout << "TextureAnim candidate path=" << path << " parse=no" << std::endl;
            continue;
        }
        ++parsed;

        int modelTimelines = 0;
        int modelScaleTimelines = 0;
        const int modelGlobalNodeTimelines = CountGlobalNodeTransformTimelines(model);
        const std::vector<int> modelAttachmentVisibilityObjectIds = CollectAttachmentVisibilityObjectIds(model);
        int modelGeosetColorTimelines = 0;
        const int modelGeosetColorFiniteSamples =
            CountFiniteAnimatedGeosetColorSamples(model, modelGeosetColorTimelines);
        int modelEventTimelines = 0;
        int modelEventFrames = 0;
        bool modelEventTriggerable = false;
        for (const MdxEventObject& eventObject : model.eventObjects) {
            for (const MdxTimeline& timeline : eventObject.timelines) {
                if (timeline.tag == TAG_KEVT) {
                    ++modelEventTimelines;
                    modelEventFrames += (int)timeline.frames.size();
                }
            }
        }
        if (!model.eventObjects.empty()) {
            ++eventObjectModels;
            eventObjects += (int)model.eventObjects.size();
            eventTimelines += modelEventTimelines;
            eventFrames += modelEventFrames;
            modelEventTriggerable = ModelTriggersAnyEvent(model);
            if (modelEventTriggerable) {
                ++eventTriggeredModels;
            }
        }
        if (modelGeosetColorTimelines > 0) {
            ++geosetColorModels;
            geosetColorTimelines += modelGeosetColorTimelines;
            geosetColorFiniteSamples += modelGeosetColorFiniteSamples;
        }
        bool modelGlobalFinite = false;
        bool modelGlobalChanged = false;
        if (modelGlobalNodeTimelines > 0) {
            ++globalNodeModels;
            globalNodeTimelines += modelGlobalNodeTimelines;

            const int sequenceIndex = FindFirstPlayableSequence(model);
            MdxAnimator animator;
            animator.Init(model);
            if (sequenceIndex >= 0 && animator.SetSequence(sequenceIndex, true, 1.0f)) {
                const std::vector<Mat4> startMatrices = animator.GetBoneMatrices();
                animator.Update(0.25f);
                const std::vector<Mat4> firstSampleMatrices = animator.GetBoneMatrices();
                animator.Update(1.0f);
                const std::vector<Mat4> secondSampleMatrices = animator.GetBoneMatrices();
                modelGlobalFinite = AllMatricesFinite(firstSampleMatrices)
                    && AllMatricesFinite(secondSampleMatrices);
                modelGlobalChanged = MatricesDiffer(startMatrices, firstSampleMatrices)
                    || MatricesDiffer(firstSampleMatrices, secondSampleMatrices);
            }
            if (modelGlobalFinite) {
                ++globalNodeFiniteModels;
            }
            if (modelGlobalChanged) {
                ++globalNodeChangingModels;
            }
        }

        bool attachmentVisibilityQueryable = false;
        if (!modelAttachmentVisibilityObjectIds.empty()) {
            ++attachmentVisibilityModels;
            attachmentVisibilityTimelines += (int)modelAttachmentVisibilityObjectIds.size();
            const int sequenceIndex = FindFirstPlayableSequence(model);
            MdxAnimator animator;
            animator.Init(model);
            if (sequenceIndex >= 0 && animator.SetSequence(sequenceIndex, true, 1.0f)) {
                attachmentVisibilityQueryable = true;
                for (int objectId : modelAttachmentVisibilityObjectIds) {
                    if (animator.GetBoneIndexForObjectId(objectId) < 0) {
                        attachmentVisibilityQueryable = false;
                        break;
                    }
                    (void)animator.IsObjectVisible(objectId);
                }
                if (attachmentVisibilityQueryable) {
                    animator.SetFrameByRatio(0.5f);
                    for (int objectId : modelAttachmentVisibilityObjectIds) {
                        (void)animator.IsObjectVisible(objectId);
                    }
                }
            }
            if (attachmentVisibilityQueryable) {
                ++attachmentVisibilityQueryableModels;
            }
        }

        for (const MdxTexAnim& texAnim : model.texAnims) {
            for (const MdxTimeline& timeline : texAnim.timelines) {
                if (timeline.tag != TAG_KTAT && timeline.tag != TAG_KTAR && timeline.tag != TAG_KTAS) {
                    continue;
                }
                ++modelTimelines;
                ++textureAnimTimelines;
                if (timeline.tag == TAG_KTAT) {
                    ++translateTimelines;
                } else if (timeline.tag == TAG_KTAR) {
                    ++rotateTimelines;
                } else if (timeline.tag == TAG_KTAS) {
                    ++scaleTimelines;
                    ++modelScaleTimelines;
                }

                const int elemCount = (timeline.tag == TAG_KTAR) ? 4 : 3;
                float sampled[4] = {};
                const uint32_t sampleFrame = timeline.frames.empty() ? 0 : timeline.frames[0];
                if (MdxSampleTimeline(timeline, sampleFrame, sampled, elemCount, timeline.tag == TAG_KTAS ? 1.0f : 0.0f)) {
                    bool finite = true;
                    for (int i = 0; i < elemCount; ++i) {
                        if (!std::isfinite(sampled[i])) {
                            finite = false;
                            break;
                        }
                    }
                    if (finite) {
                        ++finiteSamples;
                        if (timeline.tag == TAG_KTAS && std::fabs(sampled[0] - sampled[1]) > 0.0001f) {
                            ++anisotropicScaleSamples;
                        }
                    } else {
                        ++badSamples;
                    }
                } else {
                    ++badSamples;
                }
            }
        }

        if (modelTimelines > 0) {
            ++modelsWithTextureAnimations;
        }

        std::cout << "TextureAnim candidate path=" << path
                  << " tex_anims=" << model.texAnims.size()
                  << " timelines=" << modelTimelines
                  << " ktas=" << modelScaleTimelines
                  << " global_node_timelines=" << modelGlobalNodeTimelines
                  << " global_node_finite=" << (modelGlobalFinite ? "yes" : "no")
                  << " global_node_changes=" << (modelGlobalChanged ? "yes" : "no")
                  << " attachment_visibility=" << modelAttachmentVisibilityObjectIds.size()
                  << " attachment_visibility_queryable=" << (attachmentVisibilityQueryable ? "yes" : "no")
                  << " geoset_color_timelines=" << modelGeosetColorTimelines
                  << " geoset_color_finite=" << modelGeosetColorFiniteSamples
                  << " event_objects=" << model.eventObjects.size()
                  << " event_tracks=" << modelEventTimelines
                  << " event_frames=" << modelEventFrames
                  << " event_triggerable=" << (modelEventTriggerable ? "yes" : "no")
                  << std::endl;
    }

    std::cout << "TextureAnim candidates readable=" << readable
              << " parsed=" << parsed
              << " models=" << modelsWithTextureAnimations
              << " timelines=" << textureAnimTimelines
              << " ktat=" << translateTimelines
              << " ktar=" << rotateTimelines
              << " ktas=" << scaleTimelines
              << " finite=" << finiteSamples
              << " bad=" << badSamples
              << " ktas_anisotropic=" << anisotropicScaleSamples
              << " global_node_models=" << globalNodeModels
              << " global_node_timelines=" << globalNodeTimelines
              << " global_node_finite=" << globalNodeFiniteModels
              << " global_node_changing=" << globalNodeChangingModels
              << " attachment_visibility_models=" << attachmentVisibilityModels
              << " attachment_visibility_timelines=" << attachmentVisibilityTimelines
              << " attachment_visibility_queryable=" << attachmentVisibilityQueryableModels
              << " geoset_color_models=" << geosetColorModels
              << " geoset_color_timelines=" << geosetColorTimelines
              << " geoset_color_finite=" << geosetColorFiniteSamples
              << " event_object_models=" << eventObjectModels
              << " event_objects=" << eventObjects
              << " event_tracks=" << eventTimelines
              << " event_frames=" << eventFrames
              << " event_triggered_models=" << eventTriggeredModels
              << std::endl;

    return readable >= 6
        && parsed >= 6
        && modelsWithTextureAnimations >= 1
        && textureAnimTimelines >= 1
        && finiteSamples == textureAnimTimelines
        && badSamples == 0
        && globalNodeModels >= 1
        && globalNodeTimelines >= 1
        && globalNodeFiniteModels == globalNodeModels
        && globalNodeChangingModels >= 1
        && attachmentVisibilityQueryableModels == attachmentVisibilityModels
        && geosetColorFiniteSamples == geosetColorTimelines
        && eventObjectModels >= 1
        && eventTimelines == eventObjects
        && eventFrames >= eventTimelines
        && eventTriggeredModels >= 1;
}

bool TestStormLibMdxEventEffectMetadataSmoke() {
    std::cout << "\n=== StormLib MDX Event Effect Metadata Smoke Test ===\n" << std::endl;

    MpqManager mpq;
    if (!OpenDefaultMpqs(mpq)) {
        std::cout << "EventEffect metadata mpq_open=no" << std::endl;
        return false;
    }

    MdxEventEffectResolver resolver;
    if (!resolver.Load(mpq)) {
        std::cout << "EventEffect metadata tables loaded=no" << std::endl;
        return false;
    }

    const char* candidatePaths[] = {
        "UI\\Glues\\SinglePlayer\\NightElf_Exp\\NightElf_Exp.mdx",
        "UI\\Glues\\SinglePlayer\\Orc_Exp\\Orc_Exp.mdx",
        "Doodads\\Cinematic\\ArthasIllidanFight\\ArthasIllidanFight.mdx",
        "Abilities\\Spells\\Orc\\FeralSpirit\\feralspirittarget.mdx",
        "Abilities\\Spells\\Undead\\AnimateDead\\AnimateDeadTarget.mdx",
        "Abilities\\Spells\\Human\\MassTeleport\\MassTeleportTarget.mdx",
        "Abilities\\Spells\\Other\\Doom\\DoomDeath.mdx",
        "Abilities\\Spells\\Other\\Monsoon\\MonsoonBoltTarget.mdx"
    };

    int readable = 0;
    int parsed = 0;
    int eventModels = 0;
    int triggeredModels = 0;
    int resolvedModels = 0;
    int resolvedEffects = 0;
    int splatEffects = 0;
    int timedSplatEffects = 0;
    int coloredSplatEffects = 0;
    int spawnEffects = 0;
    int readableSpawnEffects = 0;
    int soundEffects = 0;
    int readableSoundEffects = 0;

    for (const char* path : candidatePaths) {
        std::vector<uint8_t> bytes;
        if (!ReadFileFromDefaultMpqs(path, bytes)) {
            std::cout << "EventEffect candidate path=" << path << " read=no" << std::endl;
            continue;
        }
        ++readable;

        MdxModel model;
        if (!MdxLoad(bytes.data(), bytes.size(), model)) {
            std::cout << "EventEffect candidate path=" << path << " parse=no" << std::endl;
            continue;
        }
        ++parsed;

        if (!model.eventObjects.empty()) {
            ++eventModels;
        }
        const bool triggerable = ModelTriggersAnyEvent(model);
        if (triggerable) {
            ++triggeredModels;
        }

        int modelSplatEffects = 0;
        int modelTimedSplatEffects = 0;
        int modelColoredSplatEffects = 0;
        int modelSpawnEffects = 0;
        int modelReadableSpawnEffects = 0;
        int modelSoundEffects = 0;
        int modelReadableSoundEffects = 0;
        const int modelResolvedEffects = CountResolvableTriggeredEventEffects(
            model, resolver, &mpq, modelSplatEffects,
            modelTimedSplatEffects, modelColoredSplatEffects, modelSpawnEffects,
            modelReadableSpawnEffects, modelSoundEffects, modelReadableSoundEffects);
        if (modelResolvedEffects > 0) {
            ++resolvedModels;
            resolvedEffects += modelResolvedEffects;
            splatEffects += modelSplatEffects;
            timedSplatEffects += modelTimedSplatEffects;
            coloredSplatEffects += modelColoredSplatEffects;
            spawnEffects += modelSpawnEffects;
            readableSpawnEffects += modelReadableSpawnEffects;
            soundEffects += modelSoundEffects;
            readableSoundEffects += modelReadableSoundEffects;
        }

        std::cout << "EventEffect candidate path=" << path
                  << " event_objects=" << model.eventObjects.size()
                  << " triggerable=" << (triggerable ? "yes" : "no")
                  << " resolved=" << modelResolvedEffects
                  << " splat=" << modelSplatEffects
                  << " splat_timed=" << modelTimedSplatEffects
                  << " splat_colored=" << modelColoredSplatEffects
                  << " spawn=" << modelSpawnEffects
                  << " spawn_readable=" << modelReadableSpawnEffects
                  << " sound=" << modelSoundEffects
                  << " sound_readable=" << modelReadableSoundEffects
                  << std::endl;
    }

    std::cout << "EventEffect metadata tables"
              << " splat=" << (resolver.HasSplatData() ? "yes" : "no")
              << " ubersplat=" << (resolver.HasUberSplatData() ? "yes" : "no")
              << " spawn=" << (resolver.HasSpawnData() ? "yes" : "no")
              << " sound=" << (resolver.HasSoundData() ? "yes" : "no")
              << " readable=" << readable
              << " parsed=" << parsed
              << " event_models=" << eventModels
              << " triggered_models=" << triggeredModels
              << " resolved_models=" << resolvedModels
              << " resolved_effects=" << resolvedEffects
              << " splat_effects=" << splatEffects
              << " timed_splat_effects=" << timedSplatEffects
              << " colored_splat_effects=" << coloredSplatEffects
              << " spawn_effects=" << spawnEffects
              << " readable_spawn_effects=" << readableSpawnEffects
              << " sound_effects=" << soundEffects
              << " readable_sound_effects=" << readableSoundEffects
              << std::endl;

    return resolver.HasSplatData()
        && resolver.HasUberSplatData()
        && resolver.HasSpawnData()
        && resolver.HasSoundData()
        && readable >= 6
        && parsed >= 6
        && eventModels >= 1
        && triggeredModels >= 1
        && resolvedModels >= 1
        && resolvedEffects >= 1
        && (splatEffects + spawnEffects + soundEffects) == resolvedEffects
        && readableSpawnEffects == spawnEffects
        && readableSoundEffects == soundEffects;
}

bool TestStormLibMdxVisibleEventEffectScan() {
    std::cout << "\n=== StormLib MDX Visible Event Effect Scan ===\n" << std::endl;

    MpqManager mpq;
    if (!OpenDefaultMpqs(mpq)) {
        std::cout << "VisibleEventEffect mpq_open=no" << std::endl;
        return false;
    }

    MdxEventEffectResolver resolver;
    if (!resolver.Load(mpq)) {
        std::cout << "VisibleEventEffect metadata loaded=no" << std::endl;
        return false;
    }

    std::vector<std::string> files = mpq.ListAllFiles("*");
    std::unordered_set<std::string> seen;
    std::vector<std::string> candidates;
    for (std::string path : files) {
        for (char& c : path) {
            if (c == '/') c = '\\';
        }
        std::string lower = path;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".mdx") {
            continue;
        }
        if (lower.find("abilities\\") == std::string::npos
            && lower.find("objects\\") == std::string::npos
            && lower.find("doodads\\") == std::string::npos
            && lower.find("units\\") == std::string::npos) {
            continue;
        }
        if (seen.insert(lower).second) {
            candidates.push_back(path);
        }
    }

    int readable = 0;
    int parsed = 0;
    int eventModels = 0;
    int visibleResolved = 0;
    int splatEffects = 0;
    int timedSplatEffects = 0;
    int coloredSplatEffects = 0;
    int spawnEffects = 0;
    int readableSpawnEffects = 0;
    int soundEffects = 0;
    int readableSoundEffects = 0;
    std::string firstVisiblePath;
    constexpr int kMaxParsed = 900;

    for (const std::string& path : candidates) {
        if (parsed >= kMaxParsed && visibleResolved == 0) {
            break;
        }
        std::vector<uint8_t> bytes;
        if (!mpq.ReadFile(path, bytes) || bytes.empty()) {
            continue;
        }
        ++readable;

        MdxModel model;
        if (!MdxLoad(bytes.data(), bytes.size(), model)) {
            continue;
        }
        ++parsed;
        if (model.eventObjects.empty()) {
            continue;
        }
        ++eventModels;

        int modelSplatEffects = 0;
        int modelTimedSplatEffects = 0;
        int modelColoredSplatEffects = 0;
        int modelSpawnEffects = 0;
        int modelReadableSpawnEffects = 0;
        int modelSoundEffects = 0;
        int modelReadableSoundEffects = 0;
        const int resolved = CountResolvableTriggeredEventEffects(
            model, resolver, &mpq, modelSplatEffects,
            modelTimedSplatEffects, modelColoredSplatEffects, modelSpawnEffects,
            modelReadableSpawnEffects, modelSoundEffects, modelReadableSoundEffects);
        if (resolved > 0) {
            splatEffects += modelSplatEffects;
            timedSplatEffects += modelTimedSplatEffects;
            coloredSplatEffects += modelColoredSplatEffects;
            spawnEffects += modelSpawnEffects;
            readableSpawnEffects += modelReadableSpawnEffects;
            soundEffects += modelSoundEffects;
            readableSoundEffects += modelReadableSoundEffects;
            const int modelVisible = modelSplatEffects + modelSpawnEffects;
            if (modelVisible > 0) {
                visibleResolved += modelVisible;
                if (firstVisiblePath.empty()) {
                    firstVisiblePath = path;
                    std::cout << "VisibleEventEffect first path=" << path
                              << " splat=" << modelSplatEffects
                              << " splat_timed=" << modelTimedSplatEffects
                              << " splat_colored=" << modelColoredSplatEffects
                              << " spawn=" << modelSpawnEffects
                              << " sound=" << modelSoundEffects
                              << std::endl;
                }
                break;
            }
        }
    }

    std::cout << "VisibleEventEffect candidates=" << candidates.size()
              << " readable=" << readable
              << " parsed=" << parsed
              << " event_models=" << eventModels
              << " visible_resolved=" << visibleResolved
              << " splat_effects=" << splatEffects
              << " timed_splat_effects=" << timedSplatEffects
              << " colored_splat_effects=" << coloredSplatEffects
              << " spawn_effects=" << spawnEffects
              << " readable_spawn_effects=" << readableSpawnEffects
              << " sound_effects=" << soundEffects
              << " readable_sound_effects=" << readableSoundEffects
              << " first_visible=" << firstVisiblePath
              << std::endl;

    return readable >= 1
        && parsed >= 1
        && eventModels >= 1
        && visibleResolved >= 1
        && timedSplatEffects == splatEffects
        && coloredSplatEffects == splatEffects;
}

bool TestStormLibMdxSpawnEventEffectScan() {
    std::cout << "\n=== StormLib MDX Spawn Event Effect Scan ===\n" << std::endl;

    MpqManager mpq;
    if (!OpenDefaultMpqs(mpq)) {
        std::cout << "SpawnEventEffect mpq_open=no" << std::endl;
        return false;
    }

    MdxEventEffectResolver resolver;
    if (!resolver.Load(mpq)) {
        std::cout << "SpawnEventEffect metadata loaded=no" << std::endl;
        return false;
    }

    std::vector<std::string> files = mpq.ListAllFiles("*");
    std::unordered_set<std::string> seen;
    std::vector<std::string> candidates;
    for (std::string path : files) {
        for (char& c : path) {
            if (c == '/') c = '\\';
        }
        std::string lower = path;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".mdx") {
            continue;
        }
        if (lower.find("abilities\\") == std::string::npos
            && lower.find("objects\\") == std::string::npos
            && lower.find("doodads\\") == std::string::npos
            && lower.find("units\\") == std::string::npos) {
            continue;
        }
        if (seen.insert(lower).second) {
            candidates.push_back(path);
        }
    }

    int readable = 0;
    int parsed = 0;
    int eventModels = 0;
    int spawnEffects = 0;
    int readableSpawnEffects = 0;
    std::string firstSpawnPath;
    constexpr int kMaxParsed = 2500;

    for (const std::string& path : candidates) {
        if (parsed >= kMaxParsed && spawnEffects == 0) {
            break;
        }
        std::vector<uint8_t> bytes;
        if (!mpq.ReadFile(path, bytes) || bytes.empty()) {
            continue;
        }
        ++readable;

        MdxModel model;
        if (!MdxLoad(bytes.data(), bytes.size(), model)) {
            continue;
        }
        ++parsed;
        if (model.eventObjects.empty()) {
            continue;
        }
        ++eventModels;

        int modelSplatEffects = 0;
        int modelTimedSplatEffects = 0;
        int modelColoredSplatEffects = 0;
        int modelSpawnEffects = 0;
        int modelReadableSpawnEffects = 0;
        int modelSoundEffects = 0;
        int modelReadableSoundEffects = 0;
        CountResolvableTriggeredEventEffects(
            model, resolver, &mpq, modelSplatEffects,
            modelTimedSplatEffects, modelColoredSplatEffects, modelSpawnEffects,
            modelReadableSpawnEffects, modelSoundEffects, modelReadableSoundEffects);

        if (modelSpawnEffects > 0) {
            spawnEffects += modelSpawnEffects;
            readableSpawnEffects += modelReadableSpawnEffects;
            firstSpawnPath = path;
            std::cout << "SpawnEventEffect first path=" << path
                      << " spawn=" << modelSpawnEffects
                      << " spawn_readable=" << modelReadableSpawnEffects
                      << std::endl;
            break;
        }
    }

    std::cout << "SpawnEventEffect candidates=" << candidates.size()
              << " readable=" << readable
              << " parsed=" << parsed
              << " event_models=" << eventModels
              << " spawn_effects=" << spawnEffects
              << " readable_spawn_effects=" << readableSpawnEffects
              << " first_spawn=" << firstSpawnPath
              << std::endl;

    return readable >= 1
        && parsed >= 1
        && eventModels >= 1
        && spawnEffects >= 1
        && readableSpawnEffects == spawnEffects;
}

bool TestStormLibMdxSpawnEventDefaultSequenceLifetimeSmoke() {
    std::cout << "\n=== StormLib MDX Spawn Event Default Sequence Lifetime Smoke Test ===\n" << std::endl;

    MpqManager mpq;
    if (!OpenDefaultMpqs(mpq)) {
        std::cout << "SpawnEventLifetime mpq_open=no" << std::endl;
        return false;
    }

    MdxEventEffectResolver resolver;
    if (!resolver.Load(mpq)) {
        std::cout << "SpawnEventLifetime metadata loaded=no" << std::endl;
        return false;
    }

    std::vector<uint8_t> flameBytes;
    const char* flamePath = "Abilities\\Spells\\Human\\FlameStrike\\FlameStrike2.mdx";
    if (!mpq.ReadFile(flamePath, flameBytes) || flameBytes.empty()) {
        std::cout << "SpawnEventLifetime source read=no path=" << flamePath << std::endl;
        return false;
    }

    MdxModel flameModel;
    if (!MdxLoad(flameBytes.data(), flameBytes.size(), flameModel)) {
        std::cout << "SpawnEventLifetime source parse=no path=" << flamePath << std::endl;
        return false;
    }

    int spawnEvents = 0;
    int resolvedSpawnEvents = 0;
    int readableSpawnModels = 0;
    float longestSequence0 = 0.0f;
    std::string firstSpawnModelPath;

    for (const MdxEventObject& eventObject : flameModel.eventObjects) {
        if (eventObject.type != "SPN") {
            continue;
        }
        ++spawnEvents;
        const MdxEventEffect effect = resolver.Resolve(eventObject);
        if (effect.modelPath.empty()) {
            continue;
        }
        ++resolvedSpawnEvents;

        std::vector<uint8_t> modelBytes;
        if (!mpq.ReadFile(effect.modelPath, modelBytes) || modelBytes.empty()) {
            continue;
        }

        MdxModel spawnedModel;
        if (!MdxLoad(modelBytes.data(), modelBytes.size(), spawnedModel)) {
            continue;
        }

        ++readableSpawnModels;
        if (firstSpawnModelPath.empty()) {
            firstSpawnModelPath = effect.modelPath;
        }
        longestSequence0 = std::max(longestSequence0,
                                    MdxSequenceDurationSeconds(spawnedModel, 0));
    }

    std::cout << "SpawnEventLifetime source=" << flamePath
              << " spawn_events=" << spawnEvents
              << " resolved_spawn_events=" << resolvedSpawnEvents
              << " readable_spawn_models=" << readableSpawnModels
              << " first_spawn_model=" << firstSpawnModelPath
              << " longest_sequence0=" << longestSequence0
              << std::endl;

    return spawnEvents >= 3
        && resolvedSpawnEvents == spawnEvents
        && readableSpawnModels == spawnEvents
        && longestSequence0 > 1.0f;
}

// Main test entry point
bool RunStormLibTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "StormLib MPQ Test Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    bool allPassed = true;
    if (!TestStormLibW3xMaps()) {
        allPassed = false;
    }

    const bool hasBaseGameMpqs =
        fs::exists(ResourcePath("war3.mpq")) &&
        (fs::exists(ResourcePath("War3x.mpq")) || fs::exists(ResourcePath("war3x.mpq")));
    if (!hasBaseGameMpqs) {
        std::cout << "StormLib: base war3.mpq/War3x.mpq not present; W3X map suite passed." << std::endl;
        return allPassed;
    }

    if (!TestStormLibEnumerateWar3Mpq()) {
        allPassed = false;
    }
    
    if (!TestStormLibExtractSamples()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignMapSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibClassicDoodadObjectModsSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignObjectModsCoverageSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibHuman01JassNativeCoverageSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignStringsMissionMapsSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignMapPathResolverSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignJassNativeCoverageSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignInitialUnitMdxSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignInitialUnitAnimatorSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibCampaignInitialUnitSkinningSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibRepresentativeCampaignMdxAnimationSkinningSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibMdxTextureAnimationSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibMdxEventEffectMetadataSmoke()) {
        allPassed = false;
    }

    if (!TestStormLibMdxVisibleEventEffectScan()) {
        allPassed = false;
    }

    if (!TestStormLibMdxSpawnEventEffectScan()) {
        allPassed = false;
    }

    if (!TestStormLibMdxSpawnEventDefaultSequenceLifetimeSmoke()) {
        allPassed = false;
    }
    
    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "All StormLib tests PASSED" << std::endl;
    } else {
        std::cout << "Some StormLib tests FAILED" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
    
    return allPassed;
}

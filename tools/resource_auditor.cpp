#include "resource/mpq_storm.h"
#include "resource/slk.h"
#include "map/w3e.h"
#include "map/w3x_storm.h"
#include "asset/BLP.h"
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

struct AuditEntry {
    std::string category;
    std::string path;
    bool found;
    uint32_t size;
    std::string format;   // "DXT1", "DXT3", "DXT5", "JPEG", "PAL", "MDX", "SLK", "TXT"
    int alphaDepth;       // for BLP: 0, 1, 4, 8
    std::string status;   // "OK", "NOT_FOUND", "DECODE_FAIL", etc.
};

static std::vector<AuditEntry> entries;

static void Report(const std::string& cat, const std::string& path, bool found,
                   uint32_t size = 0, const std::string& fmt = "",
                   int alpha = -1, const std::string& status = "") {
    std::string s = status.empty() ? (found ? "OK" : "NOT_FOUND") : status;
    entries.push_back({cat, path, found, size, fmt, alpha, s});
}

static bool ReadFromMpq(const MpqManager& mpq, const std::string& path,
                        std::vector<uint8_t>& out) {
    return mpq.ReadFile(path, out);
}

// Detect BLP format from header
static bool DetectBlpFormat(const uint8_t* data, size_t size,
                            std::string& fmt, int& alphaDepth) {
    if (size < 28) return false;
    if (std::memcmp(data, "BLP", 3) != 0) return false;
    uint32_t compression = *(const uint32_t*)(data + 4);
    alphaDepth = *(const uint32_t*)(data + 8);
    uint32_t width  = *(const uint32_t*)(data + 12);
    uint32_t height = *(const uint32_t*)(data + 16);
    switch (compression) {
        case 0: fmt = "JPEG"; break;
        case 1: fmt = "PAL"; break;
        case 2: case 4:
            switch (alphaDepth) {
                case 0: case 1: fmt = "DXT1"; break;
                case 4: fmt = "DXT3"; break;
                case 8: fmt = "DXT5"; break;
                default: fmt = "DXT?"; break;
            }
            break;
        default: fmt = "UNKNOWN"; break;
    }
    return true;
}

// Index string for a War3ID stored as LE uint32
static std::string War3IDStr(uint32_t id) {
    char s[5] = {
        (char)(id & 0xFF),
        (char)((id >> 8) & 0xFF),
        (char)((id >> 16) & 0xFF),
        (char)((id >> 24) & 0xFF),
        0
    };
    return std::string(s);
}

// War3ID from LE uint32
static uint32_t StrToWar3ID(const std::string& s) {
    if (s.size() < 4) return 0;
    return (uint32_t)(unsigned char)s[0]
         | ((uint32_t)(unsigned char)s[1] << 8)
         | ((uint32_t)(unsigned char)s[2] << 16)
         | ((uint32_t)(unsigned char)s[3] << 24);
}

static void AuditSlkLoad(const MpqManager& mpq, const std::string& path,
                         const std::string& label) {
    std::vector<uint8_t> data;
    bool ok = ReadFromMpq(mpq, path, data);
    Report("SLK", path, ok, ok ? (uint32_t)data.size() : 0, "SLK");
    if (ok) {
        std::cout << "  " << label << ": " << path << "  FOUND  (" << data.size() << " bytes)" << std::endl;
    } else {
        std::cout << "  " << label << ": " << path << "  NOT FOUND" << std::endl;
    }
}

static void AuditGroundTextures(const MpqManager& mpq, const W3EMap& w3e,
                                const SlkTable& terrainSlk) {
    int colTileID = -1, colDir = -1, colFile = -1;
    for (int i = 0; i < (int)terrainSlk.colNames_.size(); ++i) {
        if (terrainSlk.colNames_[i] == "tileID") colTileID = i;
        if (terrainSlk.colNames_[i] == "dir") colDir = i;
        if (terrainSlk.colNames_[i] == "file") colFile = i;
    }

    int missingSLK = 0, foundBLP = 0, missingBLP = 0, decodeFail = 0;
    for (size_t i = 0; i < w3e.groundTilePalette.size(); ++i) {
        uint32_t war3ID = w3e.groundTilePalette[i];
        std::string idStr = War3IDStr(war3ID);

        // Look up in SLK
        int row = -1;
        if (colTileID >= 0) row = terrainSlk.FindRow(idStr, colTileID);

        bool slkOk = (row >= 0);
        std::string dirStr, fileStr;
        if (slkOk) {
            dirStr = terrainSlk.GetString(row, colDir);
            fileStr = terrainSlk.GetString(row, colFile);
        }

        std::string blpPath;
        if (slkOk && !dirStr.empty() && !fileStr.empty()) {
            blpPath = dirStr + "\\" + fileStr + ".blp";
        }

        // Check BLP in MPQ
        std::vector<uint8_t> blpData;
        bool blpFound = !blpPath.empty() && ReadFromMpq(mpq, blpPath, blpData);

        std::string fmt;
        int alphaDepth = -1;
        bool decodeOk = false;
        if (blpFound) {
            decodeOk = DetectBlpFormat(blpData.data(), blpData.size(), fmt, alphaDepth);
        }

        std::string detail;
        char buf[256];
        if (!slkOk) {
            snprintf(buf, sizeof(buf), "[%zu] %s -> SLK row NOT FOUND", i, idStr.c_str());
            detail = buf;
            missingSLK++;
            Report("GroundTex", idStr, false, 0, "", -1, "SLK_MISS");
        } else if (!blpFound) {
            snprintf(buf, sizeof(buf), "[%zu] %s -> %s  NOT FOUND", i, idStr.c_str(), blpPath.c_str());
            detail = buf;
            missingBLP++;
            Report("GroundTex", blpPath, false, 0, fmt, alphaDepth, "BLP_MISS");
        } else if (!decodeOk) {
            snprintf(buf, sizeof(buf), "[%zu] %s -> %s  DECODE FAIL (%u bytes)", i, idStr.c_str(), blpPath.c_str(), (unsigned)blpData.size());
            detail = buf;
            decodeFail++;
            Report("GroundTex", blpPath, true, (uint32_t)blpData.size(), "?", -1, "DECODE_FAIL");
        } else {
            snprintf(buf, sizeof(buf), "[%zu] %s -> %s  FOUND  (%u bytes)  %s  alphaDepth=%d",
                     i, idStr.c_str(), blpPath.c_str(), (unsigned)blpData.size(), fmt.c_str(), alphaDepth);
            detail = buf;
            foundBLP++;
            Report("GroundTex", blpPath, true, (uint32_t)blpData.size(), fmt, alphaDepth);
        }
        std::cout << "  " << detail << std::endl;
    }

    int total = (int)w3e.groundTilePalette.size();
    std::cout << "  => " << total << " palette entries, "
              << foundBLP << " BLP OK, "
              << missingSLK << " SLK miss, "
              << missingBLP << " BLP miss, "
              << decodeFail << " decode fail" << std::endl;
}

static void AuditCliffTextures(const MpqManager& mpq, const W3EMap& w3e,
                               const SlkTable& cliffSlk) {
    int colCliffID = -1, colTexDir = -1, colTexFile = -1, colGroundTile = -1;
    int colCliffModelDir = -1, colRampModelDir = -1;
    for (int i = 0; i < (int)cliffSlk.colNames_.size(); ++i) {
        if (cliffSlk.colNames_[i] == "cliffID") colCliffID = i;
        if (cliffSlk.colNames_[i] == "texDir") colTexDir = i;
        if (cliffSlk.colNames_[i] == "texFile") colTexFile = i;
        if (cliffSlk.colNames_[i] == "groundTile") colGroundTile = i;
        if (cliffSlk.colNames_[i] == "cliffModelDir") colCliffModelDir = i;
        if (cliffSlk.colNames_[i] == "rampModelDir") colRampModelDir = i;
    }

    // Print available columns
    std::cout << "  SLK columns:";
    for (auto& c : cliffSlk.colNames_) std::cout << " \"" << c << "\"";
    std::cout << std::endl;

    int missingSLK = 0, foundBLP = 0, missingBLP = 0;
    for (size_t i = 0; i < w3e.cliffTilePalette.size(); ++i) {
        uint32_t war3ID = w3e.cliffTilePalette[i];
        std::string idStr = War3IDStr(war3ID);
        char buf[512];

        // Look up in SLK
        int row = -1;
        if (colCliffID >= 0) row = cliffSlk.FindRow(idStr, colCliffID);

        if (row < 0) {
            snprintf(buf, sizeof(buf), "  [%zu] %s -> SLK row NOT FOUND", i, idStr.c_str());
            std::cout << buf << std::endl;
            missingSLK++;
            Report("CliffTex", idStr, false, 0, "", -1, "SLK_MISS");
            continue;
        }

        std::string texDir = cliffSlk.GetString(row, colTexDir);
        std::string texFile = cliffSlk.GetString(row, colTexFile);
        std::string blpPath = texDir + "\\" + texFile + ".blp";

        // Read model dirs from CliffTypes.slk columns (matching TileSet.cpp logic)
        std::string cliffModelDir = (colCliffModelDir >= 0)
            ? cliffSlk.GetString(row, colCliffModelDir) : texFile;
        std::string rampModelDir = (colRampModelDir >= 0)
            ? cliffSlk.GetString(row, colRampModelDir) : std::string();

        // Ground tile mapping
        std::string groundTileStr;
        if (colGroundTile >= 0) groundTileStr = cliffSlk.GetString(row, colGroundTile);
        uint32_t groundWar3ID = 0;
        int groundPalIdx = -1;
        if (groundTileStr.size() >= 4) {
            groundWar3ID = StrToWar3ID(groundTileStr);
            for (size_t j = 0; j < w3e.groundTilePalette.size(); j++) {
                if (w3e.groundTilePalette[j] == groundWar3ID) {
                    groundPalIdx = (int)j;
                    break;
                }
            }
        }

        // Check BLP
        std::vector<uint8_t> blpData;
        bool blpOk = ReadFromMpq(mpq, blpPath, blpData);
        std::string fmt;
        int alphaDepth = -1;
        bool decodeOk = false;
        if (blpOk) decodeOk = DetectBlpFormat(blpData.data(), blpData.size(), fmt, alphaDepth);

        if (blpOk && decodeOk) {
            snprintf(buf, sizeof(buf),
                "  [%zu] %s -> %s  FOUND  (%u bytes)  %s  alpha=%d"
                "\n        cliffModelDir=%s  rampModelDir=%s"
                "\n        groundTile=%s -> groundPalIdx=%d",
                i, idStr.c_str(), blpPath.c_str(), (unsigned)blpData.size(),
                fmt.c_str(), alphaDepth,
                cliffModelDir.c_str(), rampModelDir.c_str(),
                groundTileStr.c_str(), groundPalIdx);
            foundBLP++;
            Report("CliffTex", blpPath, true, (uint32_t)blpData.size(), fmt, alphaDepth);
        } else if (blpOk && !decodeOk) {
            snprintf(buf, sizeof(buf), "  [%zu] %s -> %s  DECODE FAIL (%u bytes)", i, idStr.c_str(), blpPath.c_str(), (unsigned)blpData.size());
            Report("CliffTex", blpPath, true, (uint32_t)blpData.size(), "?", -1, "DECODE_FAIL");
        } else {
            snprintf(buf, sizeof(buf), "  [%zu] %s -> %s  NOT FOUND", i, idStr.c_str(), blpPath.c_str());
            missingBLP++;
            Report("CliffTex", blpPath, false);
        }
        std::cout << buf << std::endl;

        // Check cliff models: try common 4-letter codes
        const char* testCodes[] = {"AAAA", "ABAA", "BAAA", "BBAA", "AABA", "AAAB", "BABA", "ABBA"};
        for (auto code : testCodes) {
            // Cliff wall models
            std::string cwPath = "Doodads\\Terrain\\" + cliffModelDir + "\\" + cliffModelDir + code + "0.mdx";
            bool cwFound = mpq.FileExists(cwPath);
            if (cwFound || i == 0) {
                Report("CliffModel", cwPath, cwFound);
                if (cwFound)
                    std::cout << "        CliffModel: " << cwPath << "  FOUND" << std::endl;
            }

            // Ramp models
            std::string rmpPath = "Doodads\\Terrain\\" + rampModelDir + "\\" + rampModelDir + code + "0.mdx";
            bool rmpFound = mpq.FileExists(rmpPath);
            if (rmpFound || i == 0) {
                Report("RampModel", rmpPath, rmpFound);
                if (rmpFound)
                    std::cout << "        RampModel:  " << rmpPath << "  FOUND" << std::endl;
            }
        }

        // Try with variation suffixes
        for (int v = 0; v <= 7; v++) {
            std::string varPath = "Doodads\\Terrain\\" + cliffModelDir + "\\"
                + cliffModelDir + "AAAA" + std::to_string(v) + ".mdx";
            if (mpq.FileExists(varPath)) {
                std::cout << "        Variation:  " << varPath << "  FOUND" << std::endl;
            }
        }
    }

    int total = (int)w3e.cliffTilePalette.size();
    std::cout << "  => " << total << " cliff palette entries, "
              << foundBLP << " BLP OK, "
              << missingSLK << " SLK miss, "
              << missingBLP << " BLP miss" << std::endl;
}

static void AuditWaterTextures(const MpqManager& mpq) {
    std::cout << "\n--- Water Textures ---" << std::endl;
    for (int i = 0; i < 8; i++) {
        char path[128];
        snprintf(path, sizeof(path), "ReplaceableTextures\\Water\\Water%02d.blp", i);
        std::vector<uint8_t> data;
        bool ok = ReadFromMpq(mpq, path, data);
        std::string fmt;
        int alphaDepth = -1;
        if (ok) DetectBlpFormat(data.data(), data.size(), fmt, alphaDepth);
        const char* status = ok ? "FOUND" : "MISSING";
        if (ok) {
            printf("  %-50s %s  (%u bytes)  %s  alpha=%d\n",
                   path, status, (unsigned)data.size(), fmt.c_str(), alphaDepth);
        } else {
            printf("  %-50s %s\n", path, status);
        }
        Report("WaterTex", path, ok, ok ? (uint32_t)data.size() : 0, fmt, alphaDepth);
    }
}

static void AuditReplaceableTextures(const MpqManager& mpq) {
    std::cout << "\n--- Replaceable Textures ---" << std::endl;

    // Team color (IDs 1-2, player variants 00-23)
    for (int player = 0; player <= 23; player++) {
        char path[128];
        snprintf(path, sizeof(path), "ReplaceableTextures\\TeamColor\\TeamColor%02d.blp", player);
        bool ok = mpq.FileExists(path);
        if (ok || player < 2) {
            std::vector<uint8_t> data;
            if (ok) ReadFromMpq(mpq, path, data);
            std::string fmt;
            int alphaDepth = -1;
            if (ok && !data.empty()) DetectBlpFormat(data.data(), data.size(), fmt, alphaDepth);
            printf("  %-60s %s%s\n", path, ok ? "FOUND" : "MISSING",
                   ok ? (" (" + fmt + " alpha=" + std::to_string(alphaDepth) + ")").c_str() : "");
            Report("ReplaceableTex", path, ok, ok ? (uint32_t)data.size() : 0, fmt, alphaDepth);
        }
    }

    for (int player = 0; player <= 23; player++) {
        char path[128];
        snprintf(path, sizeof(path), "ReplaceableTextures\\TeamGlow\\TeamGlow%02d.blp", player);
        bool ok = mpq.FileExists(path);
        if (ok || player < 2) {
            std::vector<uint8_t> data;
            if (ok) ReadFromMpq(mpq, path, data);
            std::string fmt;
            int alphaDepth = -1;
            if (ok && !data.empty()) DetectBlpFormat(data.data(), data.size(), fmt, alphaDepth);
            printf("  %-60s %s%s\n", path, ok ? "FOUND" : "MISSING",
                   ok ? (" (" + fmt + " alpha=" + std::to_string(alphaDepth) + ")").c_str() : "");
            Report("ReplaceableTex", path, ok, ok ? (uint32_t)data.size() : 0, fmt, alphaDepth);
        }
    }

    // Cliff replaceable texture
    const char* nonPlayerPaths[] = {
        "ReplaceableTextures\\Cliff\\Cliff0.blp",
        "ReplaceableTextures\\LordaeronTree\\LordaeronSummerTree.blp",
        "ReplaceableTextures\\AshenvaleTree\\AshenTree.blp",
        "ReplaceableTextures\\BarrensTree\\BarrensTree.blp",
        "ReplaceableTextures\\NorthrendTree\\NorthTree.blp",
        "ReplaceableTextures\\Mushroom\\MushroomTree.blp",
        "ReplaceableTextures\\RuinsTree\\RuinsTree.blp",
        "ReplaceableTextures\\OutlandMushroomTree\\MushroomTree.blp",
    };
    for (auto path : nonPlayerPaths) {
        std::vector<uint8_t> data;
        bool ok = ReadFromMpq(mpq, path, data);
        std::string fmt;
        int alphaDepth = -1;
        if (ok) DetectBlpFormat(data.data(), data.size(), fmt, alphaDepth);
        printf("  %-60s %s%s\n", path, ok ? "FOUND" : "MISSING",
               ok ? (" (" + fmt + " alpha=" + std::to_string(alphaDepth) + ")").c_str() : "");
        Report("ReplaceableTex", path, ok, ok ? (uint32_t)data.size() : 0, fmt, alphaDepth);
    }
}

static void PrintSummary() {
    std::cout << "\n===================================" << std::endl;
    std::cout << "          AUDIT SUMMARY            " << std::endl;
    std::cout << "===================================" << std::endl;

    int total = (int)entries.size();
    int found = 0, missing = 0;
    for (auto& e : entries) {
        if (e.found) found++;
        else missing++;
    }

    printf("Total resources: %d\n", total);
    printf("Found:           %d\n", found);
    printf("Missing:         %d\n", missing);
    printf("Success rate:    %.1f%%\n", total > 0 ? (found * 100.0f / total) : 0.0f);

    // Per-category summary
    std::cout << "\n--- By Category ---" << std::endl;
    std::vector<std::string> cats;
    for (auto& e : entries) {
        if (std::find(cats.begin(), cats.end(), e.category) == cats.end())
            cats.push_back(e.category);
    }
    for (auto& cat : cats) {
        int cTotal = 0, cFound = 0, cMissing = 0;
        for (auto& e : entries) {
            if (e.category != cat) continue;
            cTotal++;
            if (e.found) cFound++;
            else cMissing++;
        }
        printf("  %-20s  %d total  %d found  %d missing\n", cat.c_str(), cTotal, cFound, cMissing);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Warsmash Resource Auditor" << std::endl;
    std::cout << "=========================" << std::endl;

    if (argc < 2) {
        std::cout << "Usage: " << (argc > 0 ? argv[0] : "resource_auditor") << " <map.w3x> [resources_path]" << std::endl;
        return 1;
    }

    std::string mapPath = argv[1];
    fs::path resourcesDir = fs::path(PROJECT_SOURCE_DIR) / "resources";
    if (argc >= 3) resourcesDir = argv[2];

    std::cout << "Map: " << mapPath << std::endl;
    std::cout << "Resources: " << resourcesDir << std::endl;

    // Open MPQ archives
    MpqManager mpq;
    for (const auto& name : {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"}) {
        fs::path p = resourcesDir / name;
        std::cout << "  MPQ: " << p.filename().string() << " ... "
                  << (fs::exists(p) ? (mpq.Open(p.string()) ? "OPENED" : "FAILED") : "NOT FOUND") << std::endl;
    }

    if (mpq.ArchiveCount() == 0) {
        std::cerr << "No MPQ archives opened!" << std::endl;
        return 1;
    }

    // Load map terrain data
    W3EMap w3e;
    bool mapOk = false;
    if (fs::path(mapPath).extension() == ".w3x" || fs::path(mapPath).extension() == ".w3m") {
        // Use W3X stormlib loading
        if (W3XLoadTerrain(mapPath, w3e)) {
            mapOk = true;
        } else {
            std::cerr << "Failed to load W3E from map: " << mapPath << std::endl;
        }
    }

    if (!mapOk) {
        std::cerr << "Could not load map terrain data" << std::endl;
        return 1;
    }

    std::cout << "\nTileset: " << w3e.tileset << std::endl;
    std::cout << "Ground palette: " << w3e.groundTilePalette.size() << " entries" << std::endl;
    std::cout << "Cliff palette:  " << w3e.cliffTilePalette.size() << " entries" << std::endl;
    std::cout << "Map size: " << w3e.tileWidth << "x" << w3e.tileHeight
              << " tiles (" << w3e.CornerCount() << " corners)" << std::endl;

    // --- SLK Files ---
    std::cout << "\n--- SLK Files ---" << std::endl;
    SlkTable terrainSlk;
    {
        std::vector<uint8_t> data;
        bool ok = ReadFromMpq(mpq, "TerrainArt\\Terrain.slk", data);
        AuditSlkLoad(mpq, "TerrainArt\\Terrain.slk", "Ground tile definitions");
        if (ok) terrainSlk.Load(data.data(), data.size());
    }

    SlkTable cliffSlk;
    {
        std::vector<uint8_t> data;
        bool ok = ReadFromMpq(mpq, "TerrainArt\\CliffTypes.slk", data);
        AuditSlkLoad(mpq, "TerrainArt\\CliffTypes.slk", "Cliff tile definitions");
        if (ok) cliffSlk.Load(data.data(), data.size());
    }

    {
        std::vector<uint8_t> data;
        bool ok = ReadFromMpq(mpq, "TerrainArt\\Water.slk", data);
        AuditSlkLoad(mpq, "TerrainArt\\Water.slk", "Water settings");
    }

    {
        std::vector<uint8_t> data;
        bool ok = ReadFromMpq(mpq, "UI\\WorldEditData.txt", data);
        AuditSlkLoad(mpq, "UI\\WorldEditData.txt", "WorldEdit fallback");
    }

    // Print Terrain.slk columns
    std::cout << "\n  Terrain.slk columns:";
    for (auto& c : terrainSlk.colNames_) std::cout << " \"" << c << "\"";
    std::cout << std::endl;

    // --- Ground Textures ---
    std::cout << "\n--- Ground Textures (" << w3e.groundTilePalette.size() << " palette entries) ---" << std::endl;
    AuditGroundTextures(mpq, w3e, terrainSlk);

    // --- Cliff Textures ---
    std::cout << "\n--- Cliff Textures (" << w3e.cliffTilePalette.size() << " palette entries) ---" << std::endl;
    AuditCliffTextures(mpq, w3e, cliffSlk);

    // --- Water Textures ---
    AuditWaterTextures(mpq);

    // --- Replaceable Textures ---
    AuditReplaceableTextures(mpq);

    // --- Summary ---
    PrintSummary();

    return 0;
}

#include "w3i.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>

static uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

static float ReadFloat(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

static std::string ReadString(const uint8_t* data, size_t& off, size_t size) {
    std::string s;
    while (off < size && data[off] != 0) { s += (char)data[off++]; }
    if (off < size) off++; // skip null
    return s;
}

#define W3I_CHK(n) do { if (off + (n) > size) return false; } while(0)

static uint32_t ReadTagBE(const uint8_t* p) {
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}

bool W3ILoad(const uint8_t* data, size_t size, W3iInfo& out) {
    if (size < 64) return false;

    size_t off = 0;
    out.version = (int)ReadLE32(data + off); off += 4;
    out.saves = (int)ReadLE32(data + off); off += 4;
    /* editorVersion = */ off += 4;

    if (out.version > 27) {
        if (off + 16 > size) return false;
        out.gameVersionMajor = (int)ReadLE32(data + off); off += 4;
        out.gameVersionMinor = (int)ReadLE32(data + off); off += 4;
        out.gameVersionPatch = (int)ReadLE32(data + off); off += 4;
        out.gameVersionCommit = (int)ReadLE32(data + off); off += 4;
    }

    out.name = ReadString(data, off, size);
    out.author = ReadString(data, off, size);
    out.description = ReadString(data, off, size);
    out.recommendedPlayers = ReadString(data, off, size);

    for (int i = 0; i < 8 && off + 4 <= size; ++i, off += 4)
        out.cameraBounds[i] = ReadFloat(data + off);
    for (int i = 0; i < 4 && off + 4 <= size; ++i, off += 4)
        out.cameraComplements[i] = (int)ReadLE32(data + off);

    if (off + 8 > size) return false;
    out.playableWidth = (int)ReadLE32(data + off); off += 4;
    out.playableHeight = (int)ReadLE32(data + off); off += 4;
    /* flags = */ off += 4;
    out.tileset = (char)data[off++];
    /* campaignBg = */ off += 4;

    // Field order mirrors ref War3MapW3i.load (players/forces/tech/random).
    if (out.version > 24) {
        out.loadingScreenModel = ReadString(data, off, size);
    }
    out.loadingScreenText = ReadString(data, off, size);
    out.loadingScreenTitle = ReadString(data, off, size);
    out.loadingScreenSubtitle = ReadString(data, off, size);
    W3I_CHK(4);
    out.gameDataSet = (int)ReadLE32(data + off); off += 4;
    if (out.version > 24) {
        out.prologueScreenModel = ReadString(data, off, size);
    }
    out.prologueScreenText = ReadString(data, off, size);
    out.prologueScreenTitle = ReadString(data, off, size);
    out.prologueScreenSubtitle = ReadString(data, off, size);
    if (out.version > 24) {
        W3I_CHK(4);
        out.useTerrainFog = (int)ReadLE32(data + off); off += 4;
        W3I_CHK(8);
        out.fogHeight[0] = ReadFloat(data + off); off += 4;
        out.fogHeight[1] = ReadFloat(data + off); off += 4;
        W3I_CHK(4);
        out.fogDensity = ReadFloat(data + off); off += 4;
        W3I_CHK(4);
        for (int i = 0; i < 4; i++) out.fogColor[i] = data[off++];
        W3I_CHK(4);
        out.globalWeather = (int)ReadLE32(data + off); off += 4;
        out.soundEnvironment = ReadString(data, off, size);
        W3I_CHK(1);
        out.lightEnvironmentTileset = (char)data[off++];
        W3I_CHK(4);
        for (int i = 0; i < 4; i++) out.waterVertexColor[i] = data[off++];
    }
    if (out.version > 27) {
        // unknown2ProbablyLua: 4 bytes (ref War3MapW3i).
        W3I_CHK(4);
        off += 4;
    }
    if (out.version > 30) {
        W3I_CHK(8);
        off += 8; // supportedModes + gameDataVersion
    }

    W3I_CHK(4);
    int playerCount = (int)ReadLE32(data + off); off += 4;
    if (playerCount < 0 || playerCount > 64) return false;
    out.players.resize((size_t)playerCount);
    for (int i = 0; i < playerCount; i++) {
        auto& p = out.players[(size_t)i];
        W3I_CHK(16);
        p.id = (int)ReadLE32(data + off); off += 4;
        p.type = (int)ReadLE32(data + off); off += 4;
        p.race = (int)ReadLE32(data + off); off += 4;
        p.fixedStartPosition = (int)ReadLE32(data + off); off += 4;
        p.name = ReadString(data, off, size);
        W3I_CHK(16);
        p.startX = ReadFloat(data + off); off += 4;
        p.startY = ReadFloat(data + off); off += 4;
        p.allyLowPrio = ReadLE32(data + off); off += 4;
        p.allyHighPrio = ReadLE32(data + off); off += 4;
        if (out.version > 30) {
            W3I_CHK(8);
            p.enemyLowFlags = ReadLE32(data + off); off += 4;
            p.enemyHighFlags = ReadLE32(data + off); off += 4;
        }
    }

    W3I_CHK(4);
    int forceCount = (int)ReadLE32(data + off); off += 4;
    if (forceCount < 0 || forceCount > 64) return false;
    out.forces.resize((size_t)forceCount);
    for (int i = 0; i < forceCount; i++) {
        auto& f = out.forces[(size_t)i];
        W3I_CHK(8);
        f.flags = ReadLE32(data + off); off += 4;
        f.playerMask = ReadLE32(data + off); off += 4;
        f.name = ReadString(data, off, size);
    }

    // Trailing sections are optional (protected maps may end early).
    if (off + 4 <= size) {
        int upgradeCount = (int)ReadLE32(data + off); off += 4;
        if (upgradeCount < 0 || upgradeCount > 100000) return false;
        out.upgradeAvailability.resize((size_t)upgradeCount);
        for (int i = 0; i < upgradeCount; i++) {
            W3I_CHK(16);
            auto& e = out.upgradeAvailability[(size_t)i];
            e.playerFlags = ReadLE32(data + off); off += 4;
            e.id = ReadTagBE(data + off); off += 4;
            e.levelAffected = (int)ReadLE32(data + off); off += 4;
            e.availability = (int)ReadLE32(data + off); off += 4;
        }
    }
    if (off + 4 <= size) {
        int techCount = (int)ReadLE32(data + off); off += 4;
        if (techCount < 0 || techCount > 100000) return false;
        out.techAvailability.resize((size_t)techCount);
        for (int i = 0; i < techCount; i++) {
            W3I_CHK(8);
            auto& e = out.techAvailability[(size_t)i];
            e.playerFlags = ReadLE32(data + off); off += 4;
            e.id = ReadTagBE(data + off); off += 4;
        }
    }
    if (off + 4 <= size) {
        int tableCount = (int)ReadLE32(data + off); off += 4;
        if (tableCount < 0 || tableCount > 10000) return false;
        out.randomUnitTables.resize((size_t)tableCount);
        for (int i = 0; i < tableCount; i++) {
            auto& t = out.randomUnitTables[(size_t)i];
            W3I_CHK(4);
            t.id = (int)ReadLE32(data + off); off += 4;
            t.name = ReadString(data, off, size);
            W3I_CHK(4);
            t.positions = (int)ReadLE32(data + off); off += 4;
            if (t.positions < 0 || t.positions > 64) return false;
            t.columnTypes.resize((size_t)t.positions);
            for (int c = 0; c < t.positions; c++) {
                W3I_CHK(4);
                t.columnTypes[(size_t)c] = (int)ReadLE32(data + off); off += 4;
            }
            W3I_CHK(4);
            int rowCount = (int)ReadLE32(data + off); off += 4;
            if (rowCount < 0 || rowCount > 100000) return false;
            t.units.resize((size_t)rowCount);
            for (int r = 0; r < rowCount; r++) {
                W3I_CHK(4);
                auto& e = t.units[(size_t)r];
                e.chance = (int)ReadLE32(data + off); off += 4;
                e.ids.resize((size_t)t.positions);
                for (int c = 0; c < t.positions; c++) {
                    W3I_CHK(4);
                    e.ids[(size_t)c] = ReadTagBE(data + off); off += 4;
                }
            }
        }
    }
    if (out.version > 24 && off + 4 <= size) {
        int itemTableCount = (int)ReadLE32(data + off); off += 4;
        if (itemTableCount < 0 || itemTableCount > 10000) return false;
        out.randomItemTables.resize((size_t)itemTableCount);
        for (int i = 0; i < itemTableCount; i++) {
            auto& t = out.randomItemTables[(size_t)i];
            W3I_CHK(4);
            t.id = ReadTagBE(data + off); off += 4;
            t.name = ReadString(data, off, size);
            W3I_CHK(4);
            int setCount = (int)ReadLE32(data + off); off += 4;
            if (setCount < 0 || setCount > 10000) return false;
            t.sets.resize((size_t)setCount);
            for (int s = 0; s < setCount; s++) {
                W3I_CHK(4);
                int itemCount = (int)ReadLE32(data + off); off += 4;
                if (itemCount < 0 || itemCount > 100000) return false;
                auto& set = t.sets[(size_t)s];
                set.resize((size_t)itemCount);
                for (int k = 0; k < itemCount; k++) {
                    W3I_CHK(8);
                    set[(size_t)k].chance = (int)ReadLE32(data + off); off += 4;
                    set[(size_t)k].id = ReadTagBE(data + off); off += 4;
                }
            }
        }
    }

    return true;
}

bool W3ILoadFromMpq(const std::string& mapPath, W3iInfo& out) {
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return false;

    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, "war3map.w3i", SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq);
        return false;
    }

    DWORD sz = SFileGetFileSize(hFile, nullptr);
    std::vector<uint8_t> buf(sz);
    DWORD rd = 0;
    bool ok = SFileReadFile(hFile, buf.data(), sz, &rd, nullptr) && rd == sz;
    SFileCloseFile(hFile);
    SFileCloseArchive(hMpq);

    if (!ok) return false;
    return W3ILoad(buf.data(), sz, out);
}

void W3IPrint(const W3iInfo& info) {
    std::cout << "\n=== Map Info ===" << std::endl;
    std::cout << "Name: " << info.name << std::endl;
    std::cout << "Author: " << info.author << std::endl;
    if (!info.description.empty())
        std::cout << "Desc: " << info.description << std::endl;
    if (!info.recommendedPlayers.empty())
        std::cout << "Players: " << info.recommendedPlayers << std::endl;
    std::cout << "Tileset: " << info.tileset << std::endl;
    std::cout << "Playable: " << info.playableWidth << "x" << info.playableHeight << std::endl;
    std::cout << "Camera: "
              << info.cameraBounds[0] << "," << info.cameraBounds[1] << " - "
              << info.cameraBounds[2] << "," << info.cameraBounds[3] << std::endl;
}

#include "unitsdoo.h"
#include <cstring>
#include <iostream>
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>

static uint32_t R32LE(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
static float RF32(const uint8_t* p) { float v; memcpy(&v, p, 4); return v; }
static uint32_t TagBE(const uint8_t* p) {
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}

#define CHK(n) do { if (off + (n) > size) return false; } while(0)

bool UnitsDooLoad(const uint8_t* data, size_t size, UnitsDooData& out, const W3iInfo* mapInfo) {
    size_t off = 0;
    CHK(4);
    const bool hasSkinId = mapInfo && (((mapInfo->gameVersionMajor * 100) + mapInfo->gameVersionMinor) >= 132);

    uint32_t magic = TagBE(data + off); off += 4;
    if (magic != TagBE((const uint8_t*)"W3do")) return false;

    CHK(8);
    out.version = (int)R32LE(data + off); off += 4;
    off += 4; // unknown

    int unitCount = (int)R32LE(data + off); off += 4;
    if (unitCount < 0 || unitCount > 50000) return false;
    out.units.resize(unitCount);

    for (int i = 0; i < unitCount; i++) {
        auto& u = out.units[i];

        CHK(36);
        u.id = TagBE(data + off); off += 4;
        u.variation = (int)R32LE(data + off); off += 4;
        u.pos[0] = RF32(data + off); off += 4;
        u.pos[1] = RF32(data + off); off += 4;
        u.pos[2] = RF32(data + off); off += 4;
        u.angle = RF32(data + off); off += 4;
        u.scale[0] = RF32(data + off); off += 4;
        u.scale[1] = RF32(data + off); off += 4;
        u.scale[2] = RF32(data + off); off += 4;
        if (hasSkinId) {
            CHK(4);
            u.skinId = TagBE(data + off); off += 4;
        } else {
            u.skinId = u.id;
        }

        // flags(1) + player(4) + unknown(2) + hp(4) + mana(4)
        CHK(15);
        u.flags = data[off++];
        u.player = (int)R32LE(data + off); off += 4;
        off += 2;
        u.hitpoints = (int)R32LE(data + off); off += 4;
        u.mana = (int)R32LE(data + off); off += 4;

        if (out.version > 7) { CHK(4); off += 4; }

        CHK(4);
        int dropSetCount = (int)R32LE(data + off); off += 4;
        if (dropSetCount < 0 || dropSetCount > 10000) return false;
        for (int ds = 0; ds < dropSetCount; ++ds) {
            CHK(4);
            int itemCount = (int)R32LE(data + off); off += 4;
            if (itemCount < 0 || itemCount > 10000) return false;
            if (itemCount > 0) { CHK((size_t)itemCount * 8); off += (size_t)itemCount * 8; }
        }

        CHK(16);
        u.goldAmount = (int)R32LE(data + off); off += 4;
        off += 4;
        u.heroLevel = (int)R32LE(data + off); off += 4;
        if (out.version > 7) { CHK(12); off += 12; }

        CHK(4);
        int invCount = (int)R32LE(data + off); off += 4;
        if (invCount < 0 || invCount > 10000) return false;
        if (invCount > 0) { CHK((size_t)invCount * 8); off += invCount * 8; }

        CHK(4);
        int abilityCount = (int)R32LE(data + off); off += 4;
        if (abilityCount < 0 || abilityCount > 10000) return false;
        if (abilityCount > 0) { CHK((size_t)abilityCount * 12); off += abilityCount * 12; }

        CHK(4);
        int randomFlag = (int)R32LE(data + off); off += 4;
        if (randomFlag == 0) { CHK(4); off += 4; }
        else if (randomFlag == 1) { CHK(8); off += 8; }
        else if (randomFlag == 2) {
            CHK(4);
            int randCount = (int)R32LE(data + off); off += 4;
            if (randCount < 0 || randCount > 10000) return false;
            if (randCount > 0) { CHK((size_t)randCount * 8); off += randCount * 8; }
        }

        CHK(12);
        u.customTeamColor = (int)R32LE(data + off); off += 4;
        off += 4;
        u.creationNumber = (int)R32LE(data + off); off += 4;
    }
    return true;
}

#undef CHK

bool UnitsDooLoadFromMpq(const std::string& mapPath, UnitsDooData& out, const W3iInfo* mapInfo) {
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return false;
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, "war3mapUnits.doo", SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq); return false;
    }
    DWORD sz = SFileGetFileSize(hFile, nullptr);
    std::vector<uint8_t> buf(sz);
    DWORD rd = 0;
    bool ok = SFileReadFile(hFile, buf.data(), sz, &rd, nullptr) && rd == sz;
    SFileCloseFile(hFile); SFileCloseArchive(hMpq);
    if (!ok) return false;
    return UnitsDooLoad(buf.data(), sz, out, mapInfo);
}

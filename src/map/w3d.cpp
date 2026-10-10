#include "w3d.h"
#include <cstring>
#include <iostream>
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>

static uint32_t TagBE(const uint8_t* p) {
    return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];
}
static uint32_t R32LE(const uint8_t* p) { // little-endian for non-tag values
    return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
}
static float RF32(const uint8_t* p) { float v; memcpy(&v, p, 4); return v; }

#define CHK(n) do { if (off + (n) > size) return false; } while(0)

static bool ParseDoodadRecords(const uint8_t* data, size_t size, size_t recordsOffset,
                               int version, int doodadCount, bool hasSkinId, W3dData& out) {
    size_t off = recordsOffset;
    out.doodads.resize(doodadCount);

    for (int i = 0; i < doodadCount; i++) {
        auto& d = out.doodads[i];

        CHK(36);
        d.id = TagBE(data + off); off += 4;
        d.variation = (int)R32LE(data + off); off += 4;
        d.pos[0] = RF32(data + off); off += 4;
        d.pos[1] = RF32(data + off); off += 4;
        d.pos[2] = RF32(data + off); off += 4;
        d.angle = RF32(data + off); off += 4;
        d.scale[0] = RF32(data + off); off += 4;
        d.scale[1] = RF32(data + off); off += 4;
        d.scale[2] = RF32(data + off); off += 4;

        d.skinId = 0;
        if (hasSkinId) {
            CHK(4);
            d.skinId = TagBE(data + off); off += 4;
        }

        CHK(2);
        d.flags = data[off++];
        d.life = data[off++];

        if (version > 7) {
            CHK(4);
            d.itemTable = (int)R32LE(data + off); off += 4;
            int itemSetCount = (int)R32LE(data + off); off += 4;
            if (itemSetCount < 0 || itemSetCount > 10000) return false;
            d.itemSets.resize(itemSetCount);
            for (int s = 0; s < itemSetCount; s++) {
                // ref RandomItemSet: uint32 item count, then 8 bytes per item
                // (War3ID + int32 chance).
                CHK(4);
                int itemCount = (int)R32LE(data + off); off += 4;
                if (itemCount < 0 || itemCount > 10000) return false;
                CHK((size_t)itemCount * 8);
                d.itemSets[s].items.resize(itemCount);
                for (int k = 0; k < itemCount; k++) {
                    d.itemSets[s].items[k].id = TagBE(data + off); off += 4;
                    d.itemSets[s].items[k].chance = (int)R32LE(data + off); off += 4;
                }
            }
        } else {
            d.itemTable = -1;
            d.itemSets.clear();
        }

        CHK(4);
        d.editorId = (int)R32LE(data + off); off += 4;
    }

    CHK(4);
    off += 4; // u2
    CHK(4);
    uint32_t terrainCount = R32LE(data + off); off += 4;
    if (terrainCount > 100000) return false;
    // ref TerrainDoodad: War3ID + u1 + 2x uint32 location (16 bytes each)
    CHK((size_t)terrainCount * 16);
    out.terrainDoodads.resize(terrainCount);
    for (uint32_t i = 0; i < terrainCount; i++) {
        auto& t = out.terrainDoodads[i];
        t.id = TagBE(data + off); off += 4;
        t.u1 = R32LE(data + off); off += 4;
        t.location[0] = R32LE(data + off); off += 4;
        t.location[1] = R32LE(data + off); off += 4;
    }
    out.terrainDoodadCount = terrainCount;
    return off == size;
}

bool W3dLoad(const uint8_t* data, size_t size, W3dData& out) {
    size_t off = 0;
    CHK(4);

    uint32_t magic = TagBE(data + off); off += 4;
    if (magic != TagBE((const uint8_t*)"W3do")) return false;

    CHK(8);
    out.version = (int)R32LE(data + off); off += 4;
    off += 4; // u1

    int doodadCount = (int)R32LE(data + off); off += 4;
    if (doodadCount < 0 || doodadCount > 50000) return false;
    size_t recordsOffset = off;

    W3dData parsedNoSkin = {};
    parsedNoSkin.version = out.version;
    bool okNoSkin = ParseDoodadRecords(data, size, recordsOffset, out.version, doodadCount, false, parsedNoSkin);

    W3dData parsedWithSkin = {};
    parsedWithSkin.version = out.version;
    bool okWithSkin = ParseDoodadRecords(data, size, recordsOffset, out.version, doodadCount, true, parsedWithSkin);

    bool useSkin = false;
    if (okWithSkin && !okNoSkin) useSkin = true;
    else if (!okWithSkin && okNoSkin) useSkin = false;
    else if (okWithSkin && okNoSkin) useSkin = (out.version > 7);
    else return false;

    out.doodads = useSkin ? std::move(parsedWithSkin.doodads) : std::move(parsedNoSkin.doodads);
    out.terrainDoodads = useSkin ? std::move(parsedWithSkin.terrainDoodads)
                                 : std::move(parsedNoSkin.terrainDoodads);
    out.terrainDoodadCount = useSkin ? parsedWithSkin.terrainDoodadCount : parsedNoSkin.terrainDoodadCount;

    if (!out.doodads.empty()) {
        std::cout << "W3D sample IDs:";
        int sampleCount = std::min<int>(5, (int)out.doodads.size());
        for (int i = 0; i < sampleCount; ++i) {
            uint32_t id = out.doodads[i].id;
            char s[5] = {(char)(id & 0xFF), (char)((id >> 8) & 0xFF),
                         (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF), 0};
            std::cout << " " << s;
        }
        std::cout << std::endl;
    }

    std::cout << "W3D: v" << out.version << " doodads=" << doodadCount
              << " terrain=" << out.terrainDoodadCount
              << " skinId=" << (useSkin ? "on" : "off") << std::endl;
    return true;
}

#undef CHK

bool W3dLoadFromMpq(const std::string& mapPath, W3dData& out) {
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return false;
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, "war3map.doo", SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq); return false;
    }
    DWORD sz = SFileGetFileSize(hFile, nullptr);
    std::vector<uint8_t> buf(sz);
    DWORD rd = 0;
    bool ok = SFileReadFile(hFile, buf.data(), sz, &rd, nullptr) && rd == sz;
    SFileCloseFile(hFile); SFileCloseArchive(hMpq);
    if (!ok) return false;
    return W3dLoad(buf.data(), sz, out);
}

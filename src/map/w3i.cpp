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

#include "wpm.h"
#include <cstring>
#include <iostream>
#ifndef __STORMLIB_NO_STATIC_LINK__
#define __STORMLIB_NO_STATIC_LINK__
#endif
#include <StormLib.h>

static uint32_t ReadLE32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

bool WpmData::IsWalkable(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return (pathing[y * width + x] & UNWALKABLE) == 0;
}

bool WpmData::IsFlyable(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return (pathing[y * width + x] & UNFLYABLE) == 0;
}

bool WpmData::IsBuildable(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return (pathing[y * width + x] & UNBUILDABLE) == 0;
}

bool WpmData::IsBlight(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return (pathing[y * width + x] & BLIGHTED) != 0;
}

bool WpmData::IsWater(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return (pathing[y * width + x] & UNSWIMABLE) == 0;
}

bool WpmLoad(const uint8_t* data, size_t size, WpmData& out) {
    if (size < 12) return false;
    if (std::memcmp(data, "MP3W", 4) != 0) return false;

    size_t off = 8;
    out.width = (int)ReadLE32(data + off); off += 4;
    out.height = (int)ReadLE32(data + off); off += 4;

    size_t expected = (size_t)out.width * out.height;
    if (off + expected > size) return false;

    out.pathing.assign(data + off, data + off + expected);
    return true;
}

bool WpmLoadFromMpq(const std::string& mapPath, WpmData& out) {
    HANDLE hMpq = nullptr;
    if (!SFileOpenArchive(mapPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &hMpq)) return false;
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(hMpq, "war3map.wpm", SFILE_OPEN_BASE_FILE, &hFile)) {
        SFileCloseArchive(hMpq); return false;
    }
    DWORD sz = SFileGetFileSize(hFile, nullptr);
    std::vector<uint8_t> buf(sz);
    DWORD rd = 0;
    bool ok = SFileReadFile(hFile, buf.data(), sz, &rd, nullptr) && rd == sz;
    SFileCloseFile(hFile); SFileCloseArchive(hMpq);
    if (!ok) return false;
    return WpmLoad(buf.data(), sz, out);
}

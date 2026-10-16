#include "wpm.h"
#include <cstring>
#include <iostream>
#include "map/path_tex.h"
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

bool WpmData::IsBoundary(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return true;
    // Full-nibble match: UNSWIMABLE (0x40), BLIGHTED (0x20) and
    // BLOCKVISION (0x10) all overlap the 0xF0 mask, so `!= 0` would flag
    // ordinary land/blight/vision cells as map edge.
    return (pathing[(size_t)y * (size_t)width + (size_t)x] & BOUNDARY) == BOUNDARY;
}

int WpmData::StampFootprint(float x, float y, int rotationDeg,
                            std::shared_ptr<const PathTexImage> image,
                            bool blocksVision) {
    if (!image || !image->valid || image->w <= 0 || image->h <= 0) return -1;
    if (!IsValid()) return -1;
    if (basePathing.size() != pathing.size()) basePathing = pathing;
    Stamp st;
    st.x = x;
    st.y = y;
    st.rotationDeg = rotationDeg;
    st.image = std::move(image);
    st.blocksVision = blocksVision;
    st.alive = true;
    const int id = (int)stamps.size();
    stamps.push_back(std::move(st));
    RebuildDynamic();
    return id;
}

void WpmData::UnstampFootprint(int id) {
    if (id < 0 || id >= (int)stamps.size()) return;
    if (!stamps[id].alive) return;
    stamps[id].alive = false;
    RebuildDynamic();
}

void WpmData::SetStampBlocksVision(int id, bool blocksVision) {
    if (id < 0 || id >= (int)stamps.size()) return;
    if (stamps[id].blocksVision == blocksVision) return;
    stamps[id].blocksVision = blocksVision;
    if (stamps[id].alive) RebuildDynamic();
}

void WpmData::RebuildDynamic() {
    if (basePathing.size() != pathing.size() || basePathing.empty()) {
        // First stamp on a legacy grid: treat current cells as pristine.
        if (!pathing.empty()) basePathing = pathing;
        else return;
    }
    pathing = basePathing;
    for (const Stamp& st : stamps) {
        if (!st.alive || !st.image || !st.image->valid) continue;
        const PathTexImage& img = *st.image;
        if (img.w <= 0 || img.h <= 0) continue;
        const int rotation = ((st.rotationDeg + 450) % 360 + 360) % 360;
        const int divW = (rotation % 180) != 0 ? img.h : img.w;
        const int divH = (rotation % 180) != 0 ? img.w : img.h;
        // Retail cell math truncates toward zero (ref PathingGrid.getCellX/Y).
        const int baseCx = static_cast<int>((st.x - originX) / kCellSize);
        const int baseCy = static_cast<int>((st.y - originY) / kCellSize);
        for (int j = 0; j < img.h; ++j) {
            for (int i = 0; i < img.w; ++i) {
                int rx = i, ry = j, rw = img.w, rh = img.h;
                PathTexRotate(i, j, img.w, img.h, st.rotationDeg, rx, ry, rw, rh);
                const int xx = baseCx + rx - divW / 2;
                const int yy = baseCy + ry - divH / 2;
                if (xx < 0 || yy < 0 || xx >= width || yy >= height) continue;
                // BufferedImage sample is vertically flipped (ref getRGB(i, h-1-j)).
                const uint8_t flags = PathTexPixelFlags(img, i, img.h - 1 - j, st.blocksVision);
                if (flags) pathing[(size_t)yy * (size_t)width + (size_t)xx] |= flags;
            }
        }
    }
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
    out.basePathing = out.pathing;
    out.stamps.clear();
    return true;
}

bool WpmLoadFromMpq(const std::string& mapPath, WpmData& out) {    HANDLE hMpq = nullptr;
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

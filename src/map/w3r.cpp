#include "w3r.h"
#include <cstring>
#include <iostream>
#include "resource/mpq_storm.h"

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
    if (off < size) off++;
    return s;
}

bool W3rLoad(const uint8_t* data, size_t size, W3rData& out) {
    if (size < 8) return false;

    size_t off = 0;
    out.version = (int)ReadLE32(data + off); off += 4;
    int count = (int)ReadLE32(data + off); off += 4;

    for (int i = 0; i < count; ++i) {
        if (off + 20 > size) break;
        W3rRegion r;
        r.left = ReadFloat(data + off); off += 4;
        r.bottom = ReadFloat(data + off); off += 4;
        r.right = ReadFloat(data + off); off += 4;
        r.top = ReadFloat(data + off); off += 4;
        r.name = ReadString(data, off, size);
        if (off + 4 > size) break;
        r.creationNumber = (int)ReadLE32(data + off); off += 4;

        if (off + 4 <= size) {
            r.weatherId = ReadLE32(data + off); off += 4;
        }
        r.ambientId = ReadString(data, off, size);
        if (off + 4 <= size) {
            r.color[0] = data[off++];
            r.color[1] = data[off++];
            r.color[2] = data[off++];
            r.color[3] = data[off++];
        }
        out.regions.push_back(r);
    }
    return !out.regions.empty();
}

bool W3rLoadFromMpq(const std::string& mapPath, W3rData& out) {
    std::vector<uint8_t> buf;
    if (!MpqManager::ReadSingleArchiveFile(mapPath, "war3map.w3r", buf)) return false;
    return W3rLoad(buf.data(), buf.size(), out);
}

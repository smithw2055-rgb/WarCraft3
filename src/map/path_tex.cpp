#include "path_tex.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "asset/BLP.h"
#include "stb/stb_image.h"
#include "map/wpm.h"
#include "resource/mpq_storm.h"

namespace {

std::string LowerCopy(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool EndsWith(const std::string& s, const char* suffix) {
    const std::string low = LowerCopy(s);
    const std::string suf = LowerCopy(suffix);
    return low.size() >= suf.size() &&
           low.compare(low.size() - suf.size(), suf.size(), suf) == 0;
}

} // namespace

std::shared_ptr<const PathTexImage> LoadPathTex(const MpqManager& mpq, const std::string& path) {
    static std::unordered_map<std::string, std::shared_ptr<const PathTexImage>> cache;
    if (path.empty() || path == "_") return nullptr;
    const std::string key = LowerCopy(path);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    std::shared_ptr<PathTexImage> img = std::make_shared<PathTexImage>();
    std::vector<uint8_t> buf;
    if (mpq.ReadFile(path, buf) && !buf.empty()) {
        if (EndsWith(path, ".tga")) {
            int w = 0, h = 0, channels = 0;
            stbi_uc* px = stbi_load_from_memory(buf.data(), (int)buf.size(), &w, &h, &channels, 3);
            if (px && w > 0 && h > 0) {
                img->w = w;
                img->h = h;
                img->rgb.assign(px, px + (size_t)w * (size_t)h * 3);
                img->valid = true;
            } else {
                std::cerr << "PathTex: stb_image failed for " << path << std::endl;
            }
            if (px) stbi_image_free(px);
        } else {
            BLPImage blp;
            if (DecodeBLP(buf.data(), buf.size(), blp) && blp.width > 0 && blp.height > 0 &&
                blp.pixels.size() >= (size_t)blp.width * (size_t)blp.height * 4) {
                img->w = blp.width;
                img->h = blp.height;
                img->rgb.resize((size_t)blp.width * (size_t)blp.height * 3);
                for (size_t i = 0; i < (size_t)blp.width * (size_t)blp.height; ++i) {
                    img->rgb[i * 3 + 0] = blp.pixels[i * 4 + 0];
                    img->rgb[i * 3 + 1] = blp.pixels[i * 4 + 1];
                    img->rgb[i * 3 + 2] = blp.pixels[i * 4 + 2];
                }
                img->valid = true;
            } else {
                std::cerr << "PathTex: BLP decode failed for " << path << std::endl;
            }
        }
    }
    if (!img->valid) {
        cache[key] = nullptr;
        return nullptr;
    }
    std::shared_ptr<const PathTexImage> out = img;
    cache[key] = out;
    return out;
}

uint8_t PathTexPixelFlags(const PathTexImage& img, int px, int py, bool blocksVision) {
    if (!img.valid || px < 0 || py < 0 || px >= img.w || py >= img.h) return 0;
    const size_t off = ((size_t)py * (size_t)img.w + (size_t)px) * 3;
    const uint8_t r = img.rgb[off + 0];
    const uint8_t g = img.rgb[off + 1];
    const uint8_t b = img.rgb[off + 2];
    uint8_t flags = 0;
    if (b > 127) flags |= WpmData::UNBUILDABLE;
    if (g > 127) flags |= WpmData::UNFLYABLE;
    if (r > 127) {
        flags |= (WpmData::UNWALKABLE | WpmData::UNSWIMABLE);
        if (blocksVision) flags |= WpmData::BLOCKVISION;
    }
    return flags;
}

void PathTexRotate(int i, int j, int w, int h, int rotationDeg,
                   int& x, int& y, int& rw, int& rh) {
    const int rotation = ((rotationDeg + 450) % 360 + 360) % 360;
    if ((rotation % 180) != 0) {
        rw = h;
        rh = w;
    } else {
        rw = w;
        rh = h;
    }
    x = i;
    y = j;
    switch (rotation) {
        case 90:
            x = h - 1 - j;
            y = i;
            break;
        case 180:
            x = w - 1 - i;
            y = h - 1 - j;
            break;
        case 270:
            x = j;
            y = w - 1 - i;
            break;
        default:
            break;
    }
}

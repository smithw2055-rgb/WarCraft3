#include "asset/MipChain.h"

#include <algorithm>

int FullMipCount(int w, int h) {
    if (w <= 0 || h <= 0) return 0;
    int count = 1;
    while ((w > 1 || h > 1) && count < SG_MAX_MIPMAPS) {
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
        count++;
    }
    return count;
}

std::pair<int, int> MipLevelDims(int baseW, int baseH, int level) {
    int w = baseW, h = baseH;
    for (int i = 0; i < level; i++) {
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
    }
    return {w, h};
}

void BoxDownsampleRGBA(const uint8_t* src, int sw, int sh, uint8_t* dst) {
    const int dw = std::max(sw / 2, 1);
    const int dh = std::max(sh / 2, 1);
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            const int sx0 = x * 2, sy0 = y * 2;
            const int sx1 = std::min(sx0 + 1, sw - 1);
            const int sy1 = std::min(sy0 + 1, sh - 1);
            const uint8_t* p00 = src + (sy0 * sw + sx0) * 4;
            const uint8_t* p01 = src + (sy0 * sw + sx1) * 4;
            const uint8_t* p10 = src + (sy1 * sw + sx0) * 4;
            const uint8_t* p11 = src + (sy1 * sw + sx1) * 4;
            uint8_t* d = dst + (y * dw + x) * 4;
            for (int c = 0; c < 4; ++c) {
                d[c] = (uint8_t)(((int)p00[c] + p01[c] + p10[c] + p11[c]) / 4);
            }
        }
    }
}

MipChain BuildMipChain(int baseW, int baseH, const uint8_t* basePixels,
                       std::vector<MipLevel> embedded) {
    MipChain chain;
    if (baseW <= 0 || baseH <= 0 || !basePixels) return chain;
    if (baseW > 4096 || baseH > 4096) return chain;
    const int count = FullMipCount(baseW, baseH);
    chain.levels.reserve((size_t)count);
    MipLevel base;
    base.width = baseW;
    base.height = baseH;
    base.pixels.assign(basePixels, basePixels + (size_t)baseW * baseH * 4);
    chain.levels.push_back(std::move(base));
    for (int i = 1; i < count; ++i) {
        auto [ew, eh] = MipLevelDims(baseW, baseH, i);
        bool tookEmbedded = false;
        if (i - 1 < (int)embedded.size()) {
            MipLevel& cand = embedded[(size_t)i - 1];
            // STRICT dims: GL reads exactly ew*eh texels per level, so a
            // short buffer would over-read. Mismatches fall through to the
            // generator below (e.g. JPEG-recompressed odd levels).
            if (cand.width == ew && cand.height == eh &&
                cand.pixels.size() == (size_t)ew * eh * 4) {
                chain.levels.push_back(std::move(cand));
                tookEmbedded = true;
            }
        }
        if (!tookEmbedded) {
            const MipLevel& prev = chain.levels.back();
            MipLevel gen;
            gen.width = ew;
            gen.height = eh;
            gen.pixels.resize((size_t)ew * eh * 4);
            BoxDownsampleRGBA(prev.pixels.data(), prev.width, prev.height,
                              gen.pixels.data());
            chain.levels.push_back(std::move(gen));
        }
    }
    return chain;
}

sg_image MakeChainedImage(const MipChain& chain) {
    if (chain.levels.empty()) return {0};
    if ((int)chain.levels.size() > SG_MAX_MIPMAPS) return {0};
    const MipLevel& base = chain.levels[0];
    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_2D;
    desc.width = base.width;
    desc.height = base.height;
    desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    desc.num_mipmaps = (int)chain.levels.size();
    for (size_t i = 0; i < chain.levels.size(); ++i) {
        desc.data.mip_levels[i].ptr = chain.levels[i].pixels.data();
        desc.data.mip_levels[i].size = chain.levels[i].pixels.size();
    }
    sg_image img = sg_make_image(&desc);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) return {0};
    // Immutable uploads copy synchronously: the chain may die with the
    // caller. Requires an active renderer (never call headless in tests).
    return img;
}

#pragma once
// Shared mip-chain builder for 2D RGBA8 uploads.
//
// Why this exists: every sampler in the renderer uses a mipmapped min
// filter (GL_LINEAR_MIPMAP_LINEAR), which requires a FULL chain down to
// 1x1. Uploading only level 0 leaves the texture incomplete (undefined
// sampling; black on strict drivers) and shimmers on minification.
// Terrain arrays already chain (TileSet::CreateTextureArray); this covers
// every other 2D upload (unit/UI/minimap/particle/ribbon/water).
//
// Embedded file mips (BLP/DDS) win when their dims match the expected
// halving exactly; anything missing is box-filtered from the last good
// level, so truncated files degrade instead of breaking.
#include <cstdint>
#include <utility>
#include <vector>

#include "third_party/sokol/sokol_gfx.h"

struct MipLevel {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // RGBA8, width*height*4
};

// Full chain length for w*h down to 1x1 (halving, floor, min 1), capped
// at the sokol limit. Always >= 1 for valid dims, 0 otherwise.
int FullMipCount(int w, int h);

// Expected dims of chain level i (level 0 = base).
std::pair<int, int> MipLevelDims(int baseW, int baseH, int level);

// 2x2 box filter with edge clamping; dst must hold dw*dh*4 where
// dw = max(sw/2,1), dh = max(sh/2,1). Same kernel as the terrain arrays.
void BoxDownsampleRGBA(const uint8_t* src, int sw, int sh, uint8_t* dst);

// Ownership-holding chain: embedded levels first (validated), generated
// tail after. Levels are always dense from 0; empty when base is invalid.
struct MipChain {
    std::vector<MipLevel> levels;
};

MipChain BuildMipChain(int baseW, int baseH, const uint8_t* basePixels,
                       std::vector<MipLevel> embedded = {});

// Uploads the full chain (num_mipmaps = levels). Invalid ({0}) when empty.
sg_image MakeChainedImage(const MipChain& chain);

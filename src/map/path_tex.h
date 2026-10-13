#pragma once
// Pathing-texture (pathTex) decoding and footprint helpers.
//
// Retail buildings/destructables/doodads carry a small TGA/BLP whose pixels
// encode per-cell blockers (ref PathingGrid.blit/checkPathingTexture):
//   R > 127 -> UNWALKABLE | UNSWIMABLE (+ BLOCKVISION when requested)
//   G > 127 -> UNFLYABLE
//   B > 127 -> UNBUILDABLE
// One pixel covers one 32-unit pathing cell. Rotation follows the retail
// convention ((degrees + 450) % 360, snapped to 90/180/270, else identity).
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MpqManager;

struct PathTexImage {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> rgb; // 3 bytes per pixel, row-major top-down
    bool valid = false;
};

// Loads (and caches, lowercase key) a pathTex file from the MPQ. Empty,
// "_" and missing files yield nullptr. TGA via stb_image, BLP via the
// asset decoder, mirroring War3MapViewer.loadPathingTexture.
std::shared_ptr<const PathTexImage> LoadPathTex(const MpqManager& mpq, const std::string& path);

// Pixel flags in WpmData::Flags bits. blocksVision mirrors the blit caller
// (destructable vision blocking); placement checks pass false.
uint8_t PathTexPixelFlags(const PathTexImage& img, int px, int py, bool blocksVision);

// Rotated footprint coordinate for pixel (i,j) (ref PathingGrid rotation
// switch). Outputs the cell offset (x,y) inside the rotated footprint and
// the rotated footprint size (rw,rh).
void PathTexRotate(int i, int j, int w, int h, int rotationDeg,
                   int& x, int& y, int& rw, int& rh);

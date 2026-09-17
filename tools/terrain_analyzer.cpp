#include "resource/mpq_storm.h"
#include "resource/slk.h"
#include "map/w3e.h"
#include "map/w3x_storm.h"
#include "asset/BLP.h"
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <map>
#include <set>

namespace fs = std::filesystem;

static constexpr float TILE_SIZE = 128.0f;

// ============================================================
// War3ID string conversion
// ============================================================
static std::string War3IDStr(uint32_t id) {
    char s[5] = {
        (char)(id & 0xFF),
        (char)((id >> 8) & 0xFF),
        (char)((id >> 16) & 0xFF),
        (char)((id >> 24) & 0xFF),
        0
    };
    return std::string(s);
}

static uint32_t StrToWar3ID(const std::string& s) {
    if (s.size() < 4) return 0;
    return (uint32_t)(unsigned char)s[0]
         | ((uint32_t)(unsigned char)s[1] << 8)
         | ((uint32_t)(unsigned char)s[2] << 16)
         | ((uint32_t)(unsigned char)s[3] << 24);
}

// ============================================================
// BLP format detection
// ============================================================
static std::string DetectBlpFormat(const uint8_t* data, size_t size, int& alphaDepth) {
    if (size < 28) return "TOO_SMALL";
    if (std::memcmp(data, "BLP", 3) != 0) return "NOT_BLP";
    uint32_t compression = *(const uint32_t*)(data + 4);
    alphaDepth = *(const int*)(data + 8);
    switch (compression) {
        case 0: return "JPEG";
        case 1: return "PAL";
        case 2: case 4:
            switch (alphaDepth) {
                case 0: case 1: return "DXT1";
                case 4: return "DXT3";
                case 8: return "DXT5";
                default: return "DXT?";
            }
        default: return "UNKNOWN";
    }
}

// ============================================================
// Data structures matching Java Terrain logic for comparison
// ============================================================
struct CornerEx {
    float groundHeight;
    float waterHeight;
    int mapEdge;
    int ramp;
    int blight;
    int water;
    int boundary;
    int groundTexture;
    int cliffVariation;
    int groundVariation;
    int cliffTexture;
    int layerHeight;

    CornerEx() : groundHeight(0), waterHeight(0), mapEdge(0), ramp(0), blight(0),
        water(0), boundary(0), groundTexture(0), cliffVariation(0),
        groundVariation(0), cliffTexture(0), layerHeight(0) {}

    bool isRamp() const { return ramp != 0; }
    int getCliffTexture() const { return cliffTexture; }
    int getGroundTexture() const { return groundTexture; }
    int getLayerHeight() const { return layerHeight; }
    int getGroundVariation() const { return groundVariation; }
    int getWater() const { return water; }
    float computeFinalGroundHeight() const {
        return groundHeight + layerHeight - 2.0f;
    }
};

struct RenderCornerEx : public CornerEx {
    bool cliff = false;
    bool romp = false;
    float rampAdjust = 0.0f;
    float depth = 0.0f;
    bool hideCliff = false;
};

// ============================================================
// C++ approach: RebuildCornerData
// ============================================================
static void CppRebuildCornerData(const W3EMap& map, std::vector<RenderCornerEx>& corners) {
    int count = map.CornerCount();
    corners.resize(count);
    for (int i = 0; i < count; i++) corners[i] = RenderCornerEx{};

    int w = map.tileWidth, h = map.tileHeight;

    // Ramp adjustments
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = y * w + x;
            auto& cppCorner = map.tiles[idx];
            if (!cppCorner.IsRamp()) continue;

            auto& a = map.tiles[(y - 1) * w + (x - 1)];
            auto& b = map.tiles[(y) * w + (x - 1)];
            auto& c = map.tiles[(y + 1) * w + (x - 1)];
            auto& d = map.tiles[(y + 1) * w + (x)];
            auto& e = map.tiles[(y + 1) * w + (x + 1)];
            auto& f = map.tiles[(y) * w + (x + 1)];
            auto& g = map.tiles[(y - 1) * w + (x + 1)];
            auto& h_ = map.tiles[(y - 1) * w + (x)];

            int base = cppCorner.LayerHeight();
            if ((b.IsRamp() && f.IsRamp()) || (d.IsRamp() && h_.IsRamp())) {
                float adjust = 0.0f;
                if (b.IsRamp() && f.IsRamp())
                    adjust = std::max(adjust, ((b.LayerHeight() + f.LayerHeight()) / 2.0f) - (float)base);
                if (d.IsRamp() && h_.IsRamp())
                    adjust = std::max(adjust, ((d.LayerHeight() + h_.LayerHeight()) / 2.0f) - (float)base);
                if (a.IsRamp() && e.IsRamp())
                    adjust = std::max(adjust, (((a.LayerHeight() + e.LayerHeight()) / 2.0f) - (float)base) / 2.0f);
                if (c.IsRamp() && g.IsRamp())
                    adjust = std::max(adjust, (((c.LayerHeight() + g.LayerHeight()) / 2.0f) - (float)base) / 2.0f);
                corners[idx].rampAdjust = adjust;
            }
        }
    }

    // Cliff detection
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            bool hasCliff = false;
            int cliffTex = 0;

            for (int dc = -1; dc <= 0 && !hasCliff; dc++) {
                for (int dr = -1; dr <= 0 && !hasCliff; dr++) {
                    int cx = x + dc, cy = y + dr;
                    if (cx < 0 || cy < 0 || cx >= w - 1 || cy >= h - 1) continue;
                    int cellIdx = cy * w + cx;
                    auto& bl = map.tiles[cellIdx];
                    auto& br = map.tiles[cellIdx + 1];
                    auto& tl = map.tiles[cellIdx + w];
                    auto& tr = map.tiles[cellIdx + w + 1];
                    int hBl = bl.LayerHeight(), hBr = br.LayerHeight();
                    int hTl = tl.LayerHeight(), hTr = tr.LayerHeight();
                    if (hBl != hBr || hBl != hTl || hBl != hTr) {
                        hasCliff = true;
                        if (bl.CliffTexture() != 15) cliffTex = bl.CliffTexture();
                        if (br.CliffTexture() != 15) cliffTex = br.CliffTexture();
                        if (tl.CliffTexture() != 15) cliffTex = tl.CliffTexture();
                        if (tr.CliffTexture() != 15) cliffTex = tr.CliffTexture();
                    }
                }
            }

            corners[idx].cliff = hasCliff;
            if (hasCliff && cliffTex != 15) corners[idx].cliffTexture = cliffTex;
        }
    }
}

// ============================================================
// Java approach: isCornerRampEntrance (Java-style)
// ============================================================
static bool JavaIsCornerRampEntrance(const std::vector<RenderCornerEx>& javaCorners, int columns, int rows, int x, int y) {
    if (x >= columns || y >= rows) return false;
    const auto& bl = javaCorners[y * columns + x];
    const auto& br = javaCorners[y * columns + (x + 1)];
    const auto& tl = javaCorners[(y + 1) * columns + x];
    const auto& tr = javaCorners[(y + 1) * columns + (x + 1)];
    return bl.isRamp() && tl.isRamp() && br.isRamp() && tr.isRamp()
        && !((bl.getLayerHeight() == tr.getLayerHeight())
             && (tl.getLayerHeight() == br.getLayerHeight()));
}

// ============================================================
// Java approach: Ramp calculation (fixed 0.5f)
// ============================================================
static void JavaCalculateRamps(const W3EMap& map, std::vector<RenderCornerEx>& javaCorners) {
    int columns = map.tileWidth;
    int rows = map.tileHeight;
    int w = map.tileWidth;

    for (int y = 1; y < rows - 1; ++y) {
        for (int x = 1; x < columns - 1; ++x) {
            int idx = y * w + x;
            auto& o = javaCorners[idx];
            if (!o.isRamp()) continue;

            auto& a = javaCorners[(y - 1) * w + (x - 1)];
            auto& b = javaCorners[(y) * w + (x - 1)];
            auto& c = javaCorners[(y + 1) * w + (x - 1)];
            auto& d = javaCorners[(y + 1) * w + (x)];
            auto& e = javaCorners[(y + 1) * w + (x + 1)];
            auto& f = javaCorners[(y) * w + (x + 1)];
            auto& g = javaCorners[(y - 1) * w + (x + 1)];
            auto& h_ = javaCorners[(y - 1) * w + (x)];

            int base = o.getLayerHeight();
            if ((b.isRamp() && f.isRamp()) || (d.isRamp() && h_.isRamp())) {
                float adjust = 0;
                if (b.isRamp() && f.isRamp())
                    adjust = std::max(adjust, ((b.getLayerHeight() + f.getLayerHeight()) / 2.0f) - (float)base);
                if (d.isRamp() && h_.isRamp())
                    adjust = std::max(adjust, ((d.getLayerHeight() + h_.getLayerHeight()) / 2.0f) - (float)base);
                if (a.isRamp() && e.isRamp())
                    adjust = std::max(adjust, (((a.getLayerHeight() + e.getLayerHeight()) / 2.0f) - (float)base) / 2.0f);
                if (c.isRamp() && g.isRamp())
                    adjust = std::max(adjust, (((c.getLayerHeight() + g.getLayerHeight()) / 2.0f) - (float)base) / 2.0f);
                javaCorners[idx].rampAdjust = adjust;
            }
        }
    }
}

// ============================================================
// Java-style updateGroundHeights (uses 0.5f fixed ramp)
// ============================================================
static void JavaUpdateGroundHeights(const W3EMap& map, std::vector<RenderCornerEx>& javaCorners,
                                    float* groundHeights, float* groundCornerHeights) {
    int columns = map.tileWidth;
    int rows = map.tileHeight;
    int w = columns;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < columns; x++) {
            int idx = y * w + x;
            groundHeights[idx] = javaCorners[idx].groundHeight;

            float rampHeight = 0.0f;
            // Check if in one of the configurations the bottom_left is a ramp
            bool found = false;
            for (int xOffset = -1; xOffset <= 0 && !found; xOffset++) {
                for (int yOffset = -1; yOffset <= 0 && !found; yOffset++) {
                    int cx = x + xOffset, cy = y + yOffset;
                    if (cx >= 0 && cx < columns - 1 && cy >= 0 && cy < rows - 1) {
                        int cellBase = cy * w + cx;
                        const auto& bl = javaCorners[cellBase];
                        const auto& br = javaCorners[cellBase + 1];
                        const auto& tl = javaCorners[cellBase + w];
                        const auto& tr = javaCorners[cellBase + w + 1];
                        int base = std::min({bl.getLayerHeight(), br.getLayerHeight(),
                                             tl.getLayerHeight(), tr.getLayerHeight()});
                        if (javaCorners[idx].getLayerHeight() != base) continue;
                        if (JavaIsCornerRampEntrance(javaCorners, columns, rows, cx, cy)) {
                            rampHeight = 0.5f;
                            found = true;
                        }
                    }
                }
            }

            float newGroundCornerHeight = javaCorners[idx].computeFinalGroundHeight() + rampHeight;
            groundCornerHeights[idx] = newGroundCornerHeight;
            javaCorners[idx].depth = (javaCorners[idx].getWater() != 0)
                ? (0.0f + javaCorners[idx].waterHeight) - newGroundCornerHeight
                : 0;
        }
    }
}

// ============================================================
// Java-style cliff detection (sets cliff in constructor)
// ============================================================
static void JavaDetectCliffs(const W3EMap& map, std::vector<RenderCornerEx>& javaCorners) {
    int columns = map.tileWidth;
    int rows = map.tileHeight;
    int w = columns;
    // First iteration: set cliff for bottomLeft corner of each cell
    // Java: for (int i = 0; i < (width - 1); i++) for (int j = 0; j < (height - 1); j++)
    // In Java, corners[i][j] where i is column, j is row
    // In our C++ flat array: idx = j * w + i (row * width + column)
    // But Java uses corners[i][j] = corners[column][row]
    // So bottomLeft = corners[i][j] = corners[column][row] = columns_ * row + column
    for (int x = 0; x < columns - 1; x++) {
        for (int y = 0; y < rows - 1; y++) {
            int blIdx = y * w + x;
            const auto& bl = javaCorners[blIdx];
            const auto& br = javaCorners[y * w + (x + 1)];
            const auto& tl = javaCorners[(y + 1) * w + x];
            const auto& tr = javaCorners[(y + 1) * w + (x + 1)];
            javaCorners[blIdx].cliff = (bl.getLayerHeight() != br.getLayerHeight())
                || (bl.getLayerHeight() != tl.getLayerHeight())
                || (bl.getLayerHeight() != tr.getLayerHeight());
        }
    }
}

// ============================================================
// C++ style: RealTileTexture
// ============================================================
static int CppRealTileTexture(const W3EMap& map, const std::vector<RenderCornerEx>& cornerExtra,
                             int x, int y, int columns, int rows, int tilesetTexCount,
                             int blightTexIdx, bool hasBlight) {
    int w = map.tileWidth;
    for (int dc = -1; dc <= 0; dc++) {
        for (int dr = -1; dr <= 0; dr++) {
            int blr = y + dr, blc = x + dc;
            if (blr < 0 || blc < 0 || blr >= rows - 1 || blc >= columns - 1) continue;
            int idx = blr * w + blc;
            auto& bl = map.tiles[idx];
            auto& br_rt = map.tiles[idx + 1];
            auto& tl_up = map.tiles[idx + w];
            auto& tr_diag = map.tiles[idx + w + 1];
            int hBl = bl.LayerHeight(), hBr = br_rt.LayerHeight();
            int hTl = tl_up.LayerHeight(), hTr = tr_diag.LayerHeight();
            if (hBl != hBr || hBl != hTl || hBl != hTr) {
                bool allRamp = bl.IsRamp() && br_rt.IsRamp() && tl_up.IsRamp() && tr_diag.IsRamp();
                bool anyRomp = cornerExtra[idx].romp || cornerExtra[idx + 1].romp
                            || cornerExtra[idx + w].romp || cornerExtra[idx + w + 1].romp;
                if (allRamp && !anyRomp) continue;
                int cliffPal = bl.CliffTexture();
                if (cliffPal == 15) cliffPal = 1;
                return cliffPal; // C++ uses CliffToGroundPalIdx at shader time
            }
        }
    }
    if (hasBlight && map.tiles[y * w + x].IsBlight())
        return blightTexIdx;
    return map.tiles[y * w + x].GroundTexture();
}

// ============================================================
// Java style: realTileTexture
// ============================================================
static int JavaRealTileTexture(const std::vector<RenderCornerEx>& javaCorners, int columns, int rows,
                               const std::vector<int>& cliffToGroundTexture, int blightTextureIndex, int x, int y) {
    int w = columns;
    for (int i = -1; i <= 0; i++) {
        for (int j = -1; j <= 0; j++) {
            int cx = x + i, cy = y + j;
            if (cx >= 0 && cx < columns && cy >= 0 && cy < rows) {
                int idx = cy * w + cx;
                if (javaCorners[idx].cliff) {
                    if (cx < columns - 1 && cy < rows - 1) {
                        int blIdx = cy * w + cx;
                        const auto& bl = javaCorners[blIdx];
                        const auto& br = javaCorners[cy * w + (cx + 1)];
                        const auto& tl = javaCorners[(cy + 1) * w + cx];
                        const auto& tr = javaCorners[(cy + 1) * w + (cx + 1)];
                        if (bl.isRamp() && tl.isRamp() && br.isRamp() && tr.isRamp()
                                && !bl.romp && !br.romp && !tl.romp && !tr.romp) {
                            break;
                        }
                    }
                    if (javaCorners[idx].romp || javaCorners[idx].cliff) {
                        int texture = javaCorners[idx].getCliffTexture();
                        if (texture == 15) texture -= 14;
                        return cliffToGroundTexture[texture];
                    }
                }
            }
        }
    }
    if (javaCorners[y * w + x].blight != 0)
        return blightTextureIndex;
    return javaCorners[y * w + x].getGroundTexture();
}

// ============================================================
// GetVariation (same in both)
// ============================================================
static short GetVariation(bool extended, int variation) {
    if (extended) {
        if (variation <= 15) return 16 + variation;
        else if (variation == 16) return 15;
        else return 0;
    } else {
        if (variation == 0) return 0;
        else return 15;
    }
}

// ============================================================
// C++ getTextureVariations
// ============================================================
static void CppGetTextureVariations(const W3EMap& map, const std::vector<RenderCornerEx>& cornerExtra,
                                    int x, int y, short* out, int offset,
                                    int columns, int rows, int tilesetTexCount, int blightTexIdx, bool hasBlight) {
    int bottomLeft = CppRealTileTexture(map, cornerExtra, x, y, columns, rows, tilesetTexCount, blightTexIdx, hasBlight);
    int bottomRight = CppRealTileTexture(map, cornerExtra, x + 1, y, columns, rows, tilesetTexCount, blightTexIdx, hasBlight);
    int topLeft = CppRealTileTexture(map, cornerExtra, x, y + 1, columns, rows, tilesetTexCount, blightTexIdx, hasBlight);
    int topRight = CppRealTileTexture(map, cornerExtra, x + 1, y + 1, columns, rows, tilesetTexCount, blightTexIdx, hasBlight);

    int vals[4] = {bottomLeft, bottomRight, topLeft, topRight};
    std::sort(vals, vals + 4);
    int unique[4];
    int n = 0;
    for (int i = 0; i < 4; i++)
        if (n == 0 || vals[i] != unique[n - 1]) unique[n++] = vals[i];

    for (int i = 0; i < 4; i++) out[offset + i] = 17;

    int palFirst = unique[0];
    int cornerIdx = y * columns + x;
    int variation = map.tiles[cornerIdx].GroundVariation();
    out[offset] = (short)(palFirst + (GetVariation(false, variation) << 5));

    int comp = offset + 1;
    for (int v = 1; v < n; v++) {
        int palIdx = unique[v];
        int mask = 0;
        if (bottomRight == palIdx) mask |= 1;
        if (bottomLeft == palIdx)  mask |= 2;
        if (topRight == palIdx)    mask |= 4;
        if (topLeft == palIdx)     mask |= 8;
        out[comp++] = (short)(palIdx + (mask << 5));
    }

    if (cornerExtra[cornerIdx].cliff || cornerExtra[cornerIdx].romp) {
        if (offset >= 3) {
            // Check isCornerRampEntrance
            int w = columns;
            auto& bl = map.tiles[cornerIdx];
            auto& br = map.tiles[cornerIdx + 1];
            auto& tl = map.tiles[cornerIdx + w];
            auto& tr = map.tiles[cornerIdx + w + 1];
            bool isRampEntrance = bl.IsRamp() && br.IsRamp() && tl.IsRamp() && tr.IsRamp()
                && !(bl.LayerHeight() == tr.LayerHeight() && tl.LayerHeight() == br.LayerHeight());
            if (!isRampEntrance)
                out[offset + 3] |= (short)0x8000;
        }
    }
}

// ============================================================
// Java getTextureVariations
// ============================================================
static void JavaGetTextureVariations(const std::vector<RenderCornerEx>& javaCorners, int columns, int rows,
                                     const std::vector<int>& cliffToGroundTexture, int blightTextureIndex,
                                     int x, int y, short* out, int offset) {
    int bottomLeft = JavaRealTileTexture(javaCorners, columns, rows, cliffToGroundTexture, blightTextureIndex, x, y);
    int bottomRight = JavaRealTileTexture(javaCorners, columns, rows, cliffToGroundTexture, blightTextureIndex, x + 1, y);
    int topLeft = JavaRealTileTexture(javaCorners, columns, rows, cliffToGroundTexture, blightTextureIndex, x, y + 1);
    int topRight = JavaRealTileTexture(javaCorners, columns, rows, cliffToGroundTexture, blightTextureIndex, x + 1, y + 1);

    std::set<int> set;
    set.insert(bottomLeft);
    set.insert(bottomRight);
    set.insert(topLeft);
    set.insert(topRight);

    for (int i = 0; i < 4; i++) out[offset + i] = 17;
    int component = offset + 1;

    auto it = set.begin();
    short firstValue = (short)*it;
    out[offset] = (short)(firstValue + (GetVariation(false, javaCorners[y * columns + x].getGroundVariation()) << 5));

    ++it;
    for (; it != set.end(); ++it) {
        int texture = *it;
        int mask = 0;
        mask |= (bottomRight == texture ? 1 : 0) << 0;
        mask |= (bottomLeft == texture ? 1 : 0) << 1;
        mask |= (topRight == texture ? 1 : 0) << 2;
        mask |= (topLeft == texture ? 1 : 0) << 3;
        out[component++] = (short)(texture + (mask << 5));
    }

    // Set the high bit if this is a cliff/romp cell that is NOT a ramp entrance
    if (javaCorners[y * columns + x].cliff || javaCorners[y * columns + x].romp) {
        if (!JavaIsCornerRampEntrance(javaCorners, columns, rows, x, y))
            out[offset + 3] |= (short)0x8000;
    }
}

// ============================================================
// Build cliff mesh data (C++ style)
// ============================================================
struct CliffVertex { float px, py, pz, tu, tv, nx, ny, nz; };
struct CliffMeshData {
    int textureLayer;
    std::vector<CliffVertex> verts;
    std::vector<unsigned> indices;
};

static void CppBuildCliffMeshData(const W3EMap& map, std::map<int, CliffMeshData>& cliffMeshesByTex) {
    int w = map.tileWidth, h = map.tileHeight;

    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            if (c < w - 1) {
                auto& a = map.tiles[r * w + c];
                auto& b = map.tiles[r * w + c + 1];
                if (a.LayerHeight() != b.LayerHeight()) {
                    auto& hi = (a.LayerHeight() > b.LayerHeight()) ? a : b;
                    int ci = hi.CliffTexture();
                    if (ci >= (int)map.cliffTilePalette.size()) ci = 0;
                    float ha = (a.FinalGroundHeight()) * TILE_SIZE;
                    float hb = (b.FinalGroundHeight()) * TILE_SIZE;
                    float lo = std::min(ha, hb), up = std::max(ha, hb);
                    float x = (c + 1) * TILE_SIZE + map.offsetX;
                    float z = r * TILE_SIZE + map.offsetY;

                    float dh = up - lo;

                    auto& mesh = cliffMeshesByTex[ci];
                    mesh.textureLayer = ci;
                    unsigned ba = (unsigned)mesh.verts.size();

                    // Normal for right-facing cliff wall (X edge)
                    // Cross product of (0, dh, 0) x (0, 0, TILE_SIZE)
                    float n1x = dh * TILE_SIZE;
                    float n1y = 0;
                    float n1z = 0;
                    float n1len = std::sqrt(n1x*n1x + n1y*n1y + n1z*n1z);
                    if (n1len > 1e-8f) { n1x /= n1len; n1y /= n1len; n1z /= n1len; }

                    // Normal for front-facing cliff wall (Z edge)
                    // Cross product of (0, 0, TILE_SIZE) x (0, dh, 0)
                    float n2x = 0;
                    float n2y = TILE_SIZE * dh;
                    float n2z = 0;
                    float n2len = std::sqrt(n2x*n2x + n2y*n2y + n2z*n2z);
                    if (n2len > 1e-8f) { n2x /= n2len; n2y /= n2len; n2z /= n2len; }

                    CliffVertex v;
                    v = {x, lo, z, 0, 0, n1x, n1y, n1z}; mesh.verts.push_back(v);
                    v = {x, up, z, 1, 0, n1x, n1y, n1z}; mesh.verts.push_back(v);
                    v = {x, lo, z + TILE_SIZE, 0, 1, n2x, n2y, n2z}; mesh.verts.push_back(v);
                    v = {x, up, z + TILE_SIZE, 1, 1, n2x, n2y, n2z}; mesh.verts.push_back(v);
                    mesh.indices.push_back(ba); mesh.indices.push_back(ba+1); mesh.indices.push_back(ba+2);
                    mesh.indices.push_back(ba+1); mesh.indices.push_back(ba+3); mesh.indices.push_back(ba+2);
                }
            }
            if (r < h - 1) {
                auto& a = map.tiles[r * w + c];
                auto& b = map.tiles[(r + 1) * w + c];
                if (a.LayerHeight() != b.LayerHeight()) {
                    auto& hi = (a.LayerHeight() > b.LayerHeight()) ? a : b;
                    int ci = hi.CliffTexture();
                    if (ci >= (int)map.cliffTilePalette.size()) ci = 0;
                    float ha = (a.FinalGroundHeight()) * TILE_SIZE;
                    float hb = (b.FinalGroundHeight()) * TILE_SIZE;
                    float lo = std::min(ha, hb), up = std::max(ha, hb);
                    float x = c * TILE_SIZE + map.offsetX;
                    float z = (r + 1) * TILE_SIZE + map.offsetY;

                    auto& mesh = cliffMeshesByTex[ci];
                    mesh.textureLayer = ci;
                    unsigned ba = (unsigned)mesh.verts.size();
                    float nx = (a.LayerHeight() > b.LayerHeight()) ? 1.0f : -1.0f;
                    CliffVertex v2;
                    v2 = {x, lo, z, 0, 0, nx, 0, 0}; mesh.verts.push_back(v2);
                    v2 = {x, up, z, 1, 0, nx, 0, 0}; mesh.verts.push_back(v2);
                    v2 = {x + TILE_SIZE, lo, z, 0, 1, -nx, 0, 0}; mesh.verts.push_back(v2);
                    v2 = {x + TILE_SIZE, up, z, 1, 1, -nx, 0, 0}; mesh.verts.push_back(v2);
                    mesh.indices.push_back(ba); mesh.indices.push_back(ba+1); mesh.indices.push_back(ba+2);
                    mesh.indices.push_back(ba+1); mesh.indices.push_back(ba+3); mesh.indices.push_back(ba+2);
                }
            }
        }
    }
}

// ============================================================
// Main analyzer
// ============================================================
int main(int argc, char* argv[]) {
    std::cout << "==========================================================" << std::endl;
    std::cout << "  Warcraft3 Terrain System - Comparative Analyzer" << std::endl;
    std::cout << "  (C++ implementation vs Java warsmash_ref reference)" << std::endl;
    std::cout << "==========================================================" << std::endl;
    std::cout << std::endl;

    if (argc < 2) {
        std::cout << "Usage: " << (argc > 0 ? argv[0] : "terrain_analyzer")
                  << " <map.w3x/w3m> [resources_path]" << std::endl;
        return 1;
    }

    std::string mapPath = argv[1];
    fs::path resourcesDir = fs::path(PROJECT_SOURCE_DIR) / "resources";
    if (argc >= 3) resourcesDir = argv[2];

    std::cout << "Map:       " << mapPath << std::endl;
    std::cout << "Resources: " << resourcesDir << std::endl;
    std::cout << std::endl;

    // Open MPQ archives
    MpqManager mpq;
    for (const auto& name : {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"}) {
        fs::path p = resourcesDir / name;
        bool exists = fs::exists(p);
        bool opened = false;
        if (exists) opened = mpq.Open(p.string());
        std::cout << "  MPQ " << name << ": " << (exists ? (opened ? "OPENED" : "FAILED") : "NOT FOUND") << std::endl;
    }

    if (mpq.ArchiveCount() == 0) {
        std::cerr << "No MPQ archives opened!" << std::endl;
        return 1;
    }

    // Output directory
    fs::path outputDir = fs::path("terrain_analysis");
    fs::create_directory(outputDir);
    fs::path mapBase = fs::path(mapPath).stem();
    fs::path mapOutDir = outputDir / mapBase;
    fs::create_directory(mapOutDir);

    // Open report file
    std::ofstream report((mapOutDir / "report.txt").string());
    auto reportln = [&](const std::string& s = "") {
        report << s << std::endl;
        std::cout << s << std::endl;
    };

    reportln("==========================================================");
    reportln("  TERRAIN ANALYSIS REPORT");
    reportln("  Map: " + mapPath);
    reportln("==========================================================");
    reportln();

    // ============================================================
    // Step 1: Load W3E data
    // ============================================================
    reportln("=== STEP 1: Load W3E Terrain Data ===");
    W3EMap w3e;
    if (!W3XLoadTerrain(mapPath, w3e)) {
        reportln("ERROR: Failed to load W3E from map!");
        return 1;
    }

    int columns = w3e.tileWidth;
    int rows = w3e.tileHeight;
    int cornerCount = columns * rows;
    int tileCount = (columns - 1) * (rows - 1);
    int w = columns;

    reportln("  Version:      " + std::to_string(w3e.version));
    reportln("  Tileset:      " + std::string(1, w3e.tileset));
    reportln("  Dimensions:   " + std::to_string(columns) + "x" + std::to_string(rows) + " corners");
    reportln("  Tiles:        " + std::to_string(tileCount));
    reportln("  Offset:       " + std::to_string(w3e.offsetX) + ", " + std::to_string(w3e.offsetY));
    reportln("  Ground Pal:   " + std::to_string(w3e.groundTilePalette.size()) + " entries");
    for (size_t i = 0; i < w3e.groundTilePalette.size(); i++)
        reportln("    [" + std::to_string(i) + "] " + War3IDStr(w3e.groundTilePalette[i]));
    reportln("  Cliff Pal:    " + std::to_string(w3e.cliffTilePalette.size()) + " entries");
    for (size_t i = 0; i < w3e.cliffTilePalette.size(); i++)
        reportln("    [" + std::to_string(i) + "] " + War3IDStr(w3e.cliffTilePalette[i]));
    reportln();

    // ============================================================
    // Step 2: Build Java-compatible corners from raw W3E data
    // ============================================================
    reportln("=== STEP 2: Build corner data (both implementations) ===");

    // Initialize corners with raw W3E tile data (matching Java Corner.load())
    std::vector<RenderCornerEx> javaCorners(cornerCount);
    for (int i = 0; i < cornerCount; i++) {
        auto& cppCorner = w3e.tiles[i];
        auto& jc = javaCorners[i];
        jc.groundHeight = cppCorner.DecodeGroundHeight();
        jc.waterHeight = cppCorner.DecodeWaterHeight();
        jc.mapEdge = cppCorner.waterAndBoundary & 0x4000;
        jc.ramp = cppCorner.IsRamp() ? 0b00010000 : 0;
        jc.blight = cppCorner.IsBlight() ? 0b00100000 : 0;
        jc.water = cppCorner.IsWater() ? 0b01000000 : 0;
        jc.boundary = cppCorner.IsBoundary() ? 0b10000000 : 0;
        jc.groundTexture = cppCorner.GroundTexture();
        jc.cliffVariation = cppCorner.CliffVariation();
        jc.groundVariation = cppCorner.GroundVariation();
        jc.cliffTexture = cppCorner.CliffTexture();
        jc.layerHeight = cppCorner.LayerHeight();
    }

    // C++ RebuildCornerData
    std::vector<RenderCornerEx> cppCorners;
    CppRebuildCornerData(w3e, cppCorners);

    // Java cliff detection (sets cliff field in constructor)
    JavaDetectCliffs(w3e, javaCorners);
    // Java calculate ramps
    JavaCalculateRamps(w3e, javaCorners);

    // Compare cliff detection
    reportln("  Cliff detection comparison (sample):");
    int cliffDiffCount = 0;
    for (int i = 0; i < cornerCount; i++) {
        if (cppCorners[i].cliff != javaCorners[i].cliff) cliffDiffCount++;
    }
    reportln("    Cliff flag mismatches: " + std::to_string(cliffDiffCount) + " / " + std::to_string(cornerCount));

    // Compare ramp adjustments
    int rampAdjustCount_cpp = 0, rampAdjustCount_java = 0;
    float maxRampDiff = 0.0f;
    for (int i = 0; i < cornerCount; i++) {
        if (cppCorners[i].rampAdjust > 0) rampAdjustCount_cpp++;
        if (javaCorners[i].rampAdjust > 0) rampAdjustCount_java++;
        float diff = std::abs(cppCorners[i].rampAdjust - javaCorners[i].rampAdjust);
        if (diff > maxRampDiff) maxRampDiff = diff;
    }
    reportln("    C++ ramp adjustments: " + std::to_string(rampAdjustCount_cpp));
    reportln("    Java ramp adjustments: " + std::to_string(rampAdjustCount_java));
    reportln("    Max ramp adjust diff: " + std::to_string(maxRampDiff));
    reportln();

    // Write full corner comparison
    std::ofstream cornerCsv((mapOutDir / "corners_comparison.csv").string());
    cornerCsv << "idx,x,y,"
              << "cpp_groundHeight,cpp_waterHeight,cpp_groundTex,cpp_cliffTex,cpp_layerH,cpp_ramp,cpp_blight,cpp_water,cpp_boundary,"
              << "cpp_groundVar,cpp_cliffVar,cpp_rampAdjust,cpp_cliff,cpp_finalGroundH,"
              << "java_groundHeight,java_waterHeight,java_groundTex,java_cliffTex,java_layerH,java_ramp,java_blight,java_water,java_boundary,"
              << "java_groundVar,java_cliffVar,java_rampAdjust,java_cliff,java_finalGroundH,"
              << "cpp_groundCornerH_linear,java_groundCornerH_fixed"
              << std::endl;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < columns; x++) {
            int idx = y * w + x;
            const auto& cppC = cppCorners[idx];
            const auto& javaC = javaCorners[idx];
            const auto& raw = w3e.tiles[idx];

            // Java-style: compute final height with fixed 0.5f ramp
            float javaRampedHeight;
            {
                float rampHeight = 0.0f;
                for (int xOff = -1; xOff <= 0; xOff++) {
                    for (int yOff = -1; yOff <= 0; yOff++) {
                        int cx = x + xOff, cy = y + yOff;
                        if (cx >= 0 && cx < columns - 1 && cy >= 0 && cy < rows - 1) {
                            int cellBase = cy * w + cx;
                            const auto& bl = javaCorners[cellBase];
                            const auto& br = javaCorners[cellBase + 1];
                            const auto& tl = javaCorners[cellBase + w];
                            const auto& tr = javaCorners[cellBase + w + 1];
                            int base = std::min({bl.getLayerHeight(), br.getLayerHeight(),
                                                 tl.getLayerHeight(), tr.getLayerHeight()});
                            if (javaC.getLayerHeight() != base) continue;
                            if (JavaIsCornerRampEntrance(javaCorners, columns, rows, cx, cy)) {
                                rampHeight = 0.5f;
                            }
                        }
                    }
                }
                javaRampedHeight = javaC.computeFinalGroundHeight() + rampHeight;
            }

            // C++ style
            float cppRampedHeight = raw.FinalGroundHeight() + cppC.rampAdjust;

            cornerCsv << idx << ","
                      << x << "," << y << ","
                      << raw.DecodeGroundHeight() << "," << raw.DecodeWaterHeight() << ","
                      << (int)raw.GroundTexture() << "," << (int)raw.CliffTexture() << "," << raw.LayerHeight() << ","
                      << (raw.IsRamp() ? 1 : 0) << "," << (raw.IsBlight() ? 1 : 0) << "," << (raw.IsWater() ? 1 : 0) << "," << (raw.IsBoundary() ? 1 : 0) << ","
                      << (int)raw.GroundVariation() << "," << (int)raw.CliffVariation() << ","
                      << cppC.rampAdjust << "," << (cppC.cliff ? 1 : 0) << ","
                      << raw.FinalGroundHeight() << ","
                      << javaC.groundHeight << "," << javaC.waterHeight << ","
                      << javaC.groundTexture << "," << javaC.cliffTexture << "," << javaC.layerHeight << ","
                      << (javaC.isRamp() ? 1 : 0) << "," << javaC.blight << "," << javaC.water << "," << javaC.boundary << ","
                      << javaC.groundVariation << "," << javaC.cliffVariation << ","
                      << javaC.rampAdjust << "," << (javaC.cliff ? 1 : 0) << ","
                      << javaC.computeFinalGroundHeight() << ","
                      << cppRampedHeight << ","
                      << javaRampedHeight
                      << std::endl;
        }
    }
    reportln("  Corner comparison saved to: " + (mapOutDir / "corners_comparison.csv").string());
    reportln();

    // ============================================================
    // Step 3: Resource audit
    // ============================================================
    reportln("=== STEP 3: Resource Audit ===");

    // Load SLK files
    SlkTable terrainSlk;
    std::vector<uint8_t> terrainSlkData;
    if (mpq.ReadFile("TerrainArt\\Terrain.slk", terrainSlkData))
        terrainSlk.Load(terrainSlkData.data(), terrainSlkData.size());
    reportln("  Terrain.slk: " + std::to_string(terrainSlkData.size()) + " bytes, "
                 + std::to_string(terrainSlk.colNames_.size()) + " columns, "
                 + std::to_string(terrainSlk.Rows()) + " rows");

    SlkTable cliffSlk;
    std::vector<uint8_t> cliffSlkData;
    if (mpq.ReadFile("TerrainArt\\CliffTypes.slk", cliffSlkData))
        cliffSlk.Load(cliffSlkData.data(), cliffSlkData.size());
    reportln("  CliffTypes.slk: " + std::to_string(cliffSlkData.size()) + " bytes, "
                 + std::to_string(cliffSlk.colNames_.size()) + " columns, "
                 + std::to_string(cliffSlk.Rows()) + " rows");

    // Find column indices
    int colTileID = -1, colDir = -1, colFile = -1;
    int colCliffID = -1, colTexDir = -1, colTexFile = -1, colGroundTile = -1;
    int colCliffModelDir = -1, colRampModelDir = -1;
    for (int i = 0; i < (int)terrainSlk.colNames_.size(); ++i) {
        if (terrainSlk.colNames_[i] == "tileID") colTileID = i;
        if (terrainSlk.colNames_[i] == "dir") colDir = i;
        if (terrainSlk.colNames_[i] == "file") colFile = i;
    }
    for (int i = 0; i < (int)cliffSlk.colNames_.size(); ++i) {
        if (cliffSlk.colNames_[i] == "cliffID") colCliffID = i;
        if (cliffSlk.colNames_[i] == "texDir") colTexDir = i;
        if (cliffSlk.colNames_[i] == "texFile") colTexFile = i;
        if (cliffSlk.colNames_[i] == "groundTile") colGroundTile = i;
        if (cliffSlk.colNames_[i] == "cliffModelDir") colCliffModelDir = i;
        if (cliffSlk.colNames_[i] == "rampModelDir") colRampModelDir = i;
    }

    // Ground texture audit
    std::ofstream groundTexCsv((mapOutDir / "ground_textures_audit.csv").string());
    groundTexCsv << "index,war3id,slk_ok,dir,file,blp_path,blp_found,format,alpha_depth,size_bytes" << std::endl;

    reportln("\n  Ground Textures:");
    int groundFound = 0, groundMissing = 0, groundSlkMiss = 0;
    for (size_t i = 0; i < w3e.groundTilePalette.size(); i++) {
        uint32_t war3ID = w3e.groundTilePalette[i];
        std::string idStr = War3IDStr(war3ID);

        int row = (colTileID >= 0) ? terrainSlk.FindRow(idStr, colTileID) : -1;
        std::string dirStr, fileStr, blpPath;
        bool slkOk = (row >= 0);
        if (slkOk) {
            dirStr = terrainSlk.GetString(row, colDir);
            fileStr = terrainSlk.GetString(row, colFile);
            blpPath = dirStr + "\\" + fileStr + ".blp";
        }

        std::vector<uint8_t> blpData;
        bool blpFound = !blpPath.empty() && mpq.ReadFile(blpPath, blpData);
        std::string fmt;
        int alphaDepth = -1;
        if (blpFound) fmt = DetectBlpFormat(blpData.data(), blpData.size(), alphaDepth);

        if (!slkOk) groundSlkMiss++;
        else if (blpFound) groundFound++;
        else groundMissing++;

        groundTexCsv << i << "," << idStr << "," << slkOk << ","
                     << dirStr << "," << fileStr << "," << blpPath << ","
                     << blpFound << "," << fmt << "," << alphaDepth << ","
                     << (blpFound ? blpData.size() : 0)
                     << std::endl;

        reportln("    [" + std::to_string(i) + "] " + idStr
                 + " -> " + (slkOk ? dirStr + "\\" + fileStr : "SLK_MISS")
                 + "  " + (blpFound ? "FOUND " + fmt + " a" + std::to_string(alphaDepth) : "MISSING"));
    }
    reportln("    Ground: " + std::to_string(groundFound) + " found, "
             + std::to_string(groundMissing) + " missing, "
             + std::to_string(groundSlkMiss) + " SLK miss");

    // Cliff texture audit
    std::ofstream cliffTexCsv((mapOutDir / "cliff_textures_audit.csv").string());
    cliffTexCsv << "index,war3id,tex_dir,tex_file,blp_path,blp_found,format,alpha_depth,"
                << "cliff_model_dir,ramp_model_dir,ground_tile,ground_pal_idx"
                << std::endl;

    reportln("\n  Cliff Textures:");
    std::vector<int> cliffToGroundTexture;
    int cliffFound = 0, cliffMissing = 0, cliffSlkMiss = 0;
    for (size_t i = 0; i < w3e.cliffTilePalette.size(); i++) {
        uint32_t war3ID = w3e.cliffTilePalette[i];
        std::string idStr = War3IDStr(war3ID);
        int row = (colCliffID >= 0) ? cliffSlk.FindRow(idStr, colCliffID) : -1;

        if (row < 0) {
            reportln("    [" + std::to_string(i) + "] " + idStr + " -> SLK_MISS");
            cliffTexCsv << i << "," << idStr << ",,,,,,,,," << std::endl;
            cliffSlkMiss++;
            cliffToGroundTexture.push_back(0);
            continue;
        }

        std::string texDir = cliffSlk.GetString(row, colTexDir);
        std::string texFile = cliffSlk.GetString(row, colTexFile);
        std::string blpPath = texDir + "\\" + texFile + ".blp";
        std::string cliffModelDir = (colCliffModelDir >= 0) ? cliffSlk.GetString(row, colCliffModelDir) : texFile;
        std::string rampModelDir = (colRampModelDir >= 0) ? cliffSlk.GetString(row, colRampModelDir) : std::string();

        std::string groundTileStr;
        if (colGroundTile >= 0) groundTileStr = cliffSlk.GetString(row, colGroundTile);
        uint32_t groundWar3ID = StrToWar3ID(groundTileStr);
        int groundPalIdx = -1;
        for (size_t j = 0; j < w3e.groundTilePalette.size(); j++) {
            if (w3e.groundTilePalette[j] == groundWar3ID) {
                groundPalIdx = (int)j;
                break;
            }
        }
        cliffToGroundTexture.push_back(groundPalIdx >= 0 ? groundPalIdx : (int)i);

        std::vector<uint8_t> blpData;
        bool blpFound = mpq.ReadFile(blpPath, blpData);
        std::string fmt;
        int alphaDepth = -1;
        if (blpFound) fmt = DetectBlpFormat(blpData.data(), blpData.size(), alphaDepth);

        if (blpFound) cliffFound++;
        else cliffMissing++;

        cliffTexCsv << i << "," << idStr << "," << texDir << "," << texFile << "," << blpPath << ","
                    << blpFound << "," << fmt << "," << alphaDepth << ","
                    << cliffModelDir << "," << rampModelDir << ","
                    << groundTileStr << "," << groundPalIdx
                    << std::endl;

        reportln("    [" + std::to_string(i) + "] " + idStr
                 + " -> " + blpPath
                 + "  " + (blpFound ? "FOUND " + fmt + " a" + std::to_string(alphaDepth) : "MISSING")
                 + "  groundTile=" + groundTileStr + "[" + std::to_string(groundPalIdx) + "]"
                 + "  cliffModel=" + cliffModelDir + "  rampModel=" + rampModelDir);
    }
    reportln("    Cliff: " + std::to_string(cliffFound) + " found, "
             + std::to_string(cliffMissing) + " missing, "
             + std::to_string(cliffSlkMiss) + " SLK miss");
    reportln();

    // Water texture audit
    reportln("\n  Water Textures:");
    std::ofstream waterTexCsv((mapOutDir / "water_textures_audit.csv").string());
    waterTexCsv << "index,path,found,format,alpha_depth,size_bytes" << std::endl;
    for (int i = 0; i < 8; i++) {
        char path[128];
        snprintf(path, sizeof(path), "ReplaceableTextures\\Water\\Water%02d.blp", i);
        std::vector<uint8_t> data;
        bool ok = mpq.ReadFile(path, data);
        std::string fmt;
        int alphaDepth = -1;
        if (ok) fmt = DetectBlpFormat(data.data(), data.size(), alphaDepth);
        waterTexCsv << i << "," << path << "," << ok << "," << fmt << "," << alphaDepth << "," << (ok ? data.size() : 0) << std::endl;
        reportln("    " + std::string(path) + "  " + (ok ? "FOUND " + fmt : "MISSING"));
    }

    // Cliff model audit
    reportln("\n  Cliff Models:");
    std::ofstream modelCsv((mapOutDir / "cliff_models_audit.csv").string());
    modelCsv << "type,path,found" << std::endl;
    for (size_t i = 0; i < w3e.cliffTilePalette.size(); i++) {
        uint32_t war3ID = w3e.cliffTilePalette[i];
        std::string idStr = War3IDStr(war3ID);
        int row = (colCliffID >= 0) ? cliffSlk.FindRow(idStr, colCliffID) : -1;
        if (row < 0) continue;

        std::string cliffModelDir = (colCliffModelDir >= 0) ? cliffSlk.GetString(row, colCliffModelDir) : "";
        std::string rampModelDir = (colRampModelDir >= 0) ? cliffSlk.GetString(row, colRampModelDir) : "";

        const char* testCodes[] = {"AAAA", "ABAA", "BAAA", "BBAA", "AABA", "AAAB", "BABA", "ABBA"};
        for (auto code : testCodes) {
            if (!cliffModelDir.empty()) {
                std::string cwPath = "Doodads\\Terrain\\" + cliffModelDir + "\\" + cliffModelDir + code + "0.mdx";
                bool cwFound = mpq.FileExists(cwPath);
                modelCsv << "cliff_wall," << cwPath << "," << cwFound << std::endl;
            }
            if (!rampModelDir.empty()) {
                std::string rmpPath = "Doodads\\Terrain\\" + rampModelDir + "\\" + rampModelDir + code + "0.mdx";
                bool rmpFound = mpq.FileExists(rmpPath);
                modelCsv << "ramp," << rmpPath << "," << rmpFound << std::endl;
            }
        }
    }
    reportln("    Model audit saved to: " + (mapOutDir / "cliff_models_audit.csv").string());
    reportln();

    // ============================================================
    // Step 4: Texture variations comparison
    // ============================================================
    reportln("=== STEP 4: Texture Variations Comparison ===");
    int tileW = columns - 1;
    int tileH = rows - 1;

    short* cppTexList = new short[tileCount * 4]();
    short* javaTexList = new short[tileCount * 4]();

    // C++ getTextureVariations
    for (int y = 0; y < tileH; y++) {
        for (int x = 0; x < tileW; x++) {
            CppGetTextureVariations(w3e, cppCorners, x, y, cppTexList, (y * tileW + x) * 4,
                                   columns, rows, 0, (int)w3e.groundTilePalette.size(), false);
        }
    }

    // Java getTextureVariations
    for (int y = 0; y < tileH; y++) {
        for (int x = 0; x < tileW; x++) {
            JavaGetTextureVariations(javaCorners, columns, rows, cliffToGroundTexture,
                                    (int)w3e.groundTilePalette.size(), x, y,
                                    javaTexList, (y * tileW + x) * 4);
        }
    }

    // Compare texture variations
    int texVarDiffCount = 0;
    int texVarHighBitDiff = 0;
    std::ofstream texVarCsv((mapOutDir / "texture_variations_comparison.csv").string());
    texVarCsv << "tile_x,tile_y,idx,"
              << "cpp_v0,cpp_v1,cpp_v2,cpp_v3,"
              << "java_v0,java_v1,java_v2,java_v3,"
              << "match" << std::endl;

    for (int i = 0; i < tileCount * 4; i += 4) {
        int ti = i / 4;
        int tx = ti % tileW;
        int ty = ti / tileW;

        // Compare low 15 bits (texture/variation), high bit is different
        bool match = true;
        for (int j = 0; j < 4; j++) {
            if ((cppTexList[i+j] & 0x7FFF) != (javaTexList[i+j] & 0x7FFF))
                match = false;
            if ((cppTexList[i+j] >> 15) != (javaTexList[i+j] >> 15))
                texVarHighBitDiff++;
        }
        if (!match) texVarDiffCount++;

        texVarCsv << tx << "," << ty << "," << ti << ","
                  << cppTexList[i] << "," << cppTexList[i+1] << "," << cppTexList[i+2] << "," << cppTexList[i+3] << ","
                  << javaTexList[i] << "," << javaTexList[i+1] << "," << javaTexList[i+2] << "," << javaTexList[i+3] << ","
                  << match
                  << std::endl;
    }

    reportln("  Texture variation mismatches (low 15 bits): " + std::to_string(texVarDiffCount) + " / " + std::to_string(tileCount));
    reportln("  High bit differences (cliff mask): " + std::to_string(texVarHighBitDiff));
    reportln("  Detailed comparison saved to: " + (mapOutDir / "texture_variations_comparison.csv").string());
    reportln();

    // ============================================================
    // Step 5: Cliff mesh data comparison
    // ============================================================
    reportln("=== STEP 5: Cliff Mesh Data ===");
    std::map<int, CliffMeshData> cppCliffMeshes;
    CppBuildCliffMeshData(w3e, cppCliffMeshes);

    std::ofstream cliffMeshCsv((mapOutDir / "cliff_mesh_data.csv").string());
    cliffMeshCsv << "implementation,texture_layer,vertex_index,px,py,pz,tu,tv,nx,ny,nz" << std::endl;

    int totalCliffVerts = 0;
    for (auto& [tex, mesh] : cppCliffMeshes) {
        for (size_t vi = 0; vi < mesh.verts.size(); vi++) {
            auto& v = mesh.verts[vi];
            cliffMeshCsv << "cpp," << tex << "," << vi << ","
                        << v.px << "," << v.py << "," << v.pz << ","
                        << v.tu << "," << v.tv << ","
                        << v.nx << "," << v.ny << "," << v.nz
                        << std::endl;
            totalCliffVerts++;
        }
    }
    reportln("  C++ Cliff meshes: " + std::to_string(cppCliffMeshes.size()) + " texture layers, "
             + std::to_string(totalCliffVerts) + " vertices");
    reportln("  Cliff mesh data saved to: " + (mapOutDir / "cliff_mesh_data.csv").string());

    // ============================================================
    // Step 6: Ground height data (C++ with computed ramp vs Java fixed 0.5f)
    // ============================================================
    reportln("\n=== STEP 6: Height Data Comparison ===");

    float* cppGroundHeights = new float[cornerCount];
    float* cppGroundCornerHeights = new float[cornerCount];
    float* cppWaterHeights = new float[cornerCount];
    unsigned char* cppWaterExists = new unsigned char[cornerCount];

    for (int i = 0; i < cornerCount; i++) {
        cppGroundHeights[i] = w3e.tiles[i].FinalGroundHeight();
        cppGroundCornerHeights[i] = w3e.tiles[i].FinalGroundHeight() + cppCorners[i].rampAdjust;
        cppWaterHeights[i] = w3e.tiles[i].DecodeWaterHeight();
        cppWaterExists[i] = w3e.tiles[i].IsWater() ? 255 : 0;
    }

    float* javaGroundHeights = new float[cornerCount];
    float* javaGroundCornerHeights = new float[cornerCount];
    JavaUpdateGroundHeights(w3e, javaCorners, javaGroundHeights, javaGroundCornerHeights);

    float maxHeightDiff = 0, totalHeightDiff = 0, maxRampOnlyDiff = 0;
    int heightDiffCount = 0;
    std::ofstream heightCsv((mapOutDir / "height_comparison.csv").string());
    heightCsv << "idx,x,y,"
              << "cpp_groundH,cpp_groundCornerH,cpp_waterH,cpp_waterExists,"
              << "java_groundH,java_groundCornerH,"
              << "height_diff,cornerHeight_diff"
              << std::endl;

    for (int i = 0; i < cornerCount; i++) {
        int x = i % columns;
        int y = i / columns;
        float hDiff = std::abs(cppGroundHeights[i] - javaGroundHeights[i]);
        float chDiff = std::abs(cppGroundCornerHeights[i] - javaGroundCornerHeights[i]);
        if (chDiff > maxHeightDiff) { maxHeightDiff = chDiff; }
        totalHeightDiff += chDiff;
        if (chDiff > 0.01f) heightDiffCount++;

        // Ramp-only difference: cppGroundCornerHeights - cppGroundHeights vs javaGroundCornerHeights - javaGroundHeights
        float cppRampDelta = cppGroundCornerHeights[i] - cppGroundHeights[i];
        float javaRampDelta = javaGroundCornerHeights[i] - javaGroundHeights[i];
        float rampDiff = std::abs(cppRampDelta - javaRampDelta);
        if (rampDiff > maxRampOnlyDiff) maxRampOnlyDiff = rampDiff;

        heightCsv << i << "," << x << "," << y << ","
                  << cppGroundHeights[i] << "," << cppGroundCornerHeights[i] << ","
                  << cppWaterHeights[i] << "," << (int)cppWaterExists[i] << ","
                  << javaGroundHeights[i] << "," << javaGroundCornerHeights[i] << ","
                  << hDiff << "," << chDiff
                  << std::endl;
    }

    reportln("  C++ groundHeight range: [" + std::to_string(*std::min_element(cppGroundHeights, cppGroundHeights+cornerCount))
             + ", " + std::to_string(*std::max_element(cppGroundHeights, cppGroundHeights+cornerCount)) + "]");
    reportln("  C++ groundCornerHeight range: [" + std::to_string(*std::min_element(cppGroundCornerHeights, cppGroundCornerHeights+cornerCount))
             + ", " + std::to_string(*std::max_element(cppGroundCornerHeights, cppGroundCornerHeights+cornerCount)) + "]");
    reportln("  Height data comparison:");
    reportln("    Corner height differences > 0.01: " + std::to_string(heightDiffCount) + " / " + std::to_string(cornerCount));
    reportln("    Max corner height diff: " + std::to_string(maxHeightDiff));
    reportln("    Max ramp-only diff: " + std::to_string(maxRampOnlyDiff));
    reportln("  Height comparison saved to: " + (mapOutDir / "height_comparison.csv").string());

    // ============================================================
    // Step 7: Dump decoded ground texture images
    // ============================================================
    reportln("=== STEP 7: Dump Ground Textures ===");
    fs::path texDumpDir = mapOutDir / "ground_textures";
    fs::create_directory(texDumpDir);

    // Write a README with layer info
    {
        std::ofstream readme((texDumpDir / "README.txt").string());
        readme << "Ground texture sub-image layout:\n";
        readme << "  Normal (not extended):\n";
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                readme << "    layer " << (y*4+x) << " = row " << y << " col " << x << "\n";
        readme << "  Extended (width > height):\n";
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                readme << "    layer " << (y*4+x) << " = row " << y << " col " << x << "\n";
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                readme << "    layer " << (y*4+x+16) << " = row " << y << " col " << (x+4) << " (extended)\n";
    }

    int texDumpCount = 0;
    for (size_t i = 0; i < w3e.groundTilePalette.size(); i++) {
        uint32_t war3ID = w3e.groundTilePalette[i];
        std::string idStr = War3IDStr(war3ID);

        int row = (colTileID >= 0) ? terrainSlk.FindRow(idStr, colTileID) : -1;
        if (row < 0) {
            reportln("  [" + std::to_string(i) + "] " + idStr + " -> SLK_MISS");
            continue;
        }
        std::string dirStr = terrainSlk.GetString(row, colDir);
        std::string fileStr = terrainSlk.GetString(row, colFile);
        std::string blpPath = dirStr + "\\" + fileStr + ".blp";

        std::vector<uint8_t> blpData;
        if (!mpq.ReadFile(blpPath, blpData)) {
            reportln("  [" + std::to_string(i) + "] " + idStr + " -> BLP not found: " + blpPath);
            continue;
        }

        BLPImage img;
        if (!DecodeBLP(blpData.data(), blpData.size(), img)) {
            reportln("  [" + std::to_string(i) + "] " + idStr + " -> Decode failed");
            continue;
        }

        bool extended = (img.width > img.height);
        int tileSize = img.height / 4;
        int colCount = extended ? 8 : 4;

        // Simple RGBA-to-PPM writer lambda
        auto writePpm = [](const char* path, int w, int h, const uint8_t* rgba) {
            FILE* f = fopen(path, "wb");
            if (!f) return false;
            fprintf(f, "P6\n%d %d\n255\n", w, h);
            for (int i = 0; i < w * h; i++) {
                fputc(rgba[i*4+0], f);
                fputc(rgba[i*4+1], f);
                fputc(rgba[i*4+2], f);
            }
            fclose(f);
            return true;
        };

        // Save full image
        char fullName[256];
        snprintf(fullName, sizeof(fullName), "%02d_%s_full.ppm", (int)i, idStr.c_str());
        std::string fullPath = (texDumpDir / fullName).string();
        writePpm(fullPath.c_str(), img.width, img.height, img.pixels.data());

        // Save each sub-image as individual PPM
        for (int rowY = 0; rowY < 4; rowY++) {
            for (int colX = 0; colX < colCount; colX++) {
                int layer;
                if (extended && colX >= 4)
                    layer = rowY * 4 + (colX - 4) + 16;  // cols 4..7 -> layers 16..31
                else
                    layer = rowY * 4 + colX;

                std::vector<uint8_t> sub(tileSize * tileSize * 4);
                for (int sy = 0; sy < tileSize; sy++) {
                    for (int sx = 0; sx < tileSize; sx++) {
                        int srcIdx = ((rowY * tileSize + sy) * img.width + (colX * tileSize + sx)) * 4;
                        int dstIdx = (sy * tileSize + sx) * 4;
                        sub[dstIdx+0] = img.pixels[srcIdx+0];
                        sub[dstIdx+1] = img.pixels[srcIdx+1];
                        sub[dstIdx+2] = img.pixels[srcIdx+2];
                        sub[dstIdx+3] = img.pixels[srcIdx+3];
                    }
                }

                char subName[256];
                snprintf(subName, sizeof(subName), "%02d_%s_layer%02d.ppm", (int)i, idStr.c_str(), layer);
                std::string subPath = (texDumpDir / subName).string();
                writePpm(subPath.c_str(), tileSize, tileSize, sub.data());
            }
        }

        reportln("  [" + std::to_string(i) + "] " + idStr + " -> " + blpPath
                 + " (" + std::to_string(img.width) + "x" + std::to_string(img.height)
                 + (extended ? " extended" : "")
                 + ", " + std::to_string(colCount * 4) + " sub-tiles)");
        texDumpCount++;
    }
    reportln("  Dumped " + std::to_string(texDumpCount) + " ground textures to: " + texDumpDir.string());
    reportln();

    // ============================================================
    // Step 8: Summary
    // ============================================================
    reportln("\n=== STEP 8: SUMMARY ===");
    reportln();
    std::string summary = "  Analysis complete. All data saved to: " + mapOutDir.string();
    reportln(summary);

    // Close report
    report.close();
    cornerCsv.close();
    groundTexCsv.close();
    cliffTexCsv.close();
    waterTexCsv.close();
    modelCsv.close();
    texVarCsv.close();
    cliffMeshCsv.close();
    heightCsv.close();

    delete[] cppTexList;
    delete[] javaTexList;
    delete[] cppGroundHeights;
    delete[] cppGroundCornerHeights;
    delete[] cppWaterHeights;
    delete[] cppWaterExists;
    delete[] javaGroundHeights;
    delete[] javaGroundCornerHeights;

    std::cout << "\nDone." << std::endl;
    return 0;
}

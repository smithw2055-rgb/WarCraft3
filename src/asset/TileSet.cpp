#include "TileSet.h"
#include "BLP.h"
#include "DDS.h"
#include "stb/stb_image.h"
#include "resource/slk.h"
#include "render/rhi_device.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static std::string TrimCopy(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
        value.pop_back();
    return value;
}

static std::vector<std::string> SplitCsvPaths(const std::string& text) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= text.size()) {
        size_t comma = text.find(',', start);
        std::string part = (comma == std::string::npos) ? text.substr(start) : text.substr(start, comma - start);
        parts.push_back(TrimCopy(part));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return parts;
}

struct DecodedTerrainImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
    std::string sourceFormat = "unknown";
    bool needsSRGBFix = false;
};

static std::string DescribeBlpCompression(const std::vector<uint8_t>& data) {
    if (data.size() < 20 || std::memcmp(data.data(), "BLP", 3) != 0) return "unknown";
    const uint32_t compression = *(const uint32_t*)(data.data() + 4);
    const uint32_t alphaBits = *(const uint32_t*)(data.data() + 8);
    switch (compression) {
    case 0: return "BLP-JPEG-a" + std::to_string(alphaBits);
    case 1: return "BLP-PALETTED-a" + std::to_string(alphaBits);
    case 2: return "BLP-DXT-a" + std::to_string(alphaBits);
    case 4: return "BLP-DXT4-a" + std::to_string(alphaBits);
    default: return "BLP-" + std::to_string(compression) + "-a" + std::to_string(alphaBits);
    }
}

TileSet::~TileSet() {
    if (!Renderer::IsActive()) return;
    for (auto& t : tiles_)
        if (t.glTexture.id) sg_destroy_image(t.glTexture);
    for (auto& c : cliffTiles_)
        if (c.glTexture.id) sg_destroy_image(c.glTexture);
    if (cliffTextureArray_.id) sg_destroy_image(cliffTextureArray_);
    if (cliffTextureArrayView_.id) sg_destroy_view(cliffTextureArrayView_);
    if (waterTextureArray_.id) sg_destroy_image(waterTextureArray_);
}

static void DownsampleMip(const uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh, int slices) {
    for (int s = 0; s < slices; ++s) {
        const uint8_t* srcSlice = src + s * sw * sh * 4;
        uint8_t* dstSlice = dst + s * dw * dh * 4;
        for (int y = 0; y < dh; ++y) {
            for (int x = 0; x < dw; ++x) {
                int sx0 = x * 2, sy0 = y * 2;
                int sx1 = std::min(sx0 + 1, sw - 1), sy1 = std::min(sy0 + 1, sh - 1);
                const uint8_t* p00 = srcSlice + (sy0 * sw + sx0) * 4;
                const uint8_t* p01 = srcSlice + (sy0 * sw + sx1) * 4;
                const uint8_t* p10 = srcSlice + (sy1 * sw + sx0) * 4;
                const uint8_t* p11 = srcSlice + (sy1 * sw + sx1) * 4;
                uint8_t* d = dstSlice + (y * dw + x) * 4;
                for (int c = 0; c < 4; ++c)
                    d[c] = (uint8_t)(((int)p00[c] + p01[c] + p10[c] + p11[c]) / 4);
            }
        }
    }
}

static sg_image CreateTextureArray(int tileW, int tileH, int numTiles, const std::vector<uint8_t>& allTileData) {
    if (allTileData.empty()) return {0};

    // Compute mip levels
    int numMips = 1;
    int mw = tileW, mh = tileH;
    while (mw > 1 || mh > 1) { mw = std::max(mw / 2, 1); mh = std::max(mh / 2, 1); numMips++; }

    // Build all mip levels
    std::vector<std::vector<uint8_t>> mipData(numMips);
    mipData[0] = allTileData;
    mw = tileW; mh = tileH;
    for (int m = 1; m < numMips; ++m) {
        int pw = std::max(mw / 2, 1), ph = std::max(mh / 2, 1);
        mipData[m].resize((size_t)pw * ph * 4 * numTiles);
        DownsampleMip(mipData[m-1].data(), mw, mh, mipData[m].data(), pw, ph, numTiles);
        mw = pw; mh = ph;
    }

    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_ARRAY;
    desc.width = tileW;
    desc.height = tileH;
    desc.num_slices = numTiles;
    desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    desc.num_mipmaps = numMips;
    for (int m = 0; m < numMips; ++m) {
        desc.data.mip_levels[m].ptr = mipData[m].data();
        desc.data.mip_levels[m].size = mipData[m].size();
    }
    sg_image img = sg_make_image(&desc);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) img = {0};
    return img;
}

static void CreateGroundTextureArray(GroundTextureTile& gt, const DecodedTerrainImage& img) {
    gt.tileSize = img.height / 4;
    gt.extended = (img.width > img.height);
    gt.tileCount = gt.extended ? 32 : 16;
    gt.sourceFormat = img.sourceFormat;
    gt.needsSRGBFix = img.needsSRGBFix;
    gt.sourceWidth = img.width;
    gt.sourceHeight = img.height;

    int tileSize = gt.tileSize;
    int numTiles = gt.tileCount;
    if (tileSize <= 0) return;

    // Pack tiles into a single buffer for the array texture
    std::vector<uint8_t> tileData(tileSize * tileSize * 4 * numTiles);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            int i = row * 4 + col;
            // Left half (layers 0-15)
            int sx = col * tileSize, sy = row * tileSize;
            for (int y = 0; y < tileSize; ++y) {
                int srcOff = ((sy + y) * img.width + sx) * 4;
                int dstOff = (i * tileSize * tileSize + y * tileSize) * 4;
                std::memcpy(tileData.data() + dstOff, img.pixels.data() + srcOff, tileSize * 4);
            }
            // Right half for extended textures (layers 16-31)
            if (gt.extended) {
                int i2 = i + 16;
                int sx2 = (col + 4) * tileSize, sy2 = row * tileSize;
                for (int y = 0; y < tileSize; ++y) {
                    int srcOff = ((sy2 + y) * img.width + sx2) * 4;
                    int dstOff = (i2 * tileSize * tileSize + y * tileSize) * 4;
                    std::memcpy(tileData.data() + dstOff, img.pixels.data() + srcOff, tileSize * 4);
                }
            }
        }
    }

    gt.glTexture = CreateTextureArray(tileSize, tileSize, numTiles, tileData);
    // Save first layer pixel data for combined texture
    gt.firstLayerData.resize(tileSize * tileSize * 4);
    memcpy(gt.firstLayerData.data(), tileData.data(), tileSize * tileSize * 4);
    std::cout << "  tile: " << gt.sourceFormat << " " << img.width << "x" << img.height
              << " → " << tileSize << "x" << tileSize << " " << numTiles << " layers\n";
}

static bool LoadImageFromMpq(const MpqManager& mpq, const std::string& path, DecodedTerrainImage& out) {
    std::vector<uint8_t> fileData;
    if (!mpq.ReadFile(path, fileData)) return false;

    BLPImage blp;
    if (DecodeBLP(fileData.data(), fileData.size(), blp)) {
        out.width = blp.width;
        out.height = blp.height;
        out.pixels = std::move(blp.pixels);
        out.sourceFormat = DescribeBlpCompression(fileData);
        out.needsSRGBFix = true;
        return true;
    }

    DdsImage dds;
    if (DecodeDDS(fileData.data(), fileData.size(), dds)) {
        out.width = dds.width;
        out.height = dds.height;
        out.pixels = std::move(dds.pixels);
        out.sourceFormat = "DDS";
        out.needsSRGBFix = true;
        return true;
    }

    int w, h, ch;
    stbi_uc* pixels = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels) return false;
    out.width = w;
    out.height = h;
    out.pixels.resize(w * h * 4);
    std::memcpy(out.pixels.data(), pixels, w * h * 4);
    stbi_image_free(pixels);
    out.sourceFormat = "STBI";
    out.needsSRGBFix = false;
    return true;
}

static bool LoadCliffTextureArray(CliffTextureInfo& cliff, const MpqManager& mpq) {
    for (int i = 0; i < 16; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "\\%s%d.blp", cliff.texFile.c_str(), i);
        std::string path = cliff.texDir + buf;
        DecodedTerrainImage img;
        if (!LoadImageFromMpq(mpq, path, img)) continue;

        int tileSize = img.height;
        std::vector<uint8_t> tileData(tileSize * tileSize * 4);
        std::memcpy(tileData.data(), img.pixels.data(), tileSize * tileSize * 4);

        cliff.glTexture = CreateTextureArray(tileSize, tileSize, 1, tileData);
        cliff.sourceFormat = img.sourceFormat;
        cliff.needsSRGBFix = img.needsSRGBFix;
        return true;
    }
    return false;
}

bool TileSet::LoadFromMpq(const MpqManager& mpq, const W3EMap& w3e) {
    tiles_.clear();
    paletteToTex_.clear();
    groundPaletteMappedCount_ = 0;
    groundPaletteFallbackCount_ = 0;
    fallbackMode_ = "none";

    if (w3e.groundTilePalette.empty()) return false;

    // Load Terrain.slk for War3ID → dir/file mapping
    std::vector<uint8_t> slkData;
    SlkTable slk;
    int colTileID = -1, colDir = -1, colFile = -1;
    if (mpq.ReadFile("TerrainArt\\Terrain.slk", slkData) && slk.Load(slkData.data(), slkData.size())) {
        for (int i = 0; i < (int)slk.colNames_.size(); ++i) {
            if (slk.colNames_[i] == "tileID") colTileID = i;
            if (slk.colNames_[i] == "dir") colDir = i;
            if (slk.colNames_[i] == "file") colFile = i;
        }
    }
    bool haveSlk = (colTileID >= 0 && colDir >= 0 && colFile >= 0);
    if (haveSlk) std::cout << "TileSet: Terrain.slk loaded, " << slk.Rows() << " rows\n";

    // Load terrain tiles
    for (size_t palIdx = 0; palIdx < w3e.groundTilePalette.size(); ++palIdx) {
        uint32_t war3ID = w3e.groundTilePalette[palIdx];
        char idStr[5] = {(char)(war3ID & 0xFF), (char)((war3ID >> 8) & 0xFF),
                         (char)((war3ID >> 16) & 0xFF), (char)((war3ID >> 24) & 0xFF), 0};

        bool found = false;

        if (haveSlk) {
            // Look up in Terrain.slk
            int row = slk.FindRow(idStr, colTileID);
            if (row >= 0) {
                std::string dir = slk.GetString(row, colDir);
                std::string file = slk.GetString(row, colFile);
                if (!dir.empty() && !file.empty()) {
                    char buf[128];
                    std::cout << "TileSet: SLK row " << row << " dir=\"" << dir << "\" file=\"" << file << "\"" << std::endl;
                    // dir already includes "TerrainArt\" prefix
                    std::snprintf(buf, sizeof(buf), "%s\\%s.blp", dir.c_str(), file.c_str());
                    std::cout << "TileSet: loading " << buf << " for War3ID=" << idStr << std::endl;
                    DecodedTerrainImage img;
                    if (LoadImageFromMpq(mpq, buf, img)) {
                        GroundTextureTile gt;
                        gt.id = (int)tiles_.size();
                        CreateGroundTextureArray(gt, img);
                        if (gt.glTexture.id) {
                            tiles_.push_back(gt);
                            paletteToTex_.push_back(gt.id);
                            groundPaletteMappedCount_++;
                            found = true;
                        }
                    }
                }
            }
        }

        // Fallback: try direct paths
        if (!found) {
            const char* dirs[] = {"LordaeronSummer","Ashenvale","Barrens","Northrend","Dungeon","City"},
                       *files[] = {"Lords_DirtRough","Ashen_GrassLumpy","Barrens_Dirt","North_dirtdark","Cave_Dirt","City_Grass"};
            for (int i = 0; i < 6 && !found; ++i) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "TerrainArt\\%s\\%s.blp", dirs[i], files[i]);
                DecodedTerrainImage img;
                if (LoadImageFromMpq(mpq, buf, img)) {
                    GroundTextureTile gt;
                    gt.id = (int)tiles_.size();
                    CreateGroundTextureArray(gt, img);
                    if (gt.glTexture.id) {
                        tiles_.push_back(gt);
                        paletteToTex_.push_back(gt.id);
                        groundPaletteMappedCount_++;
                        found = true;
                    }
                }
            }
        }

        if (!found) {
            groundPaletteFallbackCount_++;
        }
    }

    std::cout << "TileSet: loaded " << tiles_.size() << " ground textures from MPQ"
              << " (palette mapped: " << groundPaletteMappedCount_
              << ", fallback: " << groundPaletteFallbackCount_ << ")" << std::endl;
    return !tiles_.empty();
}

bool TileSet::LoadFromWorldEditData(const MpqManager& mpq, char tileset, int paletteSize) {
    (void)mpq;
    (void)tileset;
    (void)paletteSize;
    tiles_.clear();
    paletteToTex_.clear();
    fallbackMode_ = "worldedit";
    // Simplified: create a single placeholder texture
    GroundTextureTile gt;
    gt.id = 0;
    gt.tileSize = 64;
    gt.tileCount = 1;
    std::vector<uint8_t> data(64 * 64 * 4, 128);
    gt.glTexture = CreateTextureArray(64, 64, 1, data);
    tiles_.push_back(gt);
    paletteToTex_.push_back(0);
    std::cout << "TileSet: worldedit fallback, 1 tile" << std::endl;
    return true;
}

bool TileSet::LoadCliffTextures(const MpqManager& mpq, const W3EMap& w3e) {
    for (auto& c : cliffTiles_) {
        if (c.glTexture.id && sg_query_image_state(c.glTexture) == SG_RESOURCESTATE_VALID)
            sg_destroy_image(c.glTexture);
    }
    cliffTiles_.clear();
    paletteToCliffTex_.clear();
    cliffToGroundPal_.clear();
    cliffSrgbTextureCount_ = 0;
    cliffLinearTextureCount_ = 0;

    if (w3e.cliffTilePalette.empty()) return false;

    // Parse CliffTypes.slk
    std::vector<uint8_t> slkData;
    SlkTable slk;
    int colTileID = -1, colTexDir = -1, colTexFile = -1, colCliffModelDir = -1, colRampModelDir = -1;
    int colGroundTile = -1;
    if (mpq.ReadFile("TerrainArt\\CliffTypes.slk", slkData) && slk.Load(slkData.data(), slkData.size())) {
        std::cout << "CliffTypes.slk: " << slk.Rows() << " rows, " << slk.Cols() << " cols" << std::endl;
        for (int i = 0; i < (int)slk.colNames_.size(); ++i) {
            const auto& n = slk.colNames_[i];
            if (n == "cliffID") colTileID = i;
            else if (n == "texDir") colTexDir = i;
            else if (n == "texFile") colTexFile = i;
            else if (n == "cliffModelDir") colCliffModelDir = i;
            else if (n == "rampModelDir") colRampModelDir = i;
            else if (n == "groundTile") colGroundTile = i;
        }
    }

    bool haveSlk = (colTileID >= 0 && colTexDir >= 0 && colTexFile >= 0);

    // Build palette mappings
    int maxPalette = (int)w3e.cliffTilePalette.size();
    paletteToCliffTex_.resize(maxPalette, -1);
    cliffToGroundPal_.resize(maxPalette, -1);

    // Collect all cliff textures for unified array
    struct CliffTexData {
        std::vector<uint8_t> pixels;
        int tileSize = 0;
    };
    std::vector<CliffTexData> allCliffTex(maxPalette);
    int maxSize = 0;

    for (int palIdx = 0; palIdx < maxPalette; palIdx++) {
        uint32_t war3ID = w3e.cliffTilePalette[palIdx];
        char idStr[5] = {(char)(war3ID & 0xFF), (char)((war3ID >> 8) & 0xFF),
                         (char)((war3ID >> 16) & 0xFF), (char)((war3ID >> 24) & 0xFF), 0};

        std::string texDir, texFile, cliffModelDir, rampModelDir;
        int groundTilePal = -1;

        if (haveSlk) {
            int row = slk.FindRow(idStr, colTileID);
            if (row >= 0) {
                texDir = slk.GetString(row, colTexDir);
                texFile = slk.GetString(row, colTexFile);
                if (colCliffModelDir >= 0) cliffModelDir = slk.GetString(row, colCliffModelDir);
                if (colRampModelDir >= 0) rampModelDir = slk.GetString(row, colRampModelDir);
                if (colGroundTile >= 0) {
                    std::string gt = slk.GetString(row, colGroundTile);
                    if (!gt.empty()) {
                        char gtId[5] = {gt[0], gt[1], gt[2], gt[3], 0};
                        for (int i = 0; i < (int)w3e.groundTilePalette.size(); i++) {
                            if (w3e.groundTilePalette[i] == *(uint32_t*)gtId) {
                                groundTilePal = i; break;
                            }
                        }
                    }
                }
            }
        }

        if (texDir.empty() || texFile.empty()) {
            std::cerr << "Cliff palette " << palIdx << " (" << idStr << ") not found in CliffTypes.slk" << std::endl;
            continue;
        }

        // Load cliff texture BLP
        char buf[384];
        std::snprintf(buf, sizeof(buf), "%s\\%s.blp", texDir.c_str(), texFile.c_str());
        DecodedTerrainImage img;
        if (LoadImageFromMpq(mpq, buf, img)) {
            int tileSize = img.height;
            allCliffTex[palIdx].pixels.resize(tileSize * tileSize * 4);
            std::memcpy(allCliffTex[palIdx].pixels.data(), img.pixels.data(), tileSize * tileSize * 4);
            allCliffTex[palIdx].tileSize = tileSize;
            maxSize = std::max(maxSize, tileSize);

            CliffTextureInfo ci;
            ci.texDir = texDir;
            ci.texFile = texFile;
            ci.cliffModelDir = cliffModelDir;
            ci.rampModelDir = rampModelDir;
            ci.sourceFormat = img.sourceFormat;
            ci.needsSRGBFix = img.needsSRGBFix;
            int texIdx = (int)cliffTiles_.size();
            cliffTiles_.push_back(ci);
            paletteToCliffTex_[palIdx] = texIdx;
            cliffToGroundPal_[palIdx] = groundTilePal;
            cliffLinearTextureCount_++;
            std::cout << "Cliff palette " << palIdx << " (" << idStr << "): loaded "
                      << tileSize << "x" << tileSize << " texFile=" << texFile << std::endl;
        } else {
            std::cerr << "Cliff BLP not found: " << buf << std::endl;
        }
    }

    // Create unified cliff texture array (one layer per palette entry)
    if (maxSize > 0 && !allCliffTex.empty()) {
        // Pad all textures to the same size
        int arraySize = maxSize;
        std::vector<uint8_t> arrayData(arraySize * arraySize * 4 * maxPalette, 0);
        for (int i = 0; i < maxPalette; i++) {
            if (allCliffTex[i].pixels.empty() || allCliffTex[i].tileSize == 0) continue;
            int ts = allCliffTex[i].tileSize;
            // Copy row by row, handling size differences
            for (int y = 0; y < ts && y < arraySize; y++) {
                int srcOff = y * ts * 4;
                int dstOff = (i * arraySize * arraySize + y * arraySize) * 4;
                int copyLen = std::min(ts, arraySize) * 4;
                std::memcpy(arrayData.data() + dstOff, allCliffTex[i].pixels.data() + srcOff, copyLen);
            }
        }

        cliffTextureArray_ = CreateTextureArray(arraySize, arraySize, maxPalette, arrayData);
        if (sg_query_image_state(cliffTextureArray_) == SG_RESOURCESTATE_VALID) {
            sg_view_desc vd = {};
            vd.texture.image = cliffTextureArray_;
            cliffTextureArrayView_ = sg_make_view(&vd);
            cliffTextureArraySize_ = arraySize;
            std::cout << "CliffTextureArray: " << arraySize << "x" << arraySize
                      << " " << maxPalette << " layers, glTex=" << cliffTextureArray_.id << std::endl;
        } else {
            cliffTextureArray_ = {0};
            std::cerr << "CliffTextureArray: failed to create" << std::endl;
        }
    }

    return !cliffTiles_.empty();
}

bool TileSet::LoadCliffModelInfo(const MpqManager& mpq) {
    // Model directories already populated by LoadCliffTextures
    // This function exists to support lazy loading of cliff models
    (void)mpq;
    return !cliffTiles_.empty();
}

bool TileSet::LoadWaterInfo(const MpqManager& mpq, const W3EMap& w3e) {
    std::vector<uint8_t> slkData;
    if (!mpq.ReadFile("TerrainArt\\Water.slk", slkData)) return false;
    SlkTable slk;
    if (!slk.Load(slkData.data(), slkData.size())) return false;

    int colId = -1, colHeight = -1, colNumTex = -1, colTexRate = -1, colTexFile = -1;
    int colSminR = -1, colSminG = -1, colSminB = -1, colSminA = -1;
    int colSmaxR = -1, colSmaxG = -1, colSmaxB = -1, colSmaxA = -1;
    int colDminR = -1, colDminG = -1, colDminB = -1, colDminA = -1;
    int colDmaxR = -1, colDmaxG = -1, colDmaxB = -1, colDmaxA = -1;
    int colShoreDir = -1, colShoreSFile = -1, colShoreSVar = -1;
    int colShoreOCFile = -1, colShoreOCVar = -1;
    int colShoreICFile = -1, colShoreICVar = -1;
    for (int i = 0; i < (int)slk.colNames_.size(); ++i) {
        const std::string& n = slk.colNames_[i];
        if (n == "waterID" || n == "tilesetID") colId = i;
        else if (n == "height") colHeight = i;
        else if (n == "numTex") colNumTex = i;
        else if (n == "texRate") colTexRate = i;
        else if (n == "texFile") colTexFile = i;
        // Note: SLK uses "Smin_A" "Smin_R" "Smin_G" "Smin_B" (A,R,G,B order)
        else if (n == "Smin_R") colSminR = i; else if (n == "Smin_G") colSminG = i;
        else if (n == "Smin_B") colSminB = i; else if (n == "Smin_A") colSminA = i;
        else if (n == "Smax_R") colSmaxR = i; else if (n == "Smax_G") colSmaxG = i;
        else if (n == "Smax_B") colSmaxB = i; else if (n == "Smax_A") colSmaxA = i;
        else if (n == "Dmin_R") colDminR = i; else if (n == "Dmin_G") colDminG = i;
        else if (n == "Dmin_B") colDminB = i; else if (n == "Dmin_A") colDminA = i;
        else if (n == "Dmax_R") colDmaxR = i; else if (n == "Dmax_G") colDmaxG = i;
        else if (n == "Dmax_B") colDmaxB = i; else if (n == "Dmax_A") colDmaxA = i;
        else if (n == "shoreDir") colShoreDir = i;
        else if (n == "shoreSFile") colShoreSFile = i; else if (n == "shoreSVar") colShoreSVar = i;
        else if (n == "shoreOCFile") colShoreOCFile = i; else if (n == "shoreOCVar") colShoreOCVar = i;
        else if (n == "shoreICFile") colShoreICFile = i; else if (n == "shoreICVar") colShoreICVar = i;
    }
    if (colId < 0) return false;

    // Debug: print Water.slk column names
    std::cout << "Water.slk columns:";
    for (auto& c : slk.colNames_) std::cout << " \"" << c << "\"";
    std::cout << std::endl;

    std::string waterKey = std::string(1, w3e.tileset) + "Sha";
    int row = slk.FindRow(waterKey, colId);
    if (row < 0) { std::cout << "Water.slk: no row for " << waterKey << std::endl; return false; }
    std::cout << "Water.slk row " << row << " for " << waterKey << std::endl;

    auto getS = [&](int col) { return col >= 0 ? slk.GetString(row, col) : ""; };
    auto getF = [&](int col) -> float {
        std::string s = getS(col);
        return s.empty() ? 0.0f : (float)std::atof(s.c_str());
    };

    waterInfo_.heightOffset = getF(colHeight);
    waterInfo_.textureCount = (int)getF(colNumTex);
    waterInfo_.texRate = getF(colTexRate);
    waterInfo_.texFile = getS(colTexFile);
    waterInfo_.shoreDir = getS(colShoreDir);
    waterInfo_.shoreSFile = getS(colShoreSFile);
    waterInfo_.shoreOCFile = getS(colShoreOCFile);
    waterInfo_.shoreICFile = getS(colShoreICFile);
    waterInfo_.shoreSVar = (int)getF(colShoreSVar);
    waterInfo_.shoreOCVar = (int)getF(colShoreOCVar);
    waterInfo_.shoreICVar = (int)getF(colShoreICVar);

    auto loadColor = [&](float* out, int r, int g, int b, int a) {
        const float def[4] = {0,0.1f,0.3f,0.6f};
        std::string rs = r>=0 ? slk.GetString(row, r) : "";
        std::string gs = g>=0 ? slk.GetString(row, g) : "";
        std::string bs = b>=0 ? slk.GetString(row, b) : "";
        std::string as = a>=0 ? slk.GetString(row, a) : "";
        out[0] = rs.empty() ? def[0] : (float)std::atof(rs.c_str())/255.0f;
        out[1] = gs.empty() ? def[1] : (float)std::atof(gs.c_str())/255.0f;
        out[2] = bs.empty() ? def[2] : (float)std::atof(bs.c_str())/255.0f;
        out[3] = as.empty() ? def[3] : (float)std::atof(as.c_str())/255.0f;
    };
    loadColor(waterInfo_.minShallow, colSminR, colSminG, colSminB, colSminA);
    loadColor(waterInfo_.maxShallow, colSmaxR, colSmaxG, colSmaxB, colSmaxA);
    loadColor(waterInfo_.minDeep, colDminR, colDminG, colDminB, colDminA);
    loadColor(waterInfo_.maxDeep, colDmaxR, colDmaxG, colDmaxB, colDmaxA);

    waterLoaded_ = true;
    auto prColor = [](const char* name, float c[4]) {
        std::cout << "  " << name << "=" << c[0] << "," << c[1] << "," << c[2] << "," << c[3] << std::endl;
    };
    prColor("shallow_min", waterInfo_.minShallow);
    prColor("shallow_max", waterInfo_.maxShallow);
    prColor("deep_min", waterInfo_.minDeep);
    prColor("deep_max", waterInfo_.maxDeep);
    std::cout << "  shoreDir=" << waterInfo_.shoreDir << " sFile=" << waterInfo_.shoreSFile
              << " ocFile=" << waterInfo_.shoreOCFile << " icFile=" << waterInfo_.shoreICFile
              << " sVar=" << waterInfo_.shoreSVar << " ocVar=" << waterInfo_.shoreOCVar << " icVar=" << waterInfo_.shoreICVar << std::endl;
    std::cout << "WaterInfo: offset=" << waterInfo_.heightOffset
              << " texCount=" << waterInfo_.textureCount
              << " texRate=" << waterInfo_.texRate
              << " texFile=" << waterInfo_.texFile << std::endl;
    return true;
}

bool TileSet::LoadWaterTextures(const MpqManager& mpq) {
    if (!waterLoaded_ || waterInfo_.texFile.empty() || waterInfo_.textureCount <= 0) return false;

    // Read first frame to get texture size
    char firstPath[128];
    std::snprintf(firstPath, sizeof(firstPath), "%s00.blp", waterInfo_.texFile.c_str());
    std::vector<uint8_t> fileData;
    if (!mpq.ReadFile(firstPath, fileData)) return false;

    BLPImage blp;
    if (!DecodeBLP(fileData.data(), fileData.size(), blp)) {
        std::cout << "Water BLP decode failed" << std::endl;
        return false;
    }
    std::cout << "Water BLP decoded: " << blp.width << "x" << blp.height << " pixels=" << blp.pixels.size()
              << " format=" << DescribeBlpCompression(fileData) << std::endl;

    int tw = blp.width, th = blp.height;

    // Build combined buffer with all frames
    int numFrames = waterInfo_.textureCount;
    std::vector<uint8_t> allPixels(tw * th * 4 * numFrames);
    // Copy first frame
    std::memcpy(allPixels.data(), blp.pixels.data(), tw * th * 4);
    // Check water texture data
    if (tw > 0 && th > 0) {
        uint8_t* p = allPixels.data();
        uint32_t sum = 0;
        for (int j = 0; j < tw*th*4; ++j) sum += p[j];
        std::cout << "Water frame 0: " << tw << "x" << th << " sum=" << sum
                  << " px0=" << (int)p[0] << "," << (int)p[1] << "," << (int)p[2] << "," << (int)p[3]
                  << " px_mid=" << (int)p[th*128*4] << "," << (int)p[th*128*4+1] << "," << (int)p[th*128*4+2] << "," << (int)p[th*128*4+3]
                  << std::endl;
    }

    for (int i = 0; i < numFrames; ++i) {
        char path[128];
        std::snprintf(path, sizeof(path), "%s%02d.blp", waterInfo_.texFile.c_str(), i);
        if (i == 0) continue; // already loaded

        std::vector<uint8_t> frameData;
        if (!mpq.ReadFile(path, frameData)) continue;
        BLPImage frame;
        if (!DecodeBLP(frameData.data(), frameData.size(), frame)) continue;
        if (frame.width == tw && frame.height == th) {
            std::memcpy(&allPixels[i * tw * th * 4], frame.pixels.data(), tw * th * 4);
        }
    }

    // Full chained upload (was a bare level 0: incomplete under a
    // mipmapped sampler). Frames share tw/th (checked above per frame).
    waterTextureArray_ = CreateTextureArray(tw, th, numFrames, allPixels);
    waterTextureCount_ = numFrames;

    if (sg_query_image_state(waterTextureArray_) != SG_RESOURCESTATE_VALID) {
        waterTextureArray_ = {0};
        return false;
    }
    std::cout << "WaterTex: " << tw << "x" << th << " " << numFrames << " frames" << std::endl;
    return true;
}

int TileSet::PaletteToTexIndex(int paletteIdx) const {
    if (paletteIdx >= 0 && paletteIdx < (int)paletteToTex_.size())
        return paletteToTex_[paletteIdx];
    return -1;
}

int TileSet::CliffPaletteToTexIndex(int paletteIdx) const {
    if (paletteIdx >= 0 && paletteIdx < (int)paletteToCliffTex_.size())
        return paletteToCliffTex_[paletteIdx];
    return -1;
}

#include "render/render_server.h"
#include "textured_terrain.h"
// Keep this TU easy to touch when validating GPU-handle initialization fixes.
#include "render/rhi_device.h"
#include "terrain_shader.h"
#include "shapes.h"
#include "splat_model.h"
#include "mdx_viewer.h"
#include "variations.h"
#include "asset/TileSet.h"
#include "asset/BLP.h"
#include "stb/stb_image.h"
#include "model/mdx.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <array>
#include <queue>
#include <cmath>

static constexpr float TILE_SIZE = 128.0f;

namespace {
enum CliffBoundarySide : uint8_t {
    CliffBoundarySide_None = 0,
    CliffBoundarySide_Left = 1 << 0,
    CliffBoundarySide_Right = 1 << 1,
    CliffBoundarySide_Bottom = 1 << 2,
    CliffBoundarySide_Top = 1 << 3,
};

struct CliffBoundaryEdge {
    int cellX = 0;
    int cellY = 0;
    CliffBoundarySide side = CliffBoundarySide_None;
};

struct CliffBoundaryRegion {
    std::vector<int> cells;
};

static CliffBoundarySide OppositeBoundarySide(CliffBoundarySide side) {
    switch (side) {
        case CliffBoundarySide_Left: return CliffBoundarySide_Right;
        case CliffBoundarySide_Right: return CliffBoundarySide_Left;
        case CliffBoundarySide_Bottom: return CliffBoundarySide_Top;
        case CliffBoundarySide_Top: return CliffBoundarySide_Bottom;
        default: return CliffBoundarySide_None;
    }
}
}

static bool IsCliff(const float* hL, const float* hR) {
    if (!hL || !hR) return false;
    return std::abs(*hL - *hR) > 0.001f;
}

static bool IsWater(const unsigned char* we) {
    return we && *we > 0;
}

TexturedTerrain::TexturedTerrain() = default;

TexturedTerrain::~TexturedTerrain() { Release(); }

void TexturedTerrain::Release() {
    bool active = Renderer::IsActive();
    auto destroyImg = [active](sg_image& img) {
        if (active && img.id) sg_destroy_image(img);
        img = {0};
    };
    auto destroyBuf = [active](sg_buffer& buf) {
        if (active && buf.id) sg_destroy_buffer(buf);
        buf = {0};
    };
    auto destroySmp = [active](sg_sampler& smp) {
        if (active && smp.id) sg_destroy_sampler(smp);
        smp = {0};
    };
    auto destroyView = [active](sg_view& v) {
        if (active && v.id) sg_destroy_view(v);
        v = {0};
    };
    destroySmp(shadowSampler_);
    destroySmp(fogOfWarSampler_);
    destroySmp(linearSampler_);
    destroySmp(nearestSampler_);
    destroySmp(nonFilterSampler_);
    destroySmp(repeatSampler_);
    destroySmp(baseLevelSampler_);
    destroyView(heightTexView_);
    destroyView(cornerHeightView_);
    destroyView(texDataView_);
    destroyView(shadowView_);
    destroyView(fogView_);
    destroyView(lightView_);
    destroyView(combinedTexView_);
    if (combinedTex_.id) destroyImg(combinedTex_);
    destroyView(waterHeightView_);
    destroyView(waterExistsView_);
    destroyView(waterTexArrayView_);
    for (int i = 0; i < 17; ++i) destroyView(tileTexViews_[i]);

    auto destroyShd = [active](sg_shader& shd) {
        if (active && shd.id) sg_destroy_shader(shd);
        shd = {0};
    };
    destroyShd(groundShader_);
    destroyShd(cliffShader_);
    destroyShd(waterShader_);
    destroyShd(uberSplatShader_);

    auto destroyPip = [active](sg_pipeline& pip) {
        if (active && pip.id) sg_destroy_pipeline(pip);
        pip = {0};
    };
    destroyPip(groundPipeline_);
    destroyPip(cliffPipeline_);
    destroyPip(uberSplatPipeline_);
    destroyPip(waterPipeline_);

    for (auto& m : cliffMeshes_) {
        destroyBuf(m.vbo); destroyBuf(m.ebo);
        if (m.textureView.id) destroyView(m.textureView);
    }
    cliffMeshes_.clear();
    cliffInstanceData_.clear();

    for (auto& [k, v] : cliffViewerCache_) delete v;
    cliffViewerCache_.clear();
    for (auto& [k, v] : uberSplatModels_) delete v;
    uberSplatModels_.clear();
    uberSplatModelsList_.clear();
    uberSplatPathCache_.clear();
    runtimeUberSplatKeys_.clear();

    delete[] groundHeights_; groundHeights_ = nullptr;
    delete[] groundCornerHeights_; groundCornerHeights_ = nullptr;
    delete[] groundTextureList_; groundTextureList_ = nullptr;
    delete[] waterHeights_; waterHeights_ = nullptr;
    delete[] waterExistsData_; waterExistsData_ = nullptr;
    delete[] shadowData_; shadowData_ = nullptr;
    delete[] fogOfWarData_; fogOfWarData_ = nullptr;

    destroyBuf(groundVB_);
    destroyBuf(groundIB_);
    destroyBuf(pathingVB_);
    destroyBuf(pathingIB_);
    destroyBuf(regionVB_);
    destroyBuf(regionIB_);

    overlayState_ = engine::render::TerrainOverlayState{};
    nextShadowFootprintId_ = 1;
}

// ---- Data texture creation helpers ----

static sg_image MakeDataTexture(int w, int h, sg_pixel_format fmt, const void* data, size_t dataSize) {
    sg_image_desc desc = {};
    desc.type = SG_IMAGETYPE_2D;
    desc.width = w;
    desc.height = h;
    desc.pixel_format = fmt;
    desc.num_mipmaps = 1;
    desc.data.mip_levels[0].ptr = data;
    desc.data.mip_levels[0].size = dataSize;
    sg_image img = sg_make_image(&desc);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) img = {0};
    return img;
}

static sg_image MakeTextureArrayRGBA8(int w, int h, int slices, const void* data, size_t dataSize) {
    sg_image_desc d = {};
    d.type = SG_IMAGETYPE_ARRAY;
    d.width = w;
    d.height = h;
    d.num_slices = slices;
    d.pixel_format = SG_PIXELFORMAT_RGBA8;
    d.num_mipmaps = 1;
    d.data.mip_levels[0].ptr = data;
    d.data.mip_levels[0].size = dataSize;
    sg_image img = sg_make_image(&d);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) img = {0};
    return img;
}

static sg_image MakeTextureRGBA8(int w, int h, const void* data, size_t dataSize) {
    sg_image_desc d = {};
    d.type = SG_IMAGETYPE_2D;
    d.width = w;
    d.height = h;
    d.pixel_format = SG_PIXELFORMAT_RGBA8;
    d.num_mipmaps = 1;
    d.data.mip_levels[0].ptr = data;
    d.data.mip_levels[0].size = dataSize;
    sg_image img = sg_make_image(&d);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) img = {0};
    return img;
}

static std::string FindLocalAssetPath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::path path(relativePath);
    if (path.is_absolute() && fs::exists(path, ec)) {
        return path.lexically_normal().string();
    }

    fs::path base = fs::current_path(ec);
    if (ec) {
        base = fs::path(".");
        ec.clear();
    }

    for (int i = 0; i < 6 && !base.empty(); ++i) {
        const fs::path candidate = base / path;
        if (fs::exists(candidate, ec)) {
            return fs::absolute(candidate, ec).lexically_normal().string();
        }
        ec.clear();
        const fs::path parent = base.parent_path();
        if (parent == base) {
            break;
        }
        base = parent;
    }

    return {};
}

static sg_sampler MakeSampler(bool linear) {
    sg_sampler_desc d = {};
    d.min_filter = linear ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
    d.mag_filter = linear ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
    d.mipmap_filter = linear ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
 
    d.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    d.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    d.wrap_w = SG_WRAP_CLAMP_TO_EDGE;
    return sg_make_sampler(&d);
}

// ---- Build functions ----

void TexturedTerrain::RebuildCornerData(const W3EMap& map) {
    mapTileW_ = map.tileWidth;
    mapTileH_ = map.tileHeight;
    columns_ = map.tileWidth;
    rows_ = map.tileHeight;
    dataWidth_ = columns_;
    dataHeight_ = rows_;
    centerOffsetX_ = map.offsetX;
    centerOffsetY_ = map.offsetY;
    mapOffsetX_ = map.offsetX;
    mapOffsetY_ = map.offsetY;

    delete[] groundHeights_;
    delete[] groundCornerHeights_;
    delete[] groundTextureList_;
    delete[] waterHeights_;
    delete[] waterExistsData_;

    int cornerCount = columns_ * rows_;
    int tileCount = (columns_ - 1) * (rows_ - 1);
    groundHeights_ = new float[cornerCount]();
    groundCornerHeights_ = new float[cornerCount]();
    groundTextureList_ = new short[tileCount * 4]();
    waterHeights_ = new float[cornerCount]();
    waterExistsData_ = new unsigned char[cornerCount]();

    cornerExtra_.resize(cornerCount);

    for (int y = 0; y < rows_; ++y) {
        for (int x = 0; x < columns_; ++x) {
            int idx = y * columns_ + x;
            const auto& tile = map.tiles[idx];
            groundHeights_[idx] = tile.DecodeGroundHeight();
            waterHeights_[idx] = tile.DecodeWaterHeight();
            waterExistsData_[idx] = tile.IsWater() ? 255 : 0;

            float rampHeight = 0.0f;
            bool found = false;
            for (int xOffset = -1; xOffset <= 0 && !found; ++xOffset) {
                for (int yOffset = -1; yOffset <= 0 && !found; ++yOffset) {
                    int cx = x + xOffset, cy = y + yOffset;
                    if (cx >= 0 && cx < columns_ - 1 && cy >= 0 && cy < rows_ - 1) {
                        int cellBase = cy * columns_ + cx;
                        const auto& bl = map.tiles[cellBase];
                        const auto& br = map.tiles[cellBase + 1];
                        const auto& tl = map.tiles[cellBase + columns_];
                        const auto& tr = map.tiles[cellBase + columns_ + 1];
                        int base = std::min({bl.LayerHeight(), br.LayerHeight(),
                                             tl.LayerHeight(), tr.LayerHeight()});
                        if (tile.LayerHeight() != base) continue;
                        if (isCornerRampEntrance(map, cx, cy)) {
                            rampHeight = 0.5f;
                            found = true;
                        }
                    }
                }
            }

            groundCornerHeights_[idx] = tile.FinalGroundHeight() + rampHeight;
        }
    }

    int tileW = columns_ - 1, tileH = rows_ - 1;
    for (int y = 0; y < tileH; ++y) {
        for (int x = 0; x < tileW; ++x) {
            int idx = y * tileW + x;
            getTextureVariations(map, x, y, groundTextureList_, idx * 4);
        }
    }

    // ---- Cliff detection ----
    // Warcraft III cliff placement is cell-driven: the bottom-left corner owns the cell cliff state.
    for (auto& extra : cornerExtra_) {
        extra.cliff = false;
        extra.romp = false;
        extra.hideCliff = false;
        extra.cliffTexture = 15;
    }

    for (int y = 0; y < rows_ - 1; ++y) {
        for (int x = 0; x < columns_ - 1; ++x) {
            const int bl = y * columns_ + x;
            const int br = y * columns_ + (x + 1);
            const int tl = (y + 1) * columns_ + x;
            const int tr = (y + 1) * columns_ + (x + 1);

            const int hBl = map.tiles[bl].LayerHeight();
            const int hBr = map.tiles[br].LayerHeight();
            const int hTl = map.tiles[tl].LayerHeight();
            const int hTr = map.tiles[tr].LayerHeight();

            auto& extra = cornerExtra_[bl];
            extra.cliff = (hBl != hBr) || (hBl != hTl) || (hBl != hTr);
            if (!extra.cliff) {
                continue;
            }

            int cliffTex = map.tiles[bl].CliffTexture();
            if (cliffTex == 15) cliffTex = map.tiles[tl].CliffTexture();
            if (cliffTex == 15) cliffTex = map.tiles[tr].CliffTexture();
            if (cliffTex == 15) cliffTex = map.tiles[br].CliffTexture();
            extra.cliffTexture = cliffTex;
        }
    }

}

static Vec3 HeightColor(float h) {
    if (h < -256.0f) {
        float t = std::clamp((h + 1280.0f) / (1536.0f), 0.0f, 1.0f);
        return {0.1f * t, 0.2f * t, 0.6f * t + 0.4f * (1.0f - t)};
    } else if (h < 256.0f) {
        float t = (h + 256.0f) / (512.0f);
        return {0.2f * (1.0f - t) + 0.1f * t, 0.5f * (1.0f - t) + 0.4f * t, 0.1f * (1.0f - t)};
    } else if (h < 1280.0f) {
        float t = (h - 256.0f) / (1024.0f);
        return {0.1f + 0.3f * t, 0.4f - 0.1f * t, 0.1f};
    } else {
        float t = std::clamp((h - 1280.0f) / (2560.0f), 0.0f, 1.0f);
        return {0.4f + 0.6f * t, 0.3f + 0.7f * t, 0.1f + 0.9f * t};
    }
}

int TexturedTerrain::RealTileTexture(const W3EMap& map, int x, int y) const {
    int tw = map.tileWidth, th = map.tileHeight;
    for (int dc = -1; dc <= 0; dc++) {
        for (int dr = -1; dr <= 0; dr++) {
            int cx = x + dc, cy = y + dr;
            if (cx < 0 || cy < 0 || cx >= tw || cy >= th) continue;
            int idx = cy * tw + cx;
            if (!cornerExtra_[idx].cliff && !cornerExtra_[idx].romp) continue;
            if (cornerExtra_[idx].romp || cornerExtra_[idx].cliff) {
                int cliffPal = cornerExtra_[idx].cliffTexture;
                if (cliffPal == 15) cliffPal = 1;
                int groundPal = tileset_ ? tileset_->CliffToGroundPalIdx(cliffPal) : -1;
                if (groundPal >= 0) return groundPal;
            }
        }
    }
    if (tileset_ && map.tiles[y * tw + x].IsBlight() && tileset_->HasBlightTexture())
        return tileset_->BlightTextureIndex();
    return map.tiles[y * tw + x].GroundTexture();
}

int TexturedTerrain::GetVariation(int groundTexture, int variation) {
    if (!tileset_ || groundTexture < 0 || groundTexture >= tileset_->TextureCount()) return 0;
    const auto& tile = tileset_->GetTile(groundTexture);
    if (tile.extended) {
        if (variation <= 15) return 16 + variation;
        else if (variation == 16) return 15;
        else return 0;
    } else {
        return (variation == 0) ? 0 : 15;
    }
}

void TexturedTerrain::getTextureVariations(const W3EMap& map, int x, int y, short* out, int offset) {
    int bottomLeft = RealTileTexture(map, x, y);
    int bottomRight = RealTileTexture(map, x + 1, y);
    int topLeft = RealTileTexture(map, x, y + 1);
    int topRight = RealTileTexture(map, x + 1, y + 1);

    int vals[4] = {bottomLeft, bottomRight, topLeft, topRight};
    std::sort(vals, vals + 4);
    int unique[4] = {};
    int n = 0;
    for (int i = 0; i < 4; i++)
        if (n == 0 || vals[i] != unique[n - 1]) unique[n++] = vals[i];

    for (int i = 0; i < 4; i++) out[offset + i] = 17;

    int palFirst = unique[0];
    int firstVal = tileset_ ? tileset_->PaletteToTexIndex(palFirst) : palFirst;
    if (firstVal < 0) firstVal = 0;
    int cornerIdx = y * columns_ + x;
    int variation = map.tiles[cornerIdx].GroundVariation();
    out[offset] = (short)(firstVal + (GetVariation(firstVal, variation) << 5));

    int comp = offset + 1;
    for (int v = 1; v < n && comp < offset + 4; v++) {
        int palIdx = unique[v];
        int tex = tileset_ ? tileset_->PaletteToTexIndex(palIdx) : palIdx;
        if (tex < 0) tex = 0;
        int mask = 0;
        if (bottomRight == palIdx) mask |= 1;
        if (bottomLeft == palIdx)  mask |= 2;
        if (topRight == palIdx)    mask |= 4;
        if (topLeft == palIdx)     mask |= 8;
        out[comp++] = (short)(tex + (mask << 5));
    }
}

void TexturedTerrain::BuildGroundGPU(const W3EMap& map) {
    int tileW = columns_ - 1, tileH = rows_ - 1;

    // Final corner height texture (R32F, size = corners = columns x rows)
    groundCornerHeight_ = MakeDataTexture(columns_, rows_, SG_PIXELFORMAT_R32F,
                                           groundCornerHeights_, columns_ * rows_ * sizeof(float));
    // Base ground height texture (R32F, size = corners = columns x rows)
    groundHeight_ = MakeDataTexture(columns_, rows_, SG_PIXELFORMAT_R32F,
                                     groundHeights_, columns_ * rows_ * sizeof(float));

    // Texture index data (RGBA16UI, size = tiles = (columns-1) x (rows-1))
    std::vector<uint16_t> texIdx(tileW * tileH * 4, 0);
    for (int i = 0; i < tileW * tileH; ++i) {
        texIdx[i * 4]     = groundTextureList_[i * 4];
        texIdx[i * 4 + 1] = groundTextureList_[i * 4 + 1];
        texIdx[i * 4 + 2] = groundTextureList_[i * 4 + 2];
        texIdx[i * 4 + 3] = groundTextureList_[i * 4 + 3];
    }
    groundTextureData_ = MakeDataTexture(tileW, tileH, SG_PIXELFORMAT_RGBA16UI,
                                          texIdx.data(), texIdx.size() * sizeof(uint16_t));

    // Samplers
    nearestSampler_ = MakeSampler(false);
    linearSampler_ = MakeSampler(true);
    // Sampler: linear min/mag, forced base level only (max_lod=0.0001f, nearest mipmap) for stable terrain
    sg_sampler_desc bsd = {};
    bsd.min_filter = SG_FILTER_LINEAR;
    bsd.mag_filter = SG_FILTER_LINEAR;
    bsd.mipmap_filter = SG_FILTER_NEAREST;
    bsd.min_lod = 0.0f;
    bsd.max_lod = 0.0001f;
    bsd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    bsd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    bsd.wrap_w = SG_WRAP_CLAMP_TO_EDGE;
    baseLevelSampler_ = sg_make_sampler(&bsd);
    // Repeat sampler for water textures
    sg_sampler_desc rsd = {};
    rsd.min_filter = SG_FILTER_LINEAR; rsd.mag_filter = SG_FILTER_LINEAR; rsd.mipmap_filter = SG_FILTER_LINEAR;
    rsd.wrap_u = SG_WRAP_REPEAT; rsd.wrap_v = SG_WRAP_REPEAT;
    repeatSampler_ = sg_make_sampler(&rsd);
    // Create a non-filtering sampler for integer textures (terrain_texture_list)
    sg_sampler_desc nfDesc = {};
    nfDesc.min_filter = SG_FILTER_NEAREST;
    nfDesc.mag_filter = SG_FILTER_NEAREST;
    nfDesc.mipmap_filter =(sg_filter) 0;
    nfDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    nfDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    nfDesc.wrap_w = SG_WRAP_CLAMP_TO_EDGE;
    nonFilterSampler_ = sg_make_sampler(&nfDesc);

    // Simple instanced shader: reads height + texture index, samples correct texture array
    int texCount = tileset_ ? tileset_->TextureCount() : 0;
    if (texCount < 1) texCount = 1;
    texCount = std::clamp(texCount, 1, 17);

    // Use TerrainShaders::CreateGroundShader for multi-texture blending
    groundShader_ = TerrainShaders::CreateGroundShader(texCount);
    if (groundShader_.id == 0) { std::cerr << "TexturedTerrain: shader failed\n"; return; }

    // Create views for all textures
    auto mkView = [](sg_image img) {
        sg_view_desc vd = {}; vd.texture.image = img; return sg_make_view(&vd);
    };
    heightTexView_ = mkView(groundHeight_);
    cornerHeightView_ = mkView(groundCornerHeight_);
    texDataView_ = mkView(groundTextureData_);

    // Cache tile texture array views
    for (int i = 0; i < 17; ++i) tileTexViews_[i] = {0};
    if (tileset_) {
        int n = std::min(tileset_->TextureCount(), 17);
        for (int i = 0; i < n; ++i)
            tileTexViews_[i] = mkView(tileset_->GetTile(i).glTexture);

        int combinedTileSize = 0;
        bool canBuildCombined = n > 0;
        for (int i = 0; i < n; ++i) {
            const auto& tile = tileset_->GetTile(i);
            if (tile.firstLayerData.empty() || tile.tileSize <= 0) {
                canBuildCombined = false;
                break;
            }
            if (combinedTileSize == 0) combinedTileSize = tile.tileSize;
            if (tile.tileSize != combinedTileSize) {
                canBuildCombined = false;
                break;
            }
        }
        if (canBuildCombined) {
            std::vector<uint8_t> combinedPixels((size_t)combinedTileSize * combinedTileSize * 4 * n);
            for (int i = 0; i < n; ++i) {
                const auto& src = tileset_->GetTile(i).firstLayerData;
                std::memcpy(combinedPixels.data() + (size_t)i * combinedTileSize * combinedTileSize * 4,
                            src.data(),
                            (size_t)combinedTileSize * combinedTileSize * 4);
            }
            combinedTex_ = MakeTextureArrayRGBA8(combinedTileSize, combinedTileSize, n,
                                                 combinedPixels.data(), combinedPixels.size());
            if (combinedTex_.id) {
                combinedTexView_ = mkView(combinedTex_);
            }
        }
    }

    // Default shadow + fog
    auto makeR8 = [](uint8_t v) {
        sg_image_desc d = {}; d.type = SG_IMAGETYPE_2D; d.width = 1; d.height = 1;
        d.pixel_format = SG_PIXELFORMAT_R8; d.data.mip_levels[0].ptr = &v; d.data.mip_levels[0].size = 1;
        return sg_make_image(&d);
    };
    shadowMap_ = makeR8(0); fogOfWarMap_ = makeR8(0);
    shadowView_ = mkView(shadowMap_); fogView_ = mkView(fogOfWarMap_);

    sg_pipeline_desc pd = {};
    pd.shader = groundShader_;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.index_type = SG_INDEXTYPE_UINT32;
    // Enable depth test for proper 3D terrain
    pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pd.depth.write_enabled = true;
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.colors[0].blend.enabled = false;
    pd.layout.buffers[0].stride = 8;
    pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT2};
    groundPipeline_ = sg_make_pipeline(&pd);

    // Create uber-splat shader
    uberSplatShader_ = TerrainShaders::CreateUberSplatShader(texCount);
}

void TexturedTerrain::BuildWaterGPU(const W3EMap& map) {
    int tileW = columns_ - 1, tileH = rows_ - 1;
    waterHeight_ = MakeDataTexture(columns_, rows_, SG_PIXELFORMAT_R32F,
                                    waterHeights_, columns_ * rows_ * sizeof(float));
    waterExists_ = MakeDataTexture(columns_, rows_, SG_PIXELFORMAT_R8,
                                    waterExistsData_, columns_ * rows_);

    // Use TerrainShaders::CreateWaterShader
    waterShader_ = TerrainShaders::CreateWaterShader(tileset_ ? tileset_->WaterFrameCount() : 1);
    if (waterShader_.id == 0) { std::cerr << "TexturedTerrain: water shader failed\n"; return; }

    // Views
    auto mkView = [](sg_image img) {
        sg_view_desc vd = {}; vd.texture.image = img; return sg_make_view(&vd);
    };
    waterHeightView_ = mkView(waterHeight_);
    waterExistsView_ = mkView(waterExists_);
    if (tileset_ && tileset_->HasWaterTexture()) {
        waterTexArrayView_ = mkView(tileset_->GetWaterTextureArray());
        std::cout << "WaterTexArrayView: id=" << waterTexArrayView_.id
                  << " image_id=" << tileset_->GetWaterTextureArray().id << std::endl;
    } else {
        std::cout << "Water: no water texture array available" << std::endl;
    }

    // Use water height offset from tileset (positive = raise, negative = lower)
    if (tileset_ && tileset_->HasWaterInfo())
        waterHeightOffset_ = tileset_->GetWaterInfo().heightOffset;
    else
        waterHeightOffset_ = -0.5f;

    // Build shoreline wave models (Java-style placement)
    if (mpq_ && tileset_ && tileset_->HasWaterInfo()) {
        auto& wi = tileset_->GetWaterInfo();
        float offsets[2] = {centerOffsetX_, centerOffsetY_};
        waveBuilder_.Build(mpq_, std::string(1, map.tileset),
                           columns_, rows_, offsets,
                           groundCornerHeights_, waterHeights_, waterExistsData_, columns_,
                           waterHeightOffset_,
                           wi.shoreDir, wi.shoreSFile, wi.shoreOCFile, wi.shoreICFile);
    }

    sg_pipeline_desc pd = {};
    pd.shader = waterShader_;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.index_type = SG_INDEXTYPE_UINT32;
    pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pd.depth.write_enabled = false;  // water doesn't write depth
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.colors[0].blend.enabled = true;
    pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pd.layout.buffers[0].stride = 8;
    pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT2};
    waterPipeline_ = sg_make_pipeline(&pd);

    std::cout << "Water: " << columns_ << "x" << rows_ << " tiles" << std::endl;
}

void TexturedTerrain::BuildCliffsGPU(const W3EMap& map) {
    cliffInstanceData_.clear();
    if (cliffPipeline_.id) {
        sg_destroy_pipeline(cliffPipeline_);
        cliffPipeline_ = {};
    }
    if (cliffShader_.id) {
        sg_destroy_shader(cliffShader_);
        cliffShader_ = {};
    }
    for (auto& mesh : cliffMeshes_) {
        if (mesh.vbo.id) sg_destroy_buffer(mesh.vbo);
        if (mesh.ebo.id) sg_destroy_buffer(mesh.ebo);
    }
    cliffMeshes_.clear();

    if (!tileset_ || !tileset_->HasCliffTextureArray() || !mpq_) {
        return;
    }

    std::unordered_map<std::string, int> meshLookup;
    struct CliffVertexGPU {
        float px, py, pz;
        float tu, tv;
        float nx, ny, nz;
    };

    auto loadMesh = [&](const CliffInstance& inst) -> int {
        auto found = meshLookup.find(inst.meshPath);
        if (found != meshLookup.end()) return found->second;

        std::vector<uint8_t> data;
        if (!mpq_->ReadFile(inst.meshPath, data)) return -1;

        MdxModel model;
        if (!MdxLoad(data.data(), data.size(), model) || model.geosets.empty()) return -1;

        int totalVerts = 0;
        int totalIndices = 0;
        for (const auto& g : model.geosets) {
            totalVerts += (int)g.vertices.size() / 3;
            totalIndices += (int)g.indices.size();
        }
        if (totalVerts == 0 || totalIndices == 0) return -1;

        std::vector<CliffVertexGPU> verts(totalVerts);
        std::vector<uint32_t> indices(totalIndices);
        int vOffset = 0;
        int iOffset = 0;
        float minLocalZ = model.geosets[0].vertices.size() >= 3 ? model.geosets[0].vertices[2] : 0.0f;
        float maxLocalZ = minLocalZ;

        for (const auto& geo : model.geosets) {
            const int vc = (int)geo.vertices.size() / 3;
            const std::vector<float>* uvSource = (!geo.uvSets.empty() && !geo.uvSets[0].empty()) ? &geo.uvSets[0] : nullptr;
            for (int i = 0; i < vc; ++i) {
                verts[vOffset + i].px = geo.vertices[i * 3 + 0];
                verts[vOffset + i].py = geo.vertices[i * 3 + 1];
                verts[vOffset + i].pz = geo.vertices[i * 3 + 2];
                minLocalZ = std::min(minLocalZ, verts[vOffset + i].pz);
                maxLocalZ = std::max(maxLocalZ, verts[vOffset + i].pz);
                if (uvSource && uvSource->size() >= size_t(i * 2 + 2)) {
                    verts[vOffset + i].tu = (*uvSource)[i * 2 + 0];
                    verts[vOffset + i].tv = (*uvSource)[i * 2 + 1];
                } else {
                    verts[vOffset + i].tu = 0.0f;
                    verts[vOffset + i].tv = 0.0f;
                }
                if (!geo.normals.empty()) {
                    verts[vOffset + i].nx = geo.normals[i * 3 + 0];
                    verts[vOffset + i].ny = geo.normals[i * 3 + 1];
                    verts[vOffset + i].nz = geo.normals[i * 3 + 2];
                } else {
                    verts[vOffset + i].nx = 0.0f;
                    verts[vOffset + i].ny = 0.0f;
                    verts[vOffset + i].nz = 1.0f;
                }
            }
            for (size_t idx = 0; idx < geo.indices.size(); ++idx) {
                indices[iOffset + idx] = (uint32_t)(geo.indices[idx] + vOffset);
            }
            vOffset += vc;
            iOffset += (int)geo.indices.size();
        }

        CliffMeshGPU mesh;
        sg_buffer_desc vbd = {};
        vbd.usage.vertex_buffer = true;
        vbd.data.ptr = verts.data();
        vbd.data.size = verts.size() * sizeof(CliffVertexGPU);
        mesh.vbo = sg_make_buffer(&vbd);

        sg_buffer_desc ibd = {};
        ibd.usage.index_buffer = true;
        ibd.data.ptr = indices.data();
        ibd.data.size = indices.size() * sizeof(uint32_t);
        mesh.ebo = sg_make_buffer(&ibd);
        mesh.indexCount = (int)indices.size();
        mesh.minLocalZ = minLocalZ;
        mesh.maxLocalZ = maxLocalZ;

        const int index = (int)cliffMeshes_.size();
        cliffMeshes_.push_back(mesh);
        meshLookup[inst.meshPath] = index;
        return index;
    };

    buildStats_.cliffModelHits = 0;
    buildStats_.cliffModelMisses = 0;
    for (const auto& inst : cliffInstances_) {
        if (inst.meshPath.empty()) continue;
        const int meshIndex = loadMesh(inst);
        if (meshIndex < 0) {
            buildStats_.cliffModelMisses++;
            continue;
        }
        CliffInstanceGPU gpuInst;
        gpuInst.gridX = inst.gridX;
        gpuInst.gridY = inst.gridY;
        gpuInst.layerBase = inst.layerBase;
        gpuInst.textureLayer = std::max(0, tileset_->CliffPaletteToTexIndex(inst.cliffTexture));
        gpuInst.topCellX = inst.topCellX;
        gpuInst.topCellY = inst.topCellY;
        gpuInst.bottomCellX = inst.bottomCellX;
        gpuInst.bottomCellY = inst.bottomCellY;
        gpuInst.meshIndex = meshIndex;
        cliffInstanceData_.push_back(gpuInst);
        buildStats_.cliffModelHits++;
    }

    int groundTexCount = tileset_ ? tileset_->TextureCount() : 1;
    groundTexCount = std::clamp(groundTexCount, 1, 17);
    cliffShader_ = TerrainShaders::CreateCliffShader(tileset_->CliffTextureCount(), groundTexCount);
    if (!cliffShader_.id) return;

    sg_pipeline_desc pd = {};
    pd.shader = cliffShader_;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.index_type = SG_INDEXTYPE_UINT32;
    pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pd.depth.write_enabled = true;
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.colors[0].blend.enabled = true;
    pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pd.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    pd.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pd.layout.buffers[0].stride = sizeof(CliffVertexGPU);
    pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT3};
    pd.layout.attrs[1] = {0, 12, SG_VERTEXFORMAT_FLOAT2};
    pd.layout.attrs[2] = {0, 20, SG_VERTEXFORMAT_FLOAT3};
    cliffPipeline_ = sg_make_pipeline(&pd);

    if (tileset_->HasCliffTextureArray()) {
        std::cout << "Cliff texture array: " << tileset_->CliffTextureArraySize()
                  << "x" << tileset_->CliffTextureArraySize() << std::endl;
    }
}

void TexturedTerrain::BuildPathingOverlay(const W3EMap& map) {
    (void)map;
}

void TexturedTerrain::BuildRegionOverlay(const W3EMap& map) {
    (void)map;
}

bool TexturedTerrain::Build(const W3EMap& map, TileSet* tileset) {
    Release();
    tileset_ = tileset;
    tilesetChar_ = map.tileset;
    buildStats_ = TerrainBuildStats{};
    terrainChunkGrid_.BuildFromW3E(map, 16);
    overlayState_.Resize(map.tileWidth, map.tileHeight);
    RebuildCornerData(map);
    BuildCliffsFromMdx(map);
    BuildGroundGPU(map);
    BuildWaterGPU(map);
    BuildCliffsGPU(map);

    if (groundShader_.id == 0) {
        std::cerr << "TexturedTerrain: Build failed" << std::endl;
        return false;
    }

    center_ = {
        (map.tileWidth * TILE_SIZE + map.offsetX * 2) * 0.5f,
        0,
        (map.tileHeight * TILE_SIZE + map.offsetY * 2) * 0.5f
    };
    radius_ = std::sqrt(
        map.tileWidth * TILE_SIZE * map.tileWidth * TILE_SIZE +
        map.tileHeight * TILE_SIZE * map.tileHeight * TILE_SIZE
    ) * 0.5f;

    InitLights();

    if (tileset_) {
        buildStats_.groundTexturesLoaded = tileset_->TextureCount();
        buildStats_.blightLoaded = tileset_->HasBlightTexture() ? 1 : 0;
        buildStats_.groundSrgbTextures = tileset_->GroundSrgbTextureCount();
        buildStats_.groundLinearTextures = tileset_->GroundLinearTextureCount();
        buildStats_.cliffSrgbTextures = tileset_->CliffSrgbTextureCount();
        buildStats_.cliffLinearTextures = tileset_->CliffLinearTextureCount();
        buildStats_.waterSrgbTextures = tileset_->WaterSrgbTextureCount();
        buildStats_.waterLinearTextures = tileset_->WaterLinearTextureCount();
        buildStats_.groundPaletteMappedCount = tileset_->GroundPaletteMappedCount();
        buildStats_.groundPaletteFallbackCount = tileset_->GroundPaletteFallbackCount();
        buildStats_.fallbackMode = tileset_->FallbackMode();
    }
    buildStats_.cliffMeshesBuilt = (int)cliffMeshes_.size();
    buildStats_.uberSplatLoads = (int)uberSplatModels_.size();
    buildStats_.shadowLoads = (int)shadowTextureCache_.size();

    std::cout << "TexturedTerrain: built " << columns_ << "x" << rows_
              << " tiles, " << tileset_->TextureCount() << " textures" << std::endl;
    std::cout << "TerrainStats: groundTex=" << buildStats_.groundTexturesLoaded
              << " mapped=" << buildStats_.groundPaletteMappedCount
              << " fallback=" << buildStats_.groundPaletteFallbackCount
              << " cliffHits=" << buildStats_.cliffModelHits
              << " cliffMisses=" << buildStats_.cliffModelMisses
              << " uberSplats=" << buildStats_.uberSplatLoads
              << " uberMisses=" << buildStats_.uberSplatMisses
              << " waterSrgb=" << buildStats_.waterSrgbTextures
              << " fallbackMode=" << buildStats_.fallbackMode
              << std::endl;
    return true;
}

void TexturedTerrain::InitLights() {
    lightManager_.Init();
    lightManager_.Update();
    // Create view for light texture
    if (lightManager_.GetLightTexture().id) {
        sg_view_desc vd = {};
        vd.texture.image = lightManager_.GetLightTexture();
        lightView_ = sg_make_view(&vd);
    } else {
        lightView_ = shadowView_; // fallback
    }
}

// ---- Render ----

void TexturedTerrain::Render(const Mat4& view, const Mat4& proj, float time) {
    if (!groundShader_.id) return;
    if (terrainChunkGrid_.GetChunkCount() > 0) {
        const render::Frustum frustum = render::Frustum::FromViewProj(proj * view);
        lastChunkCullStats_ = terrainChunkGrid_.CullChunks(frustum, visibleTerrainChunks_);
        if (lastChunkCullStats_.visibleChunks == 0) {
            lastRenderTime_ = time;
            return;
        }
    }

    auto isValidBuffer = [](sg_buffer buffer) {
        return buffer.id && sg_query_buffer_state(buffer) == SG_RESOURCESTATE_VALID;
    };
    auto isValidView = [](sg_view viewHandle) {
        if (viewHandle.id && ((viewHandle.id & 0xFFFFu) >= 4096u)) {
            std::fprintf(stderr, "TexturedTerrain suspicious view handle: 0x%08x\n", viewHandle.id);
            return false;
        }
        return viewHandle.id && sg_query_view_state(viewHandle) == SG_RESOURCESTATE_VALID;
    };
    auto isValidSampler = [](sg_sampler samplerHandle) {
        return samplerHandle.id && sg_query_sampler_state(samplerHandle) == SG_RESOURCESTATE_VALID;
    };
    auto hasSuspiciousBoundView = [](const char* passName, const sg_bindings& bind, int viewCount) {
        for (int i = 0; i < viewCount; ++i) {
            if (bind.views[i].id && ((bind.views[i].id & 0xFFFFu) >= 4096u)) {
                std::fprintf(stderr, "TexturedTerrain %s suspicious view[%d]=0x%08x\n",
                    passName, i, bind.views[i].id);
                return true;
            }
        }
        return false;
    };

    Shapes& shapes = Shapes::Instance();
    shapes.Bind();
    int idxCount = shapes.IndexCount();

    // Ground (instanced, texture-based rendering)
    if (groundPipeline_.id) {
        Shapes& shapes = Shapes::Instance();
        shapes.Bind();
        int idxCount = shapes.IndexCount();
        int numTiles = (columns_ - 1) * (rows_ - 1);
        const int texCount = std::clamp(tileset_ ? tileset_->TextureCount() : 1, 1, 17);
        const sg_buffer shapeVB = shapes.GetVertexBuffer();
        const sg_buffer shapeIB = shapes.GetIndexBuffer();
        if (!isValidBuffer(shapeVB) || !isValidBuffer(shapeIB)
            || !isValidView(heightTexView_) || !isValidView(cornerHeightView_)
            || !isValidView(texDataView_) || !isValidView(shadowView_) || !isValidView(fogView_)
            || !isValidSampler(linearSampler_) || !isValidSampler(nearestSampler_)
            || !isValidSampler(nonFilterSampler_)) {
            static bool loggedGroundBindingSkip = false;
            if (!loggedGroundBindingSkip) {
                loggedGroundBindingSkip = true;
                std::cerr << "TexturedTerrain: skipping ground render due to invalid base bindings" << std::endl;
            }
            return;
        }

        sg_view fallbackGroundTileView = {0};
        for (int i = 0; i < texCount; ++i) {
            if (isValidView(tileTexViews_[i])) {
                fallbackGroundTileView = tileTexViews_[i];
                break;
            }
        }
        if (!fallbackGroundTileView.id) {
            static bool loggedGroundTextureSkip = false;
            if (!loggedGroundTextureSkip) {
                loggedGroundTextureSkip = true;
                std::cerr << "TexturedTerrain: skipping ground render due to missing valid terrain texture views" << std::endl;
            }
            return;
        }

        const sg_view terrainLightView = isValidView(lightView_) ? lightView_ : shadowView_;
        const sg_sampler terrainLightSampler = isValidSampler(lightManager_.GetLightSampler())
            ? lightManager_.GetLightSampler()
            : nearestSampler_;

        sg_apply_pipeline(groundPipeline_);
        sg_bindings bind = {};
        bind.vertex_buffers[0] = shapeVB;
        bind.index_buffer = shapeIB;

        // VS texture bindings (slots 0-3): height_texture, height_cliff_texture, terrain_texture_list, lightTexture
        bind.views[0] = heightTexView_;
        bind.views[1] = cornerHeightView_;
        // VS textures (slots 0-3): height_texture, height_cliff_texture, terrain_texture_list, lightTexture
        bind.views[0] = heightTexView_;       // height_texture (center heights)
        bind.views[1] = cornerHeightView_;     // height_cliff_texture (corner heights)
        bind.views[2] = texDataView_;          // terrain_texture_list (texture indices)
        bind.views[3] = terrainLightView;      // lightTexture
        // FS textures (slots 4+): sample0..N (full 32-layer arrays), shadowMap, fogOfWarMap
        for (int i = 0; i < texCount; ++i) {
            bind.views[4 + i] = isValidView(tileTexViews_[i]) ? tileTexViews_[i] : fallbackGroundTileView;
        }
        bind.views[4 + texCount] = shadowView_;
        bind.views[5 + texCount] = fogView_;
        // Samplers
        bind.samplers[0] = linearSampler_;
        bind.samplers[1] = nearestSampler_;
        bind.samplers[2] = nonFilterSampler_;  // integer texture → non-filtering
        bind.samplers[3] = terrainLightSampler;
        // Base level sampler (max_lod=0, clamp-to-edge) for ground texture arrays to eliminate seam bleeding
        bind.samplers[4] = baseLevelSampler_.id ? baseLevelSampler_ : linearSampler_;
        bind.samplers[5] = linearSampler_;
        bind.samplers[6] = linearSampler_;

        if (hasSuspiciousBoundView("ground", bind, 6 + texCount)) {
            return;
        }
        sg_apply_bindings(&bind);

        // VS uniforms
        Mat4 mvp = proj * view;
        struct {
            float MVP[16], DepthBiasMVP[16];
            float centerOffsetX, centerOffsetY;
            float lightCount, lightTextureHeight;
        } vsU;
        for (int i = 0; i < 16; ++i) vsU.MVP[i] = mvp.m[i];
        for (int i = 0; i < 16; ++i) vsU.DepthBiasMVP[i] = mvp.m[i];
        vsU.centerOffsetX = centerOffsetX_;
        vsU.centerOffsetY = centerOffsetY_;
        vsU.lightCount = (float)lightManager_.GetLightCount();
        vsU.lightTextureHeight = lightManager_.GetLightTextureHeight();
        sg_apply_uniforms(0, sg_rng(vsU));

        // FS uniforms
        struct {
            float showLighting, baseTexOnly;
            float fogColor[4], fogParams[4];
        } fsU = {};
        fsU.showLighting = 1.0f; // match cliff pass: terrain honors day/night lights
        fsU.baseTexOnly = (debugMode_ == 4) ? 1.0f : 0.0f;
        fsU.fogColor[0] = fogColor_[0];
        fsU.fogColor[1] = fogColor_[1];
        fsU.fogColor[2] = fogColor_[2];
        fsU.fogColor[3] = 1.0f;
        fsU.fogParams[0] = fogEnabled_ ? 1.0f : 0.0f;
        fsU.fogParams[1] = fogStart_;
        fsU.fogParams[2] = fogEnd_;
        sg_apply_uniforms(1, sg_rng(fsU));

        sg_draw(0, idxCount, numTiles);
    }

    auto renderLowPrioritySplats = [&]() {
        if (!uberSplatShader_.id || uberSplatModelsList_.empty()) return;
        Mat4 mvp = proj * view;
        float center[2] = {centerOffsetX_, centerOffsetY_};
        int texCount = tileset_ ? tileset_->TextureCount() : 1;
        texCount = std::clamp(texCount, 1, 17);
        for (auto* splat : uberSplatModelsList_) {
            if (splat && splat->IsHighPriority()) continue;
            if (splat) splat->Render(uberSplatShader_, mvp, center,
                columns_ - 1, rows_ - 1, cornerHeightView_,
                shadowView_, waterHeightView_, fogView_, waterHeightOffset_,
                texDataView_, tileTexViews_, texCount, nonFilterSampler_, linearSampler_);
        }
    };

    auto renderHighPrioritySplats = [&]() {
        if (!uberSplatShader_.id || uberSplatModelsList_.empty()) return;
        Mat4 mvp = proj * view;
        float center[2] = {centerOffsetX_, centerOffsetY_};
        int texCount = tileset_ ? tileset_->TextureCount() : 1;
        texCount = std::clamp(texCount, 1, 17);
        for (auto* splat : uberSplatModelsList_) {
            if (splat && !splat->IsHighPriority()) continue;
            if (splat) splat->Render(uberSplatShader_, mvp, center,
                columns_ - 1, rows_ - 1, cornerHeightView_,
                shadowView_, waterHeightView_, fogView_, waterHeightOffset_,
                texDataView_, tileTexViews_, texCount, nonFilterSampler_, linearSampler_);
        }
    };

    auto renderWaterPass = [&]() {
        if (!waterPipeline_.id || !waterShader_.id) return;
        if (lastRenderTime_ >= 0.0f) {
            float dt = time - lastRenderTime_;
            if (dt > 0) {
                float rate = tileset_ && tileset_->HasWaterInfo() ? tileset_->GetWaterInfo().texRate : 0;
                waterIndex_ += rate * dt;
                int nf = tileset_ && tileset_->HasWaterTexture() ? tileset_->WaterFrameCount() : 1;
                if (nf > 0) waterIndex_ = fmodf(waterIndex_, (float)nf);
                if (waterIndex_ < 0) waterIndex_ += nf;
            }
        }
        lastRenderTime_ = time;

        Shapes& shapes = Shapes::Instance();
        shapes.Bind();
        int idxCount = shapes.IndexCount();
        int numTiles = (columns_ - 1) * (rows_ - 1);

        sg_apply_pipeline(waterPipeline_);
        sg_bindings bind = {};
        bind.vertex_buffers[0] = shapes.GetVertexBuffer();
        bind.index_buffer = shapes.GetIndexBuffer();
        bind.views[0] = waterHeightView_;
        bind.views[1] = cornerHeightView_;
        bind.views[2] = waterExistsView_;
        bind.views[3] = lightView_;
        if (tileset_ && tileset_->HasWaterTexture())
            bind.views[4] = waterTexArrayView_;
        else
            bind.views[4] = fogView_;
        bind.views[5] = fogView_;
        bind.samplers[0] = nearestSampler_;
        bind.samplers[1] = nearestSampler_;
        bind.samplers[2] = nearestSampler_;
        bind.samplers[3] = lightManager_.GetLightSampler();
        bind.samplers[4] = repeatSampler_;
        bind.samplers[5] = nearestSampler_;
        if (hasSuspiciousBoundView("water", bind, 6)) {
            return;
        }
        sg_apply_bindings(&bind);

        Mat4 mvp = proj * view;
        auto* wi = tileset_ && tileset_->HasWaterInfo() ? &tileset_->GetWaterInfo() : nullptr;
        struct {
            float MVP[16];
            float cx, cy, wo, wave;
            float lc, lth;
            float scMin[4], scMax[4], dcMin[4], dcMax[4];
        } vsU;
        for (int i = 0; i < 16; ++i) vsU.MVP[i] = mvp.m[i];
        vsU.cx = centerOffsetX_; vsU.cy = centerOffsetY_;
        vsU.wo = waterHeightOffset_;
        vsU.wave = waterDeforms_ ? 8.0f * std::sin(time * 1.6f) : 0.0f;
        vsU.lc = (float)lightManager_.GetLightCount(); vsU.lth = lightManager_.GetLightTextureHeight();
        float defMin[4] = {0,0.1f,0.3f,0.6f};
        float defMax[4] = {0,0.3f,0.5f,0.8f};
        float defDMin[4] = {0,0.05f,0.15f,0.7f};
        float defDMax[4] = {0,0.1f,0.3f,0.9f};
        if (wi) {
            for (int i = 0; i < 4; ++i) vsU.scMin[i] = wi->minShallow[i];
            for (int i = 0; i < 4; ++i) vsU.scMax[i] = wi->maxShallow[i];
            for (int i = 0; i < 4; ++i) vsU.dcMin[i] = wi->minDeep[i];
            for (int i = 0; i < 4; ++i) vsU.dcMax[i] = wi->maxDeep[i];
        } else {
            for (int i = 0; i < 4; ++i) { vsU.scMin[i]=defMin[i]; vsU.scMax[i]=defMax[i]; vsU.dcMin[i]=defDMin[i]; vsU.dcMax[i]=defDMax[i]; }
        }
        sg_apply_uniforms(0, sg_rng(vsU));

        struct { float ct; float mb[4]; float fc[4]; float fp[4]; } fsU = {};
        fsU.ct = waterIndex_;
        fsU.mb[0] = mapBounds_[0]; fsU.mb[1] = mapBounds_[1];
        fsU.mb[2] = mapBounds_[2]; fsU.mb[3] = mapBounds_[3];
        fsU.fc[0] = fogColor_[0]; fsU.fc[1] = fogColor_[1];
        fsU.fc[2] = fogColor_[2]; fsU.fc[3] = 1.0f;
        fsU.fp[0] = fogEnabled_ ? 1.0f : 0.0f;
        fsU.fp[1] = fogStart_; fsU.fp[2] = fogEnd_;
        sg_apply_uniforms(1, sg_rng(fsU));

        sg_draw(0, idxCount, numTiles);
    };

    renderLowPrioritySplats();

    // Cliff rendering (dedicated terrain cliff shader)
    if (cliffPipeline_.id && cliffShader_.id && !cliffInstanceData_.empty() && tileset_ && tileset_->HasCliffTextureArray()) {
        sg_apply_pipeline(cliffPipeline_);
        struct CliffVSBlock {
            float MVP[16];
            float centerOffsetX;
            float centerOffsetY;
            float cliffOffset[4];
            float lightCount;
            float lightTextureHeight;
        };
        struct CliffFSBlock {
            float showLighting;
            float baseTextureOnly;
            float fogColor[4];
            float fogParams[4];
            float cliffBandParams0[4];
            float cliffBandParams1[4];
            float topTerrainCell[4];
            float bottomTerrainCell[4];
        };
        Mat4 mvp = proj * view;
        CliffFSBlock fsU = {};
        fsU.showLighting = 1.0f;
        fsU.baseTextureOnly = (debugMode_ == 4) ? 1.0f : 0.0f;
        fsU.fogColor[0] = fogColor_[0];
        fsU.fogColor[1] = fogColor_[1];
        fsU.fogColor[2] = fogColor_[2];
        fsU.fogColor[3] = 1.0f;
        fsU.fogParams[0] = fogEnabled_ ? 1.0f : 0.0f;
        fsU.fogParams[1] = fogStart_;
        fsU.fogParams[2] = fogEnd_;
        fsU.fogParams[3] = 0.0f;
        const int groundTexCount = std::clamp(tileset_->TextureCount(), 1, 17);
        auto isValidTerrainCell = [&](int cellX, int cellY) {
            return cellX >= 0 && cellY >= 0 && cellX < (columns_ - 1) && cellY < (rows_ - 1);
        };

        lastVisibleCliffs_ = 0;
        lastCulledCliffs_ = 0;
        const render::Frustum cliffFrustum = render::Frustum::FromViewProj(mvp);
        for (const auto& inst : cliffInstanceData_) {
            if (inst.meshIndex < 0 || inst.meshIndex >= (int)cliffMeshes_.size()) continue;
            const auto& mesh = cliffMeshes_[inst.meshIndex];
            if (!mesh.vbo.id || !mesh.ebo.id) continue;
            // Stage H2: Cull off-screen cliff meshes via 6-plane AABB test
            const float worldX0 = centerOffsetX_ + (float)(inst.gridX - 1) * 128.0f;
            const float worldX1 = centerOffsetX_ + (float)(inst.gridX + 2) * 128.0f;
            const float worldZ0 = -(centerOffsetY_ + (float)(inst.gridY + 2) * 128.0f);
            const float worldZ1 = -(centerOffsetY_ + (float)(inst.gridY - 1) * 128.0f);
            const float baseY = inst.layerBase * 128.0f;
            const Vec3 cliffMin(worldX0, baseY + mesh.minLocalZ - 64.0f, std::min(worldZ0, worldZ1));
            const Vec3 cliffMax(worldX1, baseY + mesh.maxLocalZ + 64.0f, std::max(worldZ0, worldZ1));
            if (!cliffFrustum.TestAABB(cliffMin, cliffMax)) {
                ++lastCulledCliffs_;
                continue;
            }
            ++lastVisibleCliffs_;

            sg_bindings bind = {};
            bind.vertex_buffers[0] = mesh.vbo;
            bind.index_buffer = mesh.ebo;
            bind.views[0] = heightTexView_;
            bind.views[1] = lightView_;
            bind.views[2] = tileset_->GetCliffTextureArrayView();
            bind.views[3] = shadowView_;
            bind.views[4] = fogView_;
            bind.views[5] = texDataView_;
            for (int i = 0; i < groundTexCount && i < 17; ++i) {
                bind.views[6 + i] = tileTexViews_[i];
            }
            bind.samplers[0] = linearSampler_;
            bind.samplers[1] = lightManager_.GetLightSampler();
            bind.samplers[2] = linearSampler_;
            bind.samplers[3] = linearSampler_;
            bind.samplers[4] = linearSampler_;
            bind.samplers[5] = nonFilterSampler_;
            bind.samplers[6] = baseLevelSampler_.id ? baseLevelSampler_ : linearSampler_;
            if (hasSuspiciousBoundView("cliff", bind, 6 + groundTexCount)) {
                continue;
            }
            sg_apply_bindings(&bind);

            CliffVSBlock vsU = {};
            for (int i = 0; i < 16; ++i) vsU.MVP[i] = mvp.m[i];
            vsU.centerOffsetX = centerOffsetX_;
            vsU.centerOffsetY = centerOffsetY_;
            vsU.cliffOffset[0] = (float)inst.gridX;
            vsU.cliffOffset[1] = (float)inst.gridY;
            vsU.cliffOffset[2] = inst.layerBase;
            vsU.cliffOffset[3] = (float)inst.textureLayer;
            const float cliffHeight = std::max(0.0f, mesh.maxLocalZ - mesh.minLocalZ);
            const float topBand = std::clamp(cliffHeight * 0.18f, 12.0f, 48.0f);
            const float bottomBand = std::clamp(cliffHeight * 0.14f, 10.0f, 40.0f);
            const float topSeam = std::clamp(topBand * 0.25f, 4.0f, 16.0f);
            const float bottomSeam = std::clamp(bottomBand * 0.12f, 2.0f, 8.0f);
            const bool cellsDiffer = (inst.topCellX != inst.bottomCellX) || (inst.topCellY != inst.bottomCellY);
            const bool topBlendValid = cellsDiffer && isValidTerrainCell(inst.topCellX, inst.topCellY);
            const bool bottomBlendValid = cellsDiffer && isValidTerrainCell(inst.bottomCellX, inst.bottomCellY);
            vsU.lightCount = (float)lightManager_.GetLightCount();
            vsU.lightTextureHeight = lightManager_.GetLightTextureHeight();
            fsU.cliffBandParams0[0] = mesh.minLocalZ;
            fsU.cliffBandParams0[1] = mesh.maxLocalZ;
            fsU.cliffBandParams0[2] = topBand;
            fsU.cliffBandParams0[3] = bottomBand;
            fsU.cliffBandParams1[0] = topSeam;
            fsU.cliffBandParams1[1] = bottomSeam;
            fsU.cliffBandParams1[2] = topBlendValid ? 1.0f : 0.0f;
            fsU.cliffBandParams1[3] = bottomBlendValid ? 1.0f : 0.0f;
            fsU.topTerrainCell[0] = (float)inst.topCellX;
            fsU.topTerrainCell[1] = (float)inst.topCellY;
            fsU.topTerrainCell[2] = 0.0f;
            fsU.topTerrainCell[3] = 0.0f;
            fsU.bottomTerrainCell[0] = (float)inst.bottomCellX;
            fsU.bottomTerrainCell[1] = (float)inst.bottomCellY;
            fsU.bottomTerrainCell[2] = 0.0f;
            fsU.bottomTerrainCell[3] = 0.0f;
            sg_apply_uniforms(0, sg_rng(vsU));
            sg_apply_uniforms(1, sg_rng(fsU));
            sg_draw(0, mesh.indexCount, 1);
        }
    } else if ((debugMode_ == 5) && !cliffInstances_.empty()) {
        for (auto& inst : cliffInstances_) {
            if (!inst.viewer && !inst.meshPath.empty()) {
                inst.viewer = GetOrLoadCliffModel(inst.meshPath);
                if (inst.viewer) {
                    inst.posX = inst.gridX * TILE_SIZE + mapOffsetX_;
                    inst.posZ = inst.gridY * TILE_SIZE + mapOffsetY_;
                    inst.posY = inst.layerBase * TILE_SIZE;
                    inst.scale = 1.0f;
                    inst.yaw = 0.0f;
                }
            }
            if (!inst.viewer) continue;
            Vec3 pos = {inst.posX, inst.posY, inst.posZ};
            inst.viewer->RenderAt(pos, view, proj, inst.scale, 0, inst.yaw, 1.0f, true);
        }
    }

    renderWaterPass();
    renderHighPrioritySplats();

    // Shoreline wave models (Java-style)
    if (waveBuilder_.InstanceCount() > 0) {
        waveBuilder_.SetTime(time);
        waveBuilder_.Render(view, proj);
    }
}

// ---- Setters ----

void TexturedTerrain::SetFog(const float color[3], float start, float end) {
    fogEnabled_ = true;
    fogColor_[0] = color[0]; fogColor_[1] = color[1]; fogColor_[2] = color[2];
    fogStart_ = start; fogEnd_ = end;
}

void TexturedTerrain::SetMapBounds(float minX, float minZ, float maxX, float maxZ) {
    mapBounds_[0] = minX; mapBounds_[1] = minZ;
    mapBounds_[2] = maxX; mapBounds_[3] = maxZ;
}

void TexturedTerrain::SetW3i(const float cb[8]) {
    for (int i = 0; i < 8; ++i) cameraBounds_[i] = cb[i];
}

void TexturedTerrain::SetFogOfWarData(const unsigned char* data, int width, int height) {
    delete[] fogOfWarData_;
    int sz = width * height;
    fogOfWarData_ = new unsigned char[sz];
    std::memcpy(fogOfWarData_, data, sz);

    // Update GPU texture
    if (fogOfWarMap_.id) sg_destroy_image(fogOfWarMap_);
    sg_image_desc d = {};
    d.type = SG_IMAGETYPE_2D;
    d.width = width; d.height = height;
    d.pixel_format = SG_PIXELFORMAT_R8;
    d.num_mipmaps = 1;
    d.data.mip_levels[0].ptr = fogOfWarData_;
    d.data.mip_levels[0].size = (size_t)sz;
    d.usage.immutable = true;
    fogOfWarMap_ = sg_make_image(&d);

    // Recreate view
    if (fogView_.id) sg_destroy_view(fogView_);
    sg_view_desc vd = {}; vd.texture.image = fogOfWarMap_;
    fogView_ = sg_make_view(&vd);

    // Recreate sampler if needed
    if (fogOfWarSampler_.id) sg_destroy_sampler(fogOfWarSampler_);
    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    smp.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    fogOfWarSampler_ = sg_make_sampler(&smp);
}

void TexturedTerrain::SetShadowData(const unsigned char* data, int width, int height) {
    delete[] shadowData_;
    int sz = width * height;
    shadowData_ = new unsigned char[sz];
    std::memcpy(shadowData_, data, sz);
}

void TexturedTerrain::UpdateGroundHeights(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void TexturedTerrain::UpdateGroundTextures(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }

float TexturedTerrain::GetGroundHeight(float worldX, float worldY) const {
    float gx = worldX - centerOffsetX_;
    float gy = worldY - centerOffsetY_;
    float cx = gx / TILE_SIZE, cy = gy / TILE_SIZE;
    int ix = (int)std::floor(cx), iy = (int)std::floor(cy);
    if (ix < 0 || ix >= columns_ - 1 || iy < 0 || iy >= rows_ - 1) return 0;
    float fx = cx - ix, fy = cy - iy;
    int idx = iy * columns_ + ix;
    float h00 = groundCornerHeights_[idx];
    float h10 = groundCornerHeights_[idx + 1];
    float h01 = groundCornerHeights_[idx + columns_];
    float h11 = groundCornerHeights_[idx + columns_ + 1];
    float height = 0.0f;
    if ((fx + fy) < 1.0f) {
        height = h00 + ((h10 - h00) * fx) + ((h01 - h00) * fy);
    } else {
        height = h11 + ((h10 - h11) * (1.0f - fy)) + ((h01 - h11) * (1.0f - fx));
    }
    return height * TILE_SIZE;
}

float TexturedTerrain::GetWaterHeight(float worldX, float worldY) const {
    if (!waterHeights_ || columns_ <= 1 || rows_ <= 1) return waterHeightOffset_ * TILE_SIZE;
    float gx = worldX - centerOffsetX_;
    float gy = worldY - centerOffsetY_;
    float cx = gx / TILE_SIZE, cy = gy / TILE_SIZE;
    int ix = (int)std::floor(cx), iy = (int)std::floor(cy);
    if (ix < 0 || ix >= columns_ - 1 || iy < 0 || iy >= rows_ - 1) return waterHeightOffset_ * TILE_SIZE;
    float fx = cx - ix, fy = cy - iy;
    int idx = iy * columns_ + ix;
    float h00 = waterHeights_[idx];
    float h10 = waterHeights_[idx + 1];
    float h01 = waterHeights_[idx + columns_];
    float h11 = waterHeights_[idx + columns_ + 1];
    float height = 0.0f;
    if ((fx + fy) < 1.0f) {
        height = h00 + ((h10 - h00) * fx) + ((h01 - h00) * fy);
    } else {
        height = h11 + ((h10 - h11) * (1.0f - fy)) + ((h01 - h11) * (1.0f - fx));
    }
    // Ref Terrain.getWaterHeight: (height + waterHeightOffset) * 128.
    return (height + waterHeightOffset_) * TILE_SIZE;
}
bool TexturedTerrain::InPlayableArea(float worldX, float worldY) const {
    (void)worldX; (void)worldY; return true;
}

void TexturedTerrain::AddUberSplat(const std::string& path, float x, float y, float scale, bool highPriority) {
    AddUberSplatRect(path,
                     x - scale * TILE_SIZE,
                     y - scale * TILE_SIZE,
                     x + scale * TILE_SIZE,
                     y + scale * TILE_SIZE,
                     0.0f, highPriority, false, 0.0f, false, 0.0f, 0.0f, 0.0f, 0.0f,
                     false, 0.0f, 0.0f, 0.0f, 0.0f);
}

SplatModel* TexturedTerrain::GetOrCreateUberSplatModel(const std::string& path, bool highPriority, bool terrainColorMask) {
    return GetOrCreateUberSplatModelForKey(path, path, highPriority, terrainColorMask);
}

SplatModel* TexturedTerrain::GetOrCreateUberSplatModelForKey(const std::string& cacheKey,
                                                             const std::string& texturePath,
                                                             bool highPriority,
                                                             bool terrainColorMask) {
    if (cacheKey.empty() || texturePath.empty()) {
        return nullptr;
    }

    const std::string resolvedPath = ResolveUberSplatPath(texturePath);
    if (resolvedPath.empty()) {
        buildStats_.uberSplatMisses++;
        return nullptr;
    }

    auto it = uberSplatModels_.find(cacheKey);
    if (it != uberSplatModels_.end()) {
        it->second->SetHighPriority(highPriority);
        it->second->SetUseTerrainColorMask(terrainColorMask);
        it->second->SetNoDepthTest(false);
        return it->second;
    }

    sg_image tex = {0};
    const std::filesystem::path fsPath(resolvedPath);
    if (std::filesystem::exists(fsPath)) {
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(fsPath.string().c_str(), &width, &height, &channels, 4);
        if (pixels) {
            tex = MakeTextureRGBA8(width, height, pixels, (size_t)width * height * 4);
            stbi_image_free(pixels);
        }
    } else if (mpq_) {
        std::vector<uint8_t> fileData;
        if (!mpq_->ReadFile(resolvedPath, fileData)) {
            const size_t lastDot = resolvedPath.find_last_of('.');
            if (lastDot != std::string::npos) {
                const std::string base = resolvedPath.substr(0, lastDot);
                mpq_->ReadFile(base + ".blp", fileData) || mpq_->ReadFile(base + ".tga", fileData);
            }
        }
        if (!fileData.empty()) {
            BLPImage blp;
            if (DecodeBLP(fileData.data(), fileData.size(), blp)) {
                tex = MakeTextureRGBA8(blp.width, blp.height, blp.pixels.data(), blp.pixels.size());
            } else {
                int width = 0;
                int height = 0;
                int channels = 0;
                unsigned char* pixels = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &width, &height, &channels, 4);
                if (pixels) {
                    tex = MakeTextureRGBA8(width, height, pixels, (size_t)width * height * 4);
                    stbi_image_free(pixels);
                }
            }
        }
    }

    if (!tex.id) {
        buildStats_.uberSplatMisses++;
        return nullptr;
    }

    auto* splat = new SplatModel();
    if (!splat->LoadTexture(resolvedPath, tex)) {
        delete splat;
        buildStats_.uberSplatMisses++;
        return nullptr;
    }

    splat->SetHighPriority(highPriority);
    splat->SetUseTerrainColorMask(terrainColorMask);
    splat->SetNoDepthTest(false);
    uberSplatModels_[cacheKey] = splat;
    uberSplatModelsList_.push_back(splat);
    buildStats_.uberSplatLoads = (int)uberSplatModels_.size();
    return splat;
}

void TexturedTerrain::AddUberSplatRect(const std::string& path, float x0, float y0, float x1, float y1,
                                       float zOffset, bool highPriority,
                                       bool absoluteHeight, float absoluteHeightValue,
                                       bool clampHeightToRect,
                                       float clampMinTileX, float clampMinTileY,
                                       float clampMaxTileX, float clampMaxTileY,
                                       bool useCornerHeightOverride,
                                       float cornerHeightBL, float cornerHeightBR,
                                       float cornerHeightTL, float cornerHeightTR) {
    if (path.empty()) return;

    const bool isCliffEdgeSplat = (path == "cliff_top" || path == "cliff_bottom");
    if (isCliffEdgeSplat) {
        return;
    }
    const bool terrainColorMask = isCliffEdgeSplat;
    auto* splat = GetOrCreateUberSplatModel(path, highPriority, terrainColorMask);
    if (!splat) {
        return;
    }

    splat->AddSplatRect(x0, y0, x1, y1, zOffset, absoluteHeight, absoluteHeightValue,
                        clampHeightToRect, clampMinTileX, clampMinTileY, clampMaxTileX, clampMaxTileY,
                        useCornerHeightOverride, cornerHeightBL, cornerHeightBR, cornerHeightTL, cornerHeightTR);
}

void TexturedTerrain::AddUberSplatMesh(const std::string& path, const std::vector<SplatModel::MeshVertex>& vertices,
                                       const std::vector<uint32_t>& indices, bool highPriority) {
    if (path.empty() || vertices.empty() || indices.empty()) {
        return;
    }

    const bool isCliffEdgeSplat = (path == "cliff_top" || path == "cliff_bottom");
    if (isCliffEdgeSplat) {
        return;
    }
    const bool terrainColorMask = isCliffEdgeSplat;
    auto* splat = GetOrCreateUberSplatModel(path, highPriority, terrainColorMask);
    if (!splat) {
        return;
    }

    splat->AddMesh(vertices, indices);
}

void TexturedTerrain::CreateUnitShadow(float x, float y, const std::string& shadowFile) {
    if (overlayState_.Width() > 0 && overlayState_.Height() > 0) {
        const int gx = static_cast<int>((x - mapOffsetX_) / 128.0f);
        const int gy = static_cast<int>((y - mapOffsetY_) / 128.0f);
        overlayState_.AddShadowFootprint({nextShadowFootprintId_++, gx, gy, 2, 2, 180});
    }
    if (!mpq_ || shadowFile.empty()) return;

    static std::unordered_map<std::string, sg_image> shadowTexCache;

    sg_image tex = {0};
    auto it = shadowTexCache.find(shadowFile);
    if (it != shadowTexCache.end()) {
        tex = it->second;
    } else {
        std::string path = "ReplaceableTextures\\Shadows\\" + shadowFile + ".blp";
        std::vector<uint8_t> fileData;
        if (!mpq_->ReadFile(path, fileData)) return;

        BLPImage blp;
        if (!DecodeBLP(fileData.data(), fileData.size(), blp)) return;

        sg_image_desc d = {};
        d.type = SG_IMAGETYPE_2D;
        d.width = blp.width; d.height = blp.height;
        d.pixel_format = SG_PIXELFORMAT_RGBA8;
        d.num_mipmaps = 1;
        d.data.mip_levels[0].ptr = blp.pixels.data();
        d.data.mip_levels[0].size = blp.pixels.size();
        tex = sg_make_image(&d);
        if (!tex.id) return;
        shadowTexCache[shadowFile] = tex;
    }

    // Create or reuse shadow splat model
    std::string key = "shadow_" + shadowFile;
    auto it2 = uberSplatModels_.find(key);
    if (it2 != uberSplatModels_.end()) {
        it2->second->AddSplat(x, y, 1.0f, 0.0f);
        return;
    }

    auto* splat = new SplatModel();
    splat->LoadTexture(key, tex);
    splat->SetHighPriority(false);
    splat->AddSplat(x, y, 1.0f, 0.0f);
    uberSplatModels_[key] = splat;
    uberSplatModelsList_.push_back(splat);
}

void TexturedTerrain::ClearRuntimeSplats() {
    for (const auto& key : runtimeUberSplatKeys_) {
        auto it = uberSplatModels_.find(key);
        if (it != uberSplatModels_.end() && it->second) {
            it->second->ClearGeometry();
        }
    }
}

void TexturedTerrain::AddRuntimeSplatRect(const std::string& key, const std::string& texturePath,
                                          float x0, float y0, float x1, float y1, float zOffset,
                                          bool highPriority, bool noDepthTest,
                                          bool absoluteHeight, float absoluteHeightValue,
                                          const Vec4& tint,
                                          const Vec4& uvRect,
                                          SplatModel::BlendMode blendMode,
                                          bool unshaded) {
    if (key.empty() || texturePath.empty()) {
        return;
    }

    const std::string runtimeKey = "__runtime__:" + key;
    runtimeUberSplatKeys_.insert(runtimeKey);
    auto* splat = GetOrCreateUberSplatModelForKey(runtimeKey, texturePath, highPriority, false);
    if (!splat) {
        return;
    }

    splat->SetHighPriority(highPriority);
    splat->SetNoDepthTest(noDepthTest);
    splat->SetBlendMode(blendMode);
    splat->SetUnshaded(unshaded);
    splat->AddSplatRect(x0, y0, x1, y1, zOffset,
                        absoluteHeight, absoluteHeightValue,
                        false, 0.0f, 0.0f, 0.0f, 0.0f,
                        false, 0.0f, 0.0f, 0.0f, 0.0f,
                        tint,
                        uvRect);
}

void TexturedTerrain::AddRuntimeSplatMesh(const std::string& key, const std::string& texturePath,
                                          const std::vector<SplatModel::MeshVertex>& vertices,
                                          const std::vector<uint32_t>& indices,
                                          bool highPriority, bool noDepthTest,
                                          SplatModel::BlendMode blendMode,
                                          bool unshaded) {
    if (key.empty() || texturePath.empty() || vertices.empty() || indices.empty()) {
        return;
    }

    const std::string runtimeKey = "__runtime__:" + key;
    runtimeUberSplatKeys_.insert(runtimeKey);
    auto* splat = GetOrCreateUberSplatModelForKey(runtimeKey, texturePath, highPriority, false);
    if (!splat) {
        return;
    }

    splat->SetHighPriority(highPriority);
    splat->SetNoDepthTest(noDepthTest);
    splat->SetBlendMode(blendMode);
    splat->SetUnshaded(unshaded);
    splat->AddMesh(vertices, indices);
}

MdxViewer* TexturedTerrain::GetOrLoadCliffModel(const std::string& path) {
    auto it = cliffViewerCache_.find(path);
    if (it != cliffViewerCache_.end()) return it->second;

    if (!mpq_) return nullptr;

    std::vector<uint8_t> data;
    if (!mpq_->ReadFile(path, data)) {
        std::cerr << "Cliff model not found in MPQ: " << path << std::endl;
        return nullptr;
    }

    MdxModel model;
    if (!MdxLoad(data.data(), data.size(), model)) {
        std::cerr << "Failed to parse MDX: " << path << std::endl;
        return nullptr;
    }

    auto* viewer = new MdxViewer();
    if (!viewer->Load(model, mpq_, true)) {
        std::cerr << "Failed to load cliff model: " << path << std::endl;
        delete viewer;
        return nullptr;
    }

    cliffViewerCache_[path] = viewer;
    buildStats_.cliffModelHits++;
    std::cout << "Cliff model loaded: " << path << std::endl;
    return viewer;
}
void TexturedTerrain::BuildCliffsFromMdx(const W3EMap& map) {
    // Clean up existing cliff instances
    cliffInstances_.clear();
    buildStats_.cliffModelHits = 0;
    buildStats_.cliffModelMisses = 0;

    const int tileCount = (columns_ - 1) * (rows_ - 1);
    for (int i = 0; i < tileCount; ++i) {
        groundTextureList_[i * 4 + 3] = (short)(groundTextureList_[i * 4 + 3] & 0x7FFF);
    }

    if (!tileset_ || !mpq_) return;

    // Helper: get ramp letter (L=low, H=high, X=extra high, A-D=normal)
    auto getRampLetter = [](int heightOffset, bool isRamp) -> char {
        if (isRamp) {
            switch (heightOffset) {
                case 0: return 'L';
                case 1: return 'H';
                case 2: return 'X';
                default: return 'A' + heightOffset;
            }
        }
        return (char)('A' + heightOffset);
    };

    int w = map.tileWidth, h = map.tileHeight;

    for (int i = 0; i < w - 1; i++) {
        for (int j = 0; j < h - 1; j++) {
            const int idx = j * w + i;
            if (!cornerExtra_[idx].cliff || cornerExtra_[idx].hideCliff) continue;

            const int bl = j * w + i;
            const int br = j * w + (i + 1);
            const int tl = (j + 1) * w + i;
            const int tr = (j + 1) * w + (i + 1);

            const int hBl = map.tiles[bl].LayerHeight();
            const int hBr = map.tiles[br].LayerHeight();
            const int hTl = map.tiles[tl].LayerHeight();
            const int hTr = map.tiles[tr].LayerHeight();
            const int base = std::min({hBl, hBr, hTl, hTr});
            const bool facingDown = (hTl >= hBl) && (hTr >= hBr);
            const bool facingLeft = (hBr >= hBl) && (hTr >= hTl);
            const int leftAvg = hBl + hTl;
            const int rightAvg = hBr + hTr;
            const int bottomAvg = hBl + hBr;
            const int topAvg = hTl + hTr;
            int bottomDirX = 0;
            int bottomDirY = 0;
            const int deltaX = rightAvg - leftAvg;
            const int deltaY = topAvg - bottomAvg;
            if (std::abs(deltaX) >= std::abs(deltaY)) {
                if (deltaX != 0) {
                    bottomDirX = (deltaX > 0) ? -1 : 1;
                }
            }
            if (std::abs(deltaY) > std::abs(deltaX)) {
                if (deltaY != 0) {
                    bottomDirY = (deltaY > 0) ? -1 : 1;
                }
            }

            int cliffTex = cornerExtra_[idx].cliffTexture;
            if (cliffTex == 15) {
                bool foundTexture = false;
                cliffTex = cornerExtra_[tl].cliffTexture;
                if (cliffTex != 15) {
                    foundTexture = true;
                } else {
                    cliffTex = cornerExtra_[tr].cliffTexture;
                    if (cliffTex != 15) {
                        foundTexture = true;
                    } else {
                        cliffTex = cornerExtra_[br].cliffTexture;
                        if (cliffTex != 15) {
                            foundTexture = true;
                        } else {
                            cliffTex = 1;
                        }
                    }
                }
                if (foundTexture) {
                    cornerExtra_[idx].cliffTexture = cliffTex;
                }
            }

            const int texIdx = tileset_->CliffPaletteToTexIndex(cliffTex);
            if (texIdx < 0 || texIdx >= tileset_->CliffTextureCount()) continue;
            const auto& cliffInfo = tileset_->GetCliffTile(texIdx);
            if (cliffInfo.cliffModelDir.empty()) continue;

            const bool blRamp = map.tiles[bl].IsRamp();
            const bool brRamp = map.tiles[br].IsRamp();
            const bool tlRamp = map.tiles[tl].IsRamp();
            const bool trRamp = map.tiles[tr].IsRamp();
            const bool verticalRamp = (blRamp != brRamp) && (tlRamp != trRamp);
            const bool horizontalRamp = (blRamp != tlRamp) && (brRamp != trRamp);

            const bool rampWithinFacingBounds =
                !(facingDown && (j == 0)) &&
                !(!facingDown && (j >= (h - 2))) &&
                !(facingLeft && (i == 0)) &&
                !(!facingLeft && (i >= (w - 2)));

            if ((verticalRamp || horizontalRamp) && !cliffInfo.rampModelDir.empty() && rampWithinFacingBounds) {
                const bool rampBlockedByCliff =
                    (verticalRamp && cornerExtra_[i + (j + (facingDown ? -1 : 1)) * w].cliff) ||
                    (horizontalRamp && cornerExtra_[(i + (facingLeft ? -1 : 1)) + j * w].cliff);

                const int tlH = hTl - base;
                const int trH = hTr - base;
                const int brH = hBr - base;
                const int blH = hBl - base;
                const bool invalidRamp = rampBlockedByCliff || (tlH > 1) || (trH > 1) || (brH > 1) || (blH > 1);

                if (!invalidRamp) {
                    std::string rampTag;
                    rampTag += getRampLetter(tlH, tlRamp);
                    rampTag += getRampLetter(trH, trRamp);
                    rampTag += getRampLetter(brH, brRamp);
                    rampTag += getRampLetter(blH, blRamp);

                    const std::string rampPath =
                        "Doodads\\Terrain\\" + cliffInfo.rampModelDir + "\\" + cliffInfo.rampModelDir + rampTag + "0.mdx";
                    const int rampCellX = i + ((horizontalRamp ? 1 : 0) * (facingLeft ? -1 : 0));
                    const int rampCellY = j - ((verticalRamp ? 1 : 0) * (facingDown ? 1 : 0));
                    if (rampCellX < 0 || rampCellY < 0 || rampCellX >= (w - 1) || rampCellY >= (h - 1)) {
                        continue;
                    }

                    for (auto it = cliffInstances_.begin(); it != cliffInstances_.end(); ++it) {
                        if ((it->gridX == rampCellX) && (it->gridY == rampCellY)) {
                            cliffInstances_.erase(it);
                            break;
                        }
                    }

                    CliffInstance inst = {};
                    inst.viewer = nullptr;
                    inst.cliffTexture = cliffTex;
                    inst.variation = 0;
                    inst.scale = 1.0f;
                    inst.yaw = 0.0f;
                    inst.gridX = rampCellX;
                    inst.gridY = rampCellY;
                    inst.layerBase = (float)(base - 2);
                    inst.topCellX = rampCellX;
                    inst.topCellY = rampCellY;
                    inst.bottomCellX = rampCellX;
                    inst.bottomCellY = rampCellY;
                    inst.meshPath = rampPath;
                    cliffInstances_.push_back(inst);

                    cornerExtra_[bl].romp = true;
                    cornerExtra_[br].romp = true;
                    cornerExtra_[tl].romp = true;
                    cornerExtra_[tr].romp = true;
                    cornerExtra_[bl].cliffTexture = cliffTex;
                    cornerExtra_[br].cliffTexture = cliffTex;
                    cornerExtra_[tl].cliffTexture = cliffTex;
                    cornerExtra_[tr].cliffTexture = cliffTex;

                    const int extraCornerX = i + ((facingLeft ? -1 : 1) * (horizontalRamp ? 1 : 0));
                    const int extraCornerY = j + ((facingDown ? -1 : 1) * (verticalRamp ? 1 : 0));
                    if ((extraCornerX >= 0) && (extraCornerY >= 0) && (extraCornerX < w) && (extraCornerY < h)) {
                        cornerExtra_[extraCornerY * w + extraCornerX].cliffTexture = cliffTex;
                        cornerExtra_[extraCornerY * w + extraCornerX].romp = true;
                    }
                    continue;
                }
            }

            if (isCornerRampEntrance(map, i, j)) continue;

            std::string tag;
            tag += (char)(('A' + hTl) - base);
            tag += (char)(('A' + hTr) - base);
            tag += (char)(('A' + hBr) - base);
            tag += (char)(('A' + hBl) - base);
            if (tag == "AAAA") continue;

            const int variation = map.tiles[bl].CliffVariation();
            const int clampedVariation = Variations::GetCliffVariation(cliffInfo.cliffModelDir, tag, variation);
            const std::string fullPath =
                "Doodads\\Terrain\\" + cliffInfo.cliffModelDir + "\\" + cliffInfo.cliffModelDir + tag +
                std::to_string(clampedVariation) + ".mdx";

            CliffInstance inst = {};
            inst.viewer = nullptr;
            inst.cliffTexture = cliffTex;
            inst.variation = clampedVariation;
            inst.scale = 1.0f;
            inst.yaw = 0.0f;
            inst.gridX = i;
            inst.gridY = j;
            inst.layerBase = (float)(base - 2);
            inst.topCellX = std::clamp(i - bottomDirX, 0, w - 2);
            inst.topCellY = std::clamp(j - bottomDirY, 0, h - 2);
            inst.bottomCellX = i;
            inst.bottomCellY = j;
            inst.meshPath = fullPath;
            cliffInstances_.push_back(inst);
        }
    }

    int tileW = columns_ - 1, tileH = rows_ - 1;
    for (int y = 0; y < tileH; ++y) {
        for (int x = 0; x < tileW; ++x) {
            int idx = y * tileW + x;
            getTextureVariations(map, x, y, groundTextureList_, idx * 4);
        }
    }

    for (const auto& inst : cliffInstances_) {
        if (inst.gridX < 0 || inst.gridY < 0 || inst.gridX >= (w - 1) || inst.gridY >= (h - 1)) {
            continue;
        }
        const int tileIndex = ((inst.gridY * (w - 1)) + inst.gridX) * 4 + 3;
        groundTextureList_[tileIndex] = (short)(groundTextureList_[tileIndex] | 0x8000);
    }

    buildStats_.cliffMeshesBuilt = (int)cliffInstances_.size();
    std::cout << "Cliff instances: " << cliffInstances_.size()
              << " (models loaded: " << buildStats_.cliffModelHits
              << ", missed: " << buildStats_.cliffModelMisses << ")" << std::endl;
}
bool TexturedTerrain::isCornerRampEntrance(const W3EMap& map, int x, int y) const {
    if (x < 0 || y < 0 || x >= map.tileWidth - 1 || y >= map.tileHeight - 1) return false;

    int bl = y * map.tileWidth + x;
    int br = y * map.tileWidth + (x + 1);
    int tl = (y + 1) * map.tileWidth + x;
    int tr = (y + 1) * map.tileWidth + (x + 1);

    bool allRamp = map.tiles[bl].IsRamp() && map.tiles[br].IsRamp()
                && map.tiles[tl].IsRamp() && map.tiles[tr].IsRamp();
    if (!allRamp) return false;

    // Diagonal height check: skip if same diagonal pair heights
    return !((map.tiles[bl].LayerHeight() == map.tiles[tr].LayerHeight())
          && (map.tiles[tl].LayerHeight() == map.tiles[br].LayerHeight()));
}
void TexturedTerrain::BuildCliffEdgeSplats() {
    const int cellWidth = columns_ - 1;
    const int cellHeight = rows_ - 1;
    if (cellWidth <= 0 || cellHeight <= 0 || !groundCornerHeights_) {
        return;
    }

    constexpr float kTopZOffset = 2.5f;
    constexpr float kBottomZOffset = 1.0f;

    auto inBounds = [&](int cellX, int cellY) {
        return cellX >= 0 && cellY >= 0 && cellX < cellWidth && cellY < cellHeight;
    };
    auto cellKey = [&](int cellX, int cellY) {
        return cellY * cellWidth + cellX;
    };
    auto cornerKey = [](int cornerX, int cornerY) -> uint64_t {
        return (uint64_t)(uint32_t)cornerX | ((uint64_t)(uint32_t)cornerY << 32);
    };
    auto fetchCellHeights = [&](int cellX, int cellY) {
        const int bl = cellY * columns_ + cellX;
        const int br = bl + 1;
        const int tl = bl + columns_;
        const int tr = tl + 1;
        return std::array<float, 4>{
            groundCornerHeights_[bl],
            groundCornerHeights_[br],
            groundCornerHeights_[tl],
            groundCornerHeights_[tr],
        };
    };
    auto computeDesiredHeights = [&](const std::array<float, 4>& baseHeights, uint8_t sideMask, bool preferMaxHeights) {
        std::array<float, 4> desiredHeights = baseHeights;
        for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
            const bool onLeft = (cornerIndex == 0 || cornerIndex == 2);
            const bool onRight = (cornerIndex == 1 || cornerIndex == 3);
            const bool onBottom = (cornerIndex == 0 || cornerIndex == 1);
            const bool onTop = (cornerIndex == 2 || cornerIndex == 3);
            const bool touchesBoundary =
                ((sideMask & CliffBoundarySide_Left) && onLeft) ||
                ((sideMask & CliffBoundarySide_Right) && onRight) ||
                ((sideMask & CliffBoundarySide_Bottom) && onBottom) ||
                ((sideMask & CliffBoundarySide_Top) && onTop);
            if (!touchesBoundary) {
                continue;
            }

            float resolvedHeight = baseHeights[cornerIndex];
            auto mergeCandidate = [&](int candidateCorner) {
                const float candidateHeight = baseHeights[candidateCorner];
                resolvedHeight = preferMaxHeights
                    ? std::max(resolvedHeight, candidateHeight)
                    : std::min(resolvedHeight, candidateHeight);
            };

            if ((sideMask & CliffBoundarySide_Left) && onLeft) {
                mergeCandidate(cornerIndex + 1);
            }
            if ((sideMask & CliffBoundarySide_Right) && onRight) {
                mergeCandidate(cornerIndex - 1);
            }
            if ((sideMask & CliffBoundarySide_Bottom) && onBottom) {
                mergeCandidate(cornerIndex + 2);
            }
            if ((sideMask & CliffBoundarySide_Top) && onTop) {
                mergeCandidate(cornerIndex - 2);
            }
            if (((sideMask & CliffBoundarySide_Left) && onLeft) ||
                ((sideMask & CliffBoundarySide_Right) && onRight)) {
                if (((sideMask & CliffBoundarySide_Bottom) && onBottom) ||
                    ((sideMask & CliffBoundarySide_Top) && onTop)) {
                    const int diagonalCorner = (cornerIndex == 0) ? 3 :
                                               (cornerIndex == 1) ? 2 :
                                               (cornerIndex == 2) ? 1 : 0;
                    mergeCandidate(diagonalCorner);
                }
            }

            desiredHeights[cornerIndex] = resolvedHeight;
        }
        return desiredHeights;
    };
    auto cornerAcrossValue = [](int cornerIndex, uint8_t sideMask) {
        const bool onLeft = (cornerIndex == 0 || cornerIndex == 2);
        const bool onRight = (cornerIndex == 1 || cornerIndex == 3);
        const bool onBottom = (cornerIndex == 0 || cornerIndex == 1);
        const bool onTop = (cornerIndex == 2 || cornerIndex == 3);
        if ((onLeft && (sideMask & CliffBoundarySide_Left)) ||
            (onRight && (sideMask & CliffBoundarySide_Right)) ||
            (onBottom && (sideMask & CliffBoundarySide_Bottom)) ||
            (onTop && (sideMask & CliffBoundarySide_Top))) {
            return 0.0f;
        }
        return 1.0f;
    };

    std::vector<CliffBoundaryEdge> bottomEdges;
    std::vector<CliffBoundaryEdge> topEdges;
    bottomEdges.reserve(cliffInstanceData_.size());
    topEdges.reserve(cliffInstanceData_.size());

    for (const auto& inst : cliffInstanceData_) {
        const int bottomCellX = inst.bottomCellX;
        const int bottomCellY = inst.bottomCellY;
        const int topCellX = inst.topCellX;
        const int topCellY = inst.topCellY;
        const int deltaX = topCellX - bottomCellX;
        const int deltaY = topCellY - bottomCellY;
        if ((deltaX == 0 && deltaY == 0) || (std::abs(deltaX) + std::abs(deltaY) != 1)) {
            continue;
        }

        CliffBoundarySide bottomSide = CliffBoundarySide_None;
        if (deltaX > 0) {
            bottomSide = CliffBoundarySide_Right;
        } else if (deltaX < 0) {
            bottomSide = CliffBoundarySide_Left;
        } else if (deltaY > 0) {
            bottomSide = CliffBoundarySide_Top;
        } else if (deltaY < 0) {
            bottomSide = CliffBoundarySide_Bottom;
        }
        const CliffBoundarySide topSide = OppositeBoundarySide(bottomSide);

        if (bottomSide != CliffBoundarySide_None && inBounds(bottomCellX, bottomCellY)) {
            bottomEdges.push_back({bottomCellX, bottomCellY, bottomSide});
        }
        if (topSide != CliffBoundarySide_None && inBounds(topCellX, topCellY)) {
            topEdges.push_back({topCellX, topCellY, topSide});
        }
    }

    std::vector<uint8_t> bottomMasks((size_t)cellWidth * cellHeight, 0);
    std::vector<uint8_t> topMasks((size_t)cellWidth * cellHeight, 0);
    std::vector<uint8_t> bottomActive((size_t)cellWidth * cellHeight, 0);
    std::vector<uint8_t> topActive((size_t)cellWidth * cellHeight, 0);
    for (const auto& edge : bottomEdges) {
        const int key = cellKey(edge.cellX, edge.cellY);
        bottomMasks[key] |= (uint8_t)edge.side;
        bottomActive[key] = 1;
    }
    for (const auto& edge : topEdges) {
        const int key = cellKey(edge.cellX, edge.cellY);
        topMasks[key] |= (uint8_t)edge.side;
        topActive[key] = 1;
    }

    auto collectRegions = [&](const std::vector<uint8_t>& activeCells) {
        std::vector<CliffBoundaryRegion> regions;
        std::vector<uint8_t> visited(activeCells.size(), 0);
        const int neighborOffsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

        for (int cellY = 0; cellY < cellHeight; ++cellY) {
            for (int cellX = 0; cellX < cellWidth; ++cellX) {
                const int startKey = cellKey(cellX, cellY);
                if (!activeCells[startKey] || visited[startKey]) {
                    continue;
                }

                CliffBoundaryRegion region;
                std::queue<int> pending;
                pending.push(startKey);
                visited[startKey] = 1;

                while (!pending.empty()) {
                    const int current = pending.front();
                    pending.pop();
                    region.cells.push_back(current);

                    const int currentX = current % cellWidth;
                    const int currentY = current / cellWidth;
                    for (const auto& offset : neighborOffsets) {
                        const int nextX = currentX + offset[0];
                        const int nextY = currentY + offset[1];
                        if (!inBounds(nextX, nextY)) {
                            continue;
                        }
                        const int nextKey = cellKey(nextX, nextY);
                        if (!activeCells[nextKey] || visited[nextKey]) {
                            continue;
                        }
                        visited[nextKey] = 1;
                        pending.push(nextKey);
                    }
                }

                if (!region.cells.empty()) {
                    regions.push_back(std::move(region));
                }
            }
        }

        return regions;
    };

    auto buildRegionMesh = [&](const std::string& path, const CliffBoundaryRegion& region,
                               const std::vector<uint8_t>& sideMasks, bool highPriority,
                               bool preferMaxHeights, float zOffset) {
        if (region.cells.empty()) {
            return;
        }

        struct RegionVertex {
            float worldX = 0.0f;
            float worldY = 0.0f;
            float height = 0.0f;
            float across = 1.0f;
            bool initialized = false;
        };

        std::unordered_map<uint64_t, uint32_t> vertexLookup;
        std::vector<RegionVertex> regionVertices;
        std::vector<uint32_t> regionIndices;
        regionVertices.reserve((size_t)region.cells.size() * 4);
        regionIndices.reserve((size_t)region.cells.size() * 6);

        bool hasVerticalBoundary = false;
        bool hasHorizontalBoundary = false;

        auto getOrCreateCornerVertex = [&](int cornerX, int cornerY) {
            const uint64_t key = cornerKey(cornerX, cornerY);
            const auto found = vertexLookup.find(key);
            if (found != vertexLookup.end()) {
                return found->second;
            }

            RegionVertex vertex;
            vertex.worldX = centerOffsetX_ + cornerX * TILE_SIZE;
            vertex.worldY = centerOffsetY_ + cornerY * TILE_SIZE;
            const uint32_t newIndex = (uint32_t)regionVertices.size();
            regionVertices.push_back(vertex);
            vertexLookup.emplace(key, newIndex);
            return newIndex;
        };

        for (int cell : region.cells) {
            const int cellX = cell % cellWidth;
            const int cellY = cell / cellWidth;
            const uint8_t sideMask = sideMasks[cell];
            if (sideMask == 0) {
                continue;
            }

            hasVerticalBoundary = hasVerticalBoundary ||
                ((sideMask & CliffBoundarySide_Left) != 0) ||
                ((sideMask & CliffBoundarySide_Right) != 0);
            hasHorizontalBoundary = hasHorizontalBoundary ||
                ((sideMask & CliffBoundarySide_Bottom) != 0) ||
                ((sideMask & CliffBoundarySide_Top) != 0);

            const std::array<float, 4> baseHeights = fetchCellHeights(cellX, cellY);
            const std::array<float, 4> desiredHeights =
                computeDesiredHeights(baseHeights, sideMask, preferMaxHeights);

            const int cornerCoords[4][2] = {
                {cellX, cellY},
                {cellX + 1, cellY},
                {cellX, cellY + 1},
                {cellX + 1, cellY + 1},
            };

            uint32_t cornerIndices[4] = {};
            for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
                const int cornerX = cornerCoords[cornerIndex][0];
                const int cornerY = cornerCoords[cornerIndex][1];
                const uint32_t vertexIndex = getOrCreateCornerVertex(cornerX, cornerY);
                RegionVertex& vertex = regionVertices[vertexIndex];
                if (!vertex.initialized) {
                    vertex.height = desiredHeights[cornerIndex];
                    vertex.across = cornerAcrossValue(cornerIndex, sideMask);
                    vertex.initialized = true;
                } else {
                    vertex.height = preferMaxHeights
                        ? std::max(vertex.height, desiredHeights[cornerIndex])
                        : std::min(vertex.height, desiredHeights[cornerIndex]);
                    vertex.across = std::min(vertex.across, cornerAcrossValue(cornerIndex, sideMask));
                }
                cornerIndices[cornerIndex] = vertexIndex;
            }

            regionIndices.push_back(cornerIndices[0]);
            regionIndices.push_back(cornerIndices[1]);
            regionIndices.push_back(cornerIndices[2]);
            regionIndices.push_back(cornerIndices[1]);
            regionIndices.push_back(cornerIndices[3]);
            regionIndices.push_back(cornerIndices[2]);
        }

        if (regionVertices.empty() || regionIndices.empty()) {
            return;
        }

        float minAlong = std::numeric_limits<float>::max();
        float maxAlong = std::numeric_limits<float>::lowest();
        std::vector<float> alongValues(regionVertices.size(), 0.0f);
        for (size_t i = 0; i < regionVertices.size(); ++i) {
            const RegionVertex& vertex = regionVertices[i];
            float along = 0.0f;
            if (hasVerticalBoundary && !hasHorizontalBoundary) {
                along = vertex.worldY;
            } else if (hasHorizontalBoundary && !hasVerticalBoundary) {
                along = vertex.worldX;
            } else {
                along = vertex.worldX + vertex.worldY;
            }
            alongValues[i] = along;
            minAlong = std::min(minAlong, along);
            maxAlong = std::max(maxAlong, along);
        }

        const float alongSpan = std::max(maxAlong - minAlong, TILE_SIZE);
        std::vector<SplatModel::MeshVertex> meshVertices(regionVertices.size());
        for (size_t i = 0; i < regionVertices.size(); ++i) {
            const RegionVertex& src = regionVertices[i];
            SplatModel::MeshVertex dst;
            dst.worldX = src.worldX;
            dst.worldY = src.worldY;
            dst.u = (alongValues[i] - minAlong) / alongSpan;
            dst.v = std::clamp(src.across, 0.0f, 1.0f);
            dst.zOffset = zOffset;
            dst.absoluteHeight = true;
            dst.absoluteHeightValue = src.height * TILE_SIZE;
            meshVertices[i] = dst;
        }

        AddUberSplatMesh(path, meshVertices, regionIndices, highPriority);
    };

    const auto bottomRegions = collectRegions(bottomActive);
    for (const auto& region : bottomRegions) {
        buildRegionMesh("cliff_bottom", region, bottomMasks, false, false, kBottomZOffset);
    }

    // Temporarily disable cliff_top so we can isolate bottom splat terrain welding.
    (void)topMasks;
    (void)topActive;
    (void)kTopZOffset;
}

std::string TexturedTerrain::ResolveUberSplatPath(const std::string& path) {
    auto cached = uberSplatPathCache_.find(path);
    if (cached != uberSplatPathCache_.end()) {
        return cached->second;
    }

    std::string resolved = path;
    if (path == "cliff_top" || path == "cliff_bottom") {
        const bool isTop = (path == "cliff_top");
        std::vector<std::string> candidates;
        if (tilesetChar_ == 'N') {
            candidates.push_back(isTop ? "assets/cliff_splats/northrend_top.png"
                                       : "assets/cliff_splats/northrend_bottom.png");
        }
        candidates.push_back(isTop ? "assets/cliff_splats/generic_top.png"
                                   : "assets/cliff_splats/generic_bottom.png");

        resolved.clear();
        for (const auto& candidate : candidates) {
            resolved = FindLocalAssetPath(candidate);
            if (!resolved.empty()) {
                break;
            }
        }
    }

    uberSplatPathCache_[path] = resolved;
    return resolved;
}

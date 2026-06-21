#include "textured_terrain.h"
#include "shader.h"
#include "terrain_shader.h"
#include "renderer.h"
#include "shapes.h"
#include "splat_model.h"
#include "mdx_viewer.h"
#include "variations.h"
#include "asset/TileSet.h"
#include "asset/BLP.h"
#include "asset/stb_image.h"
#include "model/mdx.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>
#include <map>
#include <cmath>

static constexpr float TILE_SIZE = 128.0f;

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

static sg_sampler MakeSampler(bool linear) {
    sg_sampler_desc d = {};
    d.min_filter = linear ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
    d.mag_filter = linear ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
    d.mipmap_filter = linear ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
 
    d.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    d.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
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
            float h = tile.FinalGroundHeight();
            groundHeights_[idx] = h;
            groundCornerHeights_[idx] = h;
            waterHeights_[idx] = tile.DecodeWaterHeight();
            waterExistsData_[idx] = tile.IsWater() ? 255 : 0;
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

    // Corner height texture (R32F, size = corners = columns x rows)
    groundCornerHeight_ = MakeDataTexture(columns_, rows_, SG_PIXELFORMAT_R32F,
                                           groundCornerHeights_, columns_ * rows_ * sizeof(float));
    // Center height (R32F, size = corners = columns x rows)
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
    // Sampler: linear min/mag, forced base level only (max_lod=0) for stable terrain
    sg_sampler_desc bsd = {};
    bsd.min_filter = SG_FILTER_LINEAR; bsd.mag_filter = SG_FILTER_LINEAR; bsd.mipmap_filter = SG_FILTER_LINEAR;
    bsd.max_lod = 0.0f;
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
    nonFilterSampler_ = sg_make_sampler(&nfDesc);

    // Simple instanced shader: reads height + texture index, samples correct texture array
    int texCount = tileset_ ? tileset_->TextureCount() : 0;
    if (texCount < 1) texCount = 1;

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
    uberSplatShader_ = TerrainShaders::CreateUberSplatShader();
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
    for (auto& mesh : cliffMeshes_) {
        if (mesh.vbo.id) sg_destroy_buffer(mesh.vbo);
        if (mesh.ebo.id) sg_destroy_buffer(mesh.ebo);
    }
    cliffMeshes_.clear();

    std::cout << "BuildCliffsFromMdx: starting..." << std::endl;
    BuildCliffsFromMdx(map);
    std::cout << "BuildCliffsFromMdx: done" << std::endl;

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

        const auto& geo = model.geosets[0];
        if (geo.vertices.empty() || geo.indices.empty()) return -1;

        const std::vector<float>* uvSource = nullptr;
        if (!geo.uvSets.empty() && !geo.uvSets[0].empty()) uvSource = &geo.uvSets[0];

        int vc = (int)geo.vertices.size() / 3;
        std::vector<CliffVertexGPU> verts(vc);
        for (int i = 0; i < vc; ++i) {
            verts[i].px = geo.vertices[i * 3 + 0];
            verts[i].py = geo.vertices[i * 3 + 1];
            verts[i].pz = geo.vertices[i * 3 + 2];
            if (uvSource && uvSource->size() >= size_t(i * 2 + 2)) {
                verts[i].tu = (*uvSource)[i * 2 + 0];
                verts[i].tv = (*uvSource)[i * 2 + 1];
            } else {
                verts[i].tu = 0.0f;
                verts[i].tv = 0.0f;
            }
            if (!geo.normals.empty()) {
                verts[i].nx = geo.normals[i * 3 + 0];
                verts[i].ny = geo.normals[i * 3 + 1];
                verts[i].nz = geo.normals[i * 3 + 2];
            } else {
                verts[i].nx = 0.0f;
                verts[i].ny = 0.0f;
                verts[i].nz = 1.0f;
            }
        }

        std::vector<uint32_t> indices(geo.indices.begin(), geo.indices.end());

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
        mesh.textureLayer = std::max(0, tileset_->CliffPaletteToTexIndex(inst.cliffTexture));

        const int index = (int)cliffMeshes_.size();
        cliffMeshes_.push_back(mesh);
        meshLookup[inst.meshPath] = index;
        return index;
    };

    for (const auto& inst : cliffInstances_) {
        if (inst.meshPath.empty()) continue;
        const int meshIndex = loadMesh(inst);
        if (meshIndex < 0) continue;
        CliffInstanceGPU gpuInst;
        gpuInst.gridX = inst.gridX;
        gpuInst.gridY = inst.gridY;
        gpuInst.layerBase = inst.layerBase;
        gpuInst.meshIndex = meshIndex;
        cliffInstanceData_.push_back(gpuInst);
    }

    cliffShader_ = TerrainShaders::CreateCliffShader(tileset_->CliffTextureCount());
    if (!cliffShader_.id) return;

    sg_pipeline_desc pd = {};
    pd.shader = cliffShader_;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.index_type = SG_INDEXTYPE_UINT32;
    pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pd.depth.write_enabled = true;
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.colors[0].blend.enabled = false;
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
    buildStats_ = TerrainBuildStats{};
    RebuildCornerData(map);
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
    buildStats_.cliffModelHits = (int)cliffInstances_.size();
    buildStats_.uberSplatLoads = (int)uberSplatModels_.size();
    buildStats_.shadowLoads = (int)shadowTextureCache_.size();

    std::cout << "TexturedTerrain: built " << columns_ << "x" << rows_
              << " tiles, " << tileset_->TextureCount() << " textures" << std::endl;
    std::cout << "TerrainStats: groundTex=" << buildStats_.groundTexturesLoaded
              << " mapped=" << buildStats_.groundPaletteMappedCount
              << " fallback=" << buildStats_.groundPaletteFallbackCount
              << " cliffHits=" << buildStats_.cliffModelHits
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

    Shapes& shapes = Shapes::Instance();
    shapes.Bind();
    int idxCount = shapes.IndexCount();

    // Ground (instanced, texture-based rendering)
    if (groundPipeline_.id) {
        Shapes& shapes = Shapes::Instance();
        shapes.Bind();
        int idxCount = shapes.IndexCount();
        int numTiles = (columns_ - 1) * (rows_ - 1);

        sg_apply_pipeline(groundPipeline_);
        sg_bindings bind = {};
        bind.vertex_buffers[0] = shapes.GetVertexBuffer();
        bind.index_buffer = shapes.GetIndexBuffer();

        // VS texture bindings (slots 0-3): height_texture, height_cliff_texture, terrain_texture_list, lightTexture
        bind.views[0] = heightTexView_;
        bind.views[1] = cornerHeightView_;
        // VS textures (slots 0-3): height_texture, height_cliff_texture, terrain_texture_list, lightTexture
        bind.views[0] = heightTexView_;       // height_texture (center heights)
        bind.views[1] = cornerHeightView_;     // height_cliff_texture (corner heights)
        bind.views[2] = texDataView_;          // terrain_texture_list (texture indices)
        bind.views[3] = lightView_;            // lightTexture
        // FS textures (slots 4+): sample0..N (full 32-layer arrays), shadowMap, fogOfWarMap
        int texCount = tileset_ ? tileset_->TextureCount() : 0;
        for (int i = 0; i < texCount && i < 17; ++i)
            bind.views[4 + i] = tileTexViews_[i];
        bind.views[4 + texCount] = shadowView_;
        bind.views[5 + texCount] = fogView_;
        // Samplers
        bind.samplers[0] = linearSampler_;
        bind.samplers[1] = nearestSampler_;
        bind.samplers[2] = nonFilterSampler_;  // integer texture → non-filtering
        bind.samplers[3] = lightManager_.GetLightSampler();
        // Trilinear mipmap sampler for ground texture arrays
        for (int i = 0; i < texCount + 2 && i < 19; ++i)
            bind.samplers[4 + i] = linearSampler_;

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
            float showPathing, showLighting, baseTexOnly;
            float fogColor[4], fogParams[4];
        } fsU = {};
        fsU.showPathing = showPathing_ ? 1.0f : 0.0f;
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

    // Water pass (instanced, textured, animated)
    if (waterPipeline_.id && waterShader_.id) {
        // Animate water texture index
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
        // VS textures: water_height(0), ground_height(1), water_exists(2), lightTexture(3)
        bind.views[0] = waterHeightView_;
        bind.views[1] = cornerHeightView_;
        bind.views[2] = waterExistsView_;
        bind.views[3] = lightView_;
        // FS textures: water_textures(4), fogOfWarMap(5)
        if (tileset_ && tileset_->HasWaterTexture())
            bind.views[4] = waterTexArrayView_;
        else
            bind.views[4] = fogView_;
        bind.views[5] = fogView_;
        // Samplers for all 6 slots
        bind.samplers[0] = nearestSampler_;
        bind.samplers[1] = nearestSampler_;
        bind.samplers[2] = nearestSampler_;
        bind.samplers[3] = lightManager_.GetLightSampler();
        bind.samplers[4] = repeatSampler_;
        bind.samplers[5] = nearestSampler_;
        sg_apply_bindings(&bind);

        Mat4 mvp = proj * view;
        // VS uniforms
        auto* wi = tileset_ && tileset_->HasWaterInfo() ? &tileset_->GetWaterInfo() : nullptr;
        struct {
            float MVP[16];
            float cx, cy, wo;
            float lc, lth;
            float scMin[4], scMax[4], dcMin[4], dcMax[4];
        } vsU;
        for (int i = 0; i < 16; ++i) vsU.MVP[i] = mvp.m[i];
        vsU.cx = centerOffsetX_; vsU.cy = centerOffsetY_;
        vsU.wo = waterHeightOffset_;
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

        // FS uniforms: current_texture, mapBounds, u_fogColor, u_fogParams
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
    }

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

        for (const auto& inst : cliffInstanceData_) {
            if (inst.meshIndex < 0 || inst.meshIndex >= (int)cliffMeshes_.size()) continue;
            const auto& mesh = cliffMeshes_[inst.meshIndex];
            if (!mesh.vbo.id || !mesh.ebo.id) continue;

            sg_bindings bind = {};
            bind.vertex_buffers[0] = mesh.vbo;
            bind.index_buffer = mesh.ebo;
            bind.views[0] = heightTexView_;
            bind.views[1] = shadowView_;
            bind.views[2] = lightView_;
            bind.views[3] = tileset_->GetCliffTextureArrayView();
            bind.views[4] = shadowView_;
            bind.views[5] = fogView_;
            bind.samplers[0] = linearSampler_;
            bind.samplers[1] = linearSampler_;
            bind.samplers[2] = lightManager_.GetLightSampler();
            bind.samplers[3] = linearSampler_;
            bind.samplers[4] = linearSampler_;
            bind.samplers[5] = linearSampler_;
            sg_apply_bindings(&bind);

            CliffVSBlock vsU = {};
            for (int i = 0; i < 16; ++i) vsU.MVP[i] = mvp.m[i];
            vsU.centerOffsetX = centerOffsetX_;
            vsU.centerOffsetY = centerOffsetY_;
            vsU.cliffOffset[0] = (float)inst.gridX;
            vsU.cliffOffset[1] = (float)inst.gridY;
            vsU.cliffOffset[2] = inst.layerBase;
            vsU.cliffOffset[3] = (float)mesh.textureLayer;
            vsU.lightCount = (float)lightManager_.GetLightCount();
            vsU.lightTextureHeight = lightManager_.GetLightTextureHeight();
            sg_apply_uniforms(0, sg_rng(vsU));
            sg_apply_uniforms(1, sg_rng(fsU));
            sg_draw(0, mesh.indexCount, 1);
        }
    } else if (!cliffInstances_.empty()) {
        for (auto& inst : cliffInstances_) {
            if (!inst.viewer) continue;
            Vec3 pos = {inst.posX, inst.posY, inst.posZ};
            inst.viewer->RenderAt(pos, view, proj, inst.scale, 0, inst.yaw, 1.0f, true);
        }
    }

    // Uber-splat rendering (building foundations, blood stains, etc.)
    if (uberSplatShader_.id && !uberSplatModelsList_.empty()) {
        Mat4 mvp = proj * view;
        float center[2] = {centerOffsetX_, centerOffsetY_};
        for (auto* splat : uberSplatModelsList_) {
            if (splat && splat->IsHighPriority()) continue; // render low-priority first
            if (splat) splat->Render(uberSplatShader_, mvp, center,
                columns_ - 1, rows_ - 1, groundCornerHeight_,
                shadowMap_, waterHeight_, fogOfWarMap_, waterHeightOffset_);
        }
        for (auto* splat : uberSplatModelsList_) {
            if (splat && !splat->IsHighPriority()) continue; // then high-priority
            if (splat) splat->Render(uberSplatShader_, mvp, center,
                columns_ - 1, rows_ - 1, groundCornerHeight_,
                shadowMap_, waterHeight_, fogOfWarMap_, waterHeightOffset_);
        }
    }

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
    float h00 = groundHeights_[idx];
    float h10 = groundHeights_[idx + 1];
    float h01 = groundHeights_[idx + columns_];
    float h11 = groundHeights_[idx + columns_ + 1];
    float h0 = h00 + (h10 - h00) * fx;
    float h1 = h01 + (h11 - h01) * fx;
    return (h0 + (h1 - h0) * fy) * TILE_SIZE;
}

float TexturedTerrain::GetWaterHeight(float worldX, float worldY) const { (void)worldX; (void)worldY; return 0; }
bool TexturedTerrain::InPlayableArea(float worldX, float worldY) const {
    (void)worldX; (void)worldY; return true;
}

void TexturedTerrain::AddUberSplat(const std::string& path, float x, float y, float scale, bool highPriority) {
    if (!mpq_ || path.empty()) return;

    // Check cache
    auto it = uberSplatModels_.find(path);
    if (it != uberSplatModels_.end()) {
        it->second->AddSplat(x, y, scale, 0.0f);
        return;
    }

    // Load texture from MPQ
    std::vector<uint8_t> fileData;
    std::string texPath = path;
    // Try .blp first, then .tga
    if (!mpq_->ReadFile(texPath, fileData)) {
        if (texPath.size() > 4) {
            std::string base = texPath.substr(0, texPath.size() - 4);
            if (!mpq_->ReadFile(base + ".tga", fileData)) return;
        } else return;
    }

    BLPImage blp;
    sg_image tex = {0};
    if (DecodeBLP(fileData.data(), fileData.size(), blp)) {
        sg_image_desc d = {};
        d.type = SG_IMAGETYPE_2D;
        d.width = blp.width; d.height = blp.height;
        d.pixel_format = SG_PIXELFORMAT_RGBA8;
        d.num_mipmaps = 1;
        d.data.mip_levels[0].ptr = blp.pixels.data();
        d.data.mip_levels[0].size = blp.pixels.size();
        tex = sg_make_image(&d);
    }
    if (!tex.id) return;

    auto* splat = new SplatModel();
    splat->LoadTexture(path, tex);
    splat->SetHighPriority(highPriority);
    splat->AddSplat(x, y, scale, 0.0f);
    uberSplatModels_[path] = splat;
    uberSplatModelsList_.push_back(splat);
}

void TexturedTerrain::CreateUnitShadow(float x, float y, const std::string& shadowFile) {
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
            int idx = j * w + i;
            if (!cornerExtra_[idx].cliff || cornerExtra_[idx].hideCliff) continue;
            if (cornerExtra_[idx].romp) continue; // already handled by ramp

            int bl = j * w + i;
            int br = j * w + (i + 1);
            int tl = (j + 1) * w + i;
            int tr = (j + 1) * w + (i + 1);

            int hBl = map.tiles[bl].LayerHeight();
            int hBr = map.tiles[br].LayerHeight();
            int hTl = map.tiles[tl].LayerHeight();
            int hTr = map.tiles[tr].LayerHeight();

            int base = std::min({hBl, hBr, hTl, hTr});
            const bool facingDown = (hTl >= hBl) && (hTr >= hBr);
            const bool facingLeft = (hBr >= hBl) && (hTr >= hTl);

            // Resolve cliff texture (fallback through corners if cliffTexture == 15)
            int cliffTex = cornerExtra_[idx].cliffTexture;
            if (cliffTex == 15) {
                cliffTex = cornerExtra_[tl].cliffTexture;
                if (cliffTex == 15) cliffTex = cornerExtra_[tr].cliffTexture;
                if (cliffTex == 15) cliffTex = cornerExtra_[br].cliffTexture;
                if (cliffTex == 15) cliffTex = 1;
                cornerExtra_[idx].cliffTexture = cliffTex;
            }

            // Get cliff model directory
            int texIdx = tileset_->CliffPaletteToTexIndex(cliffTex);
            if (texIdx < 0 || texIdx >= tileset_->CliffTextureCount()) continue;
            const auto& cliffInfo = tileset_->GetCliffTile(texIdx);
            std::string modelDir = cliffInfo.cliffModelDir;
            std::string rampDir = cliffInfo.rampModelDir;
            if (modelDir.empty()) continue;

            // Check for ramp
            bool blRamp = map.tiles[bl].IsRamp();
            bool brRamp = map.tiles[br].IsRamp();
            bool tlRamp = map.tiles[tl].IsRamp();
            bool trRamp = map.tiles[tr].IsRamp();

            bool verticalRamp = (blRamp != brRamp) && (tlRamp != trRamp);
            bool horizontalRamp = (blRamp != tlRamp) && (brRamp != trRamp);

            const bool rampWithinFacingBounds =
                !(facingDown && (j == 0)) &&
                !(!facingDown && (j >= (h - 2))) &&
                !(facingLeft && (i == 0)) &&
                !(!facingLeft && (i >= (w - 2)));

            if ((verticalRamp || horizontalRamp) && !rampDir.empty() && rampWithinFacingBounds) {
                int tlH = hTl - base;
                int trH = hTr - base;
                int brH = hBr - base;
                int blH = hBl - base;

                bool rampBlockedByCliff =
                    (verticalRamp && cornerExtra_[(j + (facingDown ? -1 : 1)) * w + i].cliff) ||
                    (horizontalRamp && cornerExtra_[j * w + (i + (facingLeft ? -1 : 1))].cliff);

                if (!rampBlockedByCliff && tlH <= 1 && trH <= 1 && brH <= 1 && blH <= 1) {
                    std::string rampTag;
                    rampTag += getRampLetter(tlH, tlRamp);
                    rampTag += getRampLetter(trH, trRamp);
                    rampTag += getRampLetter(brH, brRamp);
                    rampTag += getRampLetter(blH, blRamp);

                    std::string rampPath = "Doodads\\Terrain\\" + rampDir + "\\" + rampDir + rampTag + "0.mdx";
                    MdxViewer* rampViewer = GetOrLoadCliffModel(rampPath);
                    if (rampViewer) {
                        const int rampCellX = i - ((horizontalRamp && facingLeft) ? 1 : 0);
                        const int rampCellY = j - ((verticalRamp && facingDown) ? 1 : 0);
                        if (rampCellX < 0 || rampCellY < 0 || rampCellX >= w - 1 || rampCellY >= h - 1) {
                            continue;
                        }

                        const int extraCornerX = i + ((horizontalRamp) ? (facingLeft ? -1 : 1) : 0);
                        const int extraCornerY = j + ((verticalRamp) ? (facingDown ? -1 : 1) : 0);

                        if (extraCornerX >= 0 && extraCornerY >= 0 && extraCornerX < w && extraCornerY < h) {
                            cornerExtra_[extraCornerY * w + extraCornerX].romp = true;
                            cornerExtra_[extraCornerY * w + extraCornerX].cliffTexture = cliffTex;
                        }

                        cornerExtra_[bl].romp = true;
                        cornerExtra_[br].romp = true;
                        cornerExtra_[tl].romp = true;
                        cornerExtra_[tr].romp = true;
                        cornerExtra_[bl].cliffTexture = cliffTex;
                        cornerExtra_[br].cliffTexture = cliffTex;
                        cornerExtra_[tl].cliffTexture = cliffTex;
                        cornerExtra_[tr].cliffTexture = cliffTex;

                        float worldX = rampCellX * TILE_SIZE + map.offsetX;
                        float worldZ = rampCellY * TILE_SIZE + map.offsetY;
                        float worldY = groundHeights_[rampCellY * w + rampCellX] * TILE_SIZE;

                        for (auto it = cliffInstances_.begin(); it != cliffInstances_.end(); ++it) {
                            if (it->posX == worldX && it->posZ == worldZ) {
                                cliffInstances_.erase(it);
                                break;
                            }
                        }

                        CliffInstance inst;
                        inst.viewer = rampViewer;
                        inst.posX = worldX;
                        inst.posY = worldY;
                        inst.posZ = worldZ;
                        inst.cliffTexture = cliffTex;
                        inst.variation = 0;
                        inst.scale = 1.0f;
                        inst.yaw = 0.0f;
                        inst.gridX = rampCellX;
                        inst.gridY = rampCellY;
                        inst.layerBase = 0.0f;
                        inst.meshPath = rampPath;
                        cliffInstances_.push_back(inst);
                        continue;
                    }
                }
            }

            // Regular cliff model
            // Skip ramp entrance corners (handled by adjacent cell's ramp)
            if (isCornerRampEntrance(map, i, j)) continue;

            // Build ABCD filename from corner layer heights
            std::string tag;
            tag += (char)('A' + (hTl - base));
            tag += (char)('A' + (hTr - base));
            tag += (char)('A' + (hBr - base));
            tag += (char)('A' + (hBl - base));

            if (tag == "AAAA") continue; // no height difference

            // Get variation
            int variation = map.tiles[bl].CliffVariation();
            int clampedVariation = Variations::GetCliffVariation(modelDir, tag, variation);

            // Construct full MPQ path
            std::string fullPath = "Doodads\\Terrain\\" + modelDir + "\\" + modelDir + tag
                + std::to_string(clampedVariation) + ".mdx";

            // Load cliff model
            MdxViewer* viewer = GetOrLoadCliffModel(fullPath);
            if (!viewer) {
                buildStats_.cliffModelMisses++;
                continue;
            }

            // Compute world position
            // Use the decoded ground height at this corner (without layer height, which the model handles)
            float worldX = i * TILE_SIZE + map.offsetX;
            float worldZ = j * TILE_SIZE + map.offsetY;
            // groundHeights_ stores FinalGroundHeight() = DecodeGroundHeight() + LayerHeight() - 2
            // The MDX model expects to be placed at FinalGroundHeight * TILE_SIZE
            float worldY = groundHeights_[bl] * TILE_SIZE;

            // Create cliff instance
            CliffInstance inst;
            inst.viewer = viewer;
            inst.posX = worldX;
            inst.posY = worldY;
            inst.posZ = worldZ;
            inst.cliffTexture = cliffTex;
            inst.variation = clampedVariation;
            inst.scale = 1.0f;
            inst.yaw = 0.0f;
            inst.gridX = i;
            inst.gridY = j;
            inst.layerBase = 0.0f;
            inst.meshPath = fullPath;
            cliffInstances_.push_back(inst);
        }
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
std::string TexturedTerrain::ResolveUberSplatPath(const std::string& path) { return path; }

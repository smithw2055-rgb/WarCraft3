#include "splat_model.h"
#include "shader.h"
#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr float kTerrainTileSize = 128.0f;
constexpr float kAbsoluteHeightSentinel = -257.0f;

struct SplatVertex {
    float px, py, pz;
    float u, v;
    float absoluteHeight;
    float clampMinTileX, clampMinTileY, clampMaxTileX, clampMaxTileY;
    float cornerHeightBL, cornerHeightBR, cornerHeightTL, cornerHeightTR;
    float useCornerHeightOverride;
};
}

SplatModel::SplatModel() = default;

SplatModel::~SplatModel() {
    if (!Renderer::IsActive()) {
        texture_ = {};
        textureView_ = {};
        sampler_ = {};
        batches_.clear();
        pipeline_ = {};
        return;
    }
    if (sg_query_view_state(textureView_) == SG_RESOURCESTATE_VALID)
        sg_destroy_view(textureView_);
    if (sg_query_image_state(texture_) == SG_RESOURCESTATE_VALID)
        sg_destroy_image(texture_);
    if (sg_query_sampler_state(sampler_) == SG_RESOURCESTATE_VALID)
        sg_destroy_sampler(sampler_);
    ClearBatches();
    if (sg_query_pipeline_state(pipeline_) == SG_RESOURCESTATE_VALID)
        sg_destroy_pipeline(pipeline_);
}

bool SplatModel::LoadTexture(const std::string& path, sg_image tex) {
    (void)path;
    texture_ = tex;
    sg_view_desc vd = {};
    vd.texture.image = texture_;
    textureView_ = sg_make_view(&vd);
    sg_sampler_desc sd = {};
    sd.min_filter = SG_FILTER_LINEAR;
    sd.mag_filter = SG_FILTER_LINEAR;
    sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sampler_ = sg_make_sampler(&sd);
    return true;
}

void SplatModel::AddSplat(float centerX, float centerY, float scale, float rotation) {
    (void)rotation;
    AddSplatRect(centerX - scale * kTerrainTileSize,
                 centerY - scale * kTerrainTileSize,
                 centerX + scale * kTerrainTileSize,
                 centerY + scale * kTerrainTileSize,
                 0.0f, false, 0.0f, false, 0.0f, 0.0f, 0.0f, 0.0f,
                 false, 0.0f, 0.0f, 0.0f, 0.0f);
}

void SplatModel::AddSplatRect(float x0, float y0, float x1, float y1, float zOffset,
                              bool absoluteHeight, float absoluteHeightValue,
                              bool clampHeightToRect,
                              float clampMinTileX, float clampMinTileY,
                              float clampMaxTileX, float clampMaxTileY,
                              bool useCornerHeightOverride,
                              float cornerHeightBL, float cornerHeightBR,
                              float cornerHeightTL, float cornerHeightTR) {
    SplatInstance inst;
    inst.x0 = std::min(x0, x1);
    inst.y0 = std::min(y0, y1);
    inst.x1 = std::max(x0, x1);
    inst.y1 = std::max(y0, y1);
    inst.zOffset = zOffset;
    inst.absoluteHeight = absoluteHeight;
    inst.absoluteHeightValue = absoluteHeightValue;
    inst.clampHeightToRect = clampHeightToRect;
    inst.clampMinTileX = clampMinTileX;
    inst.clampMinTileY = clampMinTileY;
    inst.clampMaxTileX = clampMaxTileX;
    inst.clampMaxTileY = clampMaxTileY;
    inst.useCornerHeightOverride = useCornerHeightOverride;
    inst.cornerHeightBL = cornerHeightBL;
    inst.cornerHeightBR = cornerHeightBR;
    inst.cornerHeightTL = cornerHeightTL;
    inst.cornerHeightTR = cornerHeightTR;
    instances_.push_back(inst);
    dirty_ = true;
}

void SplatModel::AddMesh(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices) {
    if (vertices.empty() || indices.empty()) {
        return;
    }

    MeshInstance inst;
    inst.vertices = vertices;
    inst.indices = indices;
    meshInstances_.push_back(std::move(inst));
    dirty_ = true;
}

void SplatModel::RemoveSplat(int index) {
    if (index >= 0 && index < (int)instances_.size()) {
        instances_.erase(instances_.begin() + index);
        dirty_ = true;
    }
}

void SplatModel::SetNoDepthTest(bool v) {
    if (noDepthTest_ == v) {
        return;
    }

    noDepthTest_ = v;
    if (Renderer::IsActive() && sg_query_pipeline_state(pipeline_) == SG_RESOURCESTATE_VALID) {
        sg_destroy_pipeline(pipeline_);
    }
    pipeline_ = {};
}

void SplatModel::ClearBatches() {
    if (!Renderer::IsActive()) {
        batches_.clear();
        return;
    }
    for (auto& batch : batches_) {
        if (sg_query_buffer_state(batch.vbo) == SG_RESOURCESTATE_VALID)
            sg_destroy_buffer(batch.vbo);
        if (sg_query_buffer_state(batch.ibo) == SG_RESOURCESTATE_VALID)
            sg_destroy_buffer(batch.ibo);
    }
    batches_.clear();
}

void SplatModel::RebuildBatches(float centerOffset[2], int mapCols, int mapRows) {
    ClearBatches();
    lastCenterOffset_[0] = centerOffset[0];
    lastCenterOffset_[1] = centerOffset[1];
    lastMapCols_ = mapCols;
    lastMapRows_ = mapRows;
    dirty_ = false;

    if ((instances_.empty() && meshInstances_.empty()) || mapCols <= 0 || mapRows <= 0) {
        return;
    }

    std::vector<SplatVertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto& inst : instances_) {
        const float width = inst.x1 - inst.x0;
        const float height = inst.y1 - inst.y0;
        if (width <= 0.01f || height <= 0.01f) {
            continue;
        }

        const bool useClampedSingleCellQuad =
            inst.clampHeightToRect &&
            std::fabs((inst.clampMaxTileX - inst.clampMinTileX) - 1.0f) <= 0.001f &&
            std::fabs((inst.clampMaxTileY - inst.clampMinTileY) - 1.0f) <= 0.001f;

        if (useClampedSingleCellQuad) {
            const int gx0 = (int)std::floor(inst.clampMinTileX);
            const int gy0 = (int)std::floor(inst.clampMinTileY);
            const int gx1 = gx0 + 1;
            const int gy1 = gy0 + 1;
            const float worldX0 = centerOffset[0] + gx0 * kTerrainTileSize;
            const float worldY0 = centerOffset[1] + gy0 * kTerrainTileSize;
            const float worldX1 = centerOffset[0] + gx1 * kTerrainTileSize;
            const float worldY1 = centerOffset[1] + gy1 * kTerrainTileSize;

            const uint32_t baseVertex = (uint32_t)vertices.size();
            vertices.reserve(vertices.size() + 4);

            auto appendVertex = [&](float worldX, float worldY) {
                SplatVertex vtx = {};
                vtx.px = worldX;
                vtx.py = worldY;
                vtx.pz = inst.zOffset;
                vtx.u = (worldX - inst.x0) / width;
                vtx.v = 1.0f - ((worldY - inst.y0) / height);
                vtx.absoluteHeight = inst.absoluteHeight ? inst.absoluteHeightValue : kAbsoluteHeightSentinel;
                vtx.clampMinTileX = inst.clampMinTileX;
                vtx.clampMinTileY = inst.clampMinTileY;
                vtx.clampMaxTileX = inst.clampMaxTileX;
                vtx.clampMaxTileY = inst.clampMaxTileY;
                vtx.cornerHeightBL = inst.cornerHeightBL;
                vtx.cornerHeightBR = inst.cornerHeightBR;
                vtx.cornerHeightTL = inst.cornerHeightTL;
                vtx.cornerHeightTR = inst.cornerHeightTR;
                vtx.useCornerHeightOverride = inst.useCornerHeightOverride ? 1.0f : 0.0f;
                vertices.push_back(vtx);
            };

            appendVertex(worldX0, worldY0);
            appendVertex(worldX1, worldY0);
            appendVertex(worldX0, worldY1);
            appendVertex(worldX1, worldY1);

            indices.reserve(indices.size() + 6);
            indices.push_back(baseVertex + 0);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 1);
            indices.push_back(baseVertex + 1);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 3);
            continue;
        }

        int ix0 = (int)std::floor((inst.x0 - centerOffset[0]) / kTerrainTileSize);
        int iy0 = (int)std::floor((inst.y0 - centerOffset[1]) / kTerrainTileSize);
        int ix1 = (int)std::ceil((inst.x1 - centerOffset[0]) / kTerrainTileSize);
        int iy1 = (int)std::ceil((inst.y1 - centerOffset[1]) / kTerrainTileSize);

        const int vertsX = ix1 - ix0 + 1;
        const int vertsY = iy1 - iy0 + 1;
        if (vertsX < 2 || vertsY < 2) {
            continue;
        }

        const uint32_t baseVertex = (uint32_t)vertices.size();
        vertices.reserve(vertices.size() + ((size_t)vertsX * vertsY));
        for (int gy = iy0; gy <= iy1; ++gy) {
            for (int gx = ix0; gx <= ix1; ++gx) {
                const float worldX = centerOffset[0] + gx * kTerrainTileSize;
                const float worldY = centerOffset[1] + gy * kTerrainTileSize;
                SplatVertex vtx = {};
                vtx.px = worldX;
                vtx.py = worldY;
                vtx.pz = inst.zOffset;
                vtx.u = (worldX - inst.x0) / width;
                vtx.v = 1.0f - ((worldY - inst.y0) / height);
                vtx.absoluteHeight = inst.absoluteHeight ? inst.absoluteHeightValue : kAbsoluteHeightSentinel;
                if (inst.clampHeightToRect) {
                    vtx.clampMinTileX = inst.clampMinTileX;
                    vtx.clampMinTileY = inst.clampMinTileY;
                    vtx.clampMaxTileX = inst.clampMaxTileX;
                    vtx.clampMaxTileY = inst.clampMaxTileY;
                } else {
                    vtx.clampMinTileX = 1.0f;
                    vtx.clampMinTileY = 1.0f;
                    vtx.clampMaxTileX = 0.0f;
                    vtx.clampMaxTileY = 0.0f;
                }
                vtx.cornerHeightBL = inst.cornerHeightBL;
                vtx.cornerHeightBR = inst.cornerHeightBR;
                vtx.cornerHeightTL = inst.cornerHeightTL;
                vtx.cornerHeightTR = inst.cornerHeightTR;
                vtx.useCornerHeightOverride = inst.useCornerHeightOverride ? 1.0f : 0.0f;
                vertices.push_back(vtx);
            }
        }

        indices.reserve(indices.size() + ((size_t)(vertsX - 1) * (vertsY - 1) * 6));
        for (int y = 0; y < vertsY - 1; ++y) {
            for (int x = 0; x < vertsX - 1; ++x) {
                const uint32_t i0 = baseVertex + (uint32_t)(y * vertsX + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + (uint32_t)vertsX;
                const uint32_t i3 = i2 + 1;
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }

    }

    for (const auto& meshInst : meshInstances_) {
        if (meshInst.vertices.empty() || meshInst.indices.empty()) {
            continue;
        }

        const uint32_t baseVertex = (uint32_t)vertices.size();
        vertices.reserve(vertices.size() + meshInst.vertices.size());
        for (const auto& src : meshInst.vertices) {
            SplatVertex vtx = {};
            vtx.px = src.worldX;
            vtx.py = src.worldY;
            vtx.pz = src.zOffset;
            vtx.u = src.u;
            vtx.v = src.v;
            vtx.absoluteHeight = src.absoluteHeight ? src.absoluteHeightValue : kAbsoluteHeightSentinel;
            if (src.clampHeightToRect) {
                vtx.clampMinTileX = src.clampMinTileX;
                vtx.clampMinTileY = src.clampMinTileY;
                vtx.clampMaxTileX = src.clampMaxTileX;
                vtx.clampMaxTileY = src.clampMaxTileY;
            } else {
                vtx.clampMinTileX = 1.0f;
                vtx.clampMinTileY = 1.0f;
                vtx.clampMaxTileX = 0.0f;
                vtx.clampMaxTileY = 0.0f;
            }
            vtx.cornerHeightBL = src.cornerHeightBL;
            vtx.cornerHeightBR = src.cornerHeightBR;
            vtx.cornerHeightTL = src.cornerHeightTL;
            vtx.cornerHeightTR = src.cornerHeightTR;
            vtx.useCornerHeightOverride = src.useCornerHeightOverride ? 1.0f : 0.0f;
            vertices.push_back(vtx);
        }

        indices.reserve(indices.size() + meshInst.indices.size());
        for (uint32_t index : meshInst.indices) {
            indices.push_back(baseVertex + index);
        }
    }

    if (vertices.empty() || indices.empty()) {
        return;
    }

    BatchGPU batch = {};
    sg_buffer_desc vbd = {};
    vbd.usage.vertex_buffer = true;
    vbd.data.ptr = vertices.data();
    vbd.data.size = vertices.size() * sizeof(SplatVertex);
    batch.vbo = sg_make_buffer(&vbd);

    sg_buffer_desc ibd = {};
    ibd.usage.index_buffer = true;
    ibd.data.ptr = indices.data();
    ibd.data.size = indices.size() * sizeof(uint32_t);
    batch.ibo = sg_make_buffer(&ibd);
    batch.indexCount = (int)indices.size();

    if (batch.vbo.id && batch.ibo.id) {
        batches_.push_back(batch);
    } else {
        if (batch.vbo.id) sg_destroy_buffer(batch.vbo);
        if (batch.ibo.id) sg_destroy_buffer(batch.ibo);
    }
}

void SplatModel::Render(sg_shader uberSplatShader, const Mat4& mvp, float centerOffset[2],
                        int mapCols, int mapRows, sg_view heightMapView, sg_view shadowMapView,
                        sg_view waterHeightMapView, sg_view fogOfWarMapView, float waterHeightOffset,
                        sg_view terrainTextureListView, const sg_view* groundTextureViews,
                        int groundTextureCount, sg_sampler terrainTextureListSampler,
                        sg_sampler groundTextureSampler) {
    if ((instances_.empty() && meshInstances_.empty()) || !uberSplatShader.id || !texture_.id || !textureView_.id) return;

    if (!pipeline_.id) {
        sg_pipeline_desc pd = {};
        pd.shader = uberSplatShader;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT32;
        pd.depth.compare = noDepthTest_ ? SG_COMPAREFUNC_ALWAYS : SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = false;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
        pd.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.layout.buffers[0].stride = sizeof(SplatVertex);
        pd.layout.attrs[0].buffer_index = 0;
        pd.layout.attrs[0].offset = 0;
        pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
        pd.layout.attrs[1].buffer_index = 0;
        pd.layout.attrs[1].offset = 12;
        pd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
        pd.layout.attrs[2].buffer_index = 0;
        pd.layout.attrs[2].offset = 20;
        pd.layout.attrs[2].format = SG_VERTEXFORMAT_FLOAT;
        pd.layout.attrs[3].buffer_index = 0;
        pd.layout.attrs[3].offset = 24;
        pd.layout.attrs[3].format = SG_VERTEXFORMAT_FLOAT4;
        pd.layout.attrs[4].buffer_index = 0;
        pd.layout.attrs[4].offset = 40;
        pd.layout.attrs[4].format = SG_VERTEXFORMAT_FLOAT4;
        pd.layout.attrs[5].buffer_index = 0;
        pd.layout.attrs[5].offset = 56;
        pd.layout.attrs[5].format = SG_VERTEXFORMAT_FLOAT;
        pipeline_ = sg_make_pipeline(&pd);
    }

    if (dirty_ ||
        lastCenterOffset_[0] != centerOffset[0] ||
        lastCenterOffset_[1] != centerOffset[1] ||
        lastMapCols_ != mapCols ||
        lastMapRows_ != mapRows) {
        RebuildBatches(centerOffset, mapCols, mapRows);
    }

    if (batches_.empty()) return;

    sg_apply_pipeline(pipeline_);

    struct VSBlock {
        float MVP[16];
        float centerOffset[2];
        float mapSize[2];
        float waterHeightOffset;
    };
    struct FSBlock {
        float shadowScale[2];
        float fogColor[4];
        float fogParams[4];
        float splatParams[4];
    };

    VSBlock vsU = {};
    std::memcpy(vsU.MVP, mvp.Data(), sizeof(vsU.MVP));
    vsU.centerOffset[0] = centerOffset[0];
    vsU.centerOffset[1] = centerOffset[1];
    vsU.mapSize[0] = (float)mapCols;
    vsU.mapSize[1] = (float)mapRows;
    vsU.waterHeightOffset = waterHeightOffset;
    sg_apply_uniforms(0, sg_rng(vsU));

    FSBlock fsU = {};
    fsU.shadowScale[0] = 1.0f;
    fsU.shadowScale[1] = 1.0f;
    fsU.splatParams[0] =
        (terrainColorMask_ && terrainTextureListView.id && groundTextureViews && groundTextureCount > 0)
        ? 1.0f : 0.0f;
    sg_apply_uniforms(1, sg_rng(fsU));

    for (const auto& batch : batches_) {
        sg_bindings bind = {};
        bind.vertex_buffers[0] = batch.vbo;
        bind.index_buffer = batch.ibo;
        bind.views[0] = heightMapView;
        bind.views[1] = waterHeightMapView;
        bind.views[2] = textureView_;
        bind.views[3] = shadowMapView;
        bind.views[4] = fogOfWarMapView;
        bind.views[5] = terrainTextureListView;
        for (int i = 0; i < groundTextureCount && i < 17; ++i) {
            bind.views[6 + i] = groundTextureViews[i];
        }
        bind.samplers[0] = sampler_;
        bind.samplers[1] = sampler_;
        bind.samplers[2] = sampler_;
        bind.samplers[3] = sampler_;
        bind.samplers[4] = sampler_;
        bind.samplers[5] = terrainTextureListSampler;
        bind.samplers[6] = groundTextureSampler;
        bool suspiciousView = false;
        for (int i = 0; i < 6 + std::min(groundTextureCount, 17); ++i) {
            if (bind.views[i].id && ((bind.views[i].id & 0xFFFFu) >= 4096u)) {
                std::fprintf(stderr, "SplatModel suspicious view[%d]=0x%08x\n", i, bind.views[i].id);
                suspiciousView = true;
                break;
            }
        }
        if (suspiciousView) {
            continue;
        }
        sg_apply_bindings(&bind);
        sg_draw(0, batch.indexCount, 1);
    }
}

#include "terrain.h"
#include "shader.h"
#include "renderer.h"
#include "map/w3e.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <limits>

struct VSUniformBlock {
    float mvp[16];
};

static constexpr float TILE_SIZE = 128.0f;

static float DecodeHeight(const W3ETileCorner& c) {
    return c.FinalGroundHeight() * TILE_SIZE;
}

TerrainRenderer::TerrainRenderer() = default;

TerrainRenderer::~TerrainRenderer() {
    Release();
}

void TerrainRenderer::Release() {
    if (!Renderer::IsActive()) { vbo_ = {0}; ebo_ = {0}; shader_ = {0}; pipeline_ = {0}; return; }
    if (sg_query_buffer_state(vbo_) == SG_RESOURCESTATE_VALID)
        sg_destroy_buffer(vbo_);
    if (sg_query_buffer_state(ebo_) == SG_RESOURCESTATE_VALID)
        sg_destroy_buffer(ebo_);
    if (sg_query_shader_state(shader_) == SG_RESOURCESTATE_VALID)
        sg_destroy_shader(shader_);
    if (sg_query_pipeline_state(pipeline_) == SG_RESOURCESTATE_VALID)
        sg_destroy_pipeline(pipeline_);
    vbo_ = {0}; ebo_ = {0}; shader_ = {0}; pipeline_ = {0};
    vertexCount_ = 0;
    indexCount_  = 0;
}

Vec3 TerrainRenderer::HeightColor(float h) {
    if (h < -2.0f * TILE_SIZE) {
        float t = std::clamp((h + 10.0f * TILE_SIZE) / (12.0f * TILE_SIZE), 0.0f, 1.0f);
        return {0.1f * t, 0.2f * t, 0.6f * t + 0.4f * (1.0f - t)};
    } else if (h < 2.0f * TILE_SIZE) {
        float t = (h + 2.0f * TILE_SIZE) / (4.0f * TILE_SIZE);
        return {0.2f * (1.0f - t) + 0.1f * t, 0.5f * (1.0f - t) + 0.4f * t, 0.1f * (1.0f - t)};
    } else if (h < 10.0f * TILE_SIZE) {
        float t = (h - 2.0f * TILE_SIZE) / (8.0f * TILE_SIZE);
        return {0.1f + 0.3f * t, 0.4f - 0.1f * t, 0.1f};
    } else {
        float t = std::clamp((h - 10.0f * TILE_SIZE) / (20.0f * TILE_SIZE), 0.0f, 1.0f);
        return {0.4f + 0.6f * t, 0.3f + 0.7f * t, 0.1f + 0.9f * t};
    }
}

bool TerrainRenderer::Build(const W3EMap& map) {
    Release();

    const char* vertSrc = R"(
        #version 410 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;
        out vec3 vColor;
        uniform mat4 uMVP;
        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
            vColor = aColor;
        }
    )";

    const char* fragSrc = R"(
        #version 410 core
        in vec3 vColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(vColor, 1.0);
        }
    )";

    sg_shader_desc shdDesc = {};
    shdDesc.vertex_func.source = vertSrc;
    shdDesc.fragment_func.source = fragSrc;
    shdDesc.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
    shdDesc.uniform_blocks[0].size = (uint32_t)sizeof(VSUniformBlock);
    shdDesc.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
    shdDesc.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uMVP";
    shdDesc.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
    shdDesc.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
    shader_ = sg_make_shader(&shdDesc);
    if (sg_query_shader_state(shader_) != SG_RESOURCESTATE_VALID) {
        std::cerr << "Terrain: shader creation failed" << std::endl;
        return false;
    }

    int cornerW = map.tileWidth;
    int cornerH = map.tileHeight;
    int tileW = cornerW - 1;
    int tileH = cornerH - 1;

    float minX =  std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minZ =  std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();
    float minY =  std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();

    struct Vertex { float px, py, pz; float cr, cg, cb; };
    std::vector<Vertex> vertices;
    vertices.reserve(cornerW * cornerH);

    for (int row = 0; row < cornerH; ++row) {
        for (int col = 0; col < cornerW; ++col) {
            int idx = row * cornerW + col;
            const auto& corner = map.tiles[idx];
            float x = static_cast<float>(col) * TILE_SIZE + map.offsetX;
            float y = DecodeHeight(corner);
            float z = static_cast<float>(row) * TILE_SIZE + map.offsetY;
            Vec3 col_h = HeightColor(y);
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
            minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
            vertices.push_back({x, y, z, col_h.x, col_h.y, col_h.z});
        }
    }

    std::vector<unsigned> indices;
    indices.reserve(tileW * tileH * 6);

    for (int row = 0; row < tileH; ++row) {
        for (int col = 0; col < tileW; ++col) {
            unsigned bl = static_cast<unsigned>(row * cornerW + col);
            unsigned br = bl + 1;
            unsigned tl = static_cast<unsigned>((row + 1) * cornerW + col);
            unsigned tr = tl + 1;
            indices.push_back(bl); indices.push_back(br); indices.push_back(tl);
            indices.push_back(br); indices.push_back(tr); indices.push_back(tl);
        }
    }

    center_ = {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f};
    radius_ = std::sqrt(
        (maxX - minX) * (maxX - minX) +
        (maxY - minY) * (maxY - minY) +
        (maxZ - minZ) * (maxZ - minZ)
    ) * 0.5f;

    vertexCount_ = static_cast<int>(vertices.size());
    indexCount_  = static_cast<int>(indices.size());

    sg_buffer_desc vbDesc = {};
    vbDesc.usage.vertex_buffer = true;
    vbDesc.usage.immutable = true;
    vbDesc.data.ptr = vertices.data();
    vbDesc.data.size = vertices.size() * sizeof(Vertex);
    vbo_ = sg_make_buffer(&vbDesc);

    sg_buffer_desc ibDesc = {};
    ibDesc.usage.index_buffer = true;
    ibDesc.usage.immutable = true;
    ibDesc.data.ptr = indices.data();
    ibDesc.data.size = indices.size() * sizeof(unsigned);
    ebo_ = sg_make_buffer(&ibDesc);

    sg_pipeline_desc pipDesc = {};
    pipDesc.shader = shader_;
    pipDesc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pipDesc.index_type = SG_INDEXTYPE_UINT32;
    pipDesc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pipDesc.depth.write_enabled = true;
    pipDesc.cull_mode = SG_CULLMODE_NONE;
    pipDesc.colors[0].blend.enabled = true;
    pipDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pipDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pipDesc.layout.buffers[0].stride = sizeof(Vertex);
    pipDesc.layout.attrs[0].buffer_index = 0;
    pipDesc.layout.attrs[0].offset = offsetof(Vertex, px);
    pipDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    pipDesc.layout.attrs[1].buffer_index = 0;
    pipDesc.layout.attrs[1].offset = offsetof(Vertex, cr);
    pipDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT3;
    pipeline_ = sg_make_pipeline(&pipDesc);

    return true;
}

void TerrainRenderer::Render(const Mat4& view, const Mat4& proj) {
    if (sg_query_buffer_state(vbo_) != SG_RESOURCESTATE_VALID) return;

    Mat4 mvp = proj * view;
    VSUniformBlock ub;
    for (int i = 0; i < 16; ++i) ub.mvp[i] = mvp.m[i];

    sg_apply_pipeline(pipeline_);
    sg_bindings bind = {};
    bind.vertex_buffers[0] = vbo_;
    bind.index_buffer = ebo_;
    sg_apply_bindings(&bind);
    sg_apply_uniforms(0, sg_rng(ub));
    sg_draw(0, indexCount_, 1);
}

#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include "map/w3e.h"
#include "math.h"
#include <vector>
#include <cstdint>

class TerrainRenderer {
public:
    TerrainRenderer();
    ~TerrainRenderer();

    TerrainRenderer(const TerrainRenderer&) = delete;
    TerrainRenderer& operator=(const TerrainRenderer&) = delete;

    bool Build(const W3EMap& map);
    void Render(const Mat4& view, const Mat4& proj);

    bool IsValid() const { return sg_query_buffer_state(vbo_) == SG_RESOURCESTATE_VALID; }
    int GetVertexCount() const { return vertexCount_; }
    int GetTriangleCount() const { return indexCount_ / 3; }

    Vec3 GetCenter() const { return center_; }
    float GetRadius() const { return radius_; }

private:
    void Release();
    static Vec3 HeightColor(float h);

    sg_buffer vbo_ = {0};
    sg_buffer ebo_ = {0};
    sg_shader shader_ = {0};
    sg_pipeline pipeline_ = {0};
    int vertexCount_ = 0;
    int indexCount_  = 0;
    Vec3 center_ = {0,0,0};
    float radius_ = 0;
};

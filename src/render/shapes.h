#pragma once
#include "third_party/sokol/sokol_gfx.h"
#include <vector>

struct Shapes {
    static Shapes& Instance();

    std::vector<float> vertices = {0,0, 1,0, 0,1, 1,1};
    std::vector<unsigned> indices = {0,1,2, 1,3,2};

    sg_buffer vertexBuffer = {0};
    sg_buffer indexBuffer = {0};

    void Bind();
    sg_buffer GetVertexBuffer() { return vertexBuffer; }
    sg_buffer GetIndexBuffer() { return indexBuffer; }
    int IndexCount() const { return (int)indices.size(); }

private:
    Shapes() = default;
    void EnsureCreated();
};

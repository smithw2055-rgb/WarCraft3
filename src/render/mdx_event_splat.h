#pragma once

#include "render/math.h"
#include "render/splat_model.h"

#include <cstdint>
#include <vector>

struct MdxEventSplatMesh {
    std::vector<SplatModel::MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

MdxEventSplatMesh BuildMdxEventSplatMeshFromTransform(const Mat4& worldTransform,
                                                       float halfSize,
                                                       float zOffset = 8.0f);

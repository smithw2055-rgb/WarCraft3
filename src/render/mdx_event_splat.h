#pragma once

#include "render/math.h"
#include "render/splat_model.h"

#include <cstdint>
#include <string>
#include <vector>

struct MdxEventSplatMesh {
    std::vector<SplatModel::MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

MdxEventSplatMesh BuildMdxEventSplatMeshFromTransform(const Mat4& worldTransform,
                                                       float halfSize,
                                                       float zOffset = 8.0f);

MdxEventSplatMesh BuildMdxEventSplatMeshFromTransform(const Mat4& worldTransform,
                                                       float halfSize,
                                                       float zOffset,
                                                       const Vec3& localScale);

Vec3 MdxEventSplatScaleCompensationForType(const std::string& type, const Vec3& localScale);

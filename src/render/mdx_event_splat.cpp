#include "mdx_event_splat.h"

#include <cmath>

namespace {

static Vec3 TransformPoint(const Mat4& matrix, const Vec3& point) {
    return {
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]
    };
}

static bool IsFinitePoint(const Vec3& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

static float SafeScaleDivisor(float value) {
    if (!std::isfinite(value)) {
        return 1.0f;
    }
    const float absValue = std::fabs(value);
    return absValue > 0.0001f ? absValue : 1.0f;
}

} // namespace

MdxEventSplatMesh BuildMdxEventSplatMeshFromTransform(const Mat4& worldTransform,
                                                       float halfSize,
                                                       float zOffset) {
    return BuildMdxEventSplatMeshFromTransform(
        worldTransform, halfSize, zOffset, Vec3(1.0f, 1.0f, 1.0f));
}

MdxEventSplatMesh BuildMdxEventSplatMeshFromTransform(const Mat4& worldTransform,
                                                       float halfSize,
                                                       float zOffset,
                                                       const Vec3& localScale) {
    MdxEventSplatMesh mesh;
    if (!std::isfinite(halfSize) || halfSize <= 0.0f) {
        return mesh;
    }

    const float halfX = halfSize / SafeScaleDivisor(localScale.x);
    const float halfZ = halfSize / SafeScaleDivisor(localScale.y);
    const Vec3 localCorners[4] = {
        { halfX, 0.0f,  halfZ},
        {-halfX, 0.0f,  halfZ},
        {-halfX, 0.0f, -halfZ},
        { halfX, 0.0f, -halfZ}
    };
    const float uvs[4][2] = {
        {1.0f, 0.0f},
        {0.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f}
    };

    mesh.vertices.reserve(4);
    for (int i = 0; i < 4; ++i) {
        const Vec3 transformed = TransformPoint(worldTransform, localCorners[i]);
        if (!IsFinitePoint(transformed)) {
            mesh.vertices.clear();
            mesh.indices.clear();
            return mesh;
        }

        SplatModel::MeshVertex vertex;
        vertex.worldX = transformed.x;
        vertex.worldY = transformed.z;
        vertex.u = uvs[i][0];
        vertex.v = uvs[i][1];
        vertex.zOffset = zOffset;
        mesh.vertices.push_back(vertex);
    }

    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

Vec3 MdxEventSplatScaleCompensationForType(const std::string& type, const Vec3& localScale) {
    if (type == "SPL" || type == "FPT") {
        return localScale;
    }
    return Vec3(1.0f, 1.0f, 1.0f);
}

#include "render/mdx_collision.h"
#include "render/mdx_animator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

Vec3 MdxPointToYUp(const float point[3]) {
    return {point[0], point[2], -point[1]};
}

Vec3 TransformPoint(const Mat4& matrix, const Vec3& point) {
    return {
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14],
    };
}

float MaxBasisScale(const Mat4& matrix) {
    const float sx = std::sqrt(matrix.m[0] * matrix.m[0] + matrix.m[1] * matrix.m[1] + matrix.m[2] * matrix.m[2]);
    const float sy = std::sqrt(matrix.m[4] * matrix.m[4] + matrix.m[5] * matrix.m[5] + matrix.m[6] * matrix.m[6]);
    const float sz = std::sqrt(matrix.m[8] * matrix.m[8] + matrix.m[9] * matrix.m[9] + matrix.m[10] * matrix.m[10]);
    return std::max(0.0001f, std::max(sx, std::max(sy, sz)));
}

bool IntersectLocalBox(const picking::PickRay& ray, const Vec3& minPoint, const Vec3& maxPoint, float& outT) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::infinity();
    const float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float mins[3] = {minPoint.x, minPoint.y, minPoint.z};
    const float maxs[3] = {maxPoint.x, maxPoint.y, maxPoint.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(directions[axis]) < 0.000001f) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) {
                return false;
            }
            continue;
        }

        float axisT0 = (mins[axis] - origins[axis]) / directions[axis];
        float axisT1 = (maxs[axis] - origins[axis]) / directions[axis];
        if (axisT0 > axisT1) {
            std::swap(axisT0, axisT1);
        }
        tMin = std::max(tMin, axisT0);
        tMax = std::min(tMax, axisT1);
        if (tMin > tMax) {
            return false;
        }
    }

    outT = tMin;
    return true;
}

bool IntersectCollisionBox(const MdxCollisionShape& shape,
                           const picking::PickRay& ray,
                           const Mat4& shapeWorldMatrix,
                           float& outT) {
    HMM_Mat4 inverse = HMM_InvGeneralM4((HMM_Mat4)shapeWorldMatrix);
    const Mat4 inverseMatrix(inverse);
    const Vec3 localOrigin = TransformPoint(inverseMatrix, ray.origin);
    const Vec3 localEnd = TransformPoint(inverseMatrix, ray.origin + ray.direction);
    const Vec3 localDirection = localEnd - localOrigin;

    const Vec3 a = MdxPointToYUp(shape.vertices[0]);
    const Vec3 b = MdxPointToYUp(shape.vertices[1]);
    const Vec3 minPoint{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    const Vec3 maxPoint{std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};

    float localT = 0.0f;
    picking::PickRay localRay{localOrigin, localDirection};
    if (!IntersectLocalBox(localRay, minPoint, maxPoint, localT)) {
        return false;
    }

    const Vec3 localHit = localOrigin + localDirection * localT;
    const Vec3 worldHit = TransformPoint(shapeWorldMatrix, localHit);
    const float worldT = Dot(worldHit - ray.origin, ray.direction);
    if (worldT < 0.0f) {
        return false;
    }
    outT = worldT;
    return true;
}

} // namespace

bool MdxIntersectCollisionShapes(const MdxModel& model,
                                 const MdxAnimator& animator,
                                 const picking::PickRay& ray,
                                 const Mat4& instanceModelMatrix,
                                 float& outT) {
    bool hit = false;
    float bestT = std::numeric_limits<float>::infinity();

    for (const MdxCollisionShape& shape : model.collisionShapes) {
        Mat4 nodeTransform = Mat4::Identity();
        if (!animator.GetObjectWorldTransformYUp(shape.objectId, nodeTransform)) {
            continue;
        }

        const Mat4 shapeWorldMatrix = instanceModelMatrix * nodeTransform;
        float hitT = 0.0f;
        bool shapeHit = false;

        if (shape.type == MdxCollisionShapeType::Sphere) {
            const Vec3 center = TransformPoint(shapeWorldMatrix, MdxPointToYUp(shape.vertices[0]));
            const float radius = std::max(0.0f, shape.boundsRadius) * MaxBasisScale(shapeWorldMatrix);
            shapeHit = radius > 0.0f && picking::IntersectSphere(ray, center, radius, hitT);
        }
        else if (shape.type == MdxCollisionShapeType::Box) {
            shapeHit = IntersectCollisionBox(shape, ray, shapeWorldMatrix, hitT);
        }

        if (shapeHit && hitT < bestT) {
            bestT = hitT;
            hit = true;
        }
    }

    if (!hit) {
        return false;
    }
    outT = bestT;
    return true;
}

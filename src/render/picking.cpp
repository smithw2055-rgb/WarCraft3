#include "picking.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace picking {

bool WorldToScreen(const Vec3& worldPos, const Mat4& view, const Mat4& proj,
                   int screenW, int screenH, float& outX, float& outY) {
    HMM_Vec4 clip = HMM_MulM4V4(proj, HMM_MulM4V4(view, HMM_V4(worldPos.x, worldPos.y, worldPos.z, 1.0f)));
    if (clip.W <= 0.0f) return false;
    float ndcX = clip.X / clip.W;
    float ndcY = clip.Y / clip.W;
    outX = (ndcX * 0.5f + 0.5f) * screenW;
    outY = (1.0f - (ndcY * 0.5f + 0.5f)) * screenH; // flip Y for sokol top-left
    return true;
}

Vec3 ScreenToWorld(float screenX, float screenY, const Mat4& view, const Mat4& proj,
                   int screenW, int screenH,
                   bool terrainValid,
                   float terrainRadius,
                   const std::function<float(float, float)>& terrainGroundHeight) {
    float ndcX = 2.0f * screenX / screenW - 1.0f;
    float ndcY = 1.0f - 2.0f * screenY / screenH;
    HMM_Mat4 vp = HMM_MulM4((HMM_Mat4)proj, (HMM_Mat4)view);
    HMM_Mat4 invVP = HMM_InvGeneralM4(vp);
    HMM_Vec4 nearP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, -1.0f, 1.0f));
    HMM_Vec4 farP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, 1.0f, 1.0f));
    if (nearP.W != 0) { nearP.X /= nearP.W; nearP.Y /= nearP.W; nearP.Z /= nearP.W; }
    if (farP.W != 0) { farP.X /= farP.W; farP.Y /= farP.W; farP.Z /= farP.W; }
    const Vec3 rayOrigin(nearP.X, nearP.Y, nearP.Z);
    const Vec3 rayDirection = Normalize(Vec3(farP.X - nearP.X, farP.Y - nearP.Y, farP.Z - nearP.Z));

    if (terrainValid && rayDirection.y < -0.0001f && terrainGroundHeight) {
        const float maxDistance = std::max(8192.0f, terrainRadius * 4.0f);
        float previousT = 0.0f;

        for (int stepIndex = 1; stepIndex <= 256; ++stepIndex) {
            const float t = maxDistance * (static_cast<float>(stepIndex) / 256.0f);
            Vec3 point = rayOrigin + rayDirection * t;
            const float groundHeight = terrainGroundHeight(point.x, point.z);
            const float delta = point.y - groundHeight;

            if (delta <= 0.0f) {
                float lowT = previousT;
                float highT = t;
                Vec3 hitPoint = point;
                for (int iter = 0; iter < 12; ++iter) {
                    const float midT = (lowT + highT) * 0.5f;
                    Vec3 midPoint = rayOrigin + rayDirection * midT;
                    const float midGroundHeight = terrainGroundHeight(midPoint.x, midPoint.z);
                    const float midDelta = midPoint.y - midGroundHeight;
                    if (midDelta <= 0.0f) {
                        highT = midT;
                        hitPoint = Vec3(midPoint.x, midGroundHeight, midPoint.z);
                    } else {
                        lowT = midT;
                    }
                }
                return hitPoint;
            }

            previousT = t;
        }
    }

    float dx = farP.X - nearP.X, dy = farP.Y - nearP.Y, dz = farP.Z - nearP.Z;
    if (fabsf(dy) < 0.0001f) return Vec3(nearP.X, 0, nearP.Z);
    float t = -nearP.Y / dy;
    return Vec3(nearP.X + t * dx, 0, nearP.Z + t * dz);
}

PickRay BuildScreenPickRay(float screenX, float screenY, const Mat4& view, const Mat4& proj,
                           int screenW, int screenH) {
    float ndcX = 2.0f * screenX / screenW - 1.0f;
    float ndcY = 1.0f - 2.0f * screenY / screenH;
    HMM_Mat4 vp = HMM_MulM4((HMM_Mat4)proj, (HMM_Mat4)view);
    HMM_Mat4 invVP = HMM_InvGeneralM4(vp);
    HMM_Vec4 nearP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, -1.0f, 1.0f));
    HMM_Vec4 farP = HMM_MulM4V4(invVP, HMM_V4(ndcX, ndcY, 1.0f, 1.0f));
    if (nearP.W != 0) { nearP.X /= nearP.W; nearP.Y /= nearP.W; nearP.Z /= nearP.W; }
    if (farP.W != 0) { farP.X /= farP.W; farP.Y /= farP.W; farP.Z /= farP.W; }

    PickRay ray;
    ray.origin = Vec3(nearP.X, nearP.Y, nearP.Z);
    ray.direction = Normalize(Vec3(farP.X - nearP.X, farP.Y - nearP.Y, farP.Z - nearP.Z));
    return ray;
}

bool IntersectSphere(const PickRay& ray, const Vec3& center, float radius, float& outT) {
    const Vec3 oc = ray.origin - center;
    const float a = Dot(ray.direction, ray.direction);
    const float b = 2.0f * Dot(oc, ray.direction);
    const float c = Dot(oc, oc) - radius * radius;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return false;
    }

    const float sqrtDisc = std::sqrt(discriminant);
    const float inv2A = 0.5f / a;
    const float t0 = (-b - sqrtDisc) * inv2A;
    const float t1 = (-b + sqrtDisc) * inv2A;
    if (t0 >= 0.0f) {
        outT = t0;
        return true;
    }
    if (t1 >= 0.0f) {
        outT = t1;
        return true;
    }
    return false;
}

float DistanceToSegmentSquared(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float abLenSq = abx * abx + aby * aby;
    if (abLenSq <= 0.0001f) {
        const float dx = px - ax;
        const float dy = py - ay;
        return dx * dx + dy * dy;
    }
    const float t = std::clamp((apx * abx + apy * aby) / abLenSq, 0.0f, 1.0f);
    const float cx = ax + abx * t;
    const float cy = ay + aby * t;
    const float dx = px - cx;
    const float dy = py - cy;
    return dx * dx + dy * dy;
}

} // namespace picking
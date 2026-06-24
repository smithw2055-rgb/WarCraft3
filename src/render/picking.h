#pragma once
#include "render/math.h"
#include <functional>

namespace picking {

struct PickRay {
    Vec3 origin;
    Vec3 direction;
};

// Project a 3D world position to 2D screen coordinates (sokol top-left origin).
bool WorldToScreen(const Vec3& worldPos, const Mat4& view, const Mat4& proj,
                   int screenW, int screenH, float& outX, float& outY);

// Unproject a screen point to a world position on the terrain plane.
// `terrainValid` and `terrainGroundHeight` let the caller supply the terrain
// collision query without this module depending on TexturedTerrain.
Vec3 ScreenToWorld(float screenX, float screenY, const Mat4& view, const Mat4& proj,
                   int screenW, int screenH,
                   bool terrainValid,
                   float terrainRadius,
                   const std::function<float(float, float)>& terrainGroundHeight);

// Build a pick ray from a screen point.
PickRay BuildScreenPickRay(float screenX, float screenY, const Mat4& view, const Mat4& proj,
                           int screenW, int screenH);

// Ray/sphere intersection; returns first non-negative t.
bool IntersectSphere(const PickRay& ray, const Vec3& center, float radius, float& outT);

// Squared distance from point p to segment a-b.
float DistanceToSegmentSquared(float px, float py, float ax, float ay, float bx, float by);

} // namespace picking
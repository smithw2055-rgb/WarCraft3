#include "unit_selection_geom.h"
#include <algorithm>
#include <cmath>

namespace unit_selection_geom {

float GetTerrainHeightAt(float worldX, float worldY,
                         bool terrainValid,
                         const std::function<float(float, float)>& terrainGroundHeight) {
    return (terrainValid && terrainGroundHeight) ? terrainGroundHeight(worldX, worldY) : 0.0f;
}

float ComputeUnitVisualHeight(const CUnit& unit,
                              bool terrainValid,
                              const std::function<float(float, float)>& terrainGroundHeight) {
    float height = GetTerrainHeightAt(unit.GetX(), unit.GetY(), terrainValid, terrainGroundHeight);
    switch (unit.GetMovementType()) {
        case PathingGridMovementType::FLY:
        case PathingGridMovementType::HOVER:
        case PathingGridMovementType::FLOAT:
            height += unit.GetFlyHeight();
            break;
        default:
            break;
    }
    return height;
}

float ComputeUnitSelectionWorldRadius(const CUnit& unit) {
    float radius = unit.GetCollisionSize();
    if (radius <= 1.0f) {
        radius = unit.IsBuilding() ? 96.0f : 32.0f;
    }
    if (unit.IsBuilding()) {
        radius = std::max(radius, 72.0f);
    } else {
        radius = std::max(radius, 24.0f);
    }
    return radius;
}

float ComputeUnitSelectionVisualHeight(const CUnit& unit) {
    const CUnitType* unitType = unit.GetUnitType();
    float height = unitType ? unitType->GetImpactZ() : 0.0f;
    const float radius = ComputeUnitSelectionWorldRadius(unit);
    if (unit.IsBuilding()) {
        height = std::max(height, radius * 2.6f);
        height = std::max(height, 170.0f);
    } else {
        height = std::max(height, radius * 2.0f);
        height = std::max(height, 95.0f);
    }
    return height;
}

bool ComputeUnitSelectionScreenShape(const CUnit& unit,
                                     const Mat4& view, const Mat4& proj,
                                     int screenW, int screenH,
                                     bool terrainValid,
                                     const std::function<float(float, float)>& terrainGroundHeight,
                                     float& outFootX, float& outFootY,
                                     float& outHeadX, float& outHeadY,
                                     float& outRadiusPixels) {
    const float baseHeight = ComputeUnitVisualHeight(unit, terrainValid, terrainGroundHeight);
    const float worldRadius = ComputeUnitSelectionWorldRadius(unit);
    const float visualHeight = ComputeUnitSelectionVisualHeight(unit);

    const Vec3 footPos(unit.GetX(), baseHeight + 8.0f, unit.GetY());
    const Vec3 headPos(unit.GetX(), baseHeight + visualHeight, unit.GetY());
    if (!picking::WorldToScreen(footPos, view, proj, screenW, screenH, outFootX, outFootY)) {
        return false;
    }
    if (!picking::WorldToScreen(headPos, view, proj, screenW, screenH, outHeadX, outHeadY)) {
        return false;
    }

    const float midHeight = baseHeight + visualHeight * 0.5f;
    const Vec3 centerPos(unit.GetX(), midHeight, unit.GetY());
    float centerX = 0.0f;
    float centerY = 0.0f;
    if (!picking::WorldToScreen(centerPos, view, proj, screenW, screenH, centerX, centerY)) {
        centerX = (outFootX + outHeadX) * 0.5f;
        centerY = (outFootY + outHeadY) * 0.5f;
    }

    float radiusPixels = 0.0f;
    auto updateRadiusFromPoint = [&](const Vec3& point) {
        float sx = 0.0f;
        float sy = 0.0f;
        if (!picking::WorldToScreen(point, view, proj, screenW, screenH, sx, sy)) {
            return;
        }
        const float dx = sx - centerX;
        const float dy = sy - centerY;
        radiusPixels = std::max(radiusPixels, std::sqrt(dx * dx + dy * dy));
    };
    updateRadiusFromPoint(Vec3(unit.GetX() + worldRadius, midHeight, unit.GetY()));
    updateRadiusFromPoint(Vec3(unit.GetX(), midHeight, unit.GetY() + worldRadius));

    outRadiusPixels = std::max(radiusPixels, unit.IsBuilding() ? 28.0f : 14.0f);
    return true;
}

} // namespace unit_selection_geom
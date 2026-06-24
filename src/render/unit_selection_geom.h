#pragma once
#include "render/picking.h"
#include "ecs/unit.h"
#include <functional>

namespace unit_selection_geom {

// Sample terrain height at a world position (no-op if terrain invalid).
float GetTerrainHeightAt(float worldX, float worldY,
                         bool terrainValid,
                         const std::function<float(float, float)>& terrainGroundHeight);

// World Y (height) of a unit's visual placement, including fly height for air units.
float ComputeUnitVisualHeight(const CUnit& unit,
                              bool terrainValid,
                              const std::function<float(float, float)>& terrainGroundHeight);

// Selection radius in world units with sensible minimums based on building/non-building.
float ComputeUnitSelectionWorldRadius(const CUnit& unit);

// Approximate selection-marker visual height (foot→head span).
float ComputeUnitSelectionVisualHeight(const CUnit& unit);

// Project a unit's selection shape (foot + head on screen + radius in pixels).
bool ComputeUnitSelectionScreenShape(const CUnit& unit,
                                     const Mat4& view, const Mat4& proj,
                                     int screenW, int screenH,
                                     bool terrainValid,
                                     const std::function<float(float, float)>& terrainGroundHeight,
                                     float& outFootX, float& outFootY,
                                     float& outHeadX, float& outHeadY,
                                     float& outRadiusPixels);

} // namespace unit_selection_geom
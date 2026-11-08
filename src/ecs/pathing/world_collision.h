#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "ecs/fwd.h"

class SpatialHashGrid;

// ============================================================================
// CWorldCollision - Engine Spatial Collision & Proximity Query Server (Phase 25)
//
// - Integrates with `SpatialHashGrid` for O(1) cell-bucketed radius/rect
//   queries when populated, with automatic exact fallback for standalone unit tests.
// - Provides `FindNearestUnit` and `CountUnitsInRange` query primitives and
//   tracks collision/range query telemetry counters.
// ============================================================================
class CWorldCollision {
public:
    CWorldCollision();

    void BindSpatialGrid(const SpatialHashGrid* grid) { spatialGrid_ = grid; }
    const SpatialHashGrid* GetSpatialGrid() const { return spatialGrid_; }

    void AddUnit(CUnit* unit);
    void RemoveUnit(CUnit* unit);
    void UpdateUnit(CUnit* unit);

    void AddItem(CItem* item);
    void RemoveItem(CItem* item);

    void EnumUnitsInRect(float x, float y, float w, float h,
                         std::vector<CUnit*>& out) const;
    void EnumUnitsInRange(float cx, float cy, float radius,
                          std::vector<CUnit*>& out) const;
    void EnumItemsInRect(float x, float y, float w, float h,
                         std::vector<CItem*>& out) const;

    // Engine proximity helpers (Phase 25)
    CUnit* FindNearestUnit(float cx, float cy, float maxRadius,
                           const std::function<bool(const CUnit*)>& predicate = nullptr) const;
    size_t CountUnitsInRange(float cx, float cy, float radius) const;

    bool CheckCollision(float x, float y, float radius,
                        CUnit* ignoreUnit = nullptr) const;

    uint64_t GetCollisionChecksPerformed() const { return collisionChecksPerformed_; }
    uint64_t GetRangeQueriesPerformed() const { return rangeQueriesPerformed_; }
    void ResetTelemetry() const {
        collisionChecksPerformed_ = 0;
        rangeQueriesPerformed_ = 0;
    }

private:
    std::vector<CUnit*> allUnits_;
    std::vector<CItem*> allItems_;
    const SpatialHashGrid* spatialGrid_ = nullptr;
    mutable uint64_t collisionChecksPerformed_ = 0;
    mutable uint64_t rangeQueriesPerformed_ = 0;
};

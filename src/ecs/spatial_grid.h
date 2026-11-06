#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/unit.h"

// ============================================================
// SpatialHashGrid (Phase 5 Engine Modernization - Stage C)
//
// Deterministic 2D uniform spatial hash grid for O(1) cell lookup,
// fast neighborhood queries, and O(N) unit collision separation.
//
// Designed for RTS lockstep determinism:
// - Cell coordinates are quantized via std::floor(pos / cellSize).
// - Queries return candidates sorted by HandleId when deterministic
//   ordering is requested.
// ============================================================
class SpatialHashGrid {
public:
    explicit SpatialHashGrid(float cellSize = 256.0f)
        : cellSize_(std::max(16.0f, cellSize)), invCellSize_(1.0f / std::max(16.0f, cellSize)) {}

    void SetCellSize(float cellSize) {
        cellSize_ = std::max(16.0f, cellSize);
        invCellSize_ = 1.0f / cellSize_;
    }

    float GetCellSize() const { return cellSize_; }

    void Clear() {
        cells_.clear();
        entityCount_ = 0;
    }

    int WorldToCell(float coord) const {
        return static_cast<int>(std::floor(coord * invCellSize_));
    }

    static int64_t PackCellKey(int cx, int cy) {
        return (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cy);
    }

    // Insert a single unit into all grid cells overlapped by its bounding radius
    void Insert(CUnit* unit) {
        if (!unit || unit->IsDead() || unit->IsHidden() || unit->IsLoadedAboard()) {
            return;
        }
        const float r = std::max(8.0f, unit->GetCollisionSize());
        const float x = unit->GetX();
        const float y = unit->GetY();
        const int minCx = WorldToCell(x - r);
        const int maxCx = WorldToCell(x + r);
        const int minCy = WorldToCell(y - r);
        const int maxCy = WorldToCell(y + r);

        for (int cy = minCy; cy <= maxCy; ++cy) {
            for (int cx = minCx; cx <= maxCx; ++cx) {
                cells_[PackCellKey(cx, cy)].push_back(unit);
            }
        }
        entityCount_++;
    }

    // Rebuild the entire spatial hash grid from the active simulation unit list
    void Rebuild(const std::vector<CUnit*>& units) {
        Clear();
        for (CUnit* u : units) {
            Insert(u);
        }
    }

    // Query all live units within `radius` of (x, y) (center-to-center distance <= radius)
    std::vector<CUnit*> QueryRadius(float x, float y, float radius, bool sortByHandle = true) const {
        std::vector<CUnit*> result;
        if (radius < 0.0f || cells_.empty()) return result;

        const int minCx = WorldToCell(x - radius);
        const int maxCx = WorldToCell(x + radius);
        const int minCy = WorldToCell(y - radius);
        const int maxCy = WorldToCell(y + radius);
        const float rSq = radius * radius;

        std::unordered_set<int> visitedHandles;
        for (int cy = minCy; cy <= maxCy; ++cy) {
            for (int cx = minCx; cx <= maxCx; ++cx) {
                auto it = cells_.find(PackCellKey(cx, cy));
                if (it == cells_.end()) continue;
                for (CUnit* u : it->second) {
                    if (!u || u->IsDead() || u->IsHidden() || u->IsLoadedAboard()) continue;
                    const int h = u->GetHandleId();
                    if (!visitedHandles.insert(h).second) continue;
                    const float dx = u->GetX() - x;
                    const float dy = u->GetY() - y;
                    if (dx * dx + dy * dy <= rSq) {
                        result.push_back(u);
                    }
                }
            }
        }

        if (sortByHandle && result.size() > 1) {
            std::sort(result.begin(), result.end(), [](const CUnit* a, const CUnit* b) {
                return a->GetHandleId() < b->GetHandleId();
            });
        }
        return result;
    }

    // Query all candidate units in the 3x3 neighborhood of `unit`'s cell(s)
    void QueryPotentialColliders(const CUnit* unit, std::vector<CUnit*>& outCandidates) const {
        outCandidates.clear();
        if (!unit || cells_.empty()) return;

        const float r = std::max(8.0f, unit->GetCollisionSize()) + maxExpectedRadius_;
        const float x = unit->GetX();
        const float y = unit->GetY();
        const int minCx = WorldToCell(x - r);
        const int maxCx = WorldToCell(x + r);
        const int minCy = WorldToCell(y - r);
        const int maxCy = WorldToCell(y + r);

        const int selfHandle = unit->GetHandleId();
        for (int cy = minCy; cy <= maxCy; ++cy) {
            for (int cx = minCx; cx <= maxCx; ++cx) {
                auto it = cells_.find(PackCellKey(cx, cy));
                if (it == cells_.end()) continue;
                for (CUnit* other : it->second) {
                    if (!other || other->GetHandleId() <= selfHandle) continue;
                    outCandidates.push_back(other);
                }
            }
        }

        if (outCandidates.size() > 1) {
            std::sort(outCandidates.begin(), outCandidates.end(), [](const CUnit* a, const CUnit* b) {
                return a->GetHandleId() < b->GetHandleId();
            });
            outCandidates.erase(std::unique(outCandidates.begin(), outCandidates.end()), outCandidates.end());
        }
    }

    size_t ActiveCellCount() const { return cells_.size(); }
    size_t IndexedEntityCount() const { return entityCount_; }

private:
    float cellSize_ = 256.0f;
    float invCellSize_ = 1.0f / 256.0f;
    float maxExpectedRadius_ = 144.0f;
    size_t entityCount_ = 0;
    std::unordered_map<int64_t, std::vector<CUnit*>> cells_;
};

#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <functional>
#include <cmath>
#include <algorithm>
#include "ecs/fwd.h"
#include "ecs/unit_type.h"
#include "map/wpm.h"

// Zero-Allocation Flat A* Pathfinding Processor with WPM Pathing Map Integration
class CPathfindingProcessor {
public:
    enum class GridMapping : uint8_t {
        CELLS,
        CORNERS
    };

    CPathfindingProcessor();

    // Set the pathing map data (from war3map.wpm)
    void SetPathingMap(WpmData* wpm, float worldOffsetX = 0.0f, float worldOffsetY = 0.0f) {
        pathingMap_ = (wpm && wpm->IsValid()) ? wpm : nullptr;
        worldOffsetX_ = worldOffsetX;
        worldOffsetY_ = worldOffsetY;
        // WPM stamps use the same cell origin (ref PathingGrid.centerOffset).
        if (pathingMap_) {
            pathingMap_->originX = worldOffsetX;
            pathingMap_->originY = worldOffsetY;
            if (pathingMap_->basePathing.size() != pathingMap_->pathing.size()) {
                pathingMap_->basePathing = pathingMap_->pathing;
            }
        }
    }
    void SetWorldCollision(CWorldCollision* worldCollision) { worldCollision_ = worldCollision; }

    // Phase 25: Pluggable cell passability override hook (returns -1 to use default WPM check, 0 = blocked, 1 = pathable)
    using CustomCellPassabilityFn = std::function<int(int cellX, int cellY, PathingGridMovementType movementType)>;
    void SetCustomPassabilityChecker(CustomCellPassabilityFn fn) { customPassabilityChecker_ = std::move(fn); }
    const CustomCellPassabilityFn& GetCustomPassabilityChecker() const { return customPassabilityChecker_; }

    // Find a path on the cell grid, returns true if found
    bool FindPath(int startCellX, int startCellY, int endCellX, int endCellY,
                  std::vector<std::pair<int,int>>& outPath) const;

    // Convenience: find path using world coordinates
    bool FindPathWorld(float startX, float startY, float endX, float endY,
                       std::vector<std::pair<float,float>>& outWorldPath) const;
    bool FindPathWorld(float startX, float startY, float endX, float endY,
                       float collisionSize,
                       std::vector<std::pair<float,float>>& outWorldPath) const;
    bool FindPathWorld(float startX, float startY, float endX, float endY,
                       float collisionSize,
                       PathingGridMovementType movementType,
                       CUnit* ignoreUnit,
                       std::vector<std::pair<float,float>>& outWorldPath) const;
    bool IsWorldPointPathableForUnit(float worldX, float worldY,
                                     float collisionSize,
                                     PathingGridMovementType movementType,
                                     CUnit* ignoreUnit) const;

    // Check if a cell is walkable (within bounds and not blocked)
    bool IsCellWalkable(int cellX, int cellY, bool allowFlying = false) const;
    // Movement-type pathing (ref PathingGrid.MovementType.isPathable):
    // FLY needs flyable air, FLOAT needs swimmable water, AMPHIBIOUS takes
    // either, DISABLED passes, everything else needs walkable ground.
    // Boundary-ring cells fail for every type.
    bool IsCellPathableFor(int cellX, int cellY,
                           PathingGridMovementType movementType) const;
    bool HasPathingMap() const { return pathingMap_ && pathingMap_->IsValid(); }
    // Mutable access for script pathing edits (ref SetTerrainPathable).
    // The map stays loader-owned; edits are in place.
    WpmData* GetPathingMap() const { return pathingMap_; }
    static bool IsCollisionSizeBetterSuitedForCorners(float collisionSize);
    static GridMapping SelectGridMapping(float collisionSize) {
        return IsCollisionSizeBetterSuitedForCorners(collisionSize) ? GridMapping::CORNERS
                                                                    : GridMapping::CELLS;
    }

    // Convert world coords to cell coords
    int WorldToCellX(float worldX) const { return static_cast<int>((worldX - worldOffsetX_) / CELL_SIZE); }
    int WorldToCellY(float worldY) const { return static_cast<int>((worldY - worldOffsetY_) / CELL_SIZE); }
    float CellToWorldX(int cellX) const { return worldOffsetX_ + (float)cellX * CELL_SIZE + CELL_SIZE * 0.5f; }
    float CellToWorldY(int cellY) const { return worldOffsetY_ + (float)cellY * CELL_SIZE + CELL_SIZE * 0.5f; }
    int WorldToCornerX(float worldX) const { return static_cast<int>(((worldX + CELL_SIZE * 0.5f) - worldOffsetX_) / CELL_SIZE); }
    int WorldToCornerY(float worldY) const { return static_cast<int>(((worldY + CELL_SIZE * 0.5f) - worldOffsetY_) / CELL_SIZE); }
    float CornerToWorldX(int cornerX) const { return worldOffsetX_ + (float)cornerX * CELL_SIZE; }
    float CornerToWorldY(int cornerY) const { return worldOffsetY_ + (float)cornerY * CELL_SIZE; }

    // Zero-Allocation Flat A* Workspace Telemetry
    uint64_t GetZeroAllocQueriesExecuted() const { return flatWorkspace_.queriesExecuted; }
    uint64_t GetWorkspaceBufferAllocations() const { return flatWorkspace_.bufferAllocations; }
    uint64_t GetTotalNodesExpanded() const { return flatWorkspace_.totalNodesExpanded; }
    void ResetWorkspaceStats() const {
        flatWorkspace_.queriesExecuted = 0;
        flatWorkspace_.bufferAllocations = 0;
        flatWorkspace_.totalNodesExpanded = 0;
    }

    static constexpr int CELL_SIZE = 32;

private:
    struct FlatHeapEntry {
        float f = 0.0f;
        int nodeIdx = 0;
        uint32_t seq = 0;
        bool operator>(const FlatHeapEntry& other) const {
            if (f != other.f) return f > other.f;
            return seq > other.seq;
        }
    };

    struct FlatAStarWorkspace {
        int capacity = 0;
        uint32_t currentGen = 1;
        uint64_t queriesExecuted = 0;
        uint64_t bufferAllocations = 0;
        uint64_t totalNodesExpanded = 0;
        std::vector<uint32_t> visitedGen;
        std::vector<uint32_t> closedGen;
        std::vector<uint32_t> goalGen;
        std::vector<float> gScore;
        std::vector<float> fScore;
        std::vector<int32_t> parentIdx;
        std::vector<FlatHeapEntry> openHeap;

        void BeginSearch(int requiredNodes) {
            if (requiredNodes > capacity) {
                capacity = std::max(requiredNodes, 4096);
                visitedGen.assign(static_cast<size_t>(capacity), 0u);
                closedGen.assign(static_cast<size_t>(capacity), 0u);
                goalGen.assign(static_cast<size_t>(capacity), 0u);
                gScore.assign(static_cast<size_t>(capacity), 0.0f);
                fScore.assign(static_cast<size_t>(capacity), 0.0f);
                parentIdx.assign(static_cast<size_t>(capacity), -1);
                currentGen = 1u;
                bufferAllocations++;
            } else {
                currentGen++;
                if (currentGen == 0u) {
                    std::fill(visitedGen.begin(), visitedGen.end(), 0u);
                    std::fill(closedGen.begin(), closedGen.end(), 0u);
                    std::fill(goalGen.begin(), goalGen.end(), 0u);
                    currentGen = 1u;
                }
            }
            openHeap.clear();
            queriesExecuted++;
        }
    };

    bool FindPathMapped(int startNodeX, int startNodeY,
                        int endNodeX, int endNodeY,
                        float exactStartWorldX,
                        float exactStartWorldY,
                        float exactEndWorldX,
                        float exactEndWorldY,
                        GridMapping mapping,
                        float collisionSize,
                        PathingGridMovementType movementType,
                        CUnit* ignoreUnit,
                        std::vector<std::pair<int,int>>& outPath) const;
    bool FindNearestPathableNode(int startNodeX, int startNodeY,
                                 GridMapping mapping,
                                 float collisionSize,
                                 PathingGridMovementType movementType,
                                 CUnit* ignoreUnit,
                                 int& outX, int& outY) const;
    bool IsMappedNodePathable(int nodeX, int nodeY, GridMapping mapping, float collisionSize,
                              PathingGridMovementType movementType, CUnit* ignoreUnit) const;
    bool IsWorldPointPathable(float worldX, float worldY, float collisionSize,
                              PathingGridMovementType movementType,
                              CUnit* ignoreUnit) const;
    bool IsWorldSegmentPathable(float startX, float startY,
                                float endX, float endY,
                                float collisionSize,
                                PathingGridMovementType movementType,
                                CUnit* ignoreUnit) const;
    bool IsMappedNodeInBounds(int nodeX, int nodeY, GridMapping mapping) const;
    int MappedWidth(GridMapping mapping) const;
    int MappedHeight(GridMapping mapping) const;
    float MappedNodeToWorldX(int nodeX, GridMapping mapping) const;
    float MappedNodeToWorldY(int nodeY, GridMapping mapping) const;
    int WorldToMappedX(float worldX, GridMapping mapping) const;
    int WorldToMappedY(float worldY, GridMapping mapping) const;

    bool FindPathMappedPreResolved(int resolvedStartX, int resolvedStartY,
                                   int resolvedEndX, int resolvedEndY,
                                   float exactStartWorldX, float exactStartWorldY,
                                   float exactEndWorldX, float exactEndWorldY,
                                   GridMapping mapping, float collisionSize,
                                   PathingGridMovementType movementType,
                                   CUnit* ignoreUnit,
                                   std::vector<std::pair<int,int>>& outPath) const;

    WpmData* pathingMap_ = nullptr;
    CustomCellPassabilityFn customPassabilityChecker_;
    CWorldCollision* worldCollision_ = nullptr;
    float worldOffsetX_ = 0.0f;
    float worldOffsetY_ = 0.0f;
    mutable FlatAStarWorkspace flatWorkspace_;
};

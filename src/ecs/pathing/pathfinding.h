#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <functional>
#include <cmath>
#include "ecs/fwd.h"
#include "ecs/unit_type.h"
#include "map/wpm.h"

// Simple A* pathfinding processor with pathing map integration
class CPathfindingProcessor {
public:
    enum class GridMapping : uint8_t {
        CELLS,
        CORNERS
    };

    CPathfindingProcessor();

    // Set the pathing map data (from war3map.wpm)
    void SetPathingMap(const WpmData* wpm, float worldOffsetX = 0.0f, float worldOffsetY = 0.0f) {
        pathingMap_ = (wpm && wpm->IsValid()) ? wpm : nullptr;
        worldOffsetX_ = worldOffsetX;
        worldOffsetY_ = worldOffsetY;
    }
    void SetWorldCollision(CWorldCollision* worldCollision) { worldCollision_ = worldCollision; }

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
    bool HasPathingMap() const { return pathingMap_ && pathingMap_->IsValid(); }
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

    static constexpr int CELL_SIZE = 32;

private:
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

    const WpmData* pathingMap_ = nullptr;
    CWorldCollision* worldCollision_ = nullptr;
    float worldOffsetX_ = 0.0f;
    float worldOffsetY_ = 0.0f;
};

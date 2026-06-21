#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <functional>
#include "ecs/fwd.h"
#include "map/wpm.h"

// Simple A* pathfinding processor with pathing map integration
class CPathfindingProcessor {
public:
    CPathfindingProcessor();

    // Set the pathing map data (from war3map.wpm)
    void SetPathingMap(const WpmData* wpm) { pathingMap_ = wpm; }

    // Find a path on the cell grid, returns true if found
    bool FindPath(int startCellX, int startCellY, int endCellX, int endCellY,
                  std::vector<std::pair<int,int>>& outPath) const;

    // Convenience: find path using world coordinates
    bool FindPathWorld(float startX, float startY, float endX, float endY,
                       std::vector<std::pair<float,float>>& outWorldPath) const;

    // Check if a cell is walkable (within bounds and not blocked)
    bool IsCellWalkable(int cellX, int cellY, bool allowFlying = false) const;

    // Convert world coords to cell coords
    static int WorldToCell(float world) { return (int)(world / CELL_SIZE); }
    static float CellToWorld(int cell) { return (float)cell * CELL_SIZE + CELL_SIZE * 0.5f; }

    static constexpr int CELL_SIZE = 128;

private:
    const WpmData* pathingMap_ = nullptr;
};

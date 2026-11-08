#include "pathfinding.h"
#include "debug/move_debug.h"
#include "ecs/unit.h"
#include "world_collision.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <sstream>

CPathfindingProcessor::CPathfindingProcessor() {}

namespace {

const char* GridMappingName(CPathfindingProcessor::GridMapping mapping) {
    return mapping == CPathfindingProcessor::GridMapping::CORNERS ? "corners" : "cells";
}

void LogPathDebug(const CUnit* unit, const char* phase, float startX, float startY, float endX, float endY,
                  const std::string& extra = std::string()) {
    if (!IsMoveDebugLoggingEnabled()) {
        return;
    }
    std::ostringstream oss;
    oss << "[MoveDebug] path";
    if (unit) {
        oss << " unit=" << unit->GetHandleId()
            << " type=" << unit->GetTypeId().ToString();
    }
    oss << " phase=" << phase
        << " start=(" << startX << ", " << startY << ")"
        << " end=(" << endX << ", " << endY << ")";
    if (!extra.empty()) {
        oss << " " << extra;
    }
    MoveDebugLogLine(oss.str());
}

} // namespace

bool CPathfindingProcessor::IsCollisionSizeBetterSuitedForCorners(float collisionSize) {
    return (((2 * static_cast<int>(collisionSize)) / CELL_SIZE) % 2) == 1;
}

namespace {

bool UsesDynamicGroundCollision(PathingGridMovementType movementType) {
    switch (movementType) {
        case PathingGridMovementType::FLY:
        case PathingGridMovementType::FLOAT:
        case PathingGridMovementType::DISABLED:
        case PathingGridMovementType::FOOT_NO_COLLISION:
            return false;
        default:
            return true;
    }
}

}

bool CPathfindingProcessor::IsCellWalkable(int cellX, int cellY, bool allowFlying) const {
    return IsCellPathableFor(cellX, cellY,
                             allowFlying ? PathingGridMovementType::FLY
                                         : PathingGridMovementType::FOOT);
}

bool CPathfindingProcessor::IsCellPathableFor(int cellX, int cellY,
                                              PathingGridMovementType movementType) const {
    if (customPassabilityChecker_) {
        const int overrideRes = customPassabilityChecker_(cellX, cellY, movementType);
        if (overrideRes >= 0) return overrideRes != 0;
    }
    if (!pathingMap_) return true;
    if (pathingMap_->IsBoundary(cellX, cellY)) return false;
    switch (movementType) {
        case PathingGridMovementType::FLY:
            return pathingMap_->IsFlyable(cellX, cellY);
        case PathingGridMovementType::FLOAT:
            return pathingMap_->IsWater(cellX, cellY);
        case PathingGridMovementType::AMPHIBIOUS:
            return pathingMap_->IsWalkable(cellX, cellY) ||
                   pathingMap_->IsWater(cellX, cellY);
        case PathingGridMovementType::DISABLED:
            return true;
        case PathingGridMovementType::FOOT:
        case PathingGridMovementType::HOVER:
        case PathingGridMovementType::HORSE:
        case PathingGridMovementType::FOOT_NO_COLLISION:
        default:
            return pathingMap_->IsWalkable(cellX, cellY);
    }
}

int CPathfindingProcessor::MappedWidth(GridMapping mapping) const {
    if (!pathingMap_) {
        return 0;
    }
    return pathingMap_->width + (mapping == GridMapping::CORNERS ? 1 : 0);
}

int CPathfindingProcessor::MappedHeight(GridMapping mapping) const {
    if (!pathingMap_) {
        return 0;
    }
    return pathingMap_->height + (mapping == GridMapping::CORNERS ? 1 : 0);
}

bool CPathfindingProcessor::IsMappedNodeInBounds(int nodeX, int nodeY, GridMapping mapping) const {
    return nodeX >= 0 && nodeY >= 0
        && nodeX < MappedWidth(mapping)
        && nodeY < MappedHeight(mapping);
}

float CPathfindingProcessor::MappedNodeToWorldX(int nodeX, GridMapping mapping) const {
    return mapping == GridMapping::CORNERS ? CornerToWorldX(nodeX) : CellToWorldX(nodeX);
}

float CPathfindingProcessor::MappedNodeToWorldY(int nodeY, GridMapping mapping) const {
    return mapping == GridMapping::CORNERS ? CornerToWorldY(nodeY) : CellToWorldY(nodeY);
}

int CPathfindingProcessor::WorldToMappedX(float worldX, GridMapping mapping) const {
    return mapping == GridMapping::CORNERS ? WorldToCornerX(worldX) : WorldToCellX(worldX);
}

int CPathfindingProcessor::WorldToMappedY(float worldY, GridMapping mapping) const {
    return mapping == GridMapping::CORNERS ? WorldToCornerY(worldY) : WorldToCellY(worldY);
}

bool CPathfindingProcessor::IsWorldPointPathable(float worldX, float worldY, float collisionSize,
                                                 PathingGridMovementType movementType,
                                                 CUnit* ignoreUnit) const {
    if (!pathingMap_) {
        if (worldCollision_ && UsesDynamicGroundCollision(movementType) && collisionSize > 0.0f) {
            return !worldCollision_->CheckCollision(worldX, worldY, collisionSize, ignoreUnit);
        }
        return true;
    }

    auto isSampleWalkable = [&](float sampleX, float sampleY) {
        const int cellX = WorldToCellX(sampleX);
        const int cellY = WorldToCellY(sampleY);
        return IsCellPathableFor(cellX, cellY, movementType);
    };

    if (collisionSize <= 0.0f) {
        return isSampleWalkable(worldX, worldY);
    }

    for (int iy = -1; iy <= 1; ++iy) {
        for (int ix = -1; ix <= 1; ++ix) {
            if (!isSampleWalkable(worldX + (ix * collisionSize), worldY + (iy * collisionSize))) {
                return false;
            }
        }
    }
    return true;
}

bool CPathfindingProcessor::IsMappedNodePathable(int nodeX, int nodeY,
                                                 GridMapping mapping,
                                                 float collisionSize,
                                                 PathingGridMovementType movementType,
                                                 CUnit* ignoreUnit) const {
    if (!IsMappedNodeInBounds(nodeX, nodeY, mapping)) {
        return false;
    }
    return IsWorldPointPathable(MappedNodeToWorldX(nodeX, mapping),
                                MappedNodeToWorldY(nodeY, mapping),
                                collisionSize,
                                movementType,
                                ignoreUnit);
}

bool CPathfindingProcessor::IsWorldSegmentPathable(float startX, float startY,
                                                   float endX, float endY,
                                                   float collisionSize,
                                                   PathingGridMovementType movementType,
                                                   CUnit* ignoreUnit) const {
    const float dx = endX - startX;
    const float dy = endY - startY;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const int sampleCount = std::max(1, static_cast<int>(std::ceil(distance / 8.0f)));
    for (int i = 0; i <= sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleCount);
        const float sampleX = startX + dx * t;
        const float sampleY = startY + dy * t;
        if (!IsWorldPointPathable(sampleX, sampleY, collisionSize, movementType, ignoreUnit)) {
            return false;
        }
    }
    return true;
}

bool CPathfindingProcessor::FindNearestPathableNode(int startNodeX, int startNodeY,
                                                    GridMapping mapping,
                                                    float collisionSize,
                                                    PathingGridMovementType movementType,
                                                    CUnit* ignoreUnit,
                                                    int& outX, int& outY) const {
    const int width = MappedWidth(mapping);
    const int height = MappedHeight(mapping);
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (IsMappedNodePathable(startNodeX, startNodeY, mapping, collisionSize, movementType, ignoreUnit)) {
        outX = startNodeX;
        outY = startNodeY;
        return true;
    }

    const int maxRadius = std::max(width, height);
    for (int radius = 1; radius < maxRadius; ++radius) {
        const int minX = startNodeX - radius;
        const int maxX = startNodeX + radius;
        const int minY = startNodeY - radius;
        const int maxY = startNodeY + radius;
        bool foundCandidate = false;
        int bestX = startNodeX;
        int bestY = startNodeY;
        float bestDistSq = std::numeric_limits<float>::max();

        auto testCandidate = [&](int x, int y) {
            if (IsMappedNodeInBounds(x, y, mapping)
                && IsMappedNodePathable(x, y, mapping, collisionSize, movementType, ignoreUnit)) {
                const float dx = static_cast<float>(x - startNodeX);
                const float dy = static_cast<float>(y - startNodeY);
                const float distSq = dx * dx + dy * dy;
                if (!foundCandidate || (distSq < bestDistSq)) {
                    foundCandidate = true;
                    bestDistSq = distSq;
                    bestX = x;
                    bestY = y;
                }
            }
        };
        // Top and bottom edges of the perimeter ring: O(R) instead of O(R^2)
        for (int x = minX; x <= maxX; ++x) {
            testCandidate(x, minY);
            testCandidate(x, maxY);
        }
        // Left and right interior vertical edges of the perimeter ring
        for (int y = minY + 1; y < maxY; ++y) {
            testCandidate(minX, y);
            testCandidate(maxX, y);
        }
        if (foundCandidate) {
            outX = bestX;
            outY = bestY;
            return true;
        }
    }
    return false;
}

bool CPathfindingProcessor::FindPathMapped(int startNodeX, int startNodeY,
                                           int endNodeX, int endNodeY,
                                           float exactStartWorldX,
                                           float exactStartWorldY,
                                           float exactEndWorldX,
                                           float exactEndWorldY,
                                           GridMapping mapping,
                                           float collisionSize,
                                           PathingGridMovementType movementType,
                                           CUnit* ignoreUnit,
                                           std::vector<std::pair<int,int>>& outPath) const {
    outPath.clear();

    // If no pathing map, return direct path
    if (!pathingMap_) {
        outPath.push_back({startNodeX, startNodeY});
        outPath.push_back({endNodeX, endNodeY});
        return true;
    }

    int resolvedStartX = startNodeX;
    int resolvedStartY = startNodeY;
    int resolvedEndX = endNodeX;
    int resolvedEndY = endNodeY;

    if (!IsMappedNodePathable(resolvedStartX, resolvedStartY, mapping, collisionSize, movementType, ignoreUnit)) {
        if (!FindNearestPathableNode(resolvedStartX, resolvedStartY, mapping, collisionSize, movementType, ignoreUnit,
                                     resolvedStartX, resolvedStartY)) {
            return false;
        }
    }
    if (!IsMappedNodePathable(resolvedEndX, resolvedEndY, mapping, collisionSize, movementType, ignoreUnit)) {
        if (!FindNearestPathableNode(resolvedEndX, resolvedEndY, mapping, collisionSize, movementType, ignoreUnit,
                                     resolvedEndX, resolvedEndY)) {
            return false;
        }
    }
    return FindPathMappedPreResolved(resolvedStartX, resolvedStartY, resolvedEndX, resolvedEndY,
                                     exactStartWorldX, exactStartWorldY, exactEndWorldX, exactEndWorldY,
                                     mapping, collisionSize, movementType, ignoreUnit, outPath);
}

bool CPathfindingProcessor::FindPathMappedPreResolved(int resolvedStartX, int resolvedStartY,
                                                      int resolvedEndX, int resolvedEndY,
                                                      float exactStartWorldX, float exactStartWorldY,
                                                      float exactEndWorldX, float exactEndWorldY,
                                                      GridMapping mapping, float collisionSize,
                                                      PathingGridMovementType movementType,
                                                      CUnit* ignoreUnit,
                                                      std::vector<std::pair<int,int>>& outPath) const {
    outPath.clear();
    if (!pathingMap_) {
        outPath.push_back({resolvedStartX, resolvedStartY});
        outPath.push_back({resolvedEndX, resolvedEndY});
        return true;
    }

    // If cells are adjacent or same, direct path
    int dx = std::abs(resolvedEndX - resolvedStartX);
    int dy = std::abs(resolvedEndY - resolvedStartY);
    if (dx <= 1 && dy <= 1) {
        const float startWorldX = MappedNodeToWorldX(resolvedStartX, mapping);
        const float startWorldY = MappedNodeToWorldY(resolvedStartY, mapping);
        const float endWorldX = MappedNodeToWorldX(resolvedEndX, mapping);
        const float endWorldY = MappedNodeToWorldY(resolvedEndY, mapping);
        if (IsWorldSegmentPathable(startWorldX, startWorldY, endWorldX, endWorldY,
                                   collisionSize, movementType, ignoreUnit)) {
            outPath.push_back({resolvedStartX, resolvedStartY});
            outPath.push_back({resolvedEndX, resolvedEndY});
            return true;
        }
    }

    const int gridW = MappedWidth(mapping);
    const int gridH = MappedHeight(mapping);
    if (gridW <= 0 || gridH <= 0) {
        return false;
    }

    // Phase B1: Zero-allocation generation-stamped flat A* workspace
    flatWorkspace_.BeginSearch(gridW * gridH);
    const uint32_t gen = flatWorkspace_.currentGen;
    uint32_t nextSeq = 1u;

    auto nodeIdx = [gridW](int x, int y) -> int {
        return y * gridW + x;
    };
    auto heuristic = [](int ax, int ay, int bx, int by) -> float {
        const float ddx = static_cast<float>(bx - ax);
        const float ddy = static_cast<float>(by - ay);
        return std::sqrt(ddx * ddx + ddy * ddy);
    };
    auto worldDistanceToGoal = [&](int nodeX, int nodeY) -> float {
        const float ddx = MappedNodeToWorldX(nodeX, mapping) - exactEndWorldX;
        const float ddy = MappedNodeToWorldY(nodeY, mapping) - exactEndWorldY;
        return std::sqrt(ddx * ddx + ddy * ddy);
    };
    auto pushOpen = [&](float f, int idx) {
        flatWorkspace_.openHeap.push_back({f, idx, nextSeq++});
        std::push_heap(flatWorkspace_.openHeap.begin(),
                       flatWorkspace_.openHeap.end(),
                       std::greater<FlatHeapEntry>{});
    };
    auto popOpen = [&]() -> FlatHeapEntry {
        std::pop_heap(flatWorkspace_.openHeap.begin(),
                      flatWorkspace_.openHeap.end(),
                      std::greater<FlatHeapEntry>{});
        FlatHeapEntry top = flatWorkspace_.openHeap.back();
        flatWorkspace_.openHeap.pop_back();
        return top;
    };

    const float bestGoalDistance = worldDistanceToGoal(resolvedEndX, resolvedEndY);
    for (int gy = resolvedEndY - 1; gy <= (resolvedEndY + 1); ++gy) {
        for (int gx = resolvedEndX - 1; gx <= (resolvedEndX + 1); ++gx) {
            if (!IsMappedNodeInBounds(gx, gy, mapping)) {
                continue;
            }
            if (worldDistanceToGoal(gx, gy) <= (bestGoalDistance + 0.001f)) {
                flatWorkspace_.goalGen[static_cast<size_t>(nodeIdx(gx, gy))] = gen;
            }
        }
    }
    if (IsMappedNodeInBounds(resolvedEndX, resolvedEndY, mapping)) {
        flatWorkspace_.goalGen[static_cast<size_t>(nodeIdx(resolvedEndX, resolvedEndY))] = gen;
    }

    const float resolvedStartWorldX = MappedNodeToWorldX(resolvedStartX, mapping);
    const float resolvedStartWorldY = MappedNodeToWorldY(resolvedStartY, mapping);
    int startGridMinX = resolvedStartX;
    int startGridMaxX = resolvedStartX;
    int startGridMinY = resolvedStartY;
    int startGridMaxY = resolvedStartY;
    if (exactStartWorldX > resolvedStartWorldX) {
        startGridMaxX = resolvedStartX + 1;
    } else if (exactStartWorldX < resolvedStartWorldX) {
        startGridMinX = resolvedStartX - 1;
    }
    if (exactStartWorldY > resolvedStartWorldY) {
        startGridMaxY = resolvedStartY + 1;
    } else if (exactStartWorldY < resolvedStartWorldY) {
        startGridMinY = resolvedStartY - 1;
    }

    bool seededStart = false;
    for (int sy = startGridMinY; sy <= startGridMaxY; ++sy) {
        for (int sx = startGridMinX; sx <= startGridMaxX; ++sx) {
            if (!IsMappedNodeInBounds(sx, sy, mapping)
                || !IsMappedNodePathable(sx, sy, mapping, collisionSize, movementType, ignoreUnit)) {
                continue;
            }
            const float nodeWorldX = MappedNodeToWorldX(sx, mapping);
            const float nodeWorldY = MappedNodeToWorldY(sy, mapping);
            if (!IsWorldSegmentPathable(exactStartWorldX, exactStartWorldY, nodeWorldX, nodeWorldY,
                                        collisionSize, movementType, ignoreUnit)) {
                continue;
            }
            const int sIdx = nodeIdx(sx, sy);
            const float g = std::sqrt((nodeWorldX - exactStartWorldX) * (nodeWorldX - exactStartWorldX)
                                    + (nodeWorldY - exactStartWorldY) * (nodeWorldY - exactStartWorldY));
            const float f = g + heuristic(sx, sy, resolvedEndX, resolvedEndY);
            flatWorkspace_.visitedGen[static_cast<size_t>(sIdx)] = gen;
            flatWorkspace_.gScore[static_cast<size_t>(sIdx)] = g;
            flatWorkspace_.fScore[static_cast<size_t>(sIdx)] = f;
            flatWorkspace_.parentIdx[static_cast<size_t>(sIdx)] = -1;
            pushOpen(f, sIdx);
            seededStart = true;
        }
    }
    if (!seededStart && IsMappedNodeInBounds(resolvedStartX, resolvedStartY, mapping)) {
        const int sIdx = nodeIdx(resolvedStartX, resolvedStartY);
        const float f = heuristic(resolvedStartX, resolvedStartY, resolvedEndX, resolvedEndY);
        flatWorkspace_.visitedGen[static_cast<size_t>(sIdx)] = gen;
        flatWorkspace_.gScore[static_cast<size_t>(sIdx)] = 0.0f;
        flatWorkspace_.fScore[static_cast<size_t>(sIdx)] = f;
        flatWorkspace_.parentIdx[static_cast<size_t>(sIdx)] = -1;
        pushOpen(f, sIdx);
    }

    static const int ddx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int ddy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    int foundIdx = -1;
    const int maxIterations = 10000;
    int iter = 0;

    while (!flatWorkspace_.openHeap.empty() && iter < maxIterations) {
        iter++;
        const FlatHeapEntry current = popOpen();
        const int curIdx = current.nodeIdx;
        if (flatWorkspace_.closedGen[static_cast<size_t>(curIdx)] == gen) {
            continue;
        }
        flatWorkspace_.closedGen[static_cast<size_t>(curIdx)] = gen;
        flatWorkspace_.totalNodesExpanded++;

        if (flatWorkspace_.goalGen[static_cast<size_t>(curIdx)] == gen) {
            foundIdx = curIdx;
            break;
        }

        const int cx = curIdx % gridW;
        const int cy = curIdx / gridW;
        const float curG = flatWorkspace_.gScore[static_cast<size_t>(curIdx)];

        for (int i = 0; i < 8; i++) {
            const int nx = cx + ddx[i];
            const int ny = cy + ddy[i];

            if (!IsMappedNodePathable(nx, ny, mapping, collisionSize, movementType, ignoreUnit)) continue;
            if ((ddx[i] != 0) && (ddy[i] != 0)) {
                if (!IsMappedNodePathable(cx + ddx[i], cy, mapping, collisionSize, movementType, ignoreUnit)
                    || !IsMappedNodePathable(cx, cy + ddy[i], mapping, collisionSize, movementType, ignoreUnit)) {
                    continue;
                }
            }
            const int nIdx = nodeIdx(nx, ny);
            if (flatWorkspace_.closedGen[static_cast<size_t>(nIdx)] == gen) continue;

            const float moveCost = (ddx[i] != 0 && ddy[i] != 0) ? 1.414f : 1.0f;
            const float newG = curG + moveCost;

            if (flatWorkspace_.visitedGen[static_cast<size_t>(nIdx)] == gen) {
                if (newG >= flatWorkspace_.gScore[static_cast<size_t>(nIdx)]) continue;
            }
            const float newF = newG + heuristic(nx, ny, resolvedEndX, resolvedEndY);
            flatWorkspace_.visitedGen[static_cast<size_t>(nIdx)] = gen;
            flatWorkspace_.gScore[static_cast<size_t>(nIdx)] = newG;
            flatWorkspace_.fScore[static_cast<size_t>(nIdx)] = newF;
            flatWorkspace_.parentIdx[static_cast<size_t>(nIdx)] = curIdx;
            pushOpen(newF, nIdx);
        }
    }

    if (foundIdx >= 0) {
        int traceIdx = foundIdx;
        int guard = gridW * gridH + 1;
        while (traceIdx >= 0 && guard-- > 0) {
            outPath.push_back({traceIdx % gridW, traceIdx / gridW});
            traceIdx = flatWorkspace_.parentIdx[static_cast<size_t>(traceIdx)];
        }
        std::reverse(outPath.begin(), outPath.end());
    }

    return foundIdx >= 0;
}

bool CPathfindingProcessor::FindPath(int startCellX, int startCellY,
                                     int endCellX, int endCellY,
                                     std::vector<std::pair<int,int>>& outPath) const {
    return FindPathMapped(startCellX, startCellY, endCellX, endCellY,
                          CellToWorldX(startCellX), CellToWorldY(startCellY),
                          CellToWorldX(endCellX), CellToWorldY(endCellY),
                          GridMapping::CELLS, 0.0f, PathingGridMovementType::DISABLED, nullptr, outPath);
}

bool CPathfindingProcessor::FindPathWorld(float startX, float startY,
                                          float endX, float endY,
                                          std::vector<std::pair<float,float>>& outWorldPath) const {
    return FindPathWorld(startX, startY, endX, endY, 0.0f, outWorldPath);
}

bool CPathfindingProcessor::FindPathWorld(float startX, float startY,
                                          float endX, float endY,
                                          float collisionSize,
                                          std::vector<std::pair<float,float>>& outWorldPath) const {
    return FindPathWorld(startX, startY, endX, endY, collisionSize,
                         PathingGridMovementType::DISABLED, nullptr, outWorldPath);
}

bool CPathfindingProcessor::IsWorldPointPathableForUnit(float worldX, float worldY,
                                                        float collisionSize,
                                                        PathingGridMovementType movementType,
                                                        CUnit* ignoreUnit) const {
    return IsWorldPointPathable(worldX, worldY, collisionSize, movementType, ignoreUnit);
}

bool CPathfindingProcessor::FindPathWorld(float startX, float startY,
                                          float endX, float endY,
                                          float collisionSize,
                                          PathingGridMovementType movementType,
                                          CUnit* ignoreUnit,
                                          std::vector<std::pair<float,float>>& outWorldPath) const {
    const GridMapping mapping = SelectGridMapping(collisionSize);
    int sx = WorldToMappedX(startX, mapping);
    int sy = WorldToMappedY(startY, mapping);
    int ex = WorldToMappedX(endX, mapping);
    int ey = WorldToMappedY(endY, mapping);
    const bool targetPointPathable = !pathingMap_
        || IsWorldPointPathable(endX, endY, collisionSize, movementType, ignoreUnit);
    int resolvedStartX = sx;
    int resolvedStartY = sy;
    int resolvedEndX = ex;
    int resolvedEndY = ey;

    if (pathingMap_) {
        const bool startPathable = IsMappedNodePathable(sx, sy, mapping, collisionSize, movementType, ignoreUnit);
        if (!startPathable
            && !FindNearestPathableNode(sx, sy, mapping, collisionSize, movementType, ignoreUnit,
                                        resolvedStartX, resolvedStartY)) {
            LogPathDebug(ignoreUnit, "resolve_start_failed", startX, startY, endX, endY,
                         "grid=" + std::string(GridMappingName(mapping))
                         + " requestedStartNode=(" + std::to_string(sx) + "," + std::to_string(sy) + ")");
            return false;
        }

        const bool endPathable = IsMappedNodePathable(ex, ey, mapping, collisionSize, movementType, ignoreUnit);
        if (!endPathable
            && !FindNearestPathableNode(ex, ey, mapping, collisionSize, movementType, ignoreUnit,
                                        resolvedEndX, resolvedEndY)) {
            LogPathDebug(ignoreUnit, "resolve_end_failed", startX, startY, endX, endY,
                         "grid=" + std::string(GridMappingName(mapping))
                         + " requestedEndNode=(" + std::to_string(ex) + "," + std::to_string(ey) + ")");
            return false;
        }

        if ((resolvedStartX != sx) || (resolvedStartY != sy) || (resolvedEndX != ex) || (resolvedEndY != ey)) {
            LogPathDebug(ignoreUnit, "resolve_nodes", startX, startY, endX, endY,
                         "grid=" + std::string(GridMappingName(mapping))
                         + " requestedStartNode=(" + std::to_string(sx) + "," + std::to_string(sy) + ")"
                         + " resolvedStartNode=(" + std::to_string(resolvedStartX) + "," + std::to_string(resolvedStartY) + ")"
                         + " requestedEndNode=(" + std::to_string(ex) + "," + std::to_string(ey) + ")"
                         + " resolvedEndNode=(" + std::to_string(resolvedEndX) + "," + std::to_string(resolvedEndY) + ")"
                         + " resolvedEndWorld=(" + std::to_string(MappedNodeToWorldX(resolvedEndX, mapping))
                         + "," + std::to_string(MappedNodeToWorldY(resolvedEndY, mapping)) + ")");
        }
    }

    std::vector<std::pair<int,int>> cellPath;
    if (!pathingMap_) {
        if (worldCollision_ && UsesDynamicGroundCollision(movementType) && collisionSize > 0.0f
            && worldCollision_->CheckCollision(endX, endY, collisionSize, ignoreUnit)) {
            LogPathDebug(ignoreUnit, "direct_blocked", startX, startY, endX, endY,
                         "grid=" + std::string(GridMappingName(mapping)));
            return false;
        }
    }

    if (!FindPathMappedPreResolved(resolvedStartX, resolvedStartY, resolvedEndX, resolvedEndY,
                        startX, startY, endX, endY,
                        mapping, collisionSize, movementType, ignoreUnit, cellPath)) {
        LogPathDebug(ignoreUnit, "search_failed", startX, startY, endX, endY,
                     "grid=" + std::string(GridMappingName(mapping))
                     + " requestedStartNode=(" + std::to_string(sx) + "," + std::to_string(sy) + ")"
                     + " requestedEndNode=(" + std::to_string(ex) + "," + std::to_string(ey) + ")"
                     + " resolvedStartNode=(" + std::to_string(resolvedStartX) + "," + std::to_string(resolvedStartY) + ")"
                     + " resolvedEndNode=(" + std::to_string(resolvedEndX) + "," + std::to_string(resolvedEndY) + ")");
        return false;
    }

    outWorldPath.clear();
    for (auto& c : cellPath) {
        outWorldPath.push_back({MappedNodeToWorldX(c.first, mapping), MappedNodeToWorldY(c.second, mapping)});
    }
    bool useExactEndPoint = targetPointPathable;
    if (useExactEndPoint && outWorldPath.size() >= 2) {
        const auto& previousNode = outWorldPath[outWorldPath.size() - 2];
        useExactEndPoint = IsWorldSegmentPathable(previousNode.first, previousNode.second,
                                                  endX, endY, collisionSize, movementType, ignoreUnit);
    }
    if (!outWorldPath.empty()) {
        outWorldPath.front() = {startX, startY};
        if (useExactEndPoint) {
            outWorldPath.back() = {endX, endY};
        }
    }
    if (!outWorldPath.empty() && IsMoveDebugLoggingEnabled()) {
        const float finalDx = outWorldPath.back().first - endX;
        const float finalDy = outWorldPath.back().second - endY;
        const float finalDrift = std::sqrt(finalDx * finalDx + finalDy * finalDy);
        LogPathDebug(ignoreUnit, "search_ok", startX, startY, endX, endY,
                     "grid=" + std::string(GridMappingName(mapping))
                     + " requestedStartNode=(" + std::to_string(sx) + "," + std::to_string(sy) + ")"
                     + " requestedEndNode=(" + std::to_string(ex) + "," + std::to_string(ey) + ")"
                     + " resolvedStartNode=(" + std::to_string(resolvedStartX) + "," + std::to_string(resolvedStartY) + ")"
                     + " resolvedEndNode=(" + std::to_string(resolvedEndX) + "," + std::to_string(resolvedEndY) + ")"
                     + " pathNodes=" + std::to_string(cellPath.size())
                     + " worldNodes=" + std::to_string(outWorldPath.size())
                     + " exactEnd=" + std::to_string(useExactEndPoint ? 1 : 0)
                     + " finalNode=(" + std::to_string(outWorldPath.back().first)
                     + "," + std::to_string(outWorldPath.back().second) + ")"
                     + " finalDrift=" + std::to_string(finalDrift));
    }
    return true;
}

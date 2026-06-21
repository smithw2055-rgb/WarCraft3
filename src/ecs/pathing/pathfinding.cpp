#include "pathfinding.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_map>

CPathfindingProcessor::CPathfindingProcessor() {}

bool CPathfindingProcessor::IsCellWalkable(int cellX, int cellY, bool allowFlying) const {
    if (!pathingMap_) return true;
    if (allowFlying) return true;
    return pathingMap_->IsWalkable(cellX, cellY);
}

bool CPathfindingProcessor::FindPath(int startCellX, int startCellY,
                                      int endCellX, int endCellY,
                                      std::vector<std::pair<int,int>>& outPath) const {
    outPath.clear();

    // If no pathing map, return direct path
    if (!pathingMap_) {
        outPath.push_back({startCellX, startCellY});
        outPath.push_back({endCellX, endCellY});
        return true;
    }

    // Check if start or end are walkable
    if (!IsCellWalkable(startCellX, startCellY) || !IsCellWalkable(endCellX, endCellY)) {
        // Try to find nearest walkable cell to end
        return false;
    }

    // If cells are adjacent or same, direct path
    int dx = abs(endCellX - startCellX);
    int dy = abs(endCellY - startCellY);
    if (dx <= 1 && dy <= 1) {
        outPath.push_back({startCellX, startCellY});
        outPath.push_back({endCellX, endCellY});
        return true;
    }

    struct PathNode {
        int x, y;
        float g, f;
        PathNode* parent = nullptr;
        PathNode(int cx, int cy) : x(cx), y(cy), g(0), f(0) {}
    };

    struct NodeCmp {
        bool operator()(const PathNode* a, const PathNode* b) const {
            return a->f > b->f;
        }
    };

    auto key = [](int x, int y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; };
    auto heuristic = [](int ax, int ay, int bx, int by) -> float {
        float ddx = (float)(bx - ax);
        float ddy = (float)(by - ay);
        return std::sqrt(ddx * ddx + ddy * ddy);
    };

    std::priority_queue<PathNode*, std::vector<PathNode*>, NodeCmp> open;
    std::unordered_map<uint64_t, PathNode*> allNodes;

    auto* start = new PathNode(startCellX, startCellY);
    start->g = 0;
    start->f = heuristic(startCellX, startCellY, endCellX, endCellY);
    open.push(start);
    allNodes[key(startCellX, startCellY)] = start;

    // 8-directional movement
    const int ddx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    const int ddy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    PathNode* found = nullptr;
    int maxIterations = 10000;
    int iter = 0;

    while (!open.empty() && iter < maxIterations) {
        iter++;
        auto* current = open.top();
        open.pop();

        if (current->x == endCellX && current->y == endCellY) {
            found = current;
            break;
        }

        for (int i = 0; i < 8; i++) {
            int nx = current->x + ddx[i];
            int ny = current->y + ddy[i];

            if (!IsCellWalkable(nx, ny)) continue;

            float moveCost = (ddx[i] != 0 && ddy[i] != 0) ? 1.414f : 1.0f;
            float newG = current->g + moveCost;

            auto it = allNodes.find(key(nx, ny));
            PathNode* neighbor = nullptr;
            if (it != allNodes.end()) {
                neighbor = it->second;
                if (newG >= neighbor->g) continue;
                neighbor->g = newG;
                neighbor->f = newG + heuristic(nx, ny, endCellX, endCellY);
                neighbor->parent = current;
            } else {
                neighbor = new PathNode(nx, ny);
                neighbor->g = newG;
                neighbor->f = newG + heuristic(nx, ny, endCellX, endCellY);
                neighbor->parent = current;
                allNodes[key(nx, ny)] = neighbor;
                open.push(neighbor);
            }
        }
    }

    if (found) {
        PathNode* node = found;
        while (node) {
            outPath.push_back({node->x, node->y});
            node = node->parent;
        }
        std::reverse(outPath.begin(), outPath.end());
    }

    for (auto& p : allNodes) delete p.second;
    return found != nullptr;
}

bool CPathfindingProcessor::FindPathWorld(float startX, float startY,
                                           float endX, float endY,
                                           std::vector<std::pair<float,float>>& outWorldPath) const {
    int sx = WorldToCell(startX);
    int sy = WorldToCell(startY);
    int ex = WorldToCell(endX);
    int ey = WorldToCell(endY);

    std::vector<std::pair<int,int>> cellPath;
    if (!FindPath(sx, sy, ex, ey, cellPath))
        return false;

    outWorldPath.clear();
    for (auto& c : cellPath) {
        outWorldPath.push_back({CellToWorld(c.first), CellToWorld(c.second)});
    }
    return true;
}

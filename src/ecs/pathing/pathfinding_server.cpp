#include "ecs/pathing/pathfinding_server.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace pathfinding {

void PathQueryWorkspace::EnsureCapacity(int width, int height) {
    const size_t total = static_cast<size_t>(std::max(1, width)) * static_cast<size_t>(std::max(1, height));
    if (width_ != width || height_ != height || visitedGen_.size() < total) {
        width_ = std::max(1, width);
        height_ = std::max(1, height);
        visitedGen_.assign(total, 0u);
        closedState_.assign(total, 0u);
        gScore_.assign(total, 0.0f);
        parentIdx_.assign(total, -1);
        currentGen_ = 1u;
        bufferAllocations_++;
    }
}

void PathQueryWorkspace::BeginQuery(int width, int height) {
    EnsureCapacity(width, height);
    currentGen_++;
    if (currentGen_ == 0u) {
        std::fill(visitedGen_.begin(), visitedGen_.end(), 0u);
        currentGen_ = 1u;
    }
    openHeap_.clear();
    queriesExecuted_++;
}

bool PathQueryWorkspace::SolveAStar(const CPathfindingProcessor& processor,
                                    const PathRequest& req,
                                    PathResult& outResult) {
    outResult = PathResult{};
    outResult.requestId = req.requestId;
    outResult.unitHandle = req.unitHandle;

    const WpmData* wpm = processor.GetPathingMap();
    if (!wpm || !wpm->IsValid()) {
        // Mapless straight-line fallback
        outResult.found = true;
        outResult.waypoints.push_back({req.startWorldX, req.startWorldY});
        outResult.waypoints.push_back({req.goalWorldX, req.goalWorldY});
        return true;
    }

    const int gridW = wpm->width;
    const int gridH = wpm->height;
    BeginQuery(gridW, gridH);

    int startX = std::clamp(processor.WorldToCellX(req.startWorldX), 0, gridW - 1);
    int startY = std::clamp(processor.WorldToCellY(req.startWorldY), 0, gridH - 1);
    int goalX  = std::clamp(processor.WorldToCellX(req.goalWorldX),  0, gridW - 1);
    int goalY  = std::clamp(processor.WorldToCellY(req.goalWorldY),  0, gridH - 1);

    // If goal cell is blocked, spiral-search up to radius 8 for nearest walkable cell
    if (!processor.IsCellPathableFor(goalX, goalY, req.movementType)) {
        bool foundAlt = false;
        for (int r = 1; r <= 8 && !foundAlt; ++r) {
            for (int dy = -r; dy <= r && !foundAlt; ++dy) {
                for (int dx = -r; dx <= r && !foundAlt; ++dx) {
                    if (std::abs(dx) != r && std::abs(dy) != r) continue;
                    const int nx = goalX + dx;
                    const int ny = goalY + dy;
                    if (nx >= 0 && ny >= 0 && nx < gridW && ny < gridH &&
                        processor.IsCellPathableFor(nx, ny, req.movementType)) {
                        goalX = nx;
                        goalY = ny;
                        foundAlt = true;
                    }
                }
            }
        }
        if (!foundAlt) return false;
    }

    if (startX == goalX && startY == goalY) {
        outResult.found = true;
        outResult.waypoints.push_back({req.goalWorldX, req.goalWorldY});
        return true;
    }

    auto octileHeuristic = [](int x0, int y0, int x1, int y1) -> float {
        const float dx = static_cast<float>(std::abs(x1 - x0));
        const float dy = static_cast<float>(std::abs(y1 - y0));
        return (dx + dy) + (1.41421356f - 2.0f) * std::min(dx, dy);
    };

    const int startIdx = startY * gridW + startX;
    const int goalIdx  = goalY * gridW + goalX;

    visitedGen_[startIdx] = currentGen_;
    closedState_[startIdx] = 0u;
    gScore_[startIdx] = 0.0f;
    parentIdx_[startIdx] = -1;

    openHeap_.push_back({octileHeuristic(startX, startY, goalX, goalY), startIdx});

    static const int kDirs[8][2] = {
        { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
        { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
    };
    static const float kStepCosts[8] = {
        1.0f, 1.0f, 1.0f, 1.0f,
        1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f
    };

    const int maxNodes = std::max(64, req.maxSearchNodes);
    int expanded = 0;
    int bestFallbackIdx = startIdx;
    float bestFallbackH = octileHeuristic(startX, startY, goalX, goalY);
    bool reachedGoal = false;

    while (!openHeap_.empty() && expanded < maxNodes) {
        std::pop_heap(openHeap_.begin(), openHeap_.end(), std::greater<OpenHeapNode>{});
        const OpenHeapNode current = openHeap_.back();
        openHeap_.pop_back();

        const int curIdx = current.cellIdx;
        if (visitedGen_[curIdx] == currentGen_ && closedState_[curIdx] != 0u) {
            continue;
        }
        closedState_[curIdx] = 1u;
        expanded++;

        if (curIdx == goalIdx) {
            reachedGoal = true;
            break;
        }

        const int cx = curIdx % gridW;
        const int cy = curIdx / gridW;
        const float curG = gScore_[curIdx];

        for (int d = 0; d < 8; ++d) {
            const int nx = cx + kDirs[d][0];
            const int ny = cy + kDirs[d][1];
            if (nx < 0 || ny < 0 || nx >= gridW || ny >= gridH) continue;
            if (!processor.IsCellPathableFor(nx, ny, req.movementType)) continue;

            // Prevent corner-cutting on diagonal steps
            if (d >= 4) {
                if (!processor.IsCellPathableFor(cx + kDirs[d][0], cy, req.movementType) ||
                    !processor.IsCellPathableFor(cx, cy + kDirs[d][1], req.movementType)) {
                    continue;
                }
            }

            const int nIdx = ny * gridW + nx;
            if (visitedGen_[nIdx] == currentGen_ && closedState_[nIdx] != 0u) {
                continue;
            }

            const float tentativeG = curG + kStepCosts[d];
            if (visitedGen_[nIdx] != currentGen_ || tentativeG < gScore_[nIdx]) {
                visitedGen_[nIdx] = currentGen_;
                closedState_[nIdx] = 0u;
                gScore_[nIdx] = tentativeG;
                parentIdx_[nIdx] = curIdx;

                const float h = octileHeuristic(nx, ny, goalX, goalY);
                if (h < bestFallbackH) {
                    bestFallbackH = h;
                    bestFallbackIdx = nIdx;
                }
                openHeap_.push_back({tentativeG + h, nIdx});
                std::push_heap(openHeap_.begin(), openHeap_.end(), std::greater<OpenHeapNode>{});
            }
        }
    }

    outResult.nodesExpanded = expanded;
    const int traceEndIdx = reachedGoal ? goalIdx : bestFallbackIdx;
    if (!reachedGoal && traceEndIdx == startIdx) {
        return false;
    }

    std::vector<int> cellChain;
    for (int cur = traceEndIdx; cur != -1; cur = parentIdx_[cur]) {
        cellChain.push_back(cur);
        if (cur == startIdx) break;
    }
    std::reverse(cellChain.begin(), cellChain.end());

    outResult.waypoints.reserve(cellChain.size());
    for (int idx : cellChain) {
        const int cx = idx % gridW;
        const int cy = idx / gridW;
        outResult.waypoints.push_back({processor.CellToWorldX(cx), processor.CellToWorldY(cy)});
    }
    if (reachedGoal && !outResult.waypoints.empty()) {
        outResult.waypoints.back() = {req.goalWorldX, req.goalWorldY};
    }
    outResult.found = true;
    return true;
}

bool FlowFieldGrid::Build(const CPathfindingProcessor& processor,
                          float goalWorldX,
                          float goalWorldY,
                          int radiusCells,
                          PathingGridMovementType movementType) {
    const WpmData* wpm = processor.GetPathingMap();
    if (!wpm || !wpm->IsValid()) return false;

    const int gridW = wpm->width;
    const int gridH = wpm->height;
    goalCellX_ = std::clamp(processor.WorldToCellX(goalWorldX), 0, gridW - 1);
    goalCellY_ = std::clamp(processor.WorldToCellY(goalWorldY), 0, gridH - 1);

    const int r = std::max(4, radiusCells);
    minCellX_ = std::max(0, goalCellX_ - r);
    minCellY_ = std::max(0, goalCellY_ - r);
    const int maxCellX = std::min(gridW - 1, goalCellX_ + r);
    const int maxCellY = std::min(gridH - 1, goalCellY_ + r);
    winWidth_ = maxCellX - minCellX_ + 1;
    winHeight_ = maxCellY - minCellY_ + 1;

    const size_t total = static_cast<size_t>(winWidth_) * static_cast<size_t>(winHeight_);
    integrationCost_.assign(total, kUnreachableCost);
    dirX_.assign(total, 0.0f);
    dirY_.assign(total, 0.0f);
    reachableCells_ = 0;

    const int goalLocal = LocalIndex(goalCellX_, goalCellY_);
    if (goalLocal < 0) return false;

    std::queue<std::pair<int, int>> bfs;
    integrationCost_[goalLocal] = 0u;
    bfs.push({goalCellX_, goalCellY_});
    reachableCells_ = 1;

    static const int kDirs4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!bfs.empty()) {
        const auto [cx, cy] = bfs.front();
        bfs.pop();
        const uint32_t nextCost = integrationCost_[LocalIndex(cx, cy)] + 1u;

        for (int d = 0; d < 4; ++d) {
            const int nx = cx + kDirs4[d][0];
            const int ny = cy + kDirs4[d][1];
            const int nLocal = LocalIndex(nx, ny);
            if (nLocal < 0) continue;
            if (integrationCost_[nLocal] != kUnreachableCost) continue;
            if (!processor.IsCellPathableFor(nx, ny, movementType)) continue;

            integrationCost_[nLocal] = nextCost;
            reachableCells_++;
            bfs.push({nx, ny});
        }
    }

    // Compute normalized 2D gradient vector pointing toward lowest neighbor cost
    static const int kDirs8[8][2] = {
        { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
        { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
    };
    for (int cy = minCellY_; cy <= maxCellY; ++cy) {
        for (int cx = minCellX_; cx <= maxCellX; ++cx) {
            const int idx = LocalIndex(cx, cy);
            const uint32_t myCost = integrationCost_[idx];
            if (myCost == 0u || myCost == kUnreachableCost) continue;

            uint32_t bestCost = myCost;
            int bestDx = 0;
            int bestDy = 0;
            for (int d = 0; d < 8; ++d) {
                const int nx = cx + kDirs8[d][0];
                const int ny = cy + kDirs8[d][1];
                const int nIdx = LocalIndex(nx, ny);
                if (nIdx < 0) continue;
                if (integrationCost_[nIdx] < bestCost) {
                    bestCost = integrationCost_[nIdx];
                    bestDx = kDirs8[d][0];
                    bestDy = kDirs8[d][1];
                }
            }
            if (bestDx != 0 || bestDy != 0) {
                const float len = std::sqrt(static_cast<float>(bestDx * bestDx + bestDy * bestDy));
                dirX_[idx] = static_cast<float>(bestDx) / len;
                dirY_[idx] = static_cast<float>(bestDy) / len;
            }
        }
    }
    return reachableCells_ > 0;
}

bool FlowFieldGrid::SampleDirectionWorld(const CPathfindingProcessor& processor,
                                         float worldX,
                                         float worldY,
                                         float& outDirX,
                                         float& outDirY) const {
    const int cx = processor.WorldToCellX(worldX);
    const int cy = processor.WorldToCellY(worldY);
    const int idx = LocalIndex(cx, cy);
    if (idx < 0 || integrationCost_[idx] == kUnreachableCost) {
        outDirX = 0.0f;
        outDirY = 0.0f;
        return false;
    }
    outDirX = dirX_[idx];
    outDirY = dirY_[idx];
    return true;
}

uint32_t FlowFieldGrid::GetIntegrationCostAtCell(int cellX, int cellY) const {
    const int idx = LocalIndex(cellX, cellY);
    return (idx >= 0) ? integrationCost_[idx] : kUnreachableCost;
}

std::vector<PathResult> PathfindingServer::SolveBatch(const CPathfindingProcessor& processor,
                                                      const std::vector<PathRequest>& requests,
                                                      core::JobSystem* jobSystem,
                                                      int minBatchSize) {
    std::vector<PathResult> results(requests.size());
    if (requests.empty()) return results;

    stats_.totalRequestsSubmitted += requests.size();
    const int count = static_cast<int>(requests.size());
    const int batchSz = std::max(1, minBatchSize);

    if (jobSystem && jobSystem->IsInitialized() && count >= batchSz) {
        stats_.parallelBatchesDispatched++;
        jobSystem->ParallelForRange(count, batchSz, [&](int startIdx, int endIdx) {
            PathQueryWorkspace localWorkspace;
            for (int i = startIdx; i < endIdx; ++i) {
                localWorkspace.SolveAStar(processor, requests[static_cast<size_t>(i)], results[static_cast<size_t>(i)]);
            }
        });
    } else {
        for (int i = 0; i < count; ++i) {
            primaryWorkspace_.SolveAStar(processor, requests[static_cast<size_t>(i)], results[static_cast<size_t>(i)]);
        }
    }

    for (const auto& r : results) {
        if (r.found) stats_.totalPathsFound++;
        stats_.totalNodesExpanded += static_cast<uint64_t>(r.nodesExpanded);
    }
    return results;
}

const FlowFieldGrid* PathfindingServer::GetOrBuildFlowField(const CPathfindingProcessor& processor,
                                                            float goalWorldX,
                                                            float goalWorldY,
                                                            int radiusCells,
                                                            PathingGridMovementType movementType) {
    const int gx = processor.WorldToCellX(goalWorldX);
    const int gy = processor.WorldToCellY(goalWorldY);
    const uint64_t key = MakeFlowFieldKey(gx, gy, radiusCells, movementType);

    auto it = flowFieldCache_.find(key);
    if (it != flowFieldCache_.end()) {
        stats_.flowFieldCacheHits++;
        return &it->second;
    }

    FlowFieldGrid field;
    if (!field.Build(processor, goalWorldX, goalWorldY, radiusCells, movementType)) {
        return nullptr;
    }
    stats_.flowFieldsBuilt++;
    auto [insIt, _] = flowFieldCache_.emplace(key, std::move(field));
    return &insIt->second;
}

} // namespace pathfinding

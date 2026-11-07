#pragma once
// ============================================================================
// Stage M: Multi-Threaded Batch PathfindingServer, Zero-Allocation A*
//          PathQueryWorkspace & RTS Squad FlowFieldGrid
//          (Inspired by Unity NavMeshQuery + IJobParallelFor & Godot NavigationServer3D)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/job_system.h"
#include "ecs/pathing/pathfinding.h"

namespace pathfinding {

struct PathRequest {
    int requestId = 0;
    int unitHandle = 0;
    float startWorldX = 0.0f;
    float startWorldY = 0.0f;
    float goalWorldX = 0.0f;
    float goalWorldY = 0.0f;
    float collisionSize = 32.0f;
    PathingGridMovementType movementType = PathingGridMovementType::FOOT;
    int maxSearchNodes = 16384;
};

struct PathResult {
    int requestId = 0;
    int unitHandle = 0;
    bool found = false;
    int nodesExpanded = 0;
    std::vector<std::pair<float, float>> waypoints;
};

// ============================================================================
// 1. PathQueryWorkspace — Zero-Heap-Allocation Flat A* Search Context
//    Uses generation-stamped flat arrays (`visitedGen_`) so resetting a
//    768x768 grid between searches is O(1) (`currentGeneration_++`) with
//    zero `std::unordered_map` / `std::unordered_set` allocations.
// ============================================================================
class PathQueryWorkspace {
public:
    PathQueryWorkspace() = default;

    void EnsureCapacity(int width, int height);

    // O(1) generation increment reset (reallocates only if map dimensions grew)
    void BeginQuery(int width, int height);

    bool SolveAStar(const CPathfindingProcessor& processor,
                    const PathRequest& req,
                    PathResult& outResult);

    int GetGridWidth() const { return width_; }
    int GetGridHeight() const { return height_; }
    uint32_t GetCurrentGeneration() const { return currentGen_; }
    uint64_t GetTotalQueriesExecuted() const { return queriesExecuted_; }
    uint64_t GetBufferAllocationCount() const { return bufferAllocations_; }

private:
    struct OpenHeapNode {
        float fScore = 0.0f;
        int cellIdx = 0;
        bool operator>(const OpenHeapNode& other) const {
            if (fScore != other.fScore) return fScore > other.fScore;
            return cellIdx > other.cellIdx; // Deterministic tie-breaking
        }
    };

    int width_ = 0;
    int height_ = 0;
    uint32_t currentGen_ = 1;
    uint64_t queriesExecuted_ = 0;
    uint64_t bufferAllocations_ = 0;

    std::vector<uint32_t> visitedGen_;
    std::vector<uint8_t> closedState_;
    std::vector<float> gScore_;
    std::vector<int32_t> parentIdx_;
    std::vector<OpenHeapNode> openHeap_;
};

// ============================================================================
// 2. FlowFieldGrid — RTS Group Navigation Vector Field
//    Computes a single reverse-Dijkstra integration field around a shared
//    destination cell so N squad units can query O(1) steering vectors.
// ============================================================================
class FlowFieldGrid {
public:
    static constexpr uint32_t kUnreachableCost = 0xFFFFFFFFu;

    FlowFieldGrid() = default;

    bool Build(const CPathfindingProcessor& processor,
               float goalWorldX,
               float goalWorldY,
               int radiusCells,
               PathingGridMovementType movementType);

    bool SampleDirectionWorld(const CPathfindingProcessor& processor,
                              float worldX,
                              float worldY,
                              float& outDirX,
                              float& outDirY) const;

    uint32_t GetIntegrationCostAtCell(int cellX, int cellY) const;

    int GetGoalCellX() const { return goalCellX_; }
    int GetGoalCellY() const { return goalCellY_; }
    int GetWindowMinX() const { return minCellX_; }
    int GetWindowMinY() const { return minCellY_; }
    int GetWindowWidth() const { return winWidth_; }
    int GetWindowHeight() const { return winHeight_; }
    size_t GetReachableCellCount() const { return reachableCells_; }

private:
    int LocalIndex(int cellX, int cellY) const {
        if (cellX < minCellX_ || cellY < minCellY_ ||
            cellX >= minCellX_ + winWidth_ || cellY >= minCellY_ + winHeight_) {
            return -1;
        }
        return (cellY - minCellY_) * winWidth_ + (cellX - minCellX_);
    }

    int goalCellX_ = 0;
    int goalCellY_ = 0;
    int minCellX_ = 0;
    int minCellY_ = 0;
    int winWidth_ = 0;
    int winHeight_ = 0;
    size_t reachableCells_ = 0;
    std::vector<uint32_t> integrationCost_;
    std::vector<float> dirX_;
    std::vector<float> dirY_;
};

struct PathfindingServerStats {
    uint64_t totalRequestsSubmitted = 0;
    uint64_t totalPathsFound = 0;
    uint64_t totalNodesExpanded = 0;
    uint64_t parallelBatchesDispatched = 0;
    uint64_t flowFieldsBuilt = 0;
    uint64_t flowFieldCacheHits = 0;
};

// ============================================================================
// 3. PathfindingServer — Parallel Batch Solver & FlowField Cache
// ============================================================================
class PathfindingServer {
public:
    PathfindingServer() = default;

    // Solve a batch of path requests in parallel across JobSystem worker threads.
    // Preserves 100% deterministic output ordering matching `requests`.
    std::vector<PathResult> SolveBatch(const CPathfindingProcessor& processor,
                                       const std::vector<PathRequest>& requests,
                                       core::JobSystem* jobSystem = nullptr,
                                       int minBatchSize = 4);

    // Retrieve or build a cached FlowFieldGrid for group movement toward (goalWorldX, goalWorldY)
    const FlowFieldGrid* GetOrBuildFlowField(const CPathfindingProcessor& processor,
                                             float goalWorldX,
                                             float goalWorldY,
                                             int radiusCells = 64,
                                             PathingGridMovementType movementType = PathingGridMovementType::FOOT);

    void InvalidateFlowFields() { flowFieldCache_.clear(); }
    void ResetStats() { stats_ = PathfindingServerStats{}; }
    const PathfindingServerStats& GetStats() const { return stats_; }

private:
    static uint64_t MakeFlowFieldKey(int goalCellX, int goalCellY, int radiusCells, PathingGridMovementType moveType) {
        uint64_t k = static_cast<uint64_t>(static_cast<uint16_t>(goalCellX));
        k |= (static_cast<uint64_t>(static_cast<uint16_t>(goalCellY)) << 16);
        k |= (static_cast<uint64_t>(static_cast<uint16_t>(radiusCells)) << 32);
        k |= (static_cast<uint64_t>(static_cast<uint8_t>(moveType)) << 48);
        return k;
    }

    PathQueryWorkspace primaryWorkspace_;
    std::unordered_map<uint64_t, FlowFieldGrid> flowFieldCache_;
    PathfindingServerStats stats_;
};

} // namespace pathfinding

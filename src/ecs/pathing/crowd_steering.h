#pragma once
// ============================================================================
// Stage Q: Predictive RVO Crowd Steering & FlowField Squad Navigation System
//          (Inspired by Godot 4 NavigationAgent3D RVO & Unity DOTS Steering)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/pathing/pathfinding_server.h"

namespace pathfinding {

struct CrowdSteeringConfig {
    float neighborHorizonRadius = 160.0f;
    float separationWeight = 1.35f;
    float flowFieldWeight = 0.65f;
    float goalSeekWeight = 1.0f;
    int maxNeighborsPerAgent = 12;
};

struct CrowdAgentSteerOutput {
    int unitHandle = 0;
    float velX = 0.0f;
    float velY = 0.0f;
    float newX = 0.0f;
    float newY = 0.0f;
    float facingDeg = 0.0f;
    int neighborsEvaluated = 0;
    bool avoidanceActive = false;
};

struct CrowdSteeringStats {
    uint64_t totalAgentsSteered = 0;
    uint64_t totalNeighborsEvaluated = 0;
    uint64_t avoidanceAdjustments = 0;
    uint64_t flowFieldSamples = 0;
};

class CrowdSteeringSystem {
public:
    CrowdSteeringSystem() = default;

    void SetConfig(const CrowdSteeringConfig& cfg) { config_ = cfg; }
    const CrowdSteeringConfig& GetConfig() const { return config_; }

    // Compute a single unit's conflict-free velocity toward (goalX, goalY),
    // optionally blending a precomputed FlowFieldGrid vector and local
    // SpatialHashGrid neighbor avoidance.
    CrowdAgentSteerOutput ComputeUnitSteering(const CSimulation& sim,
                                              const CUnit& unit,
                                              float goalX,
                                              float goalY,
                                              const FlowFieldGrid* flowField = nullptr);

    // Batch-evaluate and advance a group of moving units over a timestep `dt`
    // using SpatialHashGrid neighborhood queries and deterministic handle order.
    std::vector<CrowdAgentSteerOutput> StepGroupSteering(CSimulation& sim,
                                                         const std::vector<int>& unitHandles,
                                                         float goalX,
                                                         float goalY,
                                                         float dt,
                                                         const FlowFieldGrid* flowField = nullptr);

    void ResetStats() { stats_ = CrowdSteeringStats{}; }
    const CrowdSteeringStats& GetStats() const { return stats_; }

private:
    CrowdSteeringConfig config_{};
    CrowdSteeringStats stats_{};
};

} // namespace pathfinding

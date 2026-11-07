#include "ecs/pathing/crowd_steering.h"

#include <algorithm>
#include <cmath>

#include "ecs/unit.h"

namespace pathfinding {

CrowdAgentSteerOutput CrowdSteeringSystem::ComputeUnitSteering(const CSimulation& sim,
                                                               const CUnit& unit,
                                                               float goalX,
                                                               float goalY,
                                                               const FlowFieldGrid* flowField) {
    CrowdAgentSteerOutput out;
    out.unitHandle = unit.GetHandleId();
    out.newX = unit.GetX();
    out.newY = unit.GetY();
    out.facingDeg = unit.GetFacing();

    if (unit.IsDead() || unit.IsHidden() || unit.IsBuilding()) {
        return out;
    }

    const float speed = std::max(0.0f, unit.GetSpeed());
    if (speed <= 0.0f) {
        return out;
    }

    // 1. Goal-seeking vector
    float gx = goalX - unit.GetX();
    float gy = goalY - unit.GetY();
    const float gDist = std::sqrt(gx * gx + gy * gy);
    if (gDist > 1e-4f) {
        gx /= gDist;
        gy /= gDist;
    } else {
        gx = 0.0f;
        gy = 0.0f;
    }

    float dirX = gx * config_.goalSeekWeight;
    float dirY = gy * config_.goalSeekWeight;

    // 2. Optional FlowFieldGrid vector blend
    if (flowField && sim.GetPathfinding()) {
        float fx = 0.0f;
        float fy = 0.0f;
        if (flowField->SampleDirectionWorld(*sim.GetPathfinding(), unit.GetX(), unit.GetY(), fx, fy)) {
            dirX += fx * config_.flowFieldWeight;
            dirY += fy * config_.flowFieldWeight;
            stats_.flowFieldSamples++;
        }
    }

    // 3. SpatialHashGrid neighborhood RVO / repulsive steering
    const float selfRadius = std::max(16.0f, unit.GetCollisionSize());
    const float queryRadius = std::max(config_.neighborHorizonRadius, selfRadius * 3.0f);
    std::vector<CUnit*> neighbors = sim.QueryUnitsInRadius(unit.GetX(), unit.GetY(), queryRadius);

    float avoidX = 0.0f;
    float avoidY = 0.0f;
    int considered = 0;

    for (const CUnit* other : neighbors) {
        if (!other || other->GetHandleId() == unit.GetHandleId()) continue;
        if (other->IsDead() || other->IsHidden() || other->IsLoadedAboard()) continue;
        if (considered >= config_.maxNeighborsPerAgent) break;

        const float otherRadius = std::max(16.0f, other->GetCollisionSize());
        const float combinedRadius = selfRadius + otherRadius + 12.0f;

        const float dx = unit.GetX() - other->GetX();
        const float dy = unit.GetY() - other->GetY();
        const float distSq = dx * dx + dy * dy;
        if (distSq >= queryRadius * queryRadius) continue;

        considered++;
        const float dist = std::sqrt(distSq);
        float nx = 0.0f;
        float ny = 0.0f;
        if (dist < 1e-3f) {
            // Deterministic tie-breaker by handle order
            nx = (unit.GetHandleId() < other->GetHandleId()) ? -1.0f : 1.0f;
            ny = 0.0f;
        } else {
            nx = dx / dist;
            ny = dy / dist;
        }

        // Smooth inverse-distance repulsion inside combined comfort radius
        if (dist < combinedRadius) {
            const float penetration = (combinedRadius - dist) / combinedRadius;
            const float weight = (other->IsBuilding() ? 1.75f : 1.0f) * penetration * penetration;
            avoidX += nx * weight;
            avoidY += ny * weight;
            out.avoidanceActive = true;
        }
    }

    out.neighborsEvaluated = considered;
    stats_.totalAgentsSteered++;
    stats_.totalNeighborsEvaluated += static_cast<uint64_t>(considered);
    if (out.avoidanceActive) {
        stats_.avoidanceAdjustments++;
    }

    dirX += avoidX * config_.separationWeight;
    dirY += avoidY * config_.separationWeight;

    const float totalLen = std::sqrt(dirX * dirX + dirY * dirY);
    if (totalLen > 1e-4f) {
        dirX /= totalLen;
        dirY /= totalLen;
        out.velX = dirX * speed;
        out.velY = dirY * speed;
        float deg = std::atan2(dirY, dirX) * (180.0f / 3.14159265358979323846f);
        if (deg < 0.0f) deg += 360.0f;
        out.facingDeg = deg;
    }

    return out;
}

std::vector<CrowdAgentSteerOutput> CrowdSteeringSystem::StepGroupSteering(
    CSimulation& sim,
    const std::vector<int>& unitHandles,
    float goalX,
    float goalY,
    float dt,
    const FlowFieldGrid* flowField) {
    // Ensure SpatialHashGrid is up to date before querying neighbors
    sim.SpatialGrid().Rebuild(sim.GetUnits());

    std::vector<CrowdAgentSteerOutput> results;
    results.reserve(unitHandles.size());

    // Phase 1: Compute steering velocities for all units in deterministic order
    for (int h : unitHandles) {
        const CUnit* u = sim.GetUnit(h);
        if (!u) continue;
        CrowdAgentSteerOutput step = ComputeUnitSteering(sim, *u, goalX, goalY, flowField);
        step.newX = u->GetX() + step.velX * dt;
        step.newY = u->GetY() + step.velY * dt;
        results.push_back(step);
    }

    // Phase 2: Commit new positions and facings synchronously so order doesn't skew neighbors mid-step
    for (const CrowdAgentSteerOutput& step : results) {
        if (CUnit* u = sim.GetUnit(step.unitHandle)) {
            u->SetPosition(step.newX, step.newY);
            u->SetFacing(step.facingDeg);
        }
    }
    sim.SpatialGrid().Rebuild(sim.GetUnits());
    return results;
}

} // namespace pathfinding

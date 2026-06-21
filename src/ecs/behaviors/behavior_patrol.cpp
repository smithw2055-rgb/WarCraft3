#include "behavior_patrol.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include <cmath>

CBehaviorPatrol::CBehaviorPatrol() {}

void CBehaviorPatrol::SetPoints(float x1, float y1, float x2, float y2) {
    points_.clear();
    points_.push_back({x1, y1});
    points_.push_back({x2, y2});
    currentTarget_ = 0;
}

void CBehaviorPatrol::Begin(CSimulation& simulation) {
    (void)simulation;
}

void CBehaviorPatrol::End(CSimulation& simulation) {
    (void)simulation;
}

bool CBehaviorPatrol::Update(CSimulation& simulation) {
    if (!unit_ || points_.empty()) return true;

    auto& target = points_[currentTarget_];
    float dx = target.x - unit_->GetX();
    float dy = target.y - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < arrivalDistance_) {
        // Reached waypoint, go to next
        currentTarget_ = (currentTarget_ + 1) % points_.size();
        return false;
    }

    float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
    float step = std::min(speed, dist);
    float nx = unit_->GetX() + (dx / dist) * step;
    float ny = unit_->GetY() + (dy / dist) * step;
    unit_->SetPosition(nx, ny);

    float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
    unit_->SetFacing(facing);

    return false;
}

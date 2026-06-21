#include "behavior_follow.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include <cmath>

CBehaviorFollow::CBehaviorFollow() {}

void CBehaviorFollow::SetTarget(CWidget* target, float followRange) {
    target_ = target;
    followRange_ = followRange;
}

void CBehaviorFollow::Begin(CSimulation& simulation) {
    (void)simulation;
}

void CBehaviorFollow::End(CSimulation& simulation) {
    (void)simulation;
}

bool CBehaviorFollow::Update(CSimulation& simulation) {
    if (!unit_ || !target_) return true;

    float dx = target_->GetX() - unit_->GetX();
    float dy = target_->GetY() - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist <= followRange_) {
        // In range, face target but don't move
        float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
        unit_->SetFacing(facing);
        return false;
    }

    // Move towards target
    float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
    float step = std::min(speed, dist - followRange_ + 16.0f);
    float nx = unit_->GetX() + (dx / dist) * step;
    float ny = unit_->GetY() + (dy / dist) * step;
    unit_->SetPosition(nx, ny);

    float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
    unit_->SetFacing(facing);

    return false;
}

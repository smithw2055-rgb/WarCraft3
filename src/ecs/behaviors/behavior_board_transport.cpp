#include "behavior_board_transport.h"
#include "behavior_move_into_range.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/pathing/world_collision.h"
#include <cmath>

CBehaviorBoardTransport::CBehaviorBoardTransport(float loadRange, int cargoCapacity)
    : loadRange_(loadRange), cargoCapacity_(cargoCapacity) {}

void CBehaviorBoardTransport::Begin(CSimulation& simulation) {
    (void)simulation;
}

void CBehaviorBoardTransport::End(CSimulation& simulation) {
    (void)simulation;
}

bool CBehaviorBoardTransport::TryLoadNow(CSimulation& simulation) {
    (void)simulation;
    if (!unit_ || !target_) return true; // nothing to do
    auto* passenger = dynamic_cast<CUnit*>(target_);
    if (!passenger || passenger->IsDead() || passenger == unit_) return true;
    if (passenger->IsLoadedAboard()) return true;
    if ((int)unit_->GetCargoUnits().size() >= cargoCapacity_) return true; // full
    unit_->LoadUnit(passenger);
    return true;
}

bool CBehaviorBoardTransport::Update(CSimulation& simulation) {
    if (!unit_ || !target_ || target_->IsDead()) return true;

    const float dist = MoveIntoRange::Distance2D(
        unit_->GetX(), unit_->GetY(), target_->GetX(), target_->GetY());
    if (MoveIntoRange::IsWithinRange(
            unit_->GetX(), unit_->GetY(), target_->GetX(), target_->GetY(), loadRange_)) {
        return TryLoadNow(simulation);
    }

    // Close the distance. Straight-line stepping with collision check
    // (transports are usually flyers in open space; full pathfinding is
    // a documented follow-up for ground transports in mazes).
    if (!unit_->TurnTowardPoint(target_->GetX(), target_->GetY())) {
        return false;
    }
    const float speed = unit_->GetSpeed() * simulation.GetSimStepTime();
    if (speed <= 0.0f || dist <= 0.0f) return false;
    const float step = std::min(speed, dist - loadRange_ + 16.0f);
    if (step <= 0.0f) return TryLoadNow(simulation);
    float nx = unit_->GetX(), ny = unit_->GetY();
    if (!MoveIntoRange::StepToward(unit_->GetX(), unit_->GetY(),
                                   target_->GetX(), target_->GetY(), step, nx, ny)) {
        return TryLoadNow(simulation);
    }
    if (auto* collision = simulation.GetWorldCollision()) {
        const float radius = std::max(unit_->GetCollisionSize(), 16.0f);
        if (collision->CheckCollision(nx, ny, radius, unit_)) {
            return false; // blocked; retry next tick
        }
    }
    unit_->SetPosition(nx, ny);
    return false;
}

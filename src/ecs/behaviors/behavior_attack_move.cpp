#include "behavior_attack_move.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/pathing/world_collision.h"
#include <cmath>

CBehaviorAttackMove::CBehaviorAttackMove() {}

void CBehaviorAttackMove::SetDestination(float x, float y) {
    destX_ = x;
    destY_ = y;
}

void CBehaviorAttackMove::Begin(CSimulation& simulation) {
    (void)simulation;
}

void CBehaviorAttackMove::End(CSimulation& simulation) {
    (void)simulation;
}

bool CBehaviorAttackMove::Update(CSimulation& simulation) {
    if (!unit_) return true;

    float dx = destX_ - unit_->GetX();
    float dy = destY_ - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < arrivalDistance_) {
        return true; // arrived
    }

    // Periodically scan for enemies
    int tick = simulation.GetGameTurnTick();
    if (tick - lastAttackScanTick_ > 5) { // scan every 5 ticks
        lastAttackScanTick_ = tick;
        float acqRange = unit_->GetUnitType()->GetDefaultAcquisitionRange();
        if (acqRange <= 0) acqRange = 256.0f;

        std::vector<CUnit*> enemies;
        simulation.GetWorldCollision()->EnumUnitsInRange(
            unit_->GetX(), unit_->GetY(), acqRange, enemies);

        for (auto* e : enemies) {
            if (e == unit_ || e->IsDead()) continue;
            if (e->GetPlayerIndex() == unit_->GetPlayerIndex()) continue; // skip allies
            // Found enemy, switch to attack
            targetHandle_ = e->GetHandleId();
            break;
        }
    }

    if (targetHandle_ >= 0) {
        // Found target - attack it. The main loop will handle behavior transitions.
        // For now, just move towards the enemy.
        CUnit* target = simulation.GetUnit(targetHandle_);
        if (!target || target->IsDead()) {
            targetHandle_ = -1; // target gone, resume move
        } else {
            float edx = target->GetX() - unit_->GetX();
            float edy = target->GetY() - unit_->GetY();
            float edist = std::sqrt(edx * edx + edy * edy);
            float attackRange = 128.0f;

            if (edist > attackRange) {
                float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
                float step = std::min(speed, edist);
                unit_->SetPosition(unit_->GetX() + (edx / edist) * step,
                                   unit_->GetY() + (edy / edist) * step);
                unit_->SetFacing(std::atan2(edx, edy) * 180.0f / 3.14159f);
            }
            return false;
        }
    }

    // Move towards destination
    float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
    float step = std::min(speed, dist);
    float nx = unit_->GetX() + (dx / dist) * step;
    float ny = unit_->GetY() + (dy / dist) * step;
    unit_->SetPosition(nx, ny);
    unit_->SetFacing(std::atan2(dx, dy) * 180.0f / 3.14159f);

    return false;
}

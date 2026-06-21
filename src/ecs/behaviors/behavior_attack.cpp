#include "behavior_attack.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/pathing/pathfinding.h"
#include <cmath>

CBehaviorAttack::CBehaviorAttack() {}

void CBehaviorAttack::Begin(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
    pathRecalcCooldown_ = 0;
}

void CBehaviorAttack::End(CSimulation& simulation) {
    (void)simulation;
    waypoints_.clear();
}

void CBehaviorAttack::SetTarget(CWidget* target) {
    target_ = target;
    waypoints_.clear();
}

bool CBehaviorAttack::Update(CSimulation& simulation) {
    if (!unit_ || !target_) return true;

    float dx = target_->GetX() - unit_->GetX();
    float dy = target_->GetY() - unit_->GetY();
    float dist = std::sqrt(dx * dx + dy * dy);

    // Face target
    float facing = std::atan2(dx, dy) * 180.0f / 3.14159f;
    unit_->SetFacing(facing);

    if (dist > attackRange_) {
        // Use pathfinding for longer distances, direct for short
        if (dist > 300.0f) {
            // Recalc path periodically
            pathRecalcCooldown_--;
            if (waypoints_.empty() || pathRecalcCooldown_ <= 0) {
                waypoints_.clear();
                auto* pathfinding = simulation.GetPathfinding();
                if (pathfinding) {
                    std::vector<std::pair<float,float>> worldPath;
                    if (pathfinding->FindPathWorld(unit_->GetX(), unit_->GetY(),
                                                    target_->GetX(), target_->GetY(), worldPath)) {
                        for (size_t i = 1; i < worldPath.size(); i++)
                            waypoints_.push_back(worldPath[i]);
                    }
                }
                pathRecalcCooldown_ = 8;
            }

            if (!waypoints_.empty()) {
                auto& wp = waypoints_.front();
                float wdx = wp.first - unit_->GetX();
                float wdy = wp.second - unit_->GetY();
                float wdist = std::sqrt(wdx * wdx + wdy * wdy);
                if (wdist < 48.0f) {
                    waypoints_.pop_front();
                } else {
                    float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
                    float step = std::min(speed, wdist);
                    unit_->SetPosition(unit_->GetX() + (wdx / wdist) * step,
                                       unit_->GetY() + (wdy / wdist) * step);
                    return false;
                }
            }
        }

        // Direct movement if no path or short distance
        float speed = unit_->GetSpeed() * simulation.GetSimStepTime() * 10.0f;
        float step = std::min(speed, dist - attackRange_ + 16.0f);
        float nx = unit_->GetX() + (dx / dist) * step;
        float ny = unit_->GetY() + (dy / dist) * step;
        unit_->SetPosition(nx, ny);
        return false;
    }

    // In range - attack
    int currentTick = simulation.GetGameTurnTick();
    if (currentTick - lastAttackTick_ >= 10) { // 10 ticks = 1 sec cooldown
        lastAttackTick_ = currentTick;

        // Get weapon sound from first attack
        std::string weaponSound;
        float damageAmount = 10.0f;
        auto* unitType = unit_->GetUnitType();
        if (unitType) {
            auto& attacks = unitType->GetAttacks();
            if (!attacks.empty()) {
                weaponSound = attacks[0].weaponSound;
                // Compute actual damage
                int dice = attacks[0].damageDice;
                int sides = attacks[0].damageSidesPerDie;
                int base = attacks[0].damageBase;
                if (dice > 0 && sides > 0) {
                    int roll = 0;
                    for (int d = 0; d < dice; d++)
                        roll += (std::rand() % sides) + 1;
                    damageAmount = (float)(base + roll);
                } else {
                    damageAmount = (float)base;
                }
            }
        }

        target_->Damage(simulation, unit_, true, false,
                        CAttackType::NORMAL, CDamageType::NORMAL,
                        weaponSound, damageAmount, 0);
        if (target_->IsDead()) {
            return true; // target dead, behavior done
        }
    }

    return false;
}

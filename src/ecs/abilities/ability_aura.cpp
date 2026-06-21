#include "ability_aura.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include <cmath>
#include <algorithm>

CAbilityAura::CAbilityAura(int handleId, War3ID code)
    : CAbility(handleId, code, CAbilityCategory::BUFF) {
    iconShowing_ = false;
    permanent_ = true;
}

void CAbilityAura::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    affectedUnitHandles_.clear();
}

void CAbilityAura::OnRemove(CSimulation& game, CUnit* unit) {
    // Remove aura from all affected units
    for (int handle : affectedUnitHandles_) {
        auto* target = game.GetUnit(handle);
        if (target) {
            RemoveAura(game, target);
        }
    }
    affectedUnitHandles_.clear();
}

void CAbilityAura::OnTick(CSimulation& game, CUnit* unit) {
    if (!unit || unit->IsDead()) return;

    float radiusSq = radius_ * radius_;
    auto* ownerPlayer = game.GetPlayer(unit->GetPlayerIndex());

    // Find all units in radius
    std::vector<int> currentInRange;

    for (auto* other : game.GetUnits()) {
        if (other->IsDead()) continue;
        if (other == unit && !affectsSelf_) continue;

        float dx = other->GetX() - unit->GetX();
        float dy = other->GetY() - unit->GetY();
        float distSq = dx * dx + dy * dy;

        if (distSq > radiusSq) continue;

        // Check alliance
        auto* otherPlayer = game.GetPlayer(other->GetPlayerIndex());
        bool isAlly = ownerPlayer && otherPlayer &&
                      ownerPlayer->HasAlliance(*otherPlayer, CAllianceType::ALLIED);

        bool shouldAffect = false;
        if (enemyOnly_) {
            shouldAffect = !isAlly;
        } else if (affectsAllies_) {
            shouldAffect = isAlly || (other == unit && affectsSelf_);
        } else {
            shouldAffect = (other == unit && affectsSelf_);
        }

        if (shouldAffect) {
            currentInRange.push_back(other->GetHandleId());

            // Check if already affected
            auto it = std::find(affectedUnitHandles_.begin(), affectedUnitHandles_.end(),
                                other->GetHandleId());
            if (it == affectedUnitHandles_.end()) {
                // Newly in range - apply aura
                ApplyAura(game, other);
            }
        }
    }

    // Remove aura from units that left range
    for (auto it = affectedUnitHandles_.begin(); it != affectedUnitHandles_.end();) {
        auto inRange = std::find(currentInRange.begin(), currentInRange.end(), *it);
        if (inRange == currentInRange.end()) {
            // Unit left range
            auto* target = game.GetUnit(*it);
            if (target) {
                RemoveAura(game, target);
            }
            it = affectedUnitHandles_.erase(it);
        } else {
            ++it;
        }
    }

    // Add newly affected units
    for (int handle : currentInRange) {
        auto it = std::find(affectedUnitHandles_.begin(), affectedUnitHandles_.end(), handle);
        if (it == affectedUnitHandles_.end()) {
            affectedUnitHandles_.push_back(handle);
        }
    }
}

void CAbilityAura::ApplyAura(CSimulation& game, CUnit* target) {
    (void)game;
    for (const auto& bonus : bonuses_) {
        switch (bonus.type) {
            case CAuraEffectType::MOVE_SPEED:
                if (bonus.isPercent) {
                    target->SetSpeed((int)(target->GetSpeed() * (1.0f + bonus.value)));
                } else {
                    target->SetSpeed(target->GetSpeed() + (int)bonus.value);
                }
                break;
            case CAuraEffectType::ARMOR:
                // TODO: apply armor bonus
                break;
            case CAuraEffectType::DAMAGE:
                // TODO: apply damage bonus
                break;
            case CAuraEffectType::LIFE_REGEN:
                // TODO: apply life regen bonus
                break;
            default:
                break;
        }
    }
}

void CAbilityAura::RemoveAura(CSimulation& game, CUnit* target) {
    (void)game;
    // Remove the bonuses (reverse of ApplyAura)
    for (const auto& bonus : bonuses_) {
        switch (bonus.type) {
            case CAuraEffectType::MOVE_SPEED:
                if (bonus.isPercent) {
                    target->SetSpeed((int)(target->GetSpeed() / (1.0f + bonus.value)));
                } else {
                    target->SetSpeed(target->GetSpeed() - (int)bonus.value);
                }
                break;
            default:
                break;
        }
    }
}

#include "ability_aura.h"
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include <cmath>
#include <algorithm>

namespace {
// GetUnit() only knows units owned by the simulation map. Fall back to a
// linear scan so aura removal can't leak stats when the pointer is present
// in GetUnits() but not in the handle map (also covers test harnesses).
CUnit* FindAuraUnit(CSimulation& game, int handleId) {
    if (CUnit* u = game.GetUnit(handleId)) return u;
    for (CUnit* u : game.GetUnits()) {
        if (u && u->GetHandleId() == handleId) return u;
    }
    return nullptr;
}
}

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
    (void)unit;
    // Remove aura from all affected units
    for (int handle : affectedUnitHandles_) {
        if (CUnit* target = FindAuraUnit(game, handle)) {
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
            if (CUnit* target = FindAuraUnit(game, *it)) {
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
    if (!target) return;
    for (const auto& bonus : bonuses_) {
        switch (bonus.type) {
            case CAuraEffectType::MOVE_SPEED:
                if (bonus.isPercent) {
                    target->SetSpeed((int)(target->GetSpeed() * (1.0f + bonus.value)));
                } else {
                    target->SetSpeed(target->GetSpeed() + (int)bonus.value);
                }
                break;
            case CAuraEffectType::ATTACK_SPEED:
                // Percent (e.g. +10%) multiplies attack period; flat adds
                // fractionally as well so remove stays symmetric.
                target->MultiplyAttackSpeed(1.0f + bonus.value);
                break;
            case CAuraEffectType::ARMOR:
                target->AddBonusArmor((int)std::lround(bonus.value));
                break;
            case CAuraEffectType::DAMAGE: {
                int flat = 0;
                if (bonus.isPercent) {
                    float base = 0.0f;
                    if (auto* type = target->GetUnitType()) {
                        const auto& attacks = type->GetAttacks();
                        if (!attacks.empty()) {
                            base = (float)attacks[0].damageBase;
                            if (attacks[0].damageDice > 0 && attacks[0].damageSidesPerDie > 0)
                                base += (float)attacks[0].damageDice *
                                        (float)(attacks[0].damageSidesPerDie + 1) * 0.5f;
                        }
                    }
                    flat = (int)std::lround(base * bonus.value);
                } else {
                    flat = (int)std::lround(bonus.value);
                }
                target->AddBonusDamage(flat);
                break;
            }
            case CAuraEffectType::LIFE_REGEN:
                target->AddBonusLifeRegen(bonus.value);
                break;
            case CAuraEffectType::MANA_REGEN:
                target->AddBonusManaRegen(bonus.value);
                break;
            default:
                break;
        }
    }
}

void CAbilityAura::RemoveAura(CSimulation& game, CUnit* target) {
    (void)game;
    if (!target) return;
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
            case CAuraEffectType::ATTACK_SPEED:
                target->DivideAttackSpeed(1.0f + bonus.value);
                break;
            case CAuraEffectType::ARMOR:
                target->AddBonusArmor(-(int)std::lround(bonus.value));
                break;
            case CAuraEffectType::DAMAGE: {
                int flat = 0;
                if (bonus.isPercent) {
                    float base = 0.0f;
                    if (auto* type = target->GetUnitType()) {
                        const auto& attacks = type->GetAttacks();
                        if (!attacks.empty()) {
                            base = (float)attacks[0].damageBase;
                            if (attacks[0].damageDice > 0 && attacks[0].damageSidesPerDie > 0)
                                base += (float)attacks[0].damageDice *
                                        (float)(attacks[0].damageSidesPerDie + 1) * 0.5f;
                        }
                    }
                    flat = (int)std::lround(base * bonus.value);
                } else {
                    flat = (int)std::lround(bonus.value);
                }
                target->AddBonusDamage(-flat);
                break;
            }
            case CAuraEffectType::LIFE_REGEN:
                target->AddBonusLifeRegen(-bonus.value);
                break;
            case CAuraEffectType::MANA_REGEN:
                target->AddBonusManaRegen(-bonus.value);
                break;
            default:
                break;
        }
    }
}

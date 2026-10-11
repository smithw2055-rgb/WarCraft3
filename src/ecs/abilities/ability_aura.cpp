#include "ability_aura.h"
#include "ecs/buffs.h"
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
            RemoveAura(game, unit, target);
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
                ApplyAura(game, unit, other);
            }
        }
    }

    // Remove aura from units that left range
    for (auto it = affectedUnitHandles_.begin(); it != affectedUnitHandles_.end();) {
        auto inRange = std::find(currentInRange.begin(), currentInRange.end(), *it);
        if (inRange == currentInRange.end()) {
            // Unit left range
            if (CUnit* target = FindAuraUnit(game, *it)) {
                RemoveAura(game, unit, target);
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

void CAbilityAura::ApplyAura(CSimulation& game, CUnit* caster, CUnit* target) {
    if (!target) return;
    // Buff-based application (ref BuffAuraBase): one aura buff per caster
    // carrying all bonuses, so overlapping auras take the strongest bonus
    // per slot and dispels can strip them. The unit's non-stacking table
    // does the math; single-aura results match the old direct-mutation
    // path exactly.
    std::vector<CBuffAuraStat::Bonus> bonuses;
    for (const auto& bonus : bonuses_) {
        bonuses.push_back({bonus.type, bonus.value, bonus.isPercent});
    }
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    target->AddBuff(std::make_unique<CBuffAuraStat>(GetCode(), std::move(bonuses), casterHandle),
                    game);
}

void CAbilityAura::RemoveAura(CSimulation& game, CUnit* caster, CUnit* target) {
    if (!target) return;
    // Removing this caster's aura buff unregisters its bonuses and
    // recomputes what overlapping auras still grant.
    const int casterHandle = caster ? caster->GetHandleId() : -1;
    target->RemoveBuffByIdAndCaster(GetCode(), casterHandle, game);
}

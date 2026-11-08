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
    : CAbility(handleId, code, CAbilityCategory::BUFF)
    , affectsAllies_(true)
    , affectsSelf_(true)
    , enemyOnly_(false) {
    iconShowing_ = false;
    permanent_ = true;
}

CAbilityAura::CAbilityAura(int handleId, War3ID code, float radius,
                           bool affectsAllies, bool affectsSelf, bool enemyOnly,
                           std::initializer_list<CAuraBonus> bonuses)
    : CAbilityAura(handleId, code) {
    radius_ = radius;
    affectsAllies_ = affectsAllies;
    affectsSelf_ = affectsSelf;
    enemyOnly_ = enemyOnly;
    bonuses_.assign(bonuses.begin(), bonuses.end());
}

void CAbilityAura::OnAdd(CSimulation& game, CUnit* unit) {
    (void)game; (void)unit;
    affectedUnitHandles_.clear();
}

void CAbilityAura::OnRemove(CSimulation& game, CUnit* unit) {
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

    const float radiusSq = radius_ * radius_;
    auto* ownerPlayer = game.GetPlayer(unit->GetPlayerIndex());

    // Find all units in radius
    std::vector<int> currentInRange;

    for (auto* other : game.GetUnits()) {
        if (other->IsDead()) continue;
        if (other == unit && !affectsSelf_) continue;

        const float dx = other->GetX() - unit->GetX();
        const float dy = other->GetY() - unit->GetY();
        if (dx * dx + dy * dy > radiusSq) continue;

        auto* otherPlayer = game.GetPlayer(other->GetPlayerIndex());
        const bool isAlly = ownerPlayer && otherPlayer &&
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
            const bool alreadyAffected =
                std::find(affectedUnitHandles_.begin(), affectedUnitHandles_.end(),
                          other->GetHandleId()) != affectedUnitHandles_.end();
            // Newly in range or periodic buff aura refresh
            if (!alreadyAffected || (auraBuffId_.value != 0 && auraBuffFactory_)) {
                ApplyAura(game, unit, other);
            }
        }
    }

    // Remove aura from units that left range
    for (auto it = affectedUnitHandles_.begin(); it != affectedUnitHandles_.end();) {
        if (std::find(currentInRange.begin(), currentInRange.end(), *it) == currentInRange.end()) {
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
        if (std::find(affectedUnitHandles_.begin(), affectedUnitHandles_.end(), handle) ==
            affectedUnitHandles_.end()) {
            affectedUnitHandles_.push_back(handle);
        }
    }
}

void CAbilityAura::ApplyAura(CSimulation& game, CUnit* caster, CUnit* target) {
    if (!target) return;
    if (auraBuffId_.value != 0 && auraBuffFactory_) {
        if (CBuff* existing = target->GetBuff(auraBuffId_)) {
            existing->RefreshDuration();
        } else if (auto buff = auraBuffFactory_()) {
            target->AddBuff(std::move(buff), game);
        }
    }
    if (!bonuses_.empty()) {
        std::vector<CBuffAuraStat::Bonus> bonuses;
        bonuses.reserve(bonuses_.size());
        for (const auto& bonus : bonuses_) {
            bonuses.push_back({bonus.type, bonus.value, bonus.isPercent});
        }
        const int casterHandle = caster ? caster->GetHandleId() : -1;
        target->AddBuff(std::make_unique<CBuffAuraStat>(GetCode(), std::move(bonuses), casterHandle),
                        game);
    }
}

void CAbilityAura::RemoveAura(CSimulation& game, CUnit* caster, CUnit* target) {
    if (!target) return;
    if (auraBuffId_.value != 0) {
        target->RemoveBuff(auraBuffId_, game);
    }
    if (!bonuses_.empty()) {
        const int casterHandle = caster ? caster->GetHandleId() : -1;
        target->RemoveBuffByIdAndCaster(GetCode(), casterHandle, game);
    }
}

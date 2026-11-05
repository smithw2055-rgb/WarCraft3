#include "animation_utils.h"
#include <algorithm>
#include <cctype>

namespace animation_utils {

void AppendUniqueAnimationTag(std::vector<std::string>& tags, std::string tag) {
    if (tag.empty()) {
        return;
    }
    std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
        tags.push_back(std::move(tag));
    }
}

bool IsUnitWorkingFromPassiveState(const CUnit* unit) {
    if (!unit) {
        return false;
    }
    if (const auto* queueAbility = dynamic_cast<const CAbilityQueue*>(unit->GetAbility(War3ID("Aque")))) {
        if (queueAbility->GetQueueCount() > 0) {
            return true;
        }
    }
    if (const auto* goldMineAbility = dynamic_cast<const CAbilityGoldMine*>(unit->GetAbility(War3ID("Agld")))) {
        if (goldMineAbility->GetActiveMinerCount() > 0) {
            return true;
        }
    }
    if (const auto* harvestBehavior = GetHarvestBehavior(unit)) {
        if (harvestBehavior->IsGathering()) {
            return true;
        }
    }
    if (const auto* buildBehavior = GetBuildBehavior(unit)) {
        if (buildBehavior->IsActivelyBuilding()) {
            return true;
        }
    }
    if (const auto* repairBehavior = GetRepairBehavior(unit)) {
        if (repairBehavior->IsRepairing()) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> CollectUnitAnimationPropertyTags(
    const CUnit* unit,
    const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags) {
    std::vector<std::string> tags;
    if (!unit) {
        return tags;
    }

    if (const auto* unitType = unit->GetUnitType()) {
        for (const std::string& tag : unitType->GetRequiredAnimationTags()) {
            AppendUniqueAnimationTag(tags, tag);
        }
    }

    if (scriptPropertyTags) {
        const auto scriptIt = scriptPropertyTags->find(unit->GetHandleId());
        if (scriptIt != scriptPropertyTags->end()) {
            for (const std::string& tag : scriptIt->second) {
                AppendUniqueAnimationTag(tags, tag);
            }
        }
    }

    if (IsUnitWorkingFromPassiveState(unit)) {
        AppendUniqueAnimationTag(tags, "work");
    }

    return tags;
}

std::vector<std::string> BuildContextualAnimationTags(
    const CUnit* unit,
    const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags,
    CResourceType carriedType,
    std::initializer_list<const char*> extraTags) {
    std::vector<std::string> tags = CollectUnitAnimationPropertyTags(unit, scriptPropertyTags);
    for (const char* extraTag : extraTags) {
        if (extraTag && extraTag[0]) {
            AppendUniqueAnimationTag(tags, extraTag);
        }
    }
    switch (carriedType) {
        case CResourceType::GOLD:
            AppendUniqueAnimationTag(tags, "gold");
            break;
        case CResourceType::LUMBER:
            AppendUniqueAnimationTag(tags, "lumber");
            break;
        default:
            break;
    }
    return tags;
}

const CBehaviorHarvest* GetHarvestBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorHarvest*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorAttack* GetAttackBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorAttack*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorMove* GetMoveBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorMove*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorFollow* GetFollowBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorFollow*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorPatrol* GetPatrolBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorPatrol*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorAttackMove* GetAttackMoveBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorAttackMove*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorReturnResources* GetReturnResourcesBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorReturnResources*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorBuild* GetBuildBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorBuild*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorRepair* GetRepairBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorRepair*>(unit->GetCurrentBehavior()) : nullptr;
}
const CBehaviorSpell* GetSpellBehavior(const CUnit* unit) {
    return unit ? dynamic_cast<const CBehaviorSpell*>(unit->GetCurrentBehavior()) : nullptr;
}

UnitVisualAnimationMode ResolveUnitVisualAnimationMode(
    const CUnit* unit,
    bool isMoving,
    CBehaviorCategory behaviorCategory,
    CResourceType carriedType,
    int gameTurnTick) {
    if (!unit) {
        return UnitVisualAnimationMode::NONE;
    }
    if (unit->IsDead()) {
        if (!unit->IsCorpse()) {
            return UnitVisualAnimationMode::DEAD;
        }
        if (unit->IsHero() && unit->IsBoneCorpse()) {
            return UnitVisualAnimationMode::DISSIPATE;
        }
        if (unit->IsBoneCorpse()) {
            return UnitVisualAnimationMode::BONE_CORPSE;
        }
        return UnitVisualAnimationMode::CORPSE;
    }
    if (unit->IsConstructing()) {
        return UnitVisualAnimationMode::CONSTRUCT;
    }
    if (unit->IsBuilding()) {
        return UnitVisualAnimationMode::READY;
    }

    if (const auto* moveBehavior = GetMoveBehavior(unit)) {
        if (!moveBehavior->IsPropelling()) {
            switch (carriedType) {
                case CResourceType::GOLD:
                    return UnitVisualAnimationMode::READY_GOLD;
                case CResourceType::LUMBER:
                    return UnitVisualAnimationMode::READY_LUMBER;
                default:
                    return UnitVisualAnimationMode::READY;
            }
        }
    }

    if (const auto* buildBehavior = GetBuildBehavior(unit)) {
        if (buildBehavior->IsMovingToSite()) {
            return buildBehavior->IsPropelling()
                ? UnitVisualAnimationMode::WALK_WORK
                : UnitVisualAnimationMode::READY;
        }
        if (buildBehavior->IsActivelyBuilding()) {
            return UnitVisualAnimationMode::WORK;
        }
    }

    if (const auto* repairBehavior = GetRepairBehavior(unit)) {
        if (repairBehavior->IsMovingToTarget()) {
            return repairBehavior->IsPropelling()
                ? UnitVisualAnimationMode::WALK_WORK
                : UnitVisualAnimationMode::READY;
        }
        if (repairBehavior->IsRepairing()) {
            return UnitVisualAnimationMode::WORK;
        }
    }

    if (const auto* harvestBehavior = GetHarvestBehavior(unit)) {
        if (harvestBehavior->IsMovingToResource()) {
            return harvestBehavior->IsPropelling()
                ? UnitVisualAnimationMode::WALK_WORK
                : UnitVisualAnimationMode::READY;
        }
        if (harvestBehavior->IsGathering()) {
            return UnitVisualAnimationMode::WORK;
        }
    }

    if (const auto* returnBehavior = GetReturnResourcesBehavior(unit)) {
        if (returnBehavior->IsMovingToDropoff()) {
            if (returnBehavior->IsPropelling()) {
                if (carriedType == CResourceType::GOLD) {
                    return UnitVisualAnimationMode::WALK_GOLD;
                }
                if (carriedType == CResourceType::LUMBER) {
                    return UnitVisualAnimationMode::WALK_LUMBER;
                }
                return UnitVisualAnimationMode::WALK;
            }
            if (carriedType == CResourceType::GOLD) {
                return UnitVisualAnimationMode::READY_GOLD;
            }
            if (carriedType == CResourceType::LUMBER) {
                return UnitVisualAnimationMode::READY_LUMBER;
            }
            return UnitVisualAnimationMode::READY;
        }
        if (returnBehavior->IsDepositing()) {
            if (carriedType == CResourceType::GOLD) {
                return UnitVisualAnimationMode::READY_GOLD;
            }
            if (carriedType == CResourceType::LUMBER) {
                return UnitVisualAnimationMode::READY_LUMBER;
            }
            return UnitVisualAnimationMode::READY;
        }
    }

    if (const auto* spellBehavior = GetSpellBehavior(unit)) {
        if (spellBehavior->IsMovingToRange()) {
            return spellBehavior->IsPropelling()
                ? UnitVisualAnimationMode::WALK
                : UnitVisualAnimationMode::READY;
        }
        if (spellBehavior->IsFacingTarget()) {
            return UnitVisualAnimationMode::READY;
        }
        if (spellBehavior->IsCasting() || spellBehavior->IsBackswing()) {
            return UnitVisualAnimationMode::SPELL;
        }
    }

    if (const auto* attackBehavior = GetAttackBehavior(unit)) {
        if (attackBehavior->IsPropelling()) {
            return UnitVisualAnimationMode::WALK;
        }
        if (attackBehavior->IsAttackAnimationActive(gameTurnTick)) {
            return UnitVisualAnimationMode::ATTACK;
        }
        return UnitVisualAnimationMode::READY;
    }

    if (behaviorCategory == CBehaviorCategory::RETURN_RESOURCES && isMoving) {
        if (carriedType == CResourceType::GOLD) {
            return UnitVisualAnimationMode::WALK_GOLD;
        }
        if (carriedType == CResourceType::LUMBER) {
            return UnitVisualAnimationMode::WALK_LUMBER;
        }
        return UnitVisualAnimationMode::WALK;
    }

    if (isMoving) {
        if (carriedType == CResourceType::GOLD) {
            return UnitVisualAnimationMode::WALK_GOLD;
        }
        if (carriedType == CResourceType::LUMBER) {
            return UnitVisualAnimationMode::WALK_LUMBER;
        }
        return UnitVisualAnimationMode::WALK;
    }

    switch (carriedType) {
        case CResourceType::GOLD:
            return UnitVisualAnimationMode::READY_GOLD;
        case CResourceType::LUMBER:
            return UnitVisualAnimationMode::READY_LUMBER;
        default:
            return UnitVisualAnimationMode::READY;
    }
}

} // namespace animation_utils

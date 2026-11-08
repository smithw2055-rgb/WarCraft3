#include "animation_utils.h"
#include <algorithm>
#include <cctype>

namespace animation_utils {

namespace {

UnitVisualAnimationMode ReadyModeForCarry(CResourceType carriedType) {
    switch (carriedType) {
        case CResourceType::GOLD:
            return UnitVisualAnimationMode::READY_GOLD;
        case CResourceType::LUMBER:
            return UnitVisualAnimationMode::READY_LUMBER;
        default:
            return UnitVisualAnimationMode::READY;
    }
}

UnitVisualAnimationMode WalkModeForCarry(CResourceType carriedType) {
    switch (carriedType) {
        case CResourceType::GOLD:
            return UnitVisualAnimationMode::WALK_GOLD;
        case CResourceType::LUMBER:
            return UnitVisualAnimationMode::WALK_LUMBER;
        default:
            return UnitVisualAnimationMode::WALK;
    }
}

} // namespace

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
    for (const CAbility* ability : unit->GetAbilities()) {
        if (ability && ability->IsActivelyWorking()) {
            return true;
        }
    }
    if (const CBehavior* beh = unit->GetCurrentBehavior()) {
        if (beh->IsActivelyWorking()) {
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

    if (const CBehavior* beh = unit->GetCurrentBehavior()) {
        const CBehaviorCategory cat = beh->GetCategory();
        if (cat == CBehaviorCategory::MOVE && !beh->IsPropelling()) {
            return ReadyModeForCarry(carriedType);
        }
        if (beh->IsMovingToWorkSite()) {
            return beh->IsPropelling()
                ? UnitVisualAnimationMode::WALK_WORK
                : UnitVisualAnimationMode::READY;
        }
        if (beh->IsActivelyWorking()) {
            return UnitVisualAnimationMode::WORK;
        }
        if (beh->IsMovingToDropoff()) {
            return beh->IsPropelling()
                ? WalkModeForCarry(carriedType)
                : ReadyModeForCarry(carriedType);
        }
        if (beh->IsDepositing()) {
            return ReadyModeForCarry(carriedType);
        }
        if (cat == CBehaviorCategory::SPELL) {
            if (beh->IsMovingToRange()) {
                return beh->IsPropelling()
                    ? UnitVisualAnimationMode::WALK
                    : UnitVisualAnimationMode::READY;
            }
            if (beh->IsFacingTarget()) {
                return UnitVisualAnimationMode::READY;
            }
            if (beh->IsCasting() || beh->IsBackswing()) {
                return UnitVisualAnimationMode::SPELL;
            }
        }
        if (cat == CBehaviorCategory::ATTACK) {
            if (beh->IsPropelling()) {
                return UnitVisualAnimationMode::WALK;
            }
            if (beh->IsAttackAnimationActive(gameTurnTick)) {
                return UnitVisualAnimationMode::ATTACK;
            }
            return UnitVisualAnimationMode::READY;
        }
    }

    if (behaviorCategory == CBehaviorCategory::RETURN_RESOURCES && isMoving) {
        return WalkModeForCarry(carriedType);
    }

    if (isMoving) {
        return WalkModeForCarry(carriedType);
    }

    return ReadyModeForCarry(carriedType);
}

} // namespace animation_utils

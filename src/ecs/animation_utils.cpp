#include "animation_utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace animation_utils {

void ClassifyScriptedAnimationName(const std::string& lowerName,
                                   bool& outLoop,
                                   bool& outQueueStandAfterCurrent) {
    if (lowerName.find("death") != std::string::npos
        || lowerName.find("decay") != std::string::npos
        || lowerName.find("corpse") != std::string::npos
        || lowerName.find("dissipate") != std::string::npos) {
        outLoop = false;
        outQueueStandAfterCurrent = false;
        return;
    }
    if (lowerName.find("stand") != std::string::npos
        || lowerName.find("walk") != std::string::npos
        || lowerName.find("swim") != std::string::npos
        || lowerName.find("channel") != std::string::npos
        || lowerName.find("work") != std::string::npos) {
        outLoop = true;
        outQueueStandAfterCurrent = false;
        return;
    }
    outLoop = false;
    outQueueStandAfterCurrent = true;
}

bool GetRequestedAnimationLowerName(const MdxViewer* viewer,
                                    const PendingUnitAnimation& request,
                                    std::string& outLowerName) {
    outLowerName.clear();
    if (request.useSequenceIndex) {
        std::string sequenceName;
        if (!viewer || !viewer->GetSequenceName(request.sequenceIndex, sequenceName)) {
            return false;
        }
        outLowerName = sequenceName;
    }
    else if (!request.sequenceName.empty()) {
        outLowerName = request.sequenceName;
    }
    else {
        return false;
    }
    std::transform(outLowerName.begin(), outLowerName.end(), outLowerName.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return true;
}

std::vector<std::string> ExtractAnimationSecondaryTags(const std::string& properties) {
    std::vector<std::string> tags;
    std::string lower = properties;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    auto maybeAdd = [&](const char* tag) {
        if (lower.find(tag) != std::string::npos
            && std::find(tags.begin(), tags.end(), tag) == tags.end()) {
            tags.emplace_back(tag);
        }
    };
    maybeAdd("work");
    maybeAdd("gold");
    maybeAdd("lumber");
    maybeAdd("alternate");
    maybeAdd("swim");
    maybeAdd("fast");
    maybeAdd("defend");
    maybeAdd("spell");
    maybeAdd("channel");
    maybeAdd("throw");
    return tags;
}

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

void PlayReadyVisualForUnit(CUnit* unit,
                            MdxViewer* viewer,
                            CBehaviorCategory behaviorCategory,
                            CResourceType carriedType,
                            const SplatMetadataStore& splatMeta,
                            const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags) {
    if (!viewer) {
        return;
    }

    auto tags = BuildContextualAnimationTags(unit, scriptPropertyTags, carriedType);
    const bool preferReady = behaviorCategory == CBehaviorCategory::ATTACK
        || behaviorCategory == CBehaviorCategory::ATTACK_MOVE
        || behaviorCategory == CBehaviorCategory::HOLD_POSITION;
    if (preferReady) {
        AppendUniqueAnimationTag(tags, "ready");
    }
    if (viewer->PlayStandSequenceWithTags(tags)) {
        return;
    }
    if (preferReady) {
        auto readyOnlyTags = BuildContextualAnimationTags(unit, scriptPropertyTags, CResourceType::NONE, {"ready"});
        if (viewer->PlayStandSequenceWithTags(readyOnlyTags)) {
            return;
        }
    }
    viewer->PlayStandSequence();
}

bool PlayWalkVisualForUnit(CUnit* unit,
                           MdxViewer* viewer,
                           CBehaviorCategory behaviorCategory,
                           CResourceType carriedType,
                           const SplatMetadataStore& splatMeta,
                           const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags) {
    (void)behaviorCategory;
    if (!unit || !viewer) {
        return false;
    }
    const float movementSpeed = unit->GetSpeed();
    auto tags = BuildContextualAnimationTags(unit, scriptPropertyTags, carriedType);

    float animationMoveSpeed = 0.0f;
    const float animationScale = std::max(0.001f, viewer->GetAnimationSpeedScale());
    if (const auto* meta = splatMeta.GetUnitRenderSplatMeta(*unit)) {
        const float walkSpeed = meta->animationWalkSpeed;
        const float runSpeed = meta->animationRunSpeed;
        if (walkSpeed > 0.0f && runSpeed > walkSpeed) {
            const float midpoint = (walkSpeed + runSpeed) * 0.5f;
            if (movementSpeed >= midpoint) {
                AppendUniqueAnimationTag(tags, "fast");
                animationMoveSpeed = runSpeed;
            }
            else {
                animationMoveSpeed = walkSpeed;
            }
        }
        else if (walkSpeed > 0.0f) {
            animationMoveSpeed = walkSpeed;
        }
    }
    if (animationMoveSpeed > 0.0f) {
        animationMoveSpeed *= animationScale;
    }

    if (animationMoveSpeed > 0.0f && movementSpeed > 0.0f) {
        const float speedRatio = std::clamp(movementSpeed / animationMoveSpeed, 0.1f, 4.0f);
        if (viewer->PlayWalkSequenceWithTagsAtSpeedRatio(tags, speedRatio)) {
            return true;
        }
        return viewer->PlayWalkSequenceAtSpeedRatio(speedRatio);
    }

    if (viewer->PlayWalkSequenceWithTagsAtSpeed(tags, movementSpeed)) {
        return true;
    }
    return viewer->PlayWalkSequenceAtSpeed(movementSpeed);
}

bool PlayWalkWorkVisualForUnit(CUnit* unit,
                                MdxViewer* viewer,
                                CBehaviorCategory behaviorCategory,
                                CResourceType carriedType,
                                const SplatMetadataStore& splatMeta,
                                const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags) {
    if (!unit || !viewer) {
        return false;
    }

    const float movementSpeed = unit->GetSpeed();
    auto workWalkTags = BuildContextualAnimationTags(unit, scriptPropertyTags, carriedType, {"work"});

    float animationMoveSpeed = 0.0f;
    const float animationScale = std::max(0.001f, viewer->GetAnimationSpeedScale());
    if (const auto* meta = splatMeta.GetUnitRenderSplatMeta(*unit)) {
        const float walkSpeed = meta->animationWalkSpeed;
        const float runSpeed = meta->animationRunSpeed;
        if (walkSpeed > 0.0f && runSpeed > walkSpeed) {
            const float midpoint = (walkSpeed + runSpeed) * 0.5f;
            if (movementSpeed >= midpoint) {
                AppendUniqueAnimationTag(workWalkTags, "fast");
                animationMoveSpeed = runSpeed;
            }
            else {
                animationMoveSpeed = walkSpeed;
            }
        }
        else if (walkSpeed > 0.0f) {
            animationMoveSpeed = walkSpeed;
        }
    }
    if (animationMoveSpeed > 0.0f) {
        animationMoveSpeed *= animationScale;
    }

    if (animationMoveSpeed > 0.0f && movementSpeed > 0.0f) {
        const float speedRatio = std::clamp(movementSpeed / animationMoveSpeed, 0.1f, 4.0f);
        if (viewer->PlayWalkSequenceWithTagsAtSpeedRatio(workWalkTags, speedRatio)) {
            return true;
        }
    }
    else if (viewer->PlayWalkSequenceWithTagsAtSpeed(workWalkTags, movementSpeed)) {
        return true;
    }

    return PlayWalkVisualForUnit(unit, viewer, behaviorCategory, carriedType, splatMeta, scriptPropertyTags);
}

bool PlayWorkVisualForUnit(CUnit* unit,
                           MdxViewer* viewer,
                           CBehaviorCategory behaviorCategory,
                           CResourceType carriedType,
                           const SplatMetadataStore& splatMeta,
                           const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags) {
    if (!viewer) {
        return false;
    }

    auto workTags = BuildContextualAnimationTags(unit, scriptPropertyTags, carriedType, {"work"});
    if (viewer->PlayStandSequenceWithTags(workTags)) {
        return true;
    }

    auto workOnlyTags = BuildContextualAnimationTags(unit, scriptPropertyTags, CResourceType::NONE, {"work"});
    if (viewer->PlayStandSequenceWithTags(workOnlyTags)) {
        return true;
    }

    PlayReadyVisualForUnit(unit, viewer, behaviorCategory, carriedType, splatMeta, scriptPropertyTags);
    return true;
}

bool PlayDeathVisualForUnit(CUnit* unit,
                            MdxViewer* viewer,
                            UnitVisualAnimationMode mode,
                            const CSimulation& simulation) {
    if (!unit || !viewer) {
        return false;
    }
    switch (mode) {
        case UnitVisualAnimationMode::DEAD:
            return viewer->PlayDeathSequence();
        case UnitVisualAnimationMode::CORPSE:
            return viewer->PlayDecaySequenceWithTag("flesh", simulation.GetGameplayConstants().decayTime);
        case UnitVisualAnimationMode::BONE_CORPSE:
            return viewer->PlayDecaySequenceWithTag("bone", unit->GetEndingDecayTime(simulation));
        case UnitVisualAnimationMode::DISSIPATE:
            return viewer->PlayDissipateSequence(unit->GetEndingDecayTime(simulation));
        default:
            return false;
    }
}

bool PlaySpellVisualForUnit(CUnit* unit,
                            MdxViewer* viewer,
                            const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags) {
    if (!unit || !viewer) {
        return false;
    }
    const auto* spellBehavior = GetSpellBehavior(unit);
    if (!spellBehavior) {
        return false;
    }
    const float duration = spellBehavior->GetSpellAnimationDuration();
    if (spellBehavior->IsCasting()) {
        auto spellTags = BuildContextualAnimationTags(unit, scriptPropertyTags, CResourceType::NONE, {"spell"});
        if (viewer->PlaySpellSequenceWithTags(spellTags, duration)) {
            return true;
        }
        return viewer->PlaySpellSequence(duration);
    }
    if (spellBehavior->IsBackswing()) {
        auto channelTags = BuildContextualAnimationTags(unit, scriptPropertyTags, CResourceType::NONE, {"channel"});
        if (viewer->PlayChannelSequenceWithTags(channelTags, duration)) {
            return true;
        }
        if (viewer->PlayChannelSequence(duration)) {
            return true;
        }
        auto spellTags = BuildContextualAnimationTags(unit, scriptPropertyTags, CResourceType::NONE, {"spell"});
        if (viewer->PlaySpellSequenceWithTags(spellTags, duration)) {
            return true;
        }
        return viewer->PlaySpellSequence(duration);
    }
    return false;
}

bool ComputeUnitVisualIsMoving(CUnit* unit,
                               std::unordered_map<int, std::pair<float, float>>& lastPositions) {
    if (!unit) {
        return false;
    }
    if (const auto* moveBehavior = GetMoveBehavior(unit)) {
        return moveBehavior->IsPropelling();
    }
    if (const auto* followBehavior = GetFollowBehavior(unit)) {
        return followBehavior->IsPropelling();
    }
    if (const auto* patrolBehavior = GetPatrolBehavior(unit)) {
        return patrolBehavior->IsPropelling();
    }
    if (const auto* attackMoveBehavior = GetAttackMoveBehavior(unit)) {
        return attackMoveBehavior->IsPropelling();
    }
    if (const auto* harvestBehavior = GetHarvestBehavior(unit)) {
        return harvestBehavior->IsPropelling();
    }
    if (const auto* returnBehavior = GetReturnResourcesBehavior(unit)) {
        return returnBehavior->IsPropelling();
    }
    if (const auto* buildBehavior = GetBuildBehavior(unit)) {
        return buildBehavior->IsPropelling();
    }
    if (const auto* repairBehavior = GetRepairBehavior(unit)) {
        return repairBehavior->IsPropelling();
    }
    if (const auto* spellBehavior = GetSpellBehavior(unit)) {
        return spellBehavior->IsPropelling();
    }
    if (const auto* attackBehavior = GetAttackBehavior(unit)) {
        return attackBehavior->IsPropelling();
    }

    const int handleId = unit->GetHandleId();
    const std::pair<float, float> currentPos{unit->GetX(), unit->GetY()};
    const auto lastPosIt = lastPositions.find(handleId);
    const bool hadLastPos = lastPosIt != lastPositions.end();
    bool isMoving = false;

    if (hadLastPos) {
        const float dx = currentPos.first - lastPosIt->second.first;
        const float dy = currentPos.second - lastPosIt->second.second;
        isMoving = (dx * dx + dy * dy) > 0.01f;
    }
    lastPositions[handleId] = currentPos;
    if (!isMoving) {
        auto* behavior = unit->GetCurrentBehavior();
        isMoving = behavior && (
            behavior->GetCategory() == CBehaviorCategory::MOVE
            || behavior->GetCategory() == CBehaviorCategory::FOLLOW
            || behavior->GetCategory() == CBehaviorCategory::PATROL
            || behavior->GetCategory() == CBehaviorCategory::ATTACK_MOVE);
    }
    return isMoving;
}

} // namespace animation_utils
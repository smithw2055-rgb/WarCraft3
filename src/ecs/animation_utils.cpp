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
    if (IsUnitWorkingFromPassiveState(unit)) {
        AppendUniqueAnimationTag(tags, "work");
    }
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
    (void)behaviorCategory;
    if (!unit || !viewer) {
        return false;
    }
    const float movementSpeed = unit->GetSpeed();
    auto tags = BuildContextualAnimationTags(unit, scriptPropertyTags, carriedType, {"work"});

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
    const CBehavior* spellBehavior = unit->GetCurrentBehavior();
    if (!spellBehavior || spellBehavior->GetCategory() != CBehaviorCategory::SPELL) {
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
    if (const CBehavior* beh = unit->GetCurrentBehavior()) {
        if (beh->TracksPropellingState()) {
            return beh->IsPropelling();
        }
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
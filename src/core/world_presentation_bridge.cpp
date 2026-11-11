#include "core/game_controller.h"
#include "ecs/animation_utils.h"
#include "ecs/behaviors/behavior.h"
#include "ecs/destructable.h"
#include "ecs/item.h"
#include "ecs/unit_text_utils.h"
#include "ecs/widget.h"
#include "engine/simulation_module.h"
#include "render/mdx_event_splat.h"
#include "render/picking.h"
#include "render/unit_selection_geom.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

Vec4 MdxEventSplatTint(const std::array<std::array<float, 4>, 3>& colors,
                       const std::array<float, 3>& intervalTimes,
                       int intervalCount,
                       float lifeSpanValue,
                       float lifeRemaining) {
    auto normalizeColor = [](const std::array<float, 4>& color) {
        auto channel = [](float value) {
            if (!std::isfinite(value)) {
                return 1.0f;
            }
            return std::clamp(value / 255.0f, 0.0f, 1.0f);
        };
        return Vec4(channel(color[0]), channel(color[1]), channel(color[2]), channel(color[3]));
    };
    auto lerp = [](const Vec4& a, const Vec4& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return Vec4(a.x + (b.x - a.x) * t,
                    a.y + (b.y - a.y) * t,
                    a.z + (b.z - a.z) * t,
                    a.w + (b.w - a.w) * t);
    };

    const Vec4 start = normalizeColor(colors[0]);
    const Vec4 middle = normalizeColor(colors[1]);
    const Vec4 end = normalizeColor(colors[2]);
    const float lifeSpan = std::max(lifeSpanValue, 0.0001f);
    const float age = std::clamp(lifeSpan - lifeRemaining, 0.0f, lifeSpan);

    if (intervalCount >= 3) {
        const float birth = std::max(0.0f, intervalTimes[0]);
        const float pause = std::max(0.0f, intervalTimes[1]);
        const float decay = std::max(0.0f, intervalTimes[2]);
        if (birth > 0.0001f && age < birth) {
            return lerp(start, middle, age / birth);
        }
        if (age < birth + pause || decay <= 0.0001f) {
            return middle;
        }
        return lerp(middle, end, (age - birth - pause) / decay);
    }

    if (intervalCount >= 2) {
        const float sustain = std::max(0.0f, intervalTimes[0]);
        const float decay = std::max(0.0f, intervalTimes[1]);
        if (sustain > 0.0001f && age < sustain) {
            return lerp(start, middle, age / sustain);
        }
        if (decay <= 0.0001f) {
            return middle;
        }
        return lerp(middle, end, (age - sustain) / decay);
    }

    return start;
}

float MdxEventSplatCell(const std::array<std::array<float, 3>, 2>& uvIntervals,
                        const std::array<float, 3>& intervalTimes,
                        int intervalCount,
                        int columns,
                        int rows,
                        float lifeSpanValue,
                        float lifeRemaining) {
    if (columns <= 1 && rows <= 1) {
        return 0.0f;
    }
    const float lifeSpan = std::max(lifeSpanValue, 0.0001f);
    const float age = std::clamp(lifeSpan - lifeRemaining, 0.0f, lifeSpan);
    int intervalIndex = 0;
    float factor = 0.0f;
    if (intervalCount >= 2) {
        const float sustain = std::max(0.0f, intervalTimes[0]);
        const float decay = std::max(0.0f, intervalTimes[1]);
        if (sustain > 0.0001f && age < sustain) {
            factor = age / sustain;
            intervalIndex = 0;
        } else {
            factor = decay > 0.0001f ? (age - sustain) / decay : 1.0f;
            intervalIndex = 1;
        }
    }

    const auto& interval = uvIntervals[intervalIndex];
    const float start = interval[0];
    const float end = interval[1];
    const float repeat = interval[2];
    const float spriteCount = end - start;
    const float maxCell = (float)std::max(0, columns * rows - 1);
    if (spriteCount > 0.0f) {
        const float advanced = std::fmod(std::floor(spriteCount * repeat * std::clamp(factor, 0.0f, 1.0f)),
                                         spriteCount);
        return std::clamp(start + advanced, 0.0f, maxCell);
    }
    return std::clamp(start, 0.0f, maxCell);
}

Vec4 MdxEventSplatUvRect(const std::array<std::array<float, 3>, 2>& uvIntervals,
                         const std::array<float, 3>& intervalTimes,
                         int intervalCount,
                         int columns,
                         int rows,
                         float lifeSpanValue,
                         float lifeRemaining) {
    columns = std::max(1, columns);
    rows = std::max(1, rows);
    const float cell = MdxEventSplatCell(uvIntervals, intervalTimes, intervalCount,
                                         columns, rows, lifeSpanValue, lifeRemaining);
    const float left = std::floor(std::fmod(cell, (float)columns)) / (float)columns;
    const float top = std::floor(cell / (float)columns) / (float)rows;
    return Vec4(left, top, left + (1.0f / (float)columns), top + (1.0f / (float)rows));
}

SplatModel::BlendMode MdxEventSplatBlendMode(int blendMode) {
    switch (blendMode) {
    case 1:
        return SplatModel::BlendMode::AdditiveAlpha;
    case 2:
        return SplatModel::BlendMode::Modulate;
    case 3:
        return SplatModel::BlendMode::Modulate2x;
    case 4:
        return SplatModel::BlendMode::Blend;
    case 0:
    default:
        return SplatModel::BlendMode::Blend;
    }
}

// --- Helpers (animation / visual) ---

bool CurrentAnimationRequestMatchesViewer(MdxViewer* viewer,
                                          const animation_utils::PendingUnitAnimation& request) {
    if (!viewer || !viewer->GetAnimator()) return false;
    if (request.useTags) {
        return viewer->IsPlayingSequenceByPrimaryAndTags(request.primaryTag.c_str(), request.secondaryTags);
    }
    const int currentSequence = viewer->GetAnimator()->GetCurrentSequence();
    if (request.useSequenceIndex) {
        return currentSequence == request.sequenceIndex;
    }
    if (request.sequenceName.empty()) {
        return false;
    }
    std::string currentName;
    if (!viewer->GetSequenceName(currentSequence, currentName)) {
        return false;
    }
    const std::string requested = LowerAscii(request.sequenceName);
    const std::string current = LowerAscii(currentName);
    return current.find(requested) != std::string::npos;
}

bool ApplyAnimationRequestToViewer(int handleId,
                                   MdxViewer* viewer,
                                   const animation_utils::PendingUnitAnimation& request,
                                   GameController* g) {
    if (!viewer || !viewer->GetAnimator()) return false;
    if (!request.force && CurrentAnimationRequestMatchesViewer(viewer, request)) return true;

    bool loop = false;
    bool queueStandAfterCurrent = true;
    std::string lowerName;
    if (request.useTags) {
        lowerName = request.primaryTag;
        for (const std::string& tag : request.secondaryTags) {
            lowerName += " ";
            lowerName += tag;
        }
        animation_utils::ClassifyScriptedAnimationName(lowerName, loop, queueStandAfterCurrent);
    } else if (animation_utils::GetRequestedAnimationLowerName(viewer, request, lowerName)) {
        animation_utils::ClassifyScriptedAnimationName(lowerName, loop, queueStandAfterCurrent);
    }

    bool played = false;
    const float speedRatio = std::clamp(request.speedRatio, 0.1f, 4.0f);
    if (request.useTags) {
        if (!request.force && viewer->IsPlayingSequenceByPrimaryAndTags(request.primaryTag.c_str(), request.secondaryTags)) {
            played = true;
        } else if (request.durationSeconds > 0.0f) {
            if (request.primaryTag == "attack") {
                played = viewer->PlayAttackSequenceWithTagsAndDuration(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "spell" || request.primaryTag.empty()) {
                played = viewer->PlaySpellSequenceWithTags(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "channel") {
                played = viewer->PlayChannelSequenceWithTags(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "decay") {
                played = viewer->PlayDecaySequenceWithTags(request.secondaryTags, request.durationSeconds);
            } else if (request.primaryTag == "dissipate") {
                played = viewer->PlayDissipateSequence(request.durationSeconds);
            }
        }
        if (!played) {
            played = viewer->PlaySequenceByPrimaryAndTags(request.primaryTag.c_str(), request.secondaryTags,
                                                          loop, speedRatio, queueStandAfterCurrent, request.force);
        }
    } else if (request.useSequenceIndex) {
        played = viewer->PlaySequenceByIndex(request.sequenceIndex, loop, speedRatio, queueStandAfterCurrent);
    } else if (!request.sequenceName.empty()) {
        played = viewer->PlaySequenceByName(request.sequenceName.c_str(), loop, speedRatio, queueStandAfterCurrent);
    }

    if (played) {
        auto storedRequest = request;
        storedRequest.force = false;
        g->GetPresentation().unitScriptedAnimations[handleId] = {storedRequest, loop, queueStandAfterCurrent};
    }
    return played;
}

void ApplyPendingUnitAnimation(int handleId, MdxViewer* viewer, GameController* g) {
    if (!viewer || !viewer->GetAnimator()) return;
    const auto pendingIt = g->GetPresentation().unitPendingAnimations.find(handleId);
    if (pendingIt == g->GetPresentation().unitPendingAnimations.end()) return;
    ApplyAnimationRequestToViewer(handleId, viewer, pendingIt->second, g);
    g->GetPresentation().unitPendingAnimations.erase(pendingIt);
}

void QueueUnitAnimationRequest(int handleId,
                               const animation_utils::PendingUnitAnimation& request,
                               GameController* g) {
    if (!g) return;
    g->GetPresentation().unitQueuedAnimations[handleId].push_back(request);
}

bool ApplyNextQueuedUnitAnimation(int handleId, MdxViewer* viewer, GameController* g) {
    if (!viewer || !viewer->GetAnimator() || !g) return false;
    auto queuedIt = g->GetPresentation().unitQueuedAnimations.find(handleId);
    if (queuedIt == g->GetPresentation().unitQueuedAnimations.end() || queuedIt->second.empty()) {
        return false;
    }
    auto request = queuedIt->second.front();
    queuedIt->second.pop_front();
    if (queuedIt->second.empty()) {
        g->GetPresentation().unitQueuedAnimations.erase(queuedIt);
    }
    if (ApplyAnimationRequestToViewer(handleId, viewer, request, g)) {
        g->GetPresentation().unitPendingAnimations.erase(handleId);
        return true;
    }
    g->GetPresentation().unitPendingAnimations[handleId] = std::move(request);
    return false;
}

void PlayResolvedUnitAnimation(CUnit* unit,
                               MdxViewer* viewer,
                               GameController* g,
                               animation_utils::UnitVisualAnimationMode mode) {
    if (!unit || !viewer) return;

    const int handleId = unit->GetHandleId();
    auto& splatMeta = g->GetSplatMeta();
    const auto* scriptTags = &g->GetPresentation().unitAnimationPropertyTags;

    // If script requested a specific animation, use it
    ApplyPendingUnitAnimation(handleId, viewer, g);

    const auto scriptIt = g->GetPresentation().unitScriptedAnimations.find(handleId);
    if (scriptIt != g->GetPresentation().unitScriptedAnimations.end()) {
        const auto& state = scriptIt->second;
        if (CurrentAnimationRequestMatchesViewer(viewer, state.request)) {
            return;
        }
        bool applied = false;
        const float speedRatio = std::clamp(state.request.speedRatio, 0.1f, 4.0f);
        if (state.request.useTags) {
            applied = viewer->PlaySequenceByPrimaryAndTags(
                state.request.primaryTag.c_str(), state.request.secondaryTags, state.loop, speedRatio,
                state.queueStandAfterCurrent, state.request.force);
        } else if (state.request.useSequenceIndex) {
            applied = viewer->PlaySequenceByIndex(state.request.sequenceIndex, state.loop, speedRatio,
                                                  state.queueStandAfterCurrent);
        } else if (!state.request.sequenceName.empty()) {
            applied = viewer->PlaySequenceByName(state.request.sequenceName.c_str(), state.loop, speedRatio,
                                                 state.queueStandAfterCurrent);
        }
        if (applied) {
            return;
        }
        g->GetPresentation().unitScriptedAnimations.erase(scriptIt);
    }

    const CBehaviorCategory behaviorCategory =
        unit_text_utils::GetUnitCurrentBehaviorCategory(unit);
    const CResourceType carriedType =
        unit_text_utils::GetUnitCarriedResourceType(unit);

    switch (mode) {
        case animation_utils::UnitVisualAnimationMode::DEAD:
        case animation_utils::UnitVisualAnimationMode::CORPSE:
        case animation_utils::UnitVisualAnimationMode::BONE_CORPSE:
        case animation_utils::UnitVisualAnimationMode::DISSIPATE:
            animation_utils::PlayDeathVisualForUnit(unit, viewer, mode, g->GetSimulation());
            return;
        case animation_utils::UnitVisualAnimationMode::CONSTRUCT: {
            const auto lastModeIt = g->GetPresentation().unitVisualAnimationModes.find(handleId);
            if (lastModeIt == g->GetPresentation().unitVisualAnimationModes.end()
                || lastModeIt->second != animation_utils::UnitVisualAnimationMode::CONSTRUCT) {
                viewer->PlayBirthThenStandSequence();
            }
            float buildRatio = 0.0f;
            if (const auto* unitType = unit->GetUnitType()) {
                const float buildTime = static_cast<float>(unitType->GetBuildTime());
                if (buildTime > 0.0f) {
                    buildRatio = unit->GetConstructionProgress() / buildTime;
                }
            }
            viewer->SetCurrentSequenceFrameByRatio(std::clamp(buildRatio, 0.0f, 1.0f));
            return;
        }
        case animation_utils::UnitVisualAnimationMode::READY:
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::NONE, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::READY_GOLD:
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::GOLD, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::READY_LUMBER:
            animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::LUMBER, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK:
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::NONE, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK_GOLD:
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::GOLD, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK_LUMBER:
            animation_utils::PlayWalkVisualForUnit(unit, viewer, behaviorCategory,
                CResourceType::LUMBER, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WALK_WORK:
            animation_utils::PlayWalkWorkVisualForUnit(unit, viewer, behaviorCategory,
                carriedType, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::WORK:
            animation_utils::PlayWorkVisualForUnit(unit, viewer, behaviorCategory,
                carriedType, splatMeta, scriptTags);
            return;
        case animation_utils::UnitVisualAnimationMode::SPELL:
            if (!animation_utils::PlaySpellVisualForUnit(unit, viewer, scriptTags)) {
                animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                    carriedType, splatMeta, scriptTags);
            }
            return;
        case animation_utils::UnitVisualAnimationMode::ATTACK: {
            const int currentTick = g->GetSimulation().GetGameTurnTick();
            const CBehavior* attackBehavior = unit ? unit->GetCurrentBehavior() : nullptr;
            const int behaviorAttackStartTick =
                attackBehavior ? attackBehavior->GetAttackAnimationStartTick() : -1;
            const float behaviorAttackDuration =
                attackBehavior ? attackBehavior->GetAttackAnimationDuration() : 0.0f;
            const auto lastAttackIt = g->GetPresentation().unitVisualLastAttackAnimationTicks.find(handleId);
            const int lastAttackTick = (lastAttackIt != g->GetPresentation().unitVisualLastAttackAnimationTicks.end())
                ? lastAttackIt->second
                : std::numeric_limits<int>::min();
            const bool shouldStartAttackAnimation = attackBehavior
                && attackBehavior->IsAttackAnimationActive(currentTick)
                && behaviorAttackStartTick >= 0
                && behaviorAttackStartTick != lastAttackTick;
            if (shouldStartAttackAnimation) {
                const auto attackTags = animation_utils::BuildContextualAnimationTags(
                    unit, scriptTags, CResourceType::NONE);
                if (viewer->PlayAttackSequenceWithTagsAndDuration(
                        attackTags, behaviorAttackDuration)) {
                    g->GetPresentation().unitVisualLastAttackAnimationTicks[handleId] =
                        behaviorAttackStartTick;
                } else {
                    animation_utils::PlayReadyVisualForUnit(unit, viewer, behaviorCategory,
                        carriedType, splatMeta, scriptTags);
                }
            } else if (!attackBehavior || !attackBehavior->IsAttackAnimationActive(currentTick)) {
                if (!viewer->IsPlayingSequenceWithToken("stand")
                    || viewer->IsCurrentSequenceFinished()) {
                    animation_utils::PlayReadyVisualForUnit(unit, viewer,
                        CBehaviorCategory::ATTACK, carriedType, splatMeta, scriptTags);
                }
            }
            return;
        }
        default:
            return;
    }
}

void SyncSimulationUnitAnimation(CUnit* unit, MdxViewer* viewer, GameController* g) {
    if (!unit || !viewer) return;
    const int handleId = unit->GetHandleId();

    const auto lastPosIt = g->GetPresentation().unitVisualLastPositions.find(handleId);
    bool posChanged = false;
    if (lastPosIt != g->GetPresentation().unitVisualLastPositions.end()) {
        float dx = unit->GetX() - lastPosIt->second.first;
        float dy = unit->GetY() - lastPosIt->second.second;
        posChanged = (dx * dx + dy * dy) > 0.01f;
    } else {
        posChanged = true;
    }

    if (posChanged) {
        g->GetPresentation().unitVisualLastPositions[handleId] = {unit->GetX(), unit->GetY()};
    }

    // Re-evaluate animation from live behavior, work, and carried-resource
    // state rather than only position changes.
    const bool isMoving = animation_utils::ComputeUnitVisualIsMoving(
        unit, g->GetPresentation().unitVisualLastPositions);
    const bool wasMoving = g->GetPresentation().unitVisualMovingStates.count(handleId)
        && g->GetPresentation().unitVisualMovingStates[handleId];
    const CBehaviorCategory behaviorCategory =
        unit_text_utils::GetUnitCurrentBehaviorCategory(unit);
    const CResourceType carriedType =
        unit_text_utils::GetUnitCarriedResourceType(unit);
    const std::vector<std::string> currentPropertyTags =
        animation_utils::CollectUnitAnimationPropertyTags(
            unit, &g->GetPresentation().unitAnimationPropertyTags);

    const auto lastModeIt = g->GetPresentation().unitVisualAnimationModes.find(handleId);
    const auto lastMode = (lastModeIt != g->GetPresentation().unitVisualAnimationModes.end())
        ? lastModeIt->second
        : animation_utils::UnitVisualAnimationMode::NONE;

    const auto lastPropertyTagsIt = g->GetPresentation().unitVisualPropertyTags.find(handleId);
    const bool propertyTagsChanged = (lastPropertyTagsIt == g->GetPresentation().unitVisualPropertyTags.end())
        || (lastPropertyTagsIt->second != currentPropertyTags);

    const auto lastResourceIt = g->GetPresentation().unitVisualResourceTags.find(handleId);
    const bool resourceChanged = (lastResourceIt == g->GetPresentation().unitVisualResourceTags.end())
        || (lastResourceIt->second != carriedType);

    const auto resolvedMode = animation_utils::ResolveUnitVisualAnimationMode(
        unit, isMoving, behaviorCategory, carriedType,
        g->GetSimulation().GetGameTurnTick());
    const bool shouldRefreshAttack =
        resolvedMode == animation_utils::UnitVisualAnimationMode::ATTACK
        && viewer->IsCurrentSequenceFinished();

    bool needsResolve = (isMoving != wasMoving)
        || (resolvedMode != lastMode)
        || propertyTagsChanged
        || resourceChanged
        || shouldRefreshAttack
        || (g->GetPresentation().unitPendingAnimations.count(handleId) != 0);

    if (needsResolve) {
        PlayResolvedUnitAnimation(unit, viewer, g, resolvedMode);
    }

    g->GetPresentation().unitVisualConstructionStates[handleId] = unit->IsConstructing();
    g->GetPresentation().unitVisualResourceTags[handleId] = carriedType;
    g->GetPresentation().unitVisualPropertyTags[handleId] = currentPropertyTags;
    g->GetPresentation().unitVisualAnimationModes[handleId] = resolvedMode;
    g->GetPresentation().unitVisualMovingStates[handleId] = isMoving;
    if (resolvedMode != animation_utils::UnitVisualAnimationMode::ATTACK) {
        g->GetPresentation().unitVisualLastAttackAnimationTicks.erase(handleId);
    }

    // Handle queued animations
    const auto queuedIt = g->GetPresentation().unitQueuedAnimations.find(handleId);
    if (queuedIt != g->GetPresentation().unitQueuedAnimations.end() && !queuedIt->second.empty()) {
        if (!viewer->IsCurrentSequenceLooping() && viewer->IsCurrentSequenceFinished()) {
            ApplyNextQueuedUnitAnimation(handleId, viewer, g);
        }
    }

    // Sync interpolated position/rotation/scale/tint via SceneManager & RenderInterpolationBuffer
    render::RenderTransformSnapshot interpSnap;
    const bool hasInterp = g->GetPresentation().SampleInterpolatedUnit(handleId, interpSnap);
    const float sampleX = hasInterp ? interpSnap.position.x : unit->GetX();
    const float sampleY = hasInterp ? interpSnap.position.z : unit->GetY();
    const float sampleFacing = hasInterp ? interpSnap.facingDeg : unit->GetFacing();

    float visualHeight = unit_selection_geom::ComputeUnitVisualHeight(
        *unit, g->GetTexturedTerrain().IsValid(),
        [&](float x, float z) { return g->GetTexturedTerrain().GetGroundHeight(x, z); });
    if (hasInterp) {
        const float groundH = g->GetTexturedTerrain().IsValid()
            ? g->GetTexturedTerrain().GetGroundHeight(sampleX, sampleY)
            : 0.0f;
        visualHeight = groundH + interpSnap.flyHeight;
    }
    {
        float pos[3] = {sampleX, visualHeight, sampleY};
        auto vit = g->GetPresentation().unitToVisualIdx.find(handleId);
        if (vit != g->GetPresentation().unitToVisualIdx.end()) {
            g->GetScene().UpdateInstancePosition(vit->second, pos, sampleFacing);
            // Script scale (SetUnitScale) and module visual scale rules multiply the seed scale.
            float seedScale[3] = {1.0f, 1.0f, 1.0f};
            auto seedIt = g->GetPresentation().unitVisualSeeds.find(handleId);
            if (seedIt != g->GetPresentation().unitVisualSeeds.end()) {
                seedScale[0] = seedIt->second.scale[0];
                seedScale[1] = seedIt->second.scale[1];
                seedScale[2] = seedIt->second.scale[2];
            }
            float modScaleMul = 1.0f;
            uint32_t modTintRgba = 0xFFFFFFFFu;
            if (!hasInterp) {
                if (const auto* simMod = g->GetSimulation().GetSimulationModule()) {
                    if (!simMod->OnResolveUnitVisualScaleAndTintForUnit(*unit, modScaleMul, modTintRgba)) {
                        simMod->OnResolveUnitVisualScaleAndTint(unit->GetTypeId().value, modScaleMul, modTintRgba);
                    }
                }
            }
            const float s = hasInterp ? interpSnap.scale.x : (unit->GetScale() * modScaleMul);
            const float scale[3] = {seedScale[0] * s, seedScale[1] * s, seedScale[2] * s};
            g->GetScene().SetInstanceScale(vit->second, scale);

            int vr = 255, vg = 255, vb = 255, va = 255;
            unit->GetVertexColor(vr, vg, vb, va);
            const float modR = static_cast<float>((modTintRgba >> 24) & 0xFFu) / 255.0f;
            const float modG = static_cast<float>((modTintRgba >> 16) & 0xFFu) / 255.0f;
            const float modB = static_cast<float>((modTintRgba >> 8) & 0xFFu) / 255.0f;
            const float modA = static_cast<float>(modTintRgba & 0xFFu) / 255.0f;
            g->GetScene().SetInstanceVertexColor(
                vit->second,
                hasInterp ? interpSnap.vertexColor.x : ((vr / 255.0f) * modR),
                hasInterp ? interpSnap.vertexColor.y : ((vg / 255.0f) * modG),
                hasInterp ? interpSnap.vertexColor.z : ((vb / 255.0f) * modB),
                hasInterp ? interpSnap.vertexColor.w : ((va / 255.0f) * modA)
            );
        }
    }
}

} // namespace

void GameController::WireJassCallbacks() {
    if (!jassNatives_) return;

    auto* jassData = jassNatives_->GetData().get();
    if (!jassData) return;

    // Save-slot metadata records the map (ref SaveGame slot header).
    jassData->saveMapPath = mapPath_;

    // Ref SetTerrainFogEx/ResetTerrainFog: script override wins while set;
    // ResetTerrainFog drops back to the w3i baseline (defaultFogSettings).
    jassData->terrainFogHook = [this]() {
        auto data = jassNatives_ ? jassNatives_->GetData() : nullptr;
        if (!data) return;
        const auto& fog = data->terrainFog;
        if (fog.overridden) {
            dayNight_.SetMapFog(true, fog.r, fog.g, fog.b, fog.density);
            dayNight_.SetMapFogRange(fog.start, fog.end);
            return;
        }
        if (w3i_.useTerrainFog) {
            dayNight_.SetMapFog(true,
                w3i_.fogColor[0] / 255.0f, w3i_.fogColor[1] / 255.0f,
                w3i_.fogColor[2] / 255.0f, w3i_.fogDensity);
            dayNight_.SetMapFogRange(w3i_.fogHeight[0], w3i_.fogHeight[1]);
        } else {
            engine::ISimulationModule::TilesetEnvironmentProfile tsProf;
            const auto* simMod = simulation_.GetSimulationModule();
            const char tsCode = map_.tileset ? map_.tileset : w3i_.tileset;
            if (simMod && tsCode != 0 &&
                simMod->OnResolveTilesetEnvironmentProfile(tsCode, tsProf)) {
                dayNight_.SetMapFog(false, tsProf.fogColorR, tsProf.fogColorG, tsProf.fogColorB, 0.0f);
                dayNight_.SetMapFogRange(tsProf.fogStart, tsProf.fogEnd);
            } else {
                dayNight_.SetMapFog(false, 0.5f, 0.5f, 0.5f, 0.0f);
            }
        }
    };
    // Ref GetLocationZ reads the map terrain height; route JASS queries to
    // the live textured terrain (0 when no map is loaded).
    jassData->SetTerrainHeightCallback([this](float x, float y) -> float {
        float height = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, y) : 0.0f;
        if (jassNatives_ && jassNatives_->GetData()) {
            height += jassNatives_->GetData()->TerrainDeformOffset(x, y);
        }
        return height;
    });
    jassData->SetLocalPlayerIndex(config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0);
    // Ref ForFiles enumerates the datasource listfile; route JASS queries
    // to the live archive set (empty when headless / unmounted).
    jassData->SetFileLister([this](const std::string& pattern) -> std::vector<std::string> {
        if (mpq_.ArchiveCount() == 0) return {};
        return mpq_.ListAllFiles(pattern);
    });
    // Ref doPreloadScript reads file bytes from the datasource.
    jassData->SetScriptReader([this](const std::string& path) -> std::vector<uint8_t> {
        std::vector<uint8_t> out;
        mpq_.ReadFile(path, out);
        return out;
    });

    jassData->camSetPosition = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z); camera_.ResetToHome();
    };
    jassData->camSetTarget = [this](float x, float y, float z) {
        camera_.SetTarget(x, y, z); camera_.ResetToHome();
    };
    jassData->camReset = [this]() { camera_.ResetToHome(); };
    jassData->camGetX = [this]() -> float { return camera_.GetTarget().x; };
    jassData->camGetY = [this]() -> float { return camera_.GetTarget().y; };
    jassData->camGetZ = [this]() -> float { return camera_.GetTarget().z; };
    jassData->camGetEyeX = [this]() -> float { return camera_.GetPosition().x; };
    jassData->camGetEyeY = [this]() -> float { return camera_.GetPosition().y; };
    jassData->camGetEyeZ = [this]() -> float { return camera_.GetPosition().z; };

    auto submitOrApplyAnim = [this](int simHandle, animation_utils::PendingUnitAnimation request, bool clearScripted = false) {
        if (request.force) {
            presentation_.unitQueuedAnimations.erase(simHandle);
            if (clearScripted) {
                presentation_.unitScriptedAnimations.erase(simHandle);
            }
        }
        auto it = presentation_.unitToVisualIdx.find(simHandle);
        if (it != presentation_.unitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                presentation_.unitPendingAnimations.erase(simHandle);
                return;
            }
        }
        presentation_.unitPendingAnimations[simHandle] = std::move(request);
    };
    auto queueOrApplyAnim = [this](int simHandle, animation_utils::PendingUnitAnimation request, bool fallbackWhenNoAnimator) {
        auto it = presentation_.unitToVisualIdx.find(simHandle);
        if (it != presentation_.unitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && viewer->GetAnimator()) {
                if (!viewer->IsCurrentSequenceFinished()) {
                    QueueUnitAnimationRequest(simHandle, request, this);
                    return;
                }
                if (ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                    presentation_.unitPendingAnimations.erase(simHandle);
                    return;
                }
            }
            if (!fallbackWhenNoAnimator) return;
        }
        presentation_.unitPendingAnimations[simHandle] = std::move(request);
    };

    jassData->unitSetAnimation = [submitOrApplyAnim](int simHandle, const std::string& animName) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceName = animName;
        request.speedRatio = 1.0f;
        submitOrApplyAnim(simHandle, std::move(request));
    };
    jassData->unitQueueAnimation = [queueOrApplyAnim](int simHandle, const std::string& animName) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceName = animName;
        request.speedRatio = 1.0f;
        queueOrApplyAnim(simHandle, std::move(request), false);
    };
    jassData->unitSetAnimationByIndex = [submitOrApplyAnim](int simHandle, int index) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceIndex = index;
        request.useSequenceIndex = true;
        request.speedRatio = 1.0f;
        submitOrApplyAnim(simHandle, std::move(request));
    };
    jassData->unitSetAnimationByIndexEx = [submitOrApplyAnim](int simHandle, int index, float speedRatio) {
        animation_utils::PendingUnitAnimation request;
        request.sequenceIndex = index;
        request.useSequenceIndex = true;
        request.speedRatio = speedRatio;
        submitOrApplyAnim(simHandle, std::move(request));
    };
    jassData->unitSetAnimationByTags = [submitOrApplyAnim](int simHandle,
                                                           const std::string& primaryTag,
                                                           const std::vector<std::string>& secondaryTags,
                                                           float speedRatio,
                                                           float durationSeconds,
                                                           bool force) {
        animation_utils::PendingUnitAnimation request;
        request.primaryTag = primaryTag;
        request.secondaryTags = secondaryTags;
        request.useTags = true;
        request.force = force;
        request.speedRatio = speedRatio;
        request.durationSeconds = durationSeconds;
        submitOrApplyAnim(simHandle, std::move(request), true);
    };
    jassData->unitQueueAnimationByTags = [queueOrApplyAnim](int simHandle,
                                                            const std::string& primaryTag,
                                                            const std::vector<std::string>& secondaryTags) {
        animation_utils::PendingUnitAnimation request;
        request.primaryTag = primaryTag;
        request.secondaryTags = secondaryTags;
        request.useTags = true;
        request.speedRatio = 1.0f;
        queueOrApplyAnim(simHandle, std::move(request), true);
    };
    jassData->unitAddAnimationProperties = [this](int simHandle, const std::string& properties, bool add) {
        presentation_.ModifyUnitAnimationPropertyTags(simHandle, properties, add);
    };
    jassData->unitSetAnimationSecondaryTag = [this](int simHandle, const std::string& tag, bool add) -> bool {
        return presentation_.SetUnitSecondaryAnimationTag(simHandle, tag, add);
    };
    jassData->unitForceResetAnimation = [this](int simHandle) {
        presentation_.unitPendingAnimations.erase(simHandle);
        auto it = presentation_.unitToVisualIdx.find(simHandle);
        if (it != presentation_.unitToVisualIdx.end()) {
            auto* viewer = scene_.GetViewer(it->second);
            if (viewer && ApplyNextQueuedUnitAnimation(simHandle, viewer, this)) {
                presentation_.unitVisualAnimationModes.erase(simHandle);
                return;
            }
            const auto scriptIt = presentation_.unitScriptedAnimations.find(simHandle);
            if (viewer && scriptIt != presentation_.unitScriptedAnimations.end()) {
                auto request = scriptIt->second.request;
                request.force = true;
                presentation_.unitScriptedAnimations.erase(scriptIt);
                if (ApplyAnimationRequestToViewer(simHandle, viewer, request, this)) {
                    presentation_.unitVisualAnimationModes.erase(simHandle);
                    return;
                }
            }
        } else {
            auto queuedIt = presentation_.unitQueuedAnimations.find(simHandle);
            if (queuedIt != presentation_.unitQueuedAnimations.end() && !queuedIt->second.empty()) {
                presentation_.unitPendingAnimations[simHandle] = queuedIt->second.front();
                queuedIt->second.pop_front();
                if (queuedIt->second.empty()) {
                    presentation_.unitQueuedAnimations.erase(queuedIt);
                }
                return;
            }
        }
        presentation_.unitScriptedAnimations.erase(simHandle);
        presentation_.unitVisualAnimationModes.erase(simHandle);
    };
    jassData->unitClearAnimations = [this](int simHandle) {
        presentation_.ClearUnitAnimations(simHandle);
    };

    // Bind live GameUI to JassNatives so Blz*Frame natives drive real UIFrame nodes
    jassNatives_->SetBoundGameUI(&ui_.GetGameUI());

    // Wire all JASS simulation callbacks via unified WireSimulationEvents
    jassNatives_->WireSimulationEvents(simulation_);

    // Bind UnitPresentationSystem to SimEventBus for reactive entity removal cleanup
    presentation_.BindSimulationEvents(simulation_);

    // Subscribe presentation & audio listeners via multicast SimEventBus (Godot Signal style)
    simulation_.Events().onUnitTrained.Connect(
        [this](int /*buildingHandle*/, int trainedHandle) {
            PlayUnitResponse(simulation_.GetUnit(trainedHandle), "Ready");
        }
    );
}

void GameController::SyncFogOfWar() {
    if (!simulationInitialized_ || !terrainLoaded_) return;
    if (std::getenv("WARSMASH_NO_FOG")) return; // diagnostic: leave fog texture cleared (all visible)
    const uint64_t version = simulation_.GetFogSystem()->Version();
    if (version == fogVersionSynced_) return;
    fogVersionSynced_ = version;
    const int localPlayer = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    // Fog must rasterize over the instance-grid extents (ref ground shader
    // v_suv), not the camera bounds: otherwise visible blobs shift away from
    // their units and spawn stays black.
    float bounds[4] = {-4096, -4096, 4096, 4096};
    texturedTerrain_.GetFogMapBounds(bounds);
    constexpr int kFogGrid = 128;
    fogGrid_.assign(kFogGrid * kFogGrid, 0);
    simulation_.GetFogSystem()->Rasterize(localPlayer, kFogGrid, kFogGrid,
                                          bounds[0], bounds[1], bounds[2], bounds[3],
                                          fogGrid_.data());
    texturedTerrain_.SetFogOfWarData(fogGrid_.data(), kFogGrid, kFogGrid);
}

void GameController::SyncMinimapLayer() {
    if (!simulationInitialized_ || !minimap_.IsValid()) return;
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    const CFogSystem* fog = simulation_.GetFogSystem();

    // --- Fog overlay (ref MeleeUIMinimap fog pass) ---
    // Rasterize over the minimap world bounds at modest resolution; row 0 =
    // worldMinY matches Minimap::Generate() pixel order so the overlay aligns
    // with the base image and WorldToScreen dots.
    {
        const uint64_t version = fog ? fog->Version() : 0;
        float minX = 0, minY = 0, maxX = 1, maxY = 1;
        minimap_.GetWorldBounds(minX, minY, maxX, maxY);
        constexpr int kFogW = 96;
        constexpr int kFogH = 96;
        if (version != minimapFogVersionSynced_ || minimapFogRgba_.empty()) {
            minimapFogVersionSynced_ = version;
            std::vector<unsigned char> grid((size_t)kFogW * kFogH, 0);
            if (fog) {
                fog->Rasterize(local, kFogW, kFogH, minX, minY, maxX, maxY, grid.data());
            }
            minimapFogRgba_.assign((size_t)kFogW * kFogH * 4, 0);
            for (int i = 0; i < kFogW * kFogH; ++i) {
                const unsigned char v = grid[(size_t)i];
                uint8_t alpha = 0;
                if (v >= 200) alpha = 255;       // MASKED: opaque black
                else if (v >= 64) alpha = 128;    // FOGGED: 50% black
                minimapFogRgba_[(size_t)i * 4 + 0] = 0;
                minimapFogRgba_[(size_t)i * 4 + 1] = 0;
                minimapFogRgba_[(size_t)i * 4 + 2] = 0;
                minimapFogRgba_[(size_t)i * 4 + 3] = alpha;
            }
            minimap_.SetFogOverlay(kFogW, kFogH, minimapFogRgba_);
        }
    }

    // --- Unit dots (ref MeleeUIMinimap unit pass) ---
    static const uint32_t kMinimapPlayerColors[12] = {
        0xFFFF0000u, 0xFF0042FFu, 0xFF1CE6B6u, 0xFF540081u,
        0xFFFFFC00u, 0xFFFE8A0Eu, 0xFF20C000u, 0xFFE55BB0u,
        0xFF959697u, 0xFF7EBFF1u, 0xFF106246u, 0xFF4F2B05u,
    };
    auto argbToRgba = [](uint32_t argb) -> uint32_t {
        return ((argb & 0x00FFFFFFu) << 8) | ((argb >> 24) & 0xFFu);
    };
    // Hero pulse 0.50..0.95 triangle wave over ~80 frames (ref heroAlpha).
    const int kPulsePeriod = 80;
    const int pulseT = frameCount_ % (2 * kPulsePeriod);
    const float pulseTri = pulseT < kPulsePeriod
        ? (float)pulseT / (float)kPulsePeriod
        : (float)(2 * kPulsePeriod - pulseT) / (float)kPulsePeriod;
    const float heroAlpha = 0.50f + 0.45f * pulseTri;
    const uint8_t heroAlphaByte = (uint8_t)std::clamp((int)(heroAlpha * 255.0f), 0, 255);

    const CPlayer* localPlayer = simulation_.GetPlayer(local);
    minimapDotsScratch_.clear();
    minimapDotsScratch_.reserve(simulation_.GetUnits().size());
    for (const CUnit* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead() || unit->IsHidden() || unit->IsLoadedAboard()) continue;
        if (!IsUnitVisibleInFog(unit)) continue;
        const float ux = unit->GetX();
        const float uy = unit->GetY();
        const int owner = unit->GetPlayerIndex();
        const CUnitType* type = unit->GetUnitType();

        const bool hasOverlayMine = unit->GetAbility(War3ID("Agl2")) != nullptr
            || unit->GetAbility(War3ID("Aegm")) != nullptr
            || unit->GetAbility(War3ID("Abgm")) != nullptr;
        const bool hasBaseMine = !hasOverlayMine
            && unit->GetAbility(War3ID("Agld")) != nullptr;
        const bool isNeutralIcon = type && type->IsNeutralBuildingShowMinimapIcon();
        Minimap::UnitDot dot;
        dot.worldX = ux;
        dot.worldY = uy;
        if (hasBaseMine) {
            dot.rgba = 0xFFCC00FFu;      // gold mine (yellow, opaque)
            dot.sizePixels = 11.0f;
        } else if (hasOverlayMine) {
            const bool entangled = unit->GetAbility(War3ID("Aegm")) != nullptr
                || unit->GetAbility(War3ID("Aenc")) != nullptr;
            const bool haunted = unit->GetAbility(War3ID("Abgm")) != nullptr;
            if (entangled) dot.rgba = 0x00E676FFu;       // entangled (green)
            else if (haunted) dot.rgba = 0xE040FBFFu;    // haunted (purple)
            else dot.rgba = 0xFFCC00FFu;
            dot.sizePixels = 11.0f;
        } else if (isNeutralIcon) {
            dot.rgba = 0xFFF59DFFu;      // neutral special (pale yellow)
            dot.sizePixels = 11.0f;
        } else if (unit->IsHero()) {
            bool allied = false;
            if (localPlayer && owner >= 0) {
                if (const CPlayer* other = simulation_.GetPlayer(owner)) {
                    allied = localPlayer->HasAlliance(*other, CAllianceType::PASSIVE);
                } else {
                    allied = (owner == local);
                }
            } else {
                allied = (owner == local);
            }
            if (allied) {
                dot.rgba = (0xFFFFFF00u | heroAlphaByte);  // white pulse
            } else {
                dot.rgba = (0xFF333300u | heroAlphaByte);  // red pulse
            }
            dot.sizePixels = 14.0f;
        } else {
            uint32_t argb = 0xFF888888u;
            if (owner >= 0 && owner < 12) argb = kMinimapPlayerColors[owner];
            dot.rgba = argbToRgba(argb);
            dot.sizePixels = (type && type->IsBuilding()) ? 7.0f : 4.0f;
        }
        minimapDotsScratch_.push_back(dot);
    }
    minimap_.SetUnitDots(minimapDotsScratch_);

    // Script ping markers (ref MeleeUI minimap pings): live JASS pings draw
    // on top; aging/expiry happens in JassNatives::UpdateTimers.
    minimap_.ClearScriptMarkers();
    if (jassNatives_ && jassNatives_->GetData()) {
        for (const auto& ping : jassNatives_->GetData()->minimapPings) {
            const auto clampByte = [](int v) -> uint32_t {
                return (uint32_t)std::clamp(v, 0, 255);
            };
            const uint32_t rgba = (clampByte(ping.red) << 24) | (clampByte(ping.green) << 16) |
                                  (clampByte(ping.blue) << 8) | 0xFFu;
            minimap_.AddScriptMarker(ping.x, ping.y, rgba, ping.extra ? 14.0f : 10.0f);
        }
    }
}

void GameController::SyncHudLiveData() {
    if (!simulationInitialized_) return;
    WARSMASH_PROFILE_SCOPE(profiler_, "SyncHudLiveData", "UI");
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    // Phase D1: Reactive UIDataBinder dirty-check before pushing HUD resource/selection updates
    const bool hudDirty = ui_.GetDataBinder().SyncAndRelayoutIfChanged(
        simulation_, local, selectedUnitHandle_, ui_.GetGameUI());
    if (const CPlayer* player = simulation_.GetPlayer(local)) {
        const float hod = simulation_.GetGameTimeOfDay();
        const int totalMin = (int)(hod * 60.0f);
        if (hudDirty || (frameCount_ % 15 == 0)) {
            char clock[16] = {};
            std::snprintf(clock, sizeof(clock), "%02d:%02d", (totalMin / 60) % 24, totalMin % 60);
            ui_.SetLiveResources(player->GetGold(), player->GetLumber(), player->GetFoodUsed(),
                                 player->GetFoodCap(), clock, player->GetUpkeepLevel());
        }
    }
    // Timer dialog: first displayed JASS dialog wins (ref CTimerDialog).
    std::string title, value;
    bool show = false;
    if (jassNatives_ && jassNatives_->GetData()) {
        auto data = jassNatives_->GetData();
        for (const auto& [id, dialog] : data->timerDialogs) {
            (void)id;
            if (!dialog || !dialog->displayed) continue;
            float remain = 0.0f;
            auto tit = data->timers.find(dialog->timerSimId);
            if (tit != data->timers.end() && tit->second) remain = std::max(0.0f, tit->second->remaining);
            const int totalSec = (int)remain;
            char buf[16] = {};
            std::snprintf(buf, sizeof(buf), "%d:%02d", totalSec / 60, totalSec % 60);
            title = dialog->title;
            value = buf;
            show = true;
            break;
        }
    }
    ui_.SetTimerDialog(title, value, show);
    // Cinematic presentation: letterbox follows the mode flag; the subtitle
    // panel follows the live scene while it carries speaker text or dialogue.
    bool cineMode = false;
    std::string speaker, dialogue;
    bool cineVisible = false;
    if (jassNatives_ && jassNatives_->GetData()) {
        auto data = jassNatives_->GetData();
        cineMode = data->cinematicMode;
        if (data->cinematic.playing &&
            (!data->cinematic.speakerTitle.empty() || !data->cinematic.text.empty())) {
            speaker = data->cinematic.speakerTitle;
            dialogue = data->cinematic.text;
            cineVisible = true;
        }
    }
    ui_.SetCinematicMode(cineMode);
    ui_.SetCinematicScene(speaker, dialogue, cineVisible);
    // Cinematic filter: start->end color transition over duration (ref
    // CCineFilter); texture/UV/blend variants fall back to the color tint.
    {
        uint32_t rgba = 0;
        bool showFilter = false;
        if (jassNatives_ && jassNatives_->GetData()) {
            const auto& filter = jassNatives_->GetData()->cineFilter;
            if (filter.displayed) {
                const float t = filter.duration > 0.0f
                    ? std::clamp(filter.age / filter.duration, 0.0f, 1.0f)
                    : 1.0f;
                auto lerp = [t](int a, int b) {
                    return (uint32_t)std::clamp((int)(a + (b - a) * t), 0, 255);
                };
                rgba = (lerp(filter.startColor[3], filter.endColor[3]) << 24) |
                       (lerp(filter.startColor[0], filter.endColor[0]) << 16) |
                       (lerp(filter.startColor[1], filter.endColor[1]) << 8) |
                       lerp(filter.startColor[2], filter.endColor[2]);
                showFilter = (rgba >> 24) != 0;
            }
        }
        ui_.SetCineFilter(rgba, showFilter);
    }
    // Player text messages: forward local-player messages to HUD toasts;
    // sticky messages live in the UI until cleared. A ClearTextMessages
    // since the last push also clears already-shown toasts. Remote-player
    // messages are retained for future multiplayer routing.
    // Day/night cycle follows the JASS switch (ref EnableDawnDusk).
    if (jassNatives_ && jassNatives_->GetData()) {
        auto data = jassNatives_->GetData();
        if (data->textClearSeq != lastTextClearSeq_) {
            lastTextClearSeq_ = data->textClearSeq;
            ui_.ClearGameMessages();
        }
        auto& queue = data->textMessages;
        for (const auto& msg : queue) {
            if (msg.player == local || msg.player < 0) {
                ui_.ShowGameMessage(msg.text, msg.duration > 0.0f ? msg.duration : 5.0f);
            }
        }
        queue.erase(std::remove_if(queue.begin(), queue.end(),
                                   [local](const JassNativesData::JassTextMessage& msg) {
                                       return msg.player == local || msg.player < 0;
                                   }),
                    queue.end());
        dayNight_.SetEnabled(data->dawnDuskEnabled);
    }
}

void GameController::SyncLightnings() {
    if (!simulationInitialized_ || !jassNatives_) return;
    auto* lightning = scene_.GetLightning();
    if (!lightning) return;
    // One-shot retail-table override (custom maps may ship their own
    // Splats\LightningData.slk; the map MPQ mounts on top).
    if (!lightningParamsLoaded_ && mpq_.ArchiveCount() > 0) {
        lightningParamsLoaded_ = true;
        std::vector<uint8_t> bytes;
        if (mpq_.ReadFile("Splats\\LightningData.slk", bytes) && !bytes.empty()) {
            lightning->LoadParamsFromSlk(bytes.data(), bytes.size());
        }
    }
    const auto& bolts = jassNatives_->GetData()->lightnings;
    const int localPlayer = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    std::unordered_set<int> live;
    for (const auto& [id, bolt] : bolts) {
        if (!bolt) continue;
        live.insert(id);
        // JASS (x, y, z-height) -> scene Y-up (x, z, y).
        lightning->SetBolt(id, bolt->x1, bolt->z1, bolt->y1,
                           bolt->x2, bolt->z2, bolt->y2,
                           bolt->r, bolt->g, bolt->b, bolt->a, bolt->code);
        bool show = true;
        if (bolt->checkVisibility) {
            show = simulation_.GetFogSystem()->IsVisibleTo(
                localPlayer, (bolt->x1 + bolt->x2) * 0.5f, (bolt->y1 + bolt->y2) * 0.5f);
        }
        lightning->SetBoltVisible(id, show);
    }
    for (auto it = lightningSynced_.begin(); it != lightningSynced_.end();) {
        if (!live.count(*it)) {
            lightning->RemoveBolt(*it);
            it = lightningSynced_.erase(it);
        } else {
            ++it;
        }
    }
    lightningSynced_ = std::move(live);
}

void GameController::SyncScriptDecor() {
    if (!jassNatives_ || !jassNatives_->GetData()) return;
    auto data = jassNatives_->GetData();

    static std::unordered_set<int> splatIds;
    static std::unordered_set<int> trackableIds;
    static size_t doodadCursor = 0;

    if (texturedTerrain_.IsValid()) {
        for (const auto& [id, splat] : data->ubersplats) {
            if (!splat || splatIds.count(id)) continue;
            if (!splat->render || splat->forcePaused) continue;
            texturedTerrain_.AddUberSplat(splat->name, splat->x, splat->y, 1.0f, false);
            splatIds.insert(id);
        }
    }

    if (auto* lightning = scene_.GetLightning()) {
        static std::vector<int> billboardIds;
        for (int id : billboardIds) lightning->RemoveBillboard(id);
        billboardIds.clear();
        for (const auto& [id, image] : data->images) {
            if (!image) continue;
            float ground = 0.0f;
            float water = 0.0f;
            if (texturedTerrain_.IsValid()) {
                ground = texturedTerrain_.GetGroundHeight(image->posX, image->posY);
                water = texturedTerrain_.GetWaterHeight(image->posX, image->posY);
            }
            const ImageBillboardPose pose = MakeImageBillboard(
                image->posX, image->posY, image->posZ,
                image->sizeX, image->sizeY,
                image->visible, image->render,
                image->constantHeight, image->aboveWater,
                ground, water,
                image->red, image->green, image->blue, image->alpha);
            if (!pose.visible) continue;
            const int bid = 0x72000000 + id;
            lightning->SetBillboard(bid, pose.x, pose.y, pose.z,
                                    pose.halfW, pose.halfH,
                                    pose.r, pose.g, pose.b, pose.a);
            billboardIds.push_back(bid);
        }
    }

    while (doodadCursor < data->doodadAnims.size()) {
        const auto& req = data->doodadAnims[doodadCursor++];
        scene_.PlayDoodadAnimation(req.x, req.y, req.radius, req.doodadId,
                                   req.animName, req.nearestOnly);
    }

    for (const auto& [id, track] : data->trackables) {
        if (!track || trackableIds.count(id) || track->modelPath.empty()) continue;
        float height = texturedTerrain_.IsValid()
            ? texturedTerrain_.GetGroundHeight(track->x, track->y) : 0.0f;
        float pos[3] = {track->x, height, track->y};
        if (scene_.AddTransientModelAtScenePosition(track->modelPath, pos, track->facing, 1.0f) >= 0) {
            trackableIds.insert(id);
        }
    }

    minimap_.ClearScriptMarkers();
    for (const auto& [simId, indicator] : data->unitIndicators) {
        CUnit* unit = simulation_.GetUnit(simId);
        if (!unit || unit->IsDead()) continue;
        const uint32_t rgba = (uint32_t(indicator.alpha) << 24)
            | (uint32_t(indicator.red) << 16)
            | (uint32_t(indicator.green) << 8)
            | uint32_t(indicator.blue);
        minimap_.AddScriptMarker(unit->GetX(), unit->GetY(), rgba, 8.0f);
    }

    struct PlayedDestructableAnim {
        int generation = -1;
        std::string name;
    };
    static std::unordered_map<int, PlayedDestructableAnim> playedAnims;
    for (auto* destructable : simulation_.GetDestructables()) {
        if (!destructable || destructable->IsHidden()) continue;
        const float x = destructable->GetX();
        const float y = destructable->GetY();
        scene_.SetPlaybackSpeedNear(x, y, 96.0f, destructable->GetAnimationSpeed());
        PlayedDestructableAnim& slot = playedAnims[destructable->GetHandleId()];
        const std::string& anim = destructable->GetAnimation();
        if (slot.generation != destructable->GetAnimGeneration() || slot.name != anim) {
            const bool loop = destructable->GetQueuedAnimation().empty();
            if (!anim.empty() &&
                scene_.PlayAnimationNear(x, y, 96.0f, anim, loop, false) > 0) {
                slot.generation = destructable->GetAnimGeneration();
                slot.name = anim;
            }
        } else if (!destructable->GetQueuedAnimation().empty()) {
            const std::string next = destructable->GetQueuedAnimation();
            if (scene_.PlayAnimationNear(x, y, 96.0f, next, true, true) > 0) {
                destructable->PromoteQueuedAnimation();
                slot.name = destructable->GetAnimation();
            }
        }
    }
}

void GameController::SyncWeather(float dt) {
    auto* lightning = scene_.GetLightning();
    if (!lightning) return;
    static std::vector<int> boltIds;
    for (int id : boltIds) lightning->RemoveBolt(id);
    boltIds.clear();
    if (!jassNatives_ || !jassNatives_->GetData()) return;
    struct Drop { float x = 0, y = 0, z = 0; bool live = false; };
    static std::vector<Drop> drops;
    int slot = 0;
    constexpr int kBase = 0x71000000;
    for (const auto& entry : jassNatives_->GetData()->weathers) {
        const auto& weather = entry.second;
        if (!weather || !weather->enabled) continue;
        const float width = std::max(32.0f, weather->maxX - weather->minX);
        const float height = std::max(32.0f, weather->maxY - weather->minY);
        const int count = std::min(40, std::max(8, (int)((width * height) / 50000.0f)));
        if ((int)drops.size() < slot + count) drops.resize(slot + count);
        const std::string code = weather->code.ToString();
        const bool snow = code.find('S') != std::string::npos || code.find('s') != std::string::npos;
        const float speed = snow ? 70.0f : 420.0f;
        const float length = snow ? 6.0f : 36.0f;
        for (int i = 0; i < count; i++) {
            Drop& drop = drops[slot + i];
            if (!drop.live) {
                const float u = std::fmod(i * 0.6180339f, 1.0f);
                drop.x = weather->minX + u * width;
                drop.y = weather->minY + std::fmod(u * 1.7f, 1.0f) * height;
                drop.z = 40.0f + std::fmod(u * 240.0f, 240.0f);
                drop.live = true;
            }
            drop.z -= speed * dt;
            if (drop.z < 0.0f) drop.z = 260.0f;
            if (drop.x < weather->minX || drop.x > weather->maxX ||
                drop.y < weather->minY || drop.y > weather->maxY) {
                drop.live = false;
                continue;
            }
            const int id = kBase + slot + i;
            lightning->SetBolt(id, drop.x, drop.z, drop.y,
                               drop.x, std::max(0.0f, drop.z - length), drop.y,
                               snow ? 0.9f : 0.65f, snow ? 0.9f : 0.75f, 0.95f, 0.45f, "CLPB");
            boltIds.push_back(id);
        }
        slot += count;
    }
}

void GameController::SyncJassEffectVisuals() {
    if (!jassNatives_ || !jassNatives_->GetData()) return;
    auto data = jassNatives_->GetData();

    auto groundHeight = [&](float x, float y) {
        return texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(x, y) : 0.0f;
    };

    for (auto& [effectId, e] : data->effects) {
        if (!e || e->modelPath.empty()) continue;
        auto known = presentation_.effectVisuals.find(effectId);

        if (e->targetSimHandle < 0) {
            // Point effect: one-shot transient on first sight.
            if (known == presentation_.effectVisuals.end()) {
                float pos[3] = {e->x, groundHeight(e->x, e->y), e->y};
                scene_.AddTransientModelAtScenePosition(e->modelPath, pos, 0.0f, 1.0f);
                presentation_.effectVisuals[effectId] = -1;
            }
            continue;
        }

        // Attached effect: persistent instance, repositioned every frame.
        int idx = (known == presentation_.effectVisuals.end()) ? -1 : known->second;
        if (idx < 0) {
            int player = 0;
            if (CWidget* w = simulation_.GetWidget(e->targetSimHandle)) {
                if (auto* u = ((w) ? (w)->AsUnit() : nullptr)) player = u->GetPlayerIndex();
            }
            idx = scene_.AddAttachedModel(e->modelPath, player);
            if (idx < 0) continue; // model missing; retry next frame
            presentation_.effectVisuals[effectId] = idx;
        }

        // Compose unit-instance transform * attach-node matrix (mirrors the
        // SceneManager instance convention: Y-rotation + translation).
        Mat4 finalMat = Mat4::Identity();
        bool placed = false;
        if (CWidget* w = simulation_.GetWidget(e->targetSimHandle)) {
            float wx = w->GetX(), wy = w->GetY();
            float h = groundHeight(wx, wy);
            float angle = 0.0f;
            MdxViewer* viewer = nullptr;
            int viewerIdx = -1;
            if (auto* u = ((w) ? (w)->AsUnit() : nullptr)) {
                angle = u->GetFacing();
                h = unit_selection_geom::ComputeUnitVisualHeight(
                    *u, texturedTerrain_.IsValid(),
                    [&](float x, float z) { return texturedTerrain_.GetGroundHeight(x, z); });
                auto vit = presentation_.unitToVisualIdx.find(u->GetHandleId());
                if (vit != presentation_.unitToVisualIdx.end()) {
                    viewerIdx = vit->second;
                    viewer = scene_.GetViewer(viewerIdx);
                }
            } else {
                auto vit = presentation_.itemToVisualIdx.find(w->GetHandleId());
                if (vit != presentation_.itemToVisualIdx.end()) {
                    viewerIdx = vit->second;
                    viewer = scene_.GetViewer(viewerIdx);
                }
            }
            // Instance transform shares the scene convention, including
            // the instance scale (seed x script), so attached models grow
            // with scaled units.
            float iscale[3] = {1.0f, 1.0f, 1.0f};
            if (viewerIdx >= 0) scene_.GetInstanceScale(viewerIdx, iscale);
            Mat4 inst = MakeSceneInstanceMatrix(wx, h, wy, angle,
                                                iscale[0], iscale[1], iscale[2]);
            finalMat = inst;
            if (viewer && viewer->GetAnimator() && !e->attachPoint.empty()) {
                // Pivot-inclusive attach location in model space plus the
                // node orientation (translation stripped, so the unit
                // instance transform is applied exactly once).
                Vec3 loc{0, 0, 0};
                Mat4 node = Mat4::Identity();
                if (viewer->GetAnimator()->GetAttachmentWorldMatrixYUp(e->attachPoint, node, loc)) {
                    node.m[12] = 0.0f;
                    node.m[13] = 0.0f;
                    node.m[14] = 0.0f;
                    finalMat = inst * Mat4::Translate(loc.x, loc.y, loc.z) * node;
                }
            }
            placed = true;
        }
        if (placed) {
            if (!scene_.UpdateAttachedModel(idx, finalMat)) {
                presentation_.effectVisuals.erase(effectId);
            }
        }
    }

    // Drop visuals whose effects were destroyed (or never resolved).
    std::vector<int> gone;
    for (auto& [effectId, idx] : presentation_.effectVisuals) {
        if (data->effects.find(effectId) == data->effects.end()) {
            if (idx >= 0) scene_.RemoveAttachedModel(idx);
            gone.push_back(effectId);
        }
    }
    for (int id : gone) presentation_.effectVisuals.erase(id);
}

static bool IsAllyForFog(const CSimulation& sim, int local, int owner) {
    if (owner == local) return true;
    if (local < 0 || owner < 0) return false;
    const CPlayer* self = sim.GetPlayer(local);
    const CPlayer* other = sim.GetPlayer(owner);
    return self && other && self->HasAlliance(*other, CAllianceType::ALLIED);
}

bool GameController::IsUnitVisibleInFog(const CUnit* unit) const {
    if (!unit) return false;
    // Buildings/structures persist on the map once scouted (grey fog shows
    // bases); only the black mask hides them.
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    const CFogSystem* fog = simulation_.GetFogSystem();
    if (unit->GetUnitType() && unit->GetUnitType()->IsBuilding()) {
        return !fog->IsMaskedTo(local, unit->GetX(), unit->GetY());
    }
    // Own and allied forces are always shown (ref alliance vision).
    if (IsAllyForFog(simulation_, local, unit->GetPlayerIndex())) return true;
    // Enemies (and neutrals/creeps) need live sight ...
    if (!fog->IsVisibleTo(local, unit->GetX(), unit->GetY())) return false;
    // ... and stealth needs a local detector (or Faerie Fire) on top.
    if (!unit->IsStealthed()) return true;
    for (auto* viewer : simulation_.GetUnits()) {
        if (!viewer || viewer->IsDead()) continue;
        const int vp = viewer->GetPlayerIndex();
        if (vp != local && !IsAllyForFog(simulation_, local, vp)) continue;
        if (!unit->IsInvisibleTo(viewer)) return true;
    }
    return false;
}

void GameController::SyncFogStaticsVisibility() {
    if (!simulationInitialized_) return;
    const uint64_t version = simulation_.GetFogSystem()->Version();
    if (version == fogStaticsVersionSynced_) return;
    fogStaticsVersionSynced_ = version;
    const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
    const CFogSystem* fog = simulation_.GetFogSystem();
    const int count = scene_.GetInstanceCount();
    for (int i = 0; i < count; ++i) {
        const PlacedModel* inst = scene_.GetInstance(i);
        if (!inst || inst->transient) continue;
        // Units and ground items are driven by sim state, not by position.
        if (inst->debugKind == "unit" || inst->debugKind == "item") continue;
        // Spell visuals punch through fog like in retail.
        if (inst->debugKind == "event-spawn" || inst->debugKind == "attached-effect") continue;
        const bool explored = !fog->IsMaskedTo(local, inst->pos[0], inst->pos[2]);
        scene_.SetInstanceVisible(i, explored);
    }
}

void GameController::SyncAndReconcileVisuals() {
    // Sync all sim units to visuals
    for (auto* unit : simulation_.GetUnits()) {
        if (!unit || unit->IsDead()) continue;
        int handleId = unit->GetHandleId();

        auto visualIt = presentation_.unitToVisualIdx.find(handleId);
        if (visualIt == presentation_.unitToVisualIdx.end()) {
            EnsureSimulationUnitVisual(unit);
            visualIt = presentation_.unitToVisualIdx.find(handleId);
        }
        if (visualIt != presentation_.unitToVisualIdx.end()) {
            if (auto* viewer = scene_.GetViewer(visualIt->second)) {
                if (jassNatives_ && jassNatives_->GetData()) {
                    auto blend = jassNatives_->GetData()->unitBlendTimes.find(handleId);
                    if (blend != jassNatives_->GetData()->unitBlendTimes.end()) {
                        viewer->SetBlendTime(blend->second);
                    }
                }
                SyncSimulationUnitAnimation(unit, viewer, this);
            }
            scene_.SetInstanceVisible(visualIt->second,
                !unit->IsHidden() && !unit->IsLoadedAboard() && IsUnitVisibleInFog(unit));
        }
    }

    for (auto* item : simulation_.GetItems()) {
        if (!item || item->IsDead()) continue;
        const int handleId = item->GetHandleId();
        auto visualIt = presentation_.itemToVisualIdx.find(handleId);
        if (visualIt == presentation_.itemToVisualIdx.end()) {
            EnsureSimulationItemVisual(item);
            visualIt = presentation_.itemToVisualIdx.find(handleId);
        }
        if (visualIt != presentation_.itemToVisualIdx.end()) {
            float height = texturedTerrain_.IsValid()
                ? texturedTerrain_.GetGroundHeight(item->GetX(), item->GetY())
                : 0.0f;
            float pos[3] = {item->GetX(), height, item->GetY()};
            scene_.UpdateInstancePosition(visualIt->second, pos, 0.0f);
            bool explored = true;
            if (simulationInitialized_) {
                const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
                explored = !simulation_.GetFogSystem()->IsMaskedTo(local, item->GetX(), item->GetY());
            }
            scene_.SetInstanceVisible(visualIt->second, !item->IsHidden() && explored);
        }
    }

    // Reconcile (remove dead, add new)
    std::vector<int> removedHandles;
    for (auto& [handleId, visualIdx] : presentation_.unitToVisualIdx) {
        CUnit* unit = simulation_.GetUnit(handleId);
        if (!unit || unit->IsDead()) {
            scene_.SetInstanceVisible(visualIdx, false);
            removedHandles.push_back(handleId);
        }
    }
    for (int handleId : removedHandles) {
        presentation_.OnUnitRemoved(handleId, &scene_);
        selectedUnitHandlesSet_.erase(handleId);
        selectedUnitHandles_.erase(
            std::remove(selectedUnitHandles_.begin(), selectedUnitHandles_.end(), handleId),
            selectedUnitHandles_.end());
        if (selectedUnitHandle_ == handleId) selectedUnitHandle_ = -1;
    }

    std::vector<int> removedItemHandles;
    for (auto& [handleId, visualIdx] : presentation_.itemToVisualIdx) {
        CItem* item = simulation_.GetItem(handleId);
        if (!item || item->IsDead()) {
            scene_.SetInstanceVisible(visualIdx, false);
            removedItemHandles.push_back(handleId);
        }
    }
    for (int handleId : removedItemHandles) {
        presentation_.OnItemRemoved(handleId, &scene_);
    }

    SyncFogStaticsVisibility();
    SyncJassEffectVisuals();
}

void GameController::CollectUnitOverlays(int screenW, int screenH) {
    static const uint32_t kPlayerColors[12] = {
        0xFFFF0000, 0xFF0042FF, 0xFF1CE6B6, 0xFF540081,
        0xFFFFFC00, 0xFFFE8A0E, 0xFF20C000, 0xFFE55BB0,
        0xFF959697, 0xFF7EBFF1, 0xFF106246, 0xFF4F2B05
    };

    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);

    std::vector<UnitOverlayInfo>& overlays = overlayCache_;
    overlays.clear();
    if (simulationInitialized_) {
        for (auto* unit : simulation_.GetUnits()) {
            if (unit->IsDead()) continue;
            // Bars/rings follow the same fog rule as the model itself.
            if (unit->IsHidden() || unit->IsLoadedAboard() || !IsUnitVisibleInFog(unit)) continue;

            float gndH = texturedTerrain_.IsValid() ? texturedTerrain_.GetGroundHeight(unit->GetX(), unit->GetY()) : 0.0f;

            UnitOverlayInfo info;
            info.handleId = unit->GetHandleId();
            info.isSelected = IsUnitSelected(unit->GetHandleId());
            info.isAlive = !unit->IsDead();
            info.hpRatio = unit->GetMaxLife() > 0 ? unit->GetLife() / unit->GetMaxLife() : 1.0f;
            info.mpRatio = unit->GetMaxMana() > 0 ? unit->GetMana() / (float)unit->GetMaxMana() : 0.0f;

            Vec3 headPos(unit->GetX(), gndH + 140.0f, unit->GetY());
            picking::WorldToScreen(headPos, view, proj, screenW, screenH, info.screenX, info.screenY);

            Vec3 footPos(unit->GetX(), gndH + 10.0f, unit->GetY());
            picking::WorldToScreen(footPos, view, proj, screenW, screenH, info.footX, info.footY);

            auto* unitType = unitData_.GetUnitType(unit->GetTypeId());
            info.name = unitType ? unitType->GetName() : "?";

            int pi = unit->GetPlayerIndex();
            if (pi >= 0 && pi < 12) info.playerColor = kPlayerColors[pi];
            else info.playerColor = 0xFF888888;

            overlays.push_back(info);
        }
    }
    ui_.SetOverlayData(overlays);
}

void GameController::CollectTextTagOverlays(int screenW, int screenH) {
    std::vector<TextTagOverlayInfo>& tags = textTagCache_;
    tags.clear();
    if (!jassNatives_ || !jassNatives_->GetData()) {
        ui_.SetTextTagData(tags);
        return;
    }
    float aspect = (screenH > 0) ? (float)screenW / screenH : 1.0f;
    Mat4 view = camera_.GetViewMatrix();
    Mat4 proj = camera_.GetProjectionMatrix(aspect);
    for (const auto& [id, tag] : jassNatives_->GetData()->textTags) {
        (void)id;
        if (!tag || !tag->visible || tag->text.empty()) continue;
        const float gndH = texturedTerrain_.IsValid()
            ? texturedTerrain_.GetGroundHeight(tag->x, tag->y) : 0.0f;
        Vec3 worldPos(tag->x, gndH + tag->z + 10.0f, tag->y);
        float sx = 0, sy = 0;
        if (!picking::WorldToScreen(worldPos, view, proj, screenW, screenH, sx, sy)) continue;
        // Fade out past the fadepoint (ref TextTag.update alpha ramp).
        uint32_t color = (uint32_t)tag->color;
        if (!tag->permanent && tag->lifespan > 0 && tag->fadepoint >= 0) {
            const float remaining = tag->lifespan - tag->age;
            const float fadeSpan = tag->lifespan - tag->fadepoint;
            if (fadeSpan > 0 && remaining < fadeSpan) {
                const float t = std::clamp(remaining / fadeSpan, 0.0f, 1.0f);
                const uint32_t a = (uint32_t)(((color >> 24) & 0xFF) * t);
                color = (color & 0x00FFFFFF) | (a << 24);
            }
        }
        TextTagOverlayInfo info;
        info.screenX = sx;
        info.screenY = sy;
        info.text = tag->text;
        info.color = color;
        info.fontSize = tag->fontSize > 0 ? tag->fontSize : 12.0f;
        info.centered = tag->centered;
        tags.push_back(std::move(info));
    }
    ui_.SetTextTagData(tags);
}

void GameController::SyncLeaderboard() {
    HudLeaderboard board;
    if (jassNatives_ && jassNatives_->GetData()) {
        const auto& data = *jassNatives_->GetData();
        // Prefer the local player's board (ref PlayerSetLeaderboard),
        // fall back to the first displayed board.
        const JassNativesData::JassLeaderboard* picked = nullptr;
        auto pit = data.playerLeaderboards.find(data.localPlayerIndex);
        if (pit != data.playerLeaderboards.end()) {
            auto bit = data.leaderboards.find(pit->second);
            if (bit != data.leaderboards.end() && bit->second && bit->second->displayed) {
                picked = bit->second.get();
            }
        }
        if (!picked) {
            for (const auto& [id, b] : data.leaderboards) {
                (void)id;
                if (b && b->displayed) {
                    picked = b.get();
                    break;
                }
            }
        }
        if (picked && picked->displayed) {
            board.visible = true;
            board.label = picked->label;
            for (const auto& item : picked->items) {
                HudLeaderboardItem row;
                row.label = picked->showLabel && item.showLabel ? item.label : std::string();
                row.value = item.value;
                row.showLabel = picked->showLabel && item.showLabel;
                row.showValue = picked->showValues && item.showValue;
                board.items.push_back(std::move(row));
            }
        }
    }
    ui_.SetLeaderboard(board);
}

void GameController::SyncMultiboard() {
    HudMultiboard board;
    if (jassNatives_ && jassNatives_->GetData()) {
        const auto& data = *jassNatives_->GetData();
        const JassNativesData::JassMultiboard* picked = nullptr;
        for (const auto& [id, b] : data.multiboards) {
            (void)id;
            if (b && b->displayed && !b->suppressDisplay) {
                picked = b.get();
                break;
            }
        }
        if (picked) {
            board.visible = true;
            board.minimized = picked->minimized;
            board.title = picked->title;
            board.rows = picked->rows;
            board.cols = picked->cols;
            board.cells.assign((size_t)std::max(0, board.rows) *
                               (size_t)std::max(0, board.cols),
                               HudMultiboardCell{});
            for (const auto& [pos, itemSimId] : picked->items) {
                if (pos.first < 0 || pos.first >= board.rows ||
                    pos.second < 0 || pos.second >= board.cols) {
                    continue;
                }
                auto iit = data.multiboardItems.find(itemSimId);
                if (iit == data.multiboardItems.end() || !iit->second) continue;
                const auto& item = *iit->second;
                if (!item.showValue || item.value.empty()) continue;
                HudMultiboardCell& cell =
                    board.cells[(size_t)pos.first * (size_t)board.cols + (size_t)pos.second];
                cell.value = item.value;
                if (item.showIcon && !item.icon.empty()) {
                    cell.icon = item.icon;
                    cell.showIcon = true;
                }
            }
        }
    }
    ui_.SetMultiboard(board);
}

// --- Render ---


void GameController::UpdateMidiAmbient(float dt) {
    if (!audio_.IsValid() || !simulationInitialized_) return;
    ambientCheckAcc_ += dt;
    if (ambientCheckAcc_ < 3.0f) return; // re-evaluate a few times a second
    ambientCheckAcc_ = 0.0f;

    // JASS SetSoundEnvironment wins; otherwise the map's w3i ambience.
    std::string env;
    if (jassNatives_ && jassNatives_->GetData() &&
        !jassNatives_->GetData()->music.soundEnvironment.empty()) {
        env = jassNatives_->GetData()->music.soundEnvironment;
    } else if (!w3i_.soundEnvironment.empty()) {
        env = w3i_.soundEnvironment;
    }
    engine::ISimulationModule::TilesetEnvironmentProfile tsAmbientProf;
    bool hasTilesetAmbient = false;
    if (const auto* simMod = simulation_.GetSimulationModule(); simMod != nullptr) {
        const char tsCode = map_.tileset ? map_.tileset : w3i_.tileset;
        if (tsCode != 0) {
            hasTilesetAmbient = simMod->OnResolveTilesetEnvironmentProfile(tsCode, tsAmbientProf);
        }
    }
    if (env.empty() && hasTilesetAmbient) {
        const std::string& fallbackMidi =
            simulation_.IsDaytime() ? tsAmbientProf.dayAmbientMidiPath : tsAmbientProf.nightAmbientMidiPath;
        if (!fallbackMidi.empty() && mpq_.FileExists(fallbackMidi)) {
            if (!audio_.IsMidiAmbientPlaying(fallbackMidi)) {
                audio_.PlayMidiAmbient(fallbackMidi, &mpq_, 0.45f);
            }
            return;
        }
    }
    if (env.empty()) {
        audio_.StopMidiAmbient();
        return;
    }
    // Retail layout Sound\Ambient\<env>\<file>.mid; file casing varies
    // (Wetlandsday.mid vs WetlandsNight.mid), so match case-insensitively.
    // ListAllFiles returns full archive paths for the pattern.
    const bool day = simulation_.IsDaytime();
    std::string dir = "Sound\\Ambient\\" + env + "\\";
    std::string want = day ? "day" : "night";
    std::string chosen;
    for (const std::string& name : mpq_.ListAllFiles(dir + "*.mid")) {
        std::string lower = name;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".mid") == 0 &&
            lower.find(want) != std::string::npos) {
            chosen = name;
            break;
        }
    }
    if (chosen.empty()) {
        audio_.StopMidiAmbient();
        return;
    }
    if (!audio_.IsMidiAmbientPlaying(chosen)) {
        audio_.PlayMidiAmbient(chosen, &mpq_, 0.45f);
    }
}

void GameController::SyncMusic() {
    // In-game map music only; the menu jingle is managed directly and has
    // no sim or JASS state.
    if (!audio_.IsValid() || !simulationInitialized_ || !jassNatives_) return;
    auto data = jassNatives_->GetData();
    if (!data) return;
    const bool wantPlaying = data->music.playing && !data->music.current.empty();
    const float wantVolume =
        std::max(0.0f, std::min(1.0f, (float)data->music.volume / 127.0f));
    if (wantPlaying &&
        (!musicSyncedPlaying_ || musicSyncedTrack_ != data->music.current)) {
        if (audio_.PlayMusic(data->music.current, &mpq_, wantVolume)) {
            musicSyncedTrack_ = data->music.current;
            musicSyncedPlaying_ = true;
            musicSyncedVolume_ = wantVolume;
        } else {
            // Missing file: latch as synced-stopped so a broken map does
            // not respam every frame; any state change retries.
            musicSyncedTrack_ = data->music.current;
            musicSyncedPlaying_ = false;
        }
        return;
    }
    if (!wantPlaying && musicSyncedPlaying_) {
        audio_.StopMusic();
        musicSyncedPlaying_ = false;
        return;
    }
    if (wantPlaying && musicSyncedPlaying_ && wantVolume != musicSyncedVolume_) {
        audio_.SetMusicVolume(wantVolume);
        musicSyncedVolume_ = wantVolume;
    }
}

int GameController::PlayUiSound(const std::string& key, float volume) {
    if (!audio_.IsValid() || frameCount_ <= 10) return -1;
    const int id = uiSounds_.Play(&audio_, &mpq_, key, volume);
    if (id >= 0) return id;
    // Fallback to the preloaded generic click/error samples.
    if (key == "Error") return audio_.Play2D("ui_error", volume);
    return audio_.Play2D("ui_click", volume);
}

void GameController::PlayUnitResponse(CUnit* unit, const char* kind) {
    if (!unit || !kind || !audio_.IsValid() || frameCount_ <= 10) return;
    if (!simulationInitialized_) return;
    ackBank_.PlayResponse(simulation_, unit, kind);
}

void GameController::PlaySelectResponse(CUnit* unit) {
    if (!unit) return;
    const int handle = unit->GetHandleId();
    if (handle == lastResponseHandle_) {
        responseCount_++;
    } else {
        lastResponseHandle_ = handle;
        responseCount_ = 1;
    }
    // Every 4th consecutive selection annoys the unit (retail Pissed
    // rotation); the counter keeps climbing so repeats stay irritated.
    const bool annoyed = responseCount_ >= 4 && (responseCount_ % 3 == 1);
    PlayUnitResponse(unit, annoyed ? "Pissed" : "What");
}


void GameController::RefreshRuntimeUnitSplats() {
    splatMeta_.RefreshRuntimeSplats(texturedTerrain_, simulationInitialized_, simulation_, selectedUnitHandlesSet_, hoveredUnitHandle_);
    AddActiveEventSplatsToTerrain();
}

void GameController::AddActiveEventSplatsToTerrain() {
    if (!texturedTerrain_.IsValid()) {
        return;
    }
    for (const ActiveEventSplat& splat : activeEventSplats_) {
        const Vec4 tint = MdxEventSplatTint(splat.colors, splat.intervalTimes, splat.intervalCount,
                                            splat.lifeSpan, splat.lifeRemaining);
        const Vec4 uvRect = MdxEventSplatUvRect(splat.uvIntervals, splat.intervalTimes, splat.intervalCount,
                                                splat.columns, splat.rows, splat.lifeSpan, splat.lifeRemaining);
        const SplatModel::BlendMode blendMode = MdxEventSplatBlendMode(splat.blendMode);
        if (!splat.meshVertices.empty() && !splat.meshIndices.empty()) {
            std::vector<SplatModel::MeshVertex> vertices = splat.meshVertices;
            for (auto& vertex : vertices) {
                vertex.u = uvRect.x + vertex.u * (uvRect.z - uvRect.x);
                vertex.v = uvRect.y + vertex.v * (uvRect.w - uvRect.y);
                vertex.tint = tint;
            }
            texturedTerrain_.AddRuntimeSplatMesh(
                splat.key,
                splat.texturePath,
                vertices,
                splat.meshIndices,
                splat.highPriority,
                false,
                blendMode,
                splat.unshaded);
        } else {
            texturedTerrain_.AddRuntimeSplatRect(
                splat.key,
                splat.texturePath,
                splat.x - splat.halfSize,
                splat.y - splat.halfSize,
                splat.x + splat.halfSize,
                splat.y + splat.halfSize,
                8.0f,
                splat.highPriority,
                false,
                false,
                0.0f,
                tint,
                uvRect,
                blendMode,
                splat.unshaded);
        }
    }
}

// --- Unit visual ---

bool GameController::EnsureSimulationUnitVisual(CUnit* unit) {
    if (!unit) return false;
    int handleId = unit->GetHandleId();
    if (presentation_.unitToVisualIdx.find(handleId) != presentation_.unitToVisualIdx.end()) return true;
    if (presentation_.unitVisualRegistrationFailed.find(handleId) != presentation_.unitVisualRegistrationFailed.end()) return false;

    auto seedIt = presentation_.unitVisualSeeds.find(handleId);
    float scale[3] = {1, 1, 1};
    uint32_t skinId = 0;
    int hp = 0, flags = 0;
    if (seedIt != presentation_.unitVisualSeeds.end()) {
        scale[0] = seedIt->second.scale[0]; scale[1] = seedIt->second.scale[1]; scale[2] = seedIt->second.scale[2];
        skinId = seedIt->second.skinId; hp = seedIt->second.hitpoints; flags = seedIt->second.flags;
    }
    // Script unit scale and module visual scale rules multiply the seed (per-frame sync keeps it live).
    float modScaleMul = 1.0f;
    uint32_t modTintRgba = 0xFFFFFFFFu;
    if (const auto* simMod = simulation_.GetSimulationModule()) {
        if (!simMod->OnResolveUnitVisualScaleAndTintForUnit(*unit, modScaleMul, modTintRgba)) {
            simMod->OnResolveUnitVisualScaleAndTint(unit->GetTypeId().value, modScaleMul, modTintRgba);
        }
    }
    const float us = unit->GetScale() * modScaleMul;
    scale[0] *= us;
    scale[1] *= us;
    scale[2] *= us;

    float height = texturedTerrain_.IsValid()
        ? texturedTerrain_.GetGroundHeight(unit->GetX(), unit->GetY())
        : 0.0f;
    float pos[3] = {unit->GetX(), unit->GetY(), height};
    int visualIdx = scene_.AddUnit(unit->GetTypeId().value, pos, unit->GetFacing(),
                                   scale, unit->GetPlayerIndex(), hp, flags, skinId);
    if (visualIdx < 0) {
        presentation_.unitVisualRegistrationFailed.insert(handleId);
        return false;
    }
    presentation_.unitToVisualIdx[handleId] = visualIdx;

    // Apply initial animation
    if (auto* viewer = scene_.GetViewer(visualIdx)) {
        SyncSimulationUnitAnimation(unit, viewer, this);
    }
    scene_.SetInstanceVisible(visualIdx,
        !unit->IsHidden() && !unit->IsLoadedAboard() && IsUnitVisibleInFog(unit));
    return true;
}

bool GameController::EnsureSimulationItemVisual(CItem* item) {
    if (!item) return false;
    const int handleId = item->GetHandleId();
    if (presentation_.itemToVisualIdx.find(handleId) != presentation_.itemToVisualIdx.end()) return true;
    if (presentation_.itemVisualRegistrationFailed.find(handleId) != presentation_.itemVisualRegistrationFailed.end()) return false;
    if (item->IsDead()) return false;

    float height = texturedTerrain_.IsValid()
        ? texturedTerrain_.GetGroundHeight(item->GetX(), item->GetY())
        : 0.0f;
    float pos[3] = {item->GetX(), item->GetY(), height};
    float scale[3] = {1.0f, 1.0f, 1.0f};
    const int visualIdx = scene_.AddItem(item->GetTypeId().value, pos, 0.0f, scale, 0, 0);
    if (visualIdx < 0) {
        presentation_.itemVisualRegistrationFailed.insert(handleId);
        return false;
    }

    presentation_.itemToVisualIdx[handleId] = visualIdx;
    bool explored = true;
    if (simulationInitialized_) {
        const int local = config_.localPlayerIndex >= 0 ? config_.localPlayerIndex : 0;
        explored = !simulation_.GetFogSystem()->IsMaskedTo(local, item->GetX(), item->GetY());
    }
    scene_.SetInstanceVisible(visualIdx, !item->IsHidden() && explored);
    return true;
}


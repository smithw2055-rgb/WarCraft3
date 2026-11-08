#pragma once
#include "ecs/unit.h"
#include "ecs/csimulation.h"
#include "ecs/behaviors/behavior_attack.h"
#include "ecs/behaviors/behavior_attack_move.h"
#include "ecs/behaviors/behavior_build.h"
#include "ecs/behaviors/behavior_follow.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/behaviors/behavior_move.h"
#include "ecs/behaviors/behavior_patrol.h"
#include "ecs/behaviors/behavior_repair.h"
#include "ecs/behaviors/behavior_return_resources.h"
#include "ecs/behaviors/behavior_spell.h"
#include "render/splat_metadata.h"
#include "render/mdx_viewer.h"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace animation_utils {

struct PendingUnitAnimation {
    std::string sequenceName;
    std::string primaryTag;
    std::vector<std::string> secondaryTags;
    int sequenceIndex = -1;
    bool useSequenceIndex = false;
    bool useTags = false;
    bool force = true;
    float speedRatio = 1.0f;
    float durationSeconds = 0.0f;
};

enum class UnitVisualAnimationMode : uint8_t {
    NONE,
    READY,
    READY_GOLD,
    READY_LUMBER,
    WALK,
    WALK_GOLD,
    WALK_LUMBER,
    WALK_WORK,
    WORK,
    SPELL,
    ATTACK,
    CONSTRUCT,
    DEAD,
    CORPSE,
    BONE_CORPSE,
    DISSIPATE
};

struct ScriptedUnitAnimationState {
    PendingUnitAnimation request;
    bool loop = false;
    bool queueStandAfterCurrent = false;
};

// Tag helpers
void ClassifyScriptedAnimationName(const std::string& lowerName,
                                   bool& outLoop,
                                   bool& outQueueStandAfterCurrent);

bool GetRequestedAnimationLowerName(const MdxViewer* viewer,
                                    const PendingUnitAnimation& request,
                                    std::string& outLowerName);

std::vector<std::string> ExtractAnimationSecondaryTags(const std::string& properties);

void AppendUniqueAnimationTag(std::vector<std::string>& tags, std::string tag);

bool IsUnitWorkingFromPassiveState(const CUnit* unit);

// scriptPropertyTags: optional pointer to handleId -> tags mapping (per-unit scripted tags).
// If null, only unit-type-required + passive-state tags are considered.
std::vector<std::string> CollectUnitAnimationPropertyTags(
    const CUnit* unit,
    const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags);

std::vector<std::string> BuildContextualAnimationTags(
    const CUnit* unit,
    const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags,
    CResourceType carriedType,
    std::initializer_list<const char*> extraTags = {});

// Behavior getters
const CBehaviorHarvest* GetHarvestBehavior(const CUnit* unit);
const CBehaviorAttack* GetAttackBehavior(const CUnit* unit);
const CBehaviorMove* GetMoveBehavior(const CUnit* unit);
const CBehaviorFollow* GetFollowBehavior(const CUnit* unit);
const CBehaviorPatrol* GetPatrolBehavior(const CUnit* unit);
const CBehaviorAttackMove* GetAttackMoveBehavior(const CUnit* unit);
const CBehaviorReturnResources* GetReturnResourcesBehavior(const CUnit* unit);
const CBehaviorBuild* GetBuildBehavior(const CUnit* unit);
const CBehaviorRepair* GetRepairBehavior(const CUnit* unit);
const CBehaviorSpell* GetSpellBehavior(const CUnit* unit);

// Resolve the animation mode from live unit/behavior/carry state.
UnitVisualAnimationMode ResolveUnitVisualAnimationMode(
    const CUnit* unit,
    bool isMoving,
    CBehaviorCategory behaviorCategory,
    CResourceType carriedType,
    int gameTurnTick);

// Visual playback (use SplatMetadataStore for selection-circle/uberSplat animation speeds;
// CSimulation for decay timing; scriptPropertyTags for per-unit scripted animation tags).
void PlayReadyVisualForUnit(CUnit* unit,
                            MdxViewer* viewer,
                            CBehaviorCategory behaviorCategory,
                            CResourceType carriedType,
                            const SplatMetadataStore& splatMeta,
                            const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags);

bool PlayWalkVisualForUnit(CUnit* unit,
                           MdxViewer* viewer,
                           CBehaviorCategory behaviorCategory,
                           CResourceType carriedType,
                           const SplatMetadataStore& splatMeta,
                           const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags);

bool PlayWalkWorkVisualForUnit(CUnit* unit,
                                MdxViewer* viewer,
                                CBehaviorCategory behaviorCategory,
                                CResourceType carriedType,
                                const SplatMetadataStore& splatMeta,
                                const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags);

bool PlayWorkVisualForUnit(CUnit* unit,
                           MdxViewer* viewer,
                           CBehaviorCategory behaviorCategory,
                           CResourceType carriedType,
                           const SplatMetadataStore& splatMeta,
                           const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags);

bool PlayDeathVisualForUnit(CUnit* unit,
                            MdxViewer* viewer,
                            UnitVisualAnimationMode mode,
                            const CSimulation& simulation);

bool PlaySpellVisualForUnit(CUnit* unit,
                            MdxViewer* viewer,
                            const std::unordered_map<int, std::vector<std::string>>* scriptPropertyTags);

// Movement detection for animation based on behavior state + position delta tracking.
// `lastPositions` is in/out: may insert/refresh entries for the unit.
bool ComputeUnitVisualIsMoving(CUnit* unit,
                               std::unordered_map<int, std::pair<float, float>>& lastPositions);

} // namespace animation_utils

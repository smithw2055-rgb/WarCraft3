#include "mdx_viewer.h"
#include "shader.h"
#include "renderer.h"
#include "replaceable_ids.h"
#include "mdx_sequence_select.h"
#include "mdx_collision.h"
#include "asset/BLP.h"
#include "asset/DDS.h"
#include "asset/stb_image.h"
#include <algorithm>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

static bool IsPortraitOnlyNodeName(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return lower.find("portraitbackground") != std::string::npos;
}

static bool ShouldLogModelDebug(const std::string& name) {
    (void)name;
    return false;
}

static int g_mdxLoadTraceCount = 0;

static bool MdxLoadTraceEnabled() {
    static const bool enabled = []() {
        const char* value = std::getenv("WARSMASH_MDX_LOAD_TRACE");
        return value && value[0] && value[0] != '0';
    }();
    return enabled;
}

namespace {
static bool IsSuspiciousViewHandle(sg_view view) {
    if (!view.id) {
        return false;
    }
    const uint32_t slot = view.id & 0xFFFFu;
    return (slot == 0u) || (slot > 2048u);
}

static void SafeDestroyViewHandle(sg_view& view, const char* owner) {
    if (!view.id) {
        return;
    }
    if (IsSuspiciousViewHandle(view)) {
        std::fprintf(stderr, "MdxViewer: skipped suspicious view destroy owner=%s id=0x%08x\n",
            owner, view.id);
        view = {0};
        return;
    }
    if (sg_query_view_state(view) == SG_RESOURCESTATE_VALID) {
        sg_destroy_view(view);
    }
    view = {0};
}
}

static constexpr uint32_t MakeTag(char a, char b, char c, char d) {
    return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16)
         | (uint32_t(uint8_t(c)) << 8) | uint32_t(uint8_t(d));
}

static const uint32_t TAG_KGAO = MakeTag('K','G','A','O');
static const uint32_t TAG_KGAC = MakeTag('K','G','A','C');
static const uint32_t TAG_KMTA = MakeTag('K','M','T','A');
static const uint32_t TAG_KMTF = MakeTag('K','M','T','F');
static const uint32_t TAG_KTAT = MakeTag('K','T','A','T');
static const uint32_t TAG_KTAR = MakeTag('K','T','A','R');
static const uint32_t TAG_KTAS = MakeTag('K','T','A','S');

static const MdxTimeline* FindTimeline(const std::vector<MdxTimeline>& timelines, uint32_t tag) {
    for (const auto& timeline : timelines) {
        if (timeline.tag == tag) {
            return &timeline;
        }
    }
    return nullptr;
}

static uint32_t TimelineFrameForModel(const MdxTimeline& timeline, const MdxModel* model,
                                      uint32_t sequenceFrame, float elapsedMs) {
    if (timeline.globalSequenceId >= 0 && model
        && timeline.globalSequenceId < (int)model->globalSequences.size()) {
        const uint32_t duration = model->globalSequences[(size_t)timeline.globalSequenceId];
        if (duration > 0) {
            const uint32_t elapsed = (uint32_t)std::max(0.0f, elapsedMs);
            return elapsed % duration;
        }
    }
    return sequenceFrame;
}

static bool TimelineHasKeyValues(const MdxTimeline& timeline, int key, int elemCount) {
    if (key < 0 || elemCount < 1) {
        return false;
    }
    const size_t end = ((size_t)key + 1u) * (size_t)elemCount;
    return timeline.values.size() >= end;
}

static float TimelineValueOrDefault(const MdxTimeline& timeline, int key, int elem, int elemCount,
                                    float defaultValue) {
    if (key < 0 || elem < 0 || elem >= elemCount || elemCount < 1) {
        return defaultValue;
    }
    const size_t index = (size_t)key * (size_t)elemCount + (size_t)elem;
    if (index >= timeline.values.size()) {
        return defaultValue;
    }
    return timeline.values[index];
}

static bool TimelineHasTangents(const MdxTimeline& timeline, int keyA, int keyB, int elemCount) {
    if (keyA < 0 || keyB < 0 || elemCount < 1) {
        return false;
    }
    const size_t neededA = ((size_t)keyA + 1u) * (size_t)elemCount;
    const size_t neededB = ((size_t)keyB + 1u) * (size_t)elemCount;
    const size_t needed = std::max(neededA, neededB);
    return timeline.inTans.size() >= needed && timeline.outTans.size() >= needed;
}

static float InterpolateTimelineValue(const MdxTimeline& timeline, int elem, int elemCount,
                                      int keyA, int keyB, float t) {
    const float a = TimelineValueOrDefault(timeline, keyA, elem, elemCount, 0.0f);
    const float b = TimelineValueOrDefault(timeline, keyB, elem, elemCount, a);
    switch (timeline.interpolationType) {
    case MdxInterpolation::None:
        return a;
    case MdxInterpolation::Linear:
        return a + (b - a) * t;
    case MdxInterpolation::Hermite: {
        if (!TimelineHasTangents(timeline, keyA, keyB, elemCount)) {
            return a + (b - a) * t;
        }
        const float outTan = timeline.outTans[(size_t)keyA * elemCount + elem];
        const float inTan = timeline.inTans[(size_t)keyB * elemCount + elem];
        const float t2 = t * t;
        const float t3 = t2 * t;
        return (2.0f * t3 - 3.0f * t2 + 1.0f) * a
            + (-2.0f * t3 + 3.0f * t2) * b
            + (t3 - 2.0f * t2 + t) * outTan
            + (t3 - t2) * inTan;
    }
    case MdxInterpolation::Bezier: {
        if (!TimelineHasTangents(timeline, keyA, keyB, elemCount)) {
            return a + (b - a) * t;
        }
        const float outTan = timeline.outTans[(size_t)keyA * elemCount + elem];
        const float inTan = timeline.inTans[(size_t)keyB * elemCount + elem];
        const float mt = 1.0f - t;
        return mt * mt * mt * a
            + 3.0f * mt * mt * t * outTan
            + 3.0f * mt * t * t * inTan
            + t * t * t * b;
    }
    default:
        return a;
    }
}

static bool SampleGlobalTimelineLooped(const MdxTimeline& timeline, uint32_t frame,
                                       uint32_t duration, float* out, int elemCount) {
    if (!out || timeline.frames.empty() || elemCount < 1 || duration == 0) {
        return false;
    }
    if (timeline.frames.size() == 1) {
        for (int elem = 0; elem < elemCount; ++elem) {
            out[elem] = TimelineValueOrDefault(timeline, 0, elem, elemCount, 0.0f);
        }
        return TimelineHasKeyValues(timeline, 0, elemCount);
    }

    if (!TimelineHasKeyValues(timeline, (int)timeline.frames.size() - 1, elemCount)) {
        return false;
    }

    const int lastIndex = (int)timeline.frames.size() - 1;
    int keyA = lastIndex;
    int keyB = 0;
    if (frame >= timeline.frames[0] && frame < timeline.frames[(size_t)lastIndex]) {
        for (int i = 1; i <= lastIndex; ++i) {
            if (timeline.frames[(size_t)i] > frame) {
                keyA = i - 1;
                keyB = i;
                break;
            }
        }
    }

    uint32_t startFrame = timeline.frames[(size_t)keyA];
    const uint32_t endFrame = timeline.frames[(size_t)keyB];
    int64_t timeBetweenFrames = (int64_t)endFrame - (int64_t)startFrame;
    if (timeBetweenFrames < 0) {
        timeBetweenFrames += duration;
        if (frame < startFrame) {
            startFrame = endFrame;
        }
    }
    const float t = timeBetweenFrames == 0
        ? 0.0f
        : ((float)((int64_t)frame - (int64_t)startFrame) / (float)timeBetweenFrames);

    for (int elem = 0; elem < elemCount; ++elem) {
        out[elem] = InterpolateTimelineValue(timeline, elem, elemCount, keyA, keyB, t);
    }
    return true;
}

static bool SampleTimelineForModel(const MdxTimeline& timeline, const MdxModel* model,
                                   uint32_t sequenceFrame, float elapsedMs,
                                   float* out, int elemCount, float defaultVal = 0.0f,
                                   int sequenceIndex = -1) {
    return MdxSampleTimelineForModel(timeline, model, sequenceFrame, elapsedMs,
                                     out, elemCount, defaultVal, sequenceIndex);
}

static bool ModelHasAnimatedMaterials(const MdxModel& model) {
    if (!model.texAnims.empty()) {
        return true;
    }
    if (!model.geosetAnims.empty()) {
        return true;
    }
    for (const auto& material : model.materials) {
        for (const auto& layer : material.layers) {
            if (!layer.timelines.empty()) {
                return true;
            }
        }
    }
    return false;
}

static std::string ToLowerCopy(const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return lower;
}

static std::string TrimAsciiWhitespace(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(start, end - start);
}

static std::string GetSequenceBaseNameLower(const std::string& sequenceName) {
    const size_t dash = sequenceName.find('-');
    const std::string baseName = (dash == std::string::npos)
        ? sequenceName
        : sequenceName.substr(0, dash);
    return ToLowerCopy(TrimAsciiWhitespace(baseName));
}

static const std::array<const char*, 53> kKnownSecondaryTagNames = {{
    "alternate", "alternateex", "bone", "chain", "channel", "complete", "critical", "defend",
    "drain", "eattree", "fast", "fill", "flail", "flesh", "fifth", "fire",
    "first", "five", "four", "fourth", "gold", "hit", "large", "left",
    "light", "looping", "lumber", "medium", "moderate", "off", "one", "puke",
    "ready", "right", "second", "severe", "slam", "small", "spiked", "spin",
    "spell", "cinematic", "swim", "talk", "third", "three", "throw", "two",
    "turn", "victory", "work", "wounded", "upgrade"
}};

static int GetKnownSecondaryTagOrdinal(const std::string& token) {
    for (int i = 0; i < (int)kKnownSecondaryTagNames.size(); ++i) {
        if (token == kKnownSecondaryTagNames[(size_t)i]) {
            return i;
        }
    }
    return -1;
}

static bool IsPrimarySequenceToken(const std::string& token) {
    return token == "attack"
        || token == "birth"
        || token == "death"
        || token == "decay"
        || token == "dissipate"
        || token == "morph"
        || token == "portrait"
        || token == "sleep"
        || token == "stand"
        || token == "walk";
}

static std::vector<std::string> TokenizeSequenceNameLower(const std::string& sequenceName) {
    std::vector<std::string> tokens;
    std::string token;
    token.reserve(sequenceName.size());
    for (unsigned char ch : sequenceName) {
        if (std::isalnum(ch) != 0) {
            token.push_back((char)std::tolower(ch));
        }
        else if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

static void SortSecondaryTagsByOrdinal(std::vector<std::string>& tags) {
    std::sort(tags.begin(), tags.end(), [](const std::string& a, const std::string& b) {
        const int aOrdinal = GetKnownSecondaryTagOrdinal(a);
        const int bOrdinal = GetKnownSecondaryTagOrdinal(b);
        if (aOrdinal != bOrdinal) {
            return aOrdinal < bOrdinal;
        }
        return a < b;
    });
}

static std::vector<std::string> NormalizeKnownSecondaryTags(const std::vector<std::string>& inputTags) {
    std::vector<std::string> normalizedTags;
    normalizedTags.reserve(inputTags.size());
    for (const std::string& inputTag : inputTags) {
        std::string lowerTag = ToLowerCopy(inputTag);
        if (GetKnownSecondaryTagOrdinal(lowerTag) < 0) {
            continue;
        }
        if (std::find(normalizedTags.begin(), normalizedTags.end(), lowerTag) == normalizedTags.end()) {
            normalizedTags.push_back(std::move(lowerTag));
        }
    }
    SortSecondaryTagsByOrdinal(normalizedTags);
    return normalizedTags;
}

static std::vector<std::string> ExtractKnownSequenceSecondaryTags(const std::string& sequenceName) {
    std::vector<std::string> tags;
    const auto tokens = TokenizeSequenceNameLower(sequenceName);
    for (const std::string& token : tokens) {
        if (IsPrimarySequenceToken(token)) {
            continue;
        }
        if (GetKnownSecondaryTagOrdinal(token) < 0) {
            continue;
        }
        if (std::find(tags.begin(), tags.end(), token) == tags.end()) {
            tags.push_back(token);
        }
    }
    SortSecondaryTagsByOrdinal(tags);
    return tags;
}

static bool SequenceHasPrimaryToken(const MdxSequence& sequence, const std::string& primaryToken) {
    if (primaryToken.empty()) {
        return true;
    }
    const auto tokens = TokenizeSequenceNameLower(sequence.name);
    return std::find(tokens.begin(), tokens.end(), primaryToken) != tokens.end();
}

static int ComputeRequestedSecondaryTagMatchRank(const std::vector<std::string>& requestedTags,
                                                 const std::vector<std::string>& candidateTags) {
    int matchRank = 0;
    for (const std::string& requestedTag : requestedTags) {
        if (std::find(candidateTags.begin(), candidateTags.end(), requestedTag) == candidateTags.end()) {
            continue;
        }
        const int ordinal = GetKnownSecondaryTagOrdinal(requestedTag);
        if (ordinal >= 0) {
            matchRank += 200000 + ((int)kKnownSecondaryTagNames.size() - ordinal);
        }
    }
    return matchRank;
}

static int ComputeSecondaryTagOrdinalRank(const std::vector<std::string>& candidateTags) {
    int rank = 0;
    for (const std::string& tag : candidateTags) {
        const int ordinal = GetKnownSecondaryTagOrdinal(tag);
        if (ordinal >= 0) {
            rank += ((int)kKnownSecondaryTagNames.size() - ordinal);
        }
    }
    return rank;
}

struct SequenceSelectionScore {
    bool exactRequested = false;
    bool positiveMatch = false;
    int matchRank = 0;
    int secondaryCount = 0;
    bool exactBase = false;
    int ordinalRank = 0;
    int index = -1;
};

static bool IsBetterSequenceSelectionScore(const SequenceSelectionScore& candidate,
                                           const SequenceSelectionScore& currentBest) {
    if (candidate.exactRequested != currentBest.exactRequested) {
        return candidate.exactRequested;
    }
    if (candidate.positiveMatch != currentBest.positiveMatch) {
        return candidate.positiveMatch;
    }
    if (candidate.matchRank != currentBest.matchRank) {
        return candidate.matchRank > currentBest.matchRank;
    }
    if (candidate.secondaryCount != currentBest.secondaryCount) {
        return candidate.secondaryCount < currentBest.secondaryCount;
    }
    if (candidate.exactBase != currentBest.exactBase) {
        return candidate.exactBase;
    }
    if (candidate.ordinalRank != currentBest.ordinalRank) {
        return candidate.ordinalRank > currentBest.ordinalRank;
    }
    return candidate.index < currentBest.index;
}

static int FindBestSequenceIndexByPrimaryAndTags(const MdxModel& model,
                                                 const char* primaryToken,
                                                 const std::vector<std::string>& secondaryTags) {
    return MdxSelectSequenceIndexByPrimaryAndTags(model, primaryToken, secondaryTags);
}

static int FindSequenceByBaseName(const MdxModel& model, const char* baseName) {
    const std::string target = ToLowerCopy(baseName ? std::string(baseName) : std::string());
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        if (GetSequenceBaseNameLower(model.sequences[i].name) == target) {
            return i;
        }
    }
    return -1;
}

static bool IsEmbeddedSplatTexturePath(const std::string& path) {
    const std::string lowerPath = ToLowerCopy(path);
    return lowerPath.find("replaceabletextures\\splats\\") != std::string::npos;
}

enum BatchSequenceHint : uint32_t {
    BatchSequenceHint_None = 0,
    BatchSequenceHint_Birth = 1u << 0,
    BatchSequenceHint_Death = 1u << 1,
    BatchSequenceHint_Decay = 1u << 2,
};

static void AppendSequenceHintsFromTexturePath(uint32_t& hintMask, const std::string& lowerText) {
    if (lowerText.find("birth") != std::string::npos) {
        hintMask |= BatchSequenceHint_Birth;
    }
    if (lowerText.find("death") != std::string::npos) {
        hintMask |= BatchSequenceHint_Death;
    }
    if (lowerText.find("decay") != std::string::npos
        || lowerText.find("corpse") != std::string::npos
        || lowerText.find("flesh") != std::string::npos
        || lowerText.find("bone") != std::string::npos
        || lowerText.find("gutz") != std::string::npos
        || lowerText.find("smug") != std::string::npos) {
        hintMask |= BatchSequenceHint_Decay;
    }
}

static void AppendSequenceHintsFromNodeName(uint32_t& hintMask, const std::string& lowerText) {
    if (lowerText.find("birth") != std::string::npos) {
        hintMask |= BatchSequenceHint_Birth;
    }
    if (lowerText.find("death") != std::string::npos) {
        hintMask |= BatchSequenceHint_Death;
    }
    if (lowerText.find("decay") != std::string::npos
        || lowerText.find("corpse") != std::string::npos
        || lowerText.find("flesh") != std::string::npos
        || lowerText.find("gutz") != std::string::npos
        || lowerText.find("smug") != std::string::npos) {
        hintMask |= BatchSequenceHint_Decay;
    }
}

static int FindBestDefaultStandSequenceIndex(const MdxModel& model) {
    return FindBestSequenceIndexByPrimaryAndTags(model, "stand", {});
}

static int FindBestTaggedStandSequenceIndex(const MdxModel& model, const char* secondaryTag) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return FindBestSequenceIndexByPrimaryAndTags(model, "stand", tags);
}

static int FindBestBirthSequenceIndex(const MdxModel& model) {
    const int exactBirth = FindSequenceByBaseName(model, "birth");
    if (exactBirth >= 0) {
        return exactBirth;
    }

    int fallbackBirth = -1;
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const std::string lowerName = ToLowerCopy(model.sequences[i].name);
        if (lowerName.rfind("birth", 0) != 0) {
            continue;
        }
        const bool hasDisallowedTag = lowerName.find("alternate") != std::string::npos
            || lowerName.find("work") != std::string::npos
            || lowerName.find("swim") != std::string::npos
            || lowerName.find("ready") != std::string::npos
            || lowerName.find("gold") != std::string::npos
            || lowerName.find("lumber") != std::string::npos
            || lowerName.find("talk") != std::string::npos
            || lowerName.find("channel") != std::string::npos
            || lowerName.find("spell") != std::string::npos
            || lowerName.find("defend") != std::string::npos
            || lowerName.find("victory") != std::string::npos
            || lowerName.find("hit") != std::string::npos
            || lowerName.find("flesh") != std::string::npos
            || lowerName.find("bone") != std::string::npos;
        if (!hasDisallowedTag && fallbackBirth < 0) {
            fallbackBirth = i;
        }
    }
    return fallbackBirth;
}

static int FindBestWalkSequenceIndex(const MdxModel& model) {
    return FindBestSequenceIndexByPrimaryAndTags(model, "walk", {});
}

static int FindBestTaggedWalkSequenceIndex(const MdxModel& model, const char* secondaryTag) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return FindBestSequenceIndexByPrimaryAndTags(model, "walk", tags);
}

static int FindBestAttackSequenceIndex(const MdxModel& model) {
    return FindBestSequenceIndexByPrimaryAndTags(model, "attack", {});
}

static int FindBestPrimaryActionSequenceIndex(const MdxModel& model,
                                              const char* primaryToken,
                                              const char* secondaryTag) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return FindBestSequenceIndexByPrimaryAndTags(model, primaryToken, tags);
}

static float ComputeWalkSequenceSpeedRatio(const MdxModel& model,
                                           int sequenceIndex,
                                           float currentMovementSpeed,
                                           float animationScale) {
    if (sequenceIndex < 0 || sequenceIndex >= (int)model.sequences.size()) {
        return 1.0f;
    }
    const float animationMoveSpeed = model.sequences[sequenceIndex].moveSpeed * std::max(0.001f, animationScale);
    if (animationMoveSpeed <= 0.0f || currentMovementSpeed <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(currentMovementSpeed / animationMoveSpeed, 0.25f, 4.0f);
}

static float ComputeSequenceDurationSpeedRatio(const MdxModel& model,
                                               int sequenceIndex,
                                               float durationSeconds) {
    if (sequenceIndex < 0 || sequenceIndex >= (int)model.sequences.size() || durationSeconds <= 0.0f) {
        return 1.0f;
    }
    const float sequenceDurationSeconds = std::max(0.001f, MdxSequenceDurationSeconds(model, sequenceIndex));
    return std::clamp(sequenceDurationSeconds / durationSeconds, 0.1f, 4.0f);
}

static int FindBestDeathSequenceIndex(const MdxModel& model) {
    const int exactDeath = FindSequenceByBaseName(model, "death");
    if (exactDeath >= 0) {
        return exactDeath;
    }

    int fallbackDeath = -1;
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const std::string lowerName = ToLowerCopy(model.sequences[i].name);
        if (lowerName.rfind("death", 0) != 0) {
            continue;
        }
        const bool hasDisallowedTag = lowerName.find("alternate") != std::string::npos
            || lowerName.find("work") != std::string::npos
            || lowerName.find("swim") != std::string::npos
            || lowerName.find("ready") != std::string::npos
            || lowerName.find("gold") != std::string::npos
            || lowerName.find("lumber") != std::string::npos
            || lowerName.find("talk") != std::string::npos
            || lowerName.find("channel") != std::string::npos
            || lowerName.find("spell") != std::string::npos
            || lowerName.find("defend") != std::string::npos
            || lowerName.find("victory") != std::string::npos
            || lowerName.find("hit") != std::string::npos;
        if (!hasDisallowedTag && fallbackDeath < 0) {
            fallbackDeath = i;
        }
    }
    return fallbackDeath;
}

static int FindBestDecaySequenceIndex(const MdxModel& model, const char* secondaryTag) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    int decayIndex = FindBestSequenceIndexByPrimaryAndTags(model, "decay", tags);
    if (decayIndex >= 0) {
        return decayIndex;
    }
    const auto normalizedTags = NormalizeKnownSecondaryTags(tags);
    bool foundCorpse = false;
    SequenceSelectionScore bestCorpseScore;
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const auto& sequence = model.sequences[(size_t)i];
        const auto tokens = TokenizeSequenceNameLower(sequence.name);
        if (std::find(tokens.begin(), tokens.end(), "corpse") == tokens.end()) {
            continue;
        }
        const auto candidateTags = ExtractKnownSequenceSecondaryTags(sequence.name);
        SequenceSelectionScore candidateScore;
        candidateScore.exactRequested = candidateTags == normalizedTags;
        candidateScore.matchRank = ComputeRequestedSecondaryTagMatchRank(normalizedTags, candidateTags);
        candidateScore.positiveMatch = candidateScore.matchRank > 0;
        candidateScore.secondaryCount = (int)candidateTags.size();
        candidateScore.exactBase = GetSequenceBaseNameLower(sequence.name) == "corpse";
        candidateScore.ordinalRank = ComputeSecondaryTagOrdinalRank(candidateTags);
        candidateScore.index = i;
        if (!foundCorpse || IsBetterSequenceSelectionScore(candidateScore, bestCorpseScore)) {
            bestCorpseScore = candidateScore;
            foundCorpse = true;
        }
    }
    return foundCorpse ? bestCorpseScore.index : -1;
}

static int FindBestDissipateSequenceIndex(const MdxModel& model) {
    const int exactDissipate = FindSequenceByBaseName(model, "dissipate");
    if (exactDissipate >= 0) {
        return exactDissipate;
    }

    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const std::string lowerName = ToLowerCopy(model.sequences[i].name);
        if (lowerName.find("dissipate") != std::string::npos) {
            return i;
        }
    }
    return FindBestDecaySequenceIndex(model, "bone");
}

static bool SequenceNameContains(const std::string& lowerName, const char* token) {
    return token && lowerName.find(token) != std::string::npos;
}

MdxViewer::MdxViewer()
    : shader_{0}
    , pipeline_{0}
    , batches_{}
    , name_{}
    , mpq_{nullptr}
    , center_{0, 0, 0}
    , radius_{0.0f}
    , modelData_{}
    , ownsModelData_{false}
    , ownsBatchGpuResources_{true}
    , modelSource_{nullptr}
    , animator_{}
    , hasAnimations_{false}
    , hasAnimatedMaterials_{false}
    , initializedAnimator_{false}
    , animatedGeosetStates_{}
    , animatedLayerAlphas_{}
    , cachedBoneView_{0}
    , cachedBoneTexId_{0}
    , particles_{}
    , hasParticles_{false}
    , eventEffectResolver_{}
    , eventEffectResolverLoaded_{false}
    , resolvedEventEffects_{}
    , triggeredEventCount_{0}
    , resolvedEventEffectCount_{0}
    , pendingStandSequenceAfterCurrent_{false}
    , sharedSource_{nullptr} {
}

MdxViewer::~MdxViewer() { Release(); }

sg_shader MdxViewer::sharedShader_ = {0};
sg_pipeline MdxViewer::sharedPipeline_ = {0};
sg_pipeline MdxViewer::sharedNoDepthWritePipeline_ = {0};
sg_pipeline MdxViewer::sharedBlendPipeline_ = {0};
sg_pipeline MdxViewer::sharedAdditivePipeline_ = {0};
sg_pipeline MdxViewer::sharedAddAlphaPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlayPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlayBlendPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlayAdditivePipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlayAddAlphaPipeline_ = {0};
sg_shader MdxViewer::sharedSkinnedShader_ = {0};
sg_pipeline MdxViewer::sharedSkinnedPipeline_ = {0};
sg_pipeline MdxViewer::sharedSkinnedNoDepthWritePipeline_ = {0};
sg_pipeline MdxViewer::sharedSkinnedBlendPipeline_ = {0};
sg_pipeline MdxViewer::sharedSkinnedAdditivePipeline_ = {0};
sg_pipeline MdxViewer::sharedSkinnedAddAlphaPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlaySkinnedPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlaySkinnedBlendPipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlaySkinnedAdditivePipeline_ = {0};
sg_pipeline MdxViewer::sharedOverlaySkinnedAddAlphaPipeline_ = {0};
sg_sampler MdxViewer::sharedSampler_ = {0};
sg_image MdxViewer::sharedWhiteTexture_ = {0};
sg_view MdxViewer::sharedWhiteTextureView_ = {0};

void MdxViewer::DebugLogState(const char* tag) const {
    if (!MdxLoadTraceEnabled()) {
        return;
    }
    std::fprintf(stderr,
        "MdxViewer::State tag=%s this=%p name=%s batches=%zu ownsModel=%d ownsGpu=%d modelSource=%p sharedSource=%p hasAnim=%d hasAnimMats=%d initAnim=%d particles=%d boneView=0x%08x boneTexId=%u geoStates=%zu layerStates=%zu\n",
        tag ? tag : "",
        this,
        name_.c_str(),
        batches_.size(),
        ownsModelData_ ? 1 : 0,
        ownsBatchGpuResources_ ? 1 : 0,
        modelSource_,
        sharedSource_,
        hasAnimations_ ? 1 : 0,
        hasAnimatedMaterials_ ? 1 : 0,
        initializedAnimator_ ? 1 : 0,
        hasParticles_ ? 1 : 0,
        cachedBoneView_.id,
        cachedBoneTexId_,
        animatedGeosetStates_.size(),
        animatedLayerAlphas_.size());
    std::fflush(stderr);
}

void MdxViewer::SetFog(const float color[3], float start, float end) {
    fogEnabled_ = true;
    fogColor_[0] = color ? color[0] : 0.0f;
    fogColor_[1] = color ? color[1] : 0.0f;
    fogColor_[2] = color ? color[2] : 0.0f;
    fogStart_ = start;
    fogEnd_ = std::max(start + 0.001f, end);
}

void MdxViewer::DisableFog() {
    fogEnabled_ = false;
}

void MdxViewer::InitSharedShader() {
    if (sharedShader_.id) return;

    // Create shared sampler
    sg_sampler_desc smpDesc = {};
    smpDesc.min_filter = SG_FILTER_LINEAR;
    smpDesc.mag_filter = SG_FILTER_LINEAR;
    smpDesc.mipmap_filter = SG_FILTER_LINEAR;
    smpDesc.wrap_u = SG_WRAP_REPEAT;
    smpDesc.wrap_v = SG_WRAP_REPEAT;
    sharedSampler_ = sg_make_sampler(&smpDesc);

    const uint32_t whitePixel = 0xFFFFFFFFu;
    sg_image_desc whiteDesc = {};
    whiteDesc.type = SG_IMAGETYPE_2D;
    whiteDesc.width = 1;
    whiteDesc.height = 1;
    whiteDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
    whiteDesc.data.mip_levels[0].ptr = &whitePixel;
    whiteDesc.data.mip_levels[0].size = sizeof(whitePixel);
    sharedWhiteTexture_ = sg_make_image(&whiteDesc);
    if (sg_query_image_state(sharedWhiteTexture_) == SG_RESOURCESTATE_VALID) {
        sg_view_desc whiteViewDesc = {};
        whiteViewDesc.texture.image = sharedWhiteTexture_;
        sharedWhiteTextureView_ = sg_make_view(&whiteViewDesc);
    } else {
        sharedWhiteTexture_ = {0};
    }

    // ---- Static (non-skinned) shader ----
    {
        const char* vertSrc = R"(
            #version 410 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNorm;
            layout(location = 2) in vec2 aUV;
            out vec2 vUV;
            out vec3 vNorm;
            uniform mat4 uMVP;
            uniform mat4 uModel;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
                vUV = aUV;
                vNorm = mat3(uModel) * aNorm;
            }
        )";
        const char* fragSrc = R"(
            #version 410 core
            in vec2 vUV;
            in vec3 vNorm;
            out vec4 FragColor;
            uniform sampler2D uTexture;
            uniform vec4 uDebugColor;
            uniform vec4 uUvTransRot;
            uniform vec4 uTexFlags;
            uniform vec4 uFogColor;
            uniform vec4 uFogParams;
            vec2 quat_transform(vec2 q, vec2 v) {
                vec2 uv = vec2(-q.x * v.y, q.x * v.x);
                vec2 uuv = vec2(-q.x * uv.y, q.x * uv.x);
                return v + 2.0 * (uv * q.y + uuv);
            }
            void main() {
                vec3 light = normalize(vec3(0.5, 1.0, 0.3));
                float diff = max(abs(dot(normalize(vNorm), light)), 0.35);
                vec4 color = uDebugColor;
                vec2 uv = vUV;
                uv += uUvTransRot.xy;
                uv = quat_transform(uUvTransRot.zw, uv - 0.5) + 0.5;
                uv = uTexFlags.zw * (uv - 0.5) + 0.5;
                if (uTexFlags.x > 0.5) color = texture(uTexture, uv) * uDebugColor;
                if (uTexFlags.y == 1.0 && color.a < 0.75) discard;
                if (uTexFlags.y >= 5.0 && color.a < 0.02) discard;
                FragColor = vec4(color.rgb * diff, color.a);
                if (uFogParams.x > 0.5) {
                    float fogAmount = clamp(((1.0 / gl_FragCoord.w) - uFogParams.y) / (uFogParams.z - uFogParams.y), 0.0, 1.0);
                    FragColor.rgb = FragColor.rgb * (1.0 - fogAmount) + uFogColor.rgb * fogAmount;
                }
                if (FragColor.a < 0.01) discard;
            }
        )";

        sg_shader_desc sd = {};
        sd.vertex_func.source = vertSrc;
        sd.fragment_func.source = fragSrc;
        sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.uniform_blocks[0].size = 128;
        sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uMVP";
        sd.uniform_blocks[0].glsl_uniforms[1].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[1].glsl_name = "uModel";
        sd.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.uniform_blocks[1].size = 80;
        sd.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[1].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[0].glsl_name = "uDebugColor";
        sd.uniform_blocks[1].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[1].glsl_name = "uUvTransRot";
        sd.uniform_blocks[1].glsl_uniforms[2].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[2].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[2].glsl_name = "uTexFlags";
        sd.uniform_blocks[1].glsl_uniforms[3].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[3].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[3].glsl_name = "uFogColor";
        sd.uniform_blocks[1].glsl_uniforms[4].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[4].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[4].glsl_name = "uFogParams";
        sd.views[0].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.texture_sampler_pairs[0].glsl_name = "uTexture";
        sd.texture_sampler_pairs[0].view_slot = 0;
        sd.texture_sampler_pairs[0].sampler_slot = 0;
        sharedShader_ = sg_make_shader(&sd);
    }

    // ---- Skinned shader ----
    {
        const char* vertSrc = R"(
            #version 410 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNorm;
            layout(location = 2) in vec2 aUV;
            layout(location = 3) in vec4 aBoneIndices0;
            layout(location = 4) in vec4 aBoneIndices1;
            layout(location = 5) in vec4 aBoneWeights0;
            layout(location = 6) in vec4 aBoneWeights1;
            layout(location = 7) in float aBoneCount;

            out vec2 vUV;
            out vec3 vNorm;
            uniform mat4 uMVP;
            uniform mat4 uModel;
            uniform sampler2D uBoneTexture;
            uniform int uBoneCount;

            mat4 getBoneMatrix(int idx) {
                ivec2 texSize = textureSize(uBoneTexture, 0);
                int base = idx * 4;
                vec4 c0 = texelFetch(uBoneTexture, ivec2(base + 0, 0), 0);
                vec4 c1 = texelFetch(uBoneTexture, ivec2(base + 1, 0), 0);
                vec4 c2 = texelFetch(uBoneTexture, ivec2(base + 2, 0), 0);
                vec4 c3 = texelFetch(uBoneTexture, ivec2(base + 3, 0), 0);
                return mat4(c0, c1, c2, c3);
            }

            void main() {
                vec3 pos = vec3(0);
                vec3 norm = vec3(0);
                int numBones = int(aBoneCount);
                for (int i = 0; i < 8 && i < numBones; i++) {
                    int bi = i < 4 ? int(aBoneIndices0[i]) : int(aBoneIndices1[i - 4]);
                    float w = i < 4 ? aBoneWeights0[i] : aBoneWeights1[i - 4];
                    if (bi >= 0 && bi < uBoneCount) {
                        mat4 m = getBoneMatrix(bi);
                        pos += (m * vec4(aPos, 1.0)).xyz * w;
                        norm += (mat3(m) * aNorm) * w;
                    }
                }
                gl_Position = uMVP * vec4(pos, 1.0);
                vUV = aUV;
                vNorm = normalize(mat3(uModel) * norm);
            }
        )";
        const char* fragSrc = R"(
            #version 410 core
            in vec2 vUV;
            in vec3 vNorm;
            out vec4 FragColor;
            uniform sampler2D uTexture;
            uniform vec4 uDebugColor;
            uniform vec4 uUvTransRot;
            uniform vec4 uTexFlags;
            uniform vec4 uFogColor;
            uniform vec4 uFogParams;
            vec2 quat_transform(vec2 q, vec2 v) {
                vec2 uv = vec2(-q.x * v.y, q.x * v.x);
                vec2 uuv = vec2(-q.x * uv.y, q.x * uv.x);
                return v + 2.0 * (uv * q.y + uuv);
            }
            void main() {
                vec3 light = normalize(vec3(0.5, 1.0, 0.3));
                float diff = max(abs(dot(normalize(vNorm), light)), 0.35);
                vec4 color = uDebugColor;
                vec2 uv = vUV;
                uv += uUvTransRot.xy;
                uv = quat_transform(uUvTransRot.zw, uv - 0.5) + 0.5;
                uv = uTexFlags.zw * (uv - 0.5) + 0.5;
                if (uTexFlags.x > 0.5) color = texture(uTexture, uv) * uDebugColor;
                if (uTexFlags.y == 1.0 && color.a < 0.75) discard;
                if (uTexFlags.y >= 5.0 && color.a < 0.02) discard;
                FragColor = vec4(color.rgb * diff, color.a);
                if (uFogParams.x > 0.5) {
                    float fogAmount = clamp(((1.0 / gl_FragCoord.w) - uFogParams.y) / (uFogParams.z - uFogParams.y), 0.0, 1.0);
                    FragColor.rgb = FragColor.rgb * (1.0 - fogAmount) + uFogColor.rgb * fogAmount;
                }
                if (FragColor.a < 0.01) discard;
            }
        )";

        sg_shader_desc sd = {};
        sd.vertex_func.source = vertSrc;
        sd.fragment_func.source = fragSrc;

        // VS uniforms: uMVP(64) + uModel(64) + uBoneCount(4) = 132 bytes
        sd.uniform_blocks[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.uniform_blocks[0].size = 132;
        sd.uniform_blocks[0].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[0].glsl_uniforms[0].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[0].glsl_name = "uMVP";
        sd.uniform_blocks[0].glsl_uniforms[1].type = SG_UNIFORMTYPE_MAT4;
        sd.uniform_blocks[0].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[1].glsl_name = "uModel";
        sd.uniform_blocks[0].glsl_uniforms[2].type = SG_UNIFORMTYPE_INT;
        sd.uniform_blocks[0].glsl_uniforms[2].array_count = 1;
        sd.uniform_blocks[0].glsl_uniforms[2].glsl_name = "uBoneCount";

        sd.uniform_blocks[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.uniform_blocks[1].size = 80;
        sd.uniform_blocks[1].layout = SG_UNIFORMLAYOUT_NATIVE;
        sd.uniform_blocks[1].glsl_uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[0].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[0].glsl_name = "uDebugColor";
        sd.uniform_blocks[1].glsl_uniforms[1].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[1].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[1].glsl_name = "uUvTransRot";
        sd.uniform_blocks[1].glsl_uniforms[2].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[2].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[2].glsl_name = "uTexFlags";
        sd.uniform_blocks[1].glsl_uniforms[3].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[3].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[3].glsl_name = "uFogColor";
        sd.uniform_blocks[1].glsl_uniforms[4].type = SG_UNIFORMTYPE_FLOAT4;
        sd.uniform_blocks[1].glsl_uniforms[4].array_count = 1;
        sd.uniform_blocks[1].glsl_uniforms[4].glsl_name = "uFogParams";

        // Bone texture (VS, slot 1)
        sd.views[0].texture.stage = SG_SHADERSTAGE_VERTEX;
        sd.views[0].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[0].stage = SG_SHADERSTAGE_VERTEX;
        sd.texture_sampler_pairs[0].glsl_name = "uBoneTexture";
        sd.texture_sampler_pairs[0].view_slot = 0;
        sd.texture_sampler_pairs[0].sampler_slot = 0;

        // Diffuse texture (FS, slot 1)
        sd.views[1].texture.stage = SG_SHADERSTAGE_FRAGMENT;
        sd.views[1].texture.image_type = SG_IMAGETYPE_2D;
        sd.views[1].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        sd.samplers[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.samplers[1].sampler_type = SG_SAMPLERTYPE_FILTERING;
        sd.texture_sampler_pairs[1].stage = SG_SHADERSTAGE_FRAGMENT;
        sd.texture_sampler_pairs[1].glsl_name = "uTexture";
        sd.texture_sampler_pairs[1].view_slot = 1;
        sd.texture_sampler_pairs[1].sampler_slot = 1;

        sharedSkinnedShader_ = sg_make_shader(&sd);
    }

    // ---- Static pipeline ----
    {
        sg_pipeline_desc pd = {};
        pd.shader = sharedShader_;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT32;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = true;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = false;
        pd.layout.buffers[0].stride = 32; // 8 floats x 4 bytes
        pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT3};
        pd.layout.attrs[1] = {0, 12, SG_VERTEXFORMAT_FLOAT3};
        pd.layout.attrs[2] = {0, 24, SG_VERTEXFORMAT_FLOAT2};
        sharedPipeline_ = sg_make_pipeline(&pd);

        pd.depth.write_enabled = false;
        sharedNoDepthWritePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.depth.write_enabled = false;
        sharedBlendPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedAdditivePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedAddAlphaPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = false;
        pd.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pd.depth.write_enabled = false;
        sharedOverlayPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        sharedOverlayBlendPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedOverlayAdditivePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedOverlayAddAlphaPipeline_ = sg_make_pipeline(&pd);
    }

    // ---- Skinned pipeline ----
    {
        sg_pipeline_desc pd = {};
        pd.shader = sharedSkinnedShader_;
        pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pd.index_type = SG_INDEXTYPE_UINT32;
        pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
        pd.depth.write_enabled = true;
        pd.cull_mode = SG_CULLMODE_NONE;
        pd.colors[0].blend.enabled = false;
        // Vertex: pos(3f) + norm(3f) + uv(2f) + boneIdx(8ub) + boneWt(8f) + boneCount(1f)
        // Stride = 12 + 12 + 8 + 8 + 32 + 4 = 76 bytes
        pd.layout.buffers[0].stride = sizeof(MdxSkinnedVertex);
        pd.layout.attrs[0] = {0, 0, SG_VERTEXFORMAT_FLOAT3};          // aPos
        pd.layout.attrs[1] = {0, 12, SG_VERTEXFORMAT_FLOAT3};         // aNorm
        pd.layout.attrs[2] = {0, 24, SG_VERTEXFORMAT_FLOAT2};         // aUV
        pd.layout.attrs[3] = {0, 32, SG_VERTEXFORMAT_UBYTE4};         // aBoneIndices0
        pd.layout.attrs[4] = {0, 36, SG_VERTEXFORMAT_UBYTE4};         // aBoneIndices1
        pd.layout.attrs[5] = {0, 40, SG_VERTEXFORMAT_FLOAT4};         // aBoneWeights0
        pd.layout.attrs[6] = {0, 56, SG_VERTEXFORMAT_FLOAT4};         // aBoneWeights1
        pd.layout.attrs[7] = {0, 72, SG_VERTEXFORMAT_FLOAT};          // aBoneCount
        sharedSkinnedPipeline_ = sg_make_pipeline(&pd);

        pd.depth.write_enabled = false;
        sharedSkinnedNoDepthWritePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pd.depth.write_enabled = false;
        sharedSkinnedBlendPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedSkinnedAdditivePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedSkinnedAddAlphaPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = false;
        pd.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pd.depth.write_enabled = false;
        sharedOverlaySkinnedPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.enabled = true;
        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        sharedOverlaySkinnedBlendPipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_ONE;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedOverlaySkinnedAdditivePipeline_ = sg_make_pipeline(&pd);

        pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE;
        sharedOverlaySkinnedAddAlphaPipeline_ = sg_make_pipeline(&pd);
    }
}

void MdxViewer::ReleaseSharedShader() {
    if (sharedOverlayAddAlphaPipeline_.id) sg_destroy_pipeline(sharedOverlayAddAlphaPipeline_);
    if (sharedOverlayAdditivePipeline_.id) sg_destroy_pipeline(sharedOverlayAdditivePipeline_);
    if (sharedOverlayBlendPipeline_.id) sg_destroy_pipeline(sharedOverlayBlendPipeline_);
    if (sharedOverlayPipeline_.id) sg_destroy_pipeline(sharedOverlayPipeline_);
    if (sharedAddAlphaPipeline_.id) sg_destroy_pipeline(sharedAddAlphaPipeline_);
    if (sharedAdditivePipeline_.id) sg_destroy_pipeline(sharedAdditivePipeline_);
    if (sharedBlendPipeline_.id) sg_destroy_pipeline(sharedBlendPipeline_);
    if (sharedNoDepthWritePipeline_.id) sg_destroy_pipeline(sharedNoDepthWritePipeline_);
    if (sharedPipeline_.id) sg_destroy_pipeline(sharedPipeline_);
    if (sharedOverlaySkinnedAddAlphaPipeline_.id) sg_destroy_pipeline(sharedOverlaySkinnedAddAlphaPipeline_);
    if (sharedOverlaySkinnedAdditivePipeline_.id) sg_destroy_pipeline(sharedOverlaySkinnedAdditivePipeline_);
    if (sharedOverlaySkinnedBlendPipeline_.id) sg_destroy_pipeline(sharedOverlaySkinnedBlendPipeline_);
    if (sharedOverlaySkinnedPipeline_.id) sg_destroy_pipeline(sharedOverlaySkinnedPipeline_);
    if (sharedSkinnedAddAlphaPipeline_.id) sg_destroy_pipeline(sharedSkinnedAddAlphaPipeline_);
    if (sharedSkinnedAdditivePipeline_.id) sg_destroy_pipeline(sharedSkinnedAdditivePipeline_);
    if (sharedSkinnedBlendPipeline_.id) sg_destroy_pipeline(sharedSkinnedBlendPipeline_);
    if (sharedSkinnedNoDepthWritePipeline_.id) sg_destroy_pipeline(sharedSkinnedNoDepthWritePipeline_);
    if (sharedShader_.id) sg_destroy_shader(sharedShader_);
    if (sharedSkinnedPipeline_.id) sg_destroy_pipeline(sharedSkinnedPipeline_);
    if (sharedSkinnedShader_.id) sg_destroy_shader(sharedSkinnedShader_);
    if (sharedWhiteTextureView_.id) sg_destroy_view(sharedWhiteTextureView_);
    if (sharedWhiteTexture_.id) sg_destroy_image(sharedWhiteTexture_);
    if (sharedSampler_.id) sg_destroy_sampler(sharedSampler_);
    sharedShader_ = {0}; sharedPipeline_ = {0}; sharedNoDepthWritePipeline_ = {0}; sharedBlendPipeline_ = {0};
    sharedAdditivePipeline_ = {0}; sharedAddAlphaPipeline_ = {0};
    sharedOverlayPipeline_ = {0}; sharedOverlayBlendPipeline_ = {0};
    sharedOverlayAdditivePipeline_ = {0}; sharedOverlayAddAlphaPipeline_ = {0};
    sharedSkinnedShader_ = {0}; sharedSkinnedPipeline_ = {0}; sharedSkinnedNoDepthWritePipeline_ = {0}; sharedSkinnedBlendPipeline_ = {0};
    sharedSkinnedAdditivePipeline_ = {0}; sharedSkinnedAddAlphaPipeline_ = {0};
    sharedOverlaySkinnedPipeline_ = {0}; sharedOverlaySkinnedBlendPipeline_ = {0};
    sharedOverlaySkinnedAdditivePipeline_ = {0}; sharedOverlaySkinnedAddAlphaPipeline_ = {0};
    sharedSampler_ = {0};
    sharedWhiteTexture_ = {0};
    sharedWhiteTextureView_ = {0};
}

void MdxViewer::Release() {
    const bool traceRelease = MdxLoadTraceEnabled();
    if (traceRelease) {
        std::fprintf(stderr, "MdxViewer::Release begin name=%s batches=%zu ownsGpu=%d rendererActive=%d\n",
            name_.c_str(), batches_.size(), ownsBatchGpuResources_ ? 1 : 0, Renderer::IsActive() ? 1 : 0);
        std::fflush(stderr);
    }
    const bool emptyViewerState = name_.empty()
        && batches_.empty()
        && !ownsModelData_
        && !hasParticles_;
    if (emptyViewerState) {
        cachedBoneView_ = {0};
        cachedBoneTexId_ = 0;
        shader_ = {0};
        modelSource_ = nullptr;
        sharedSource_ = nullptr;
        hasAnimations_ = false;
        hasAnimatedMaterials_ = false;
        initializedAnimator_ = false;
        animatedGeosetStates_.clear();
        animatedLayerAlphas_.clear();
        modelTextures_.clear();
        elapsedMs_ = 0.0f;
        ownsBatchGpuResources_ = true;
        pendingStandSequenceAfterCurrent_ = false;
        if (traceRelease) {
            std::fprintf(stderr, "MdxViewer::Release end empty\n");
            std::fflush(stderr);
        }
        return;
    }
    if (!Renderer::IsActive()) {
        batches_.clear();
        shader_ = {0};
        modelData_ = MdxModel{};
        ownsModelData_ = false;
        ownsBatchGpuResources_ = true;
        modelSource_ = nullptr;
        sharedSource_ = nullptr;
        hasAnimations_ = false;
        hasAnimatedMaterials_ = false;
        initializedAnimator_ = false;
        animatedGeosetStates_.clear();
        animatedLayerAlphas_.clear();
        modelTextures_.clear();
        elapsedMs_ = 0.0f;
        if (traceRelease) {
            std::fprintf(stderr, "MdxViewer::Release end inactive name=%s\n", name_.c_str());
            std::fflush(stderr);
        }
        return;
    }
    for (auto& b : batches_) {
        if (ownsBatchGpuResources_) {
            if (b.vbo.id) sg_destroy_buffer(b.vbo);
            if (b.ebo.id) sg_destroy_buffer(b.ebo);
            if (b.sampler.id) sg_destroy_sampler(b.sampler);
        }
        if (b.playerTexHandle.id) sg_destroy_image(b.playerTexHandle);
        SafeDestroyViewHandle(b.playerTexView, "batch.playerTexView");
    }
    if (ownsBatchGpuResources_) {
        for (auto& texture : modelTextures_) {
            SafeDestroyViewHandle(texture.view, "modelTexture.view");
            if (texture.handle.id) {
                sg_destroy_image(texture.handle);
                texture.handle = {0};
            }
        }
    }
    modelTextures_.clear();
    if (traceRelease) {
        std::fprintf(stderr, "MdxViewer::Release before bone-view id=0x%08x texId=%u\n",
            cachedBoneView_.id, cachedBoneTexId_);
        std::fflush(stderr);
    }
    SafeDestroyViewHandle(cachedBoneView_, "cachedBoneView");
    if (traceRelease) {
        std::fprintf(stderr, "MdxViewer::Release after bone-view\n");
        std::fflush(stderr);
    }
    cachedBoneTexId_ = 0;
    batches_.clear();
    shader_ = {0};
    if (hasRibbons_) {
        ribbons_.Destroy();
        hasRibbons_ = false;
    }
    if (hasParticles_) {
        if (traceRelease) {
            std::fprintf(stderr, "MdxViewer::Release destroy particles begin\n");
            std::fflush(stderr);
        }
        particles_.Destroy();
        hasParticles_ = false;
        if (traceRelease) {
            std::fprintf(stderr, "MdxViewer::Release destroy particles end\n");
            std::fflush(stderr);
        }
    }
    modelData_ = MdxModel{};
    ownsModelData_ = false;
    ownsBatchGpuResources_ = true;
    modelSource_ = nullptr;
    sharedSource_ = nullptr;
    hasAnimations_ = false;
    hasAnimatedMaterials_ = false;
    initializedAnimator_ = false;
    animatedGeosetStates_.clear();
    animatedLayerAlphas_.clear();
    elapsedMs_ = 0.0f;
    pendingStandSequenceAfterCurrent_ = false;
    if (traceRelease) {
        std::fprintf(stderr, "MdxViewer::Release end active name=%s\n", name_.c_str());
        std::fflush(stderr);
    }
}

bool MdxViewer::ShareModelResourcesFrom(const MdxViewer& other) {
    Release();
    if (!other.IsValid()) {
        return false;
    }

    batches_ = other.batches_;
    modelTextures_ = other.modelTextures_;
    for (auto& batch : batches_) {
        batch.playerTexHandle = {0};
        batch.playerTexView = {0};
        batch.cachedPlayer = -1;
    }
    ownsBatchGpuResources_ = false;
    sharedSource_ = nullptr;
    mpq_ = other.mpq_;
    name_ = other.name_;
    center_ = other.center_;
    radius_ = other.radius_;
    hasAnimations_ = other.hasAnimations_;
    hasAnimatedMaterials_ = other.hasAnimatedMaterials_;
    elapsedMs_ = 0.0f;
    modelSource_ = other.modelSource_ ? other.modelSource_ : &other.modelData_;
    if (!modelSource_) {
        return false;
    }

    if (hasAnimations_ || hasAnimatedMaterials_) {
        animator_.Init(*modelSource_);
        initializedAnimator_ = true;
        PlayStandSequence();
        InitializeAnimatedStates();
        UpdateAnimatedStates();
    }

    // Particle systems stay disabled for now; each runtime instance owns only animation state.
    hasParticles_ = false;
    return true;
}

bool MdxViewer::IntersectRayWithCollision(const picking::PickRay& ray,
                                          const Mat4& modelMatrix,
                                          float& outT) const {
    if (!modelSource_ || modelSource_->collisionShapes.empty() || !initializedAnimator_) {
        return false;
    }
    return MdxIntersectCollisionShapes(*modelSource_, animator_, ray, modelMatrix, outT);
}

static sg_image CreateSGImage(int w, int h, const uint8_t* pixels) {
    sg_image_desc d = {};
    d.type = SG_IMAGETYPE_2D;
    d.width = w; d.height = h;
    d.pixel_format = SG_PIXELFORMAT_RGBA8;
    d.num_mipmaps = 0;
    d.data.mip_levels[0].ptr = pixels;
    d.data.mip_levels[0].size = (size_t)(w * h * 4);
    sg_image img = sg_make_image(&d);
    if (sg_query_image_state(img) != SG_RESOURCESTATE_VALID) img = {0};
    return img;
}

static sg_image LoadTextureFromMpq(MpqManager* mpq, const std::string& path) {
    if (!mpq) return {0};
    auto tryLoad = [&](const std::string& p) -> sg_image {
        std::vector<uint8_t> data;
        if (!mpq->ReadFile(p, data)) return {0};
        BLPImage img;
        if (DecodeBLP(data.data(), data.size(), img))
            return CreateSGImage(img.width, img.height, img.pixels.data());
        DdsImage dds;
        if (DecodeDDS(data.data(), data.size(), dds))
            return CreateSGImage(dds.width, dds.height, dds.pixels.data());
        int w, h, ch;
        stbi_uc* pixels = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &ch, STBI_rgb_alpha);
        if (!pixels) return {0};
        sg_image tex = CreateSGImage(w, h, pixels);
        stbi_image_free(pixels);
        return tex;
    };
    sg_image tex = tryLoad(path);
    if (tex.id) return tex;
    if (path.size() > 4) {
        std::string base = path.substr(0, path.size() - 4);
        tex = tryLoad(base + ".tga");
        if (tex.id) return tex;
        tex = tryLoad(base + ".dds");
        if (tex.id) return tex;
    }
    return {0};
}

bool MdxViewer::Load(const MdxModel& model, MpqManager* mpq, bool cliffMode, bool enableParticles) {
    DebugLogState("load-pre-release");
    if (MdxLoadTraceEnabled()) {
        std::fprintf(stderr, "MdxViewer::Load pre-release incoming=%s\n", model.name.c_str());
        std::fflush(stderr);
    }
    Release();
    if (MdxLoadTraceEnabled()) {
        std::fprintf(stderr, "MdxViewer::Load post-release incoming=%s\n", model.name.c_str());
        std::fflush(stderr);
    }
    modelData_ = model;
    ownsModelData_ = true;
    ownsBatchGpuResources_ = true;
    modelSource_ = &modelData_;
    sharedSource_ = nullptr;
    const MdxModel& sourceModel = *modelSource_;
    elapsedMs_ = 0.0f;

    name_ = sourceModel.name;
    const bool logModelDebug = ShouldLogModelDebug(name_);
    const bool traceLoad = MdxLoadTraceEnabled() && g_mdxLoadTraceCount < 4;
    if (traceLoad) {
        ++g_mdxLoadTraceCount;
        std::fprintf(stderr,
            "MdxViewer::Load begin name=%s geosets=%zu bones=%zu helpers=%zu textures=%zu\n",
            name_.c_str(), sourceModel.geosets.size(), sourceModel.bones.size(),
            sourceModel.helpers.size(), sourceModel.textures.size());
    }

    InitSharedShader();
    if (!sharedShader_.id || !sharedPipeline_.id) return false;

    // Many Warcraft III models rely on bones/helpers even without animation sequences.
    // Treat any skeletal hierarchy as needing the animator path so bind-pose transforms
    // are still applied.
    hasAnimations_ = !sourceModel.bones.empty() || !sourceModel.helpers.empty()
        || !sourceModel.particleEmittersPopcorn.empty()
        || !sourceModel.collisionShapes.empty();
    hasAnimatedMaterials_ = ModelHasAnimatedMaterials(sourceModel);

    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.mipmap_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_REPEAT;
    smp.wrap_v = SG_WRAP_REPEAT;

    if (hasAnimations_ || hasAnimatedMaterials_) {
        if (traceLoad) {
            std::fprintf(stderr, "MdxViewer::Load animator init name=%s\n", name_.c_str());
            std::fflush(stderr);
        }
        animator_.Init(*modelSource_);
        if (traceLoad) {
            std::fprintf(stderr, "MdxViewer::Load animator init returned name=%s\n", name_.c_str());
            std::fflush(stderr);
        }
        initializedAnimator_ = true;
        if (traceLoad) {
            std::fprintf(stderr, "MdxViewer::Load play stand name=%s\n", name_.c_str());
            std::fflush(stderr);
        }
        PlayStandSequence();
        if (traceLoad) {
            std::fprintf(stderr, "MdxViewer::Load init animated states name=%s\n", name_.c_str());
            std::fflush(stderr);
        }
        InitializeAnimatedStates();
        if (traceLoad) {
            std::fprintf(stderr, "MdxViewer::Load update animated states name=%s\n", name_.c_str());
            std::fflush(stderr);
        }
        UpdateAnimatedStates();
        if (traceLoad) {
            std::fprintf(stderr, "MdxViewer::Load animator ready name=%s\n", name_.c_str());
            std::fflush(stderr);
        }
    }
    std::unordered_map<int, const MdxNode*> debugNodesByObjectId;
    std::unordered_map<int, std::string> nodeNamesLowerByObjectId;
    for (const auto& bone : sourceModel.bones) {
        nodeNamesLowerByObjectId[bone.objectId] = ToLowerCopy(bone.name);
    }
    for (const auto& helper : sourceModel.helpers) {
        nodeNamesLowerByObjectId[helper.objectId] = ToLowerCopy(helper.name);
    }
    if (logModelDebug) {
        std::cout << "ModelDebug[" << name_ << "]: bones=" << sourceModel.bones.size()
                  << " helpers=" << sourceModel.helpers.size() << std::endl;
        for (const auto& bone : sourceModel.bones) {
            debugNodesByObjectId[bone.objectId] = &bone;
            std::cout << "ModelDebug[" << name_ << "]: bone objectId=" << bone.objectId
                      << " parentId=" << bone.parentId
                      << " flags=0x" << std::hex << bone.flags << std::dec
                      << " geosetId=" << bone.geosetId
                      << " geosetAnimId=" << bone.geosetAnimationId
                      << " name=" << bone.name << std::endl;
        }
        for (const auto& helper : sourceModel.helpers) {
            debugNodesByObjectId[helper.objectId] = &helper;
            std::cout << "ModelDebug[" << name_ << "]: helper objectId=" << helper.objectId
                      << " parentId=" << helper.parentId
                      << " flags=0x" << std::hex << helper.flags << std::dec
                      << " name=" << helper.name << std::endl;
        }
        for (const auto& attachment : sourceModel.attachments) {
            std::cout << "ModelDebug[" << name_ << "]: attachment objectId=" << attachment.objectId
                      << " parentId=" << attachment.parentId
                      << " flags=0x" << std::hex << attachment.flags << std::dec
                      << " attachmentId=" << attachment.attachmentId
                      << " path=" << attachment.path
                      << " name=" << attachment.name << std::endl;
        }
        for (const auto& light : sourceModel.lights) {
            std::cout << "ModelDebug[" << name_ << "]: light objectId=" << light.objectId
                      << " parentId=" << light.parentId
                      << " flags=0x" << std::hex << light.flags << std::dec
                      << " type=" << (int)light.type
                      << " name=" << light.name << std::endl;
        }
        for (const auto& emitter : sourceModel.particleEmitters1) {
            std::cout << "ModelDebug[" << name_ << "]: emitter1 objectId=" << emitter.objectId
                      << " parentId=" << emitter.parentId
                      << " flags=0x" << std::hex << emitter.flags << std::dec
                      << " textureId=" << emitter.textureId
                      << " replaceableId=" << emitter.replaceableId
                      << " name=" << emitter.name << std::endl;
        }
        for (const auto& emitter : sourceModel.particleEmitters2) {
            std::cout << "ModelDebug[" << name_ << "]: emitter2 objectId=" << emitter.objectId
                      << " parentId=" << emitter.parentId
                      << " flags=0x" << std::hex << emitter.flags << std::dec
                      << " textureId=" << emitter.textureId
                      << " replaceableId=" << emitter.replaceableId
                      << " name=" << emitter.name << std::endl;
        }
        for (const auto& ribbon : sourceModel.ribbonEmitters) {
            std::cout << "ModelDebug[" << name_ << "]: ribbon objectId=" << ribbon.objectId
                      << " parentId=" << ribbon.parentId
                      << " flags=0x" << std::hex << ribbon.flags << std::dec
                      << " materialId=" << ribbon.materialId
                      << " name=" << ribbon.name << std::endl;
        }
        for (const auto& eventObject : sourceModel.eventObjects) {
            std::cout << "ModelDebug[" << name_ << "]: event objectId=" << eventObject.objectId
                      << " parentId=" << eventObject.parentId
                      << " flags=0x" << std::hex << eventObject.flags << std::dec
                      << " type=" << eventObject.type
                      << " id=" << eventObject.id
                      << " name=" << eventObject.name << std::endl;
        }
    }

    std::unordered_set<int> portraitOnlyGeosets;
    for (const auto& bone : sourceModel.bones) {
        if (bone.geosetId >= 0 && IsPortraitOnlyNodeName(bone.name)) {
            portraitOnlyGeosets.insert(bone.geosetId);
        }
    }
    for (const auto& helper : sourceModel.helpers) {
        if (helper.geosetId >= 0 && IsPortraitOnlyNodeName(helper.name)) {
            portraitOnlyGeosets.insert(helper.geosetId);
        }
    }

    // Resolve textures for all materials. Batches reference these slots so KMTF
    // material animation can switch to any texture in the model.
    modelTextures_.assign(sourceModel.textures.size(), ModelTextureSlot{});
    if (traceLoad) {
        std::fprintf(stderr, "MdxViewer::Load resolving textures name=%s count=%zu\n",
            name_.c_str(), sourceModel.textures.size());
    }
    for (size_t ti = 0; ti < sourceModel.textures.size(); ti++) {
        auto& texRef = sourceModel.textures[ti];
        auto& cache = modelTextures_[ti];
        if (!texRef.path.empty()) {
            std::string path = texRef.path;
            for (auto& c : path) if (c == '/') c = '\\';
            cache.handle = LoadTextureFromMpq(mpq, path);
        } else if (texRef.replaceableId != 0) {
            cache.replaceableId = texRef.replaceableId;
            const char* rp = ReplaceableIds::GetPathString(texRef.replaceableId);
            if (rp && rp[0]) {
                cache.replaceablePath = rp;
                if (texRef.replaceableId == 1 || texRef.replaceableId == 2) {
                    std::string fp = "ReplaceableTextures\\" + std::string(rp)
                        + ReplaceableIds::GetIdString(0) + ".blp";
                    cache.handle = LoadTextureFromMpq(mpq, fp);
                } else {
                    std::string fp = "ReplaceableTextures\\" + std::string(rp) + ".blp";
                    cache.handle = LoadTextureFromMpq(mpq, fp);
                }
            }
        }
        if (cache.handle.id) {
            sg_view_desc vd = {};
            vd.texture.image = cache.handle;
            cache.view = sg_make_view(&vd);
        }
    }
    if (traceLoad) {
        std::fprintf(stderr, "MdxViewer::Load textures ready name=%s\n", name_.c_str());
    }

    // Create batches for each geoset
    if (traceLoad) {
        std::fprintf(stderr, "MdxViewer::Load geosets begin name=%s count=%zu\n",
            name_.c_str(), sourceModel.geosets.size());
    }
    for (int gi = 0; gi < (int)sourceModel.geosets.size(); gi++) {
        if (portraitOnlyGeosets.find(gi) != portraitOnlyGeosets.end()) continue;
        const auto& geo = sourceModel.geosets[gi];
        if (geo.vertices.empty() || geo.indices.empty()) continue;
        if (logModelDebug) {
            std::cout << "ModelDebug[" << name_ << "]: geoset=" << gi
                      << " vertices=" << (geo.vertices.size() / 3)
                      << " indices=" << geo.indices.size()
                      << " materialId=" << geo.materialId
                      << " vertexGroups=" << geo.vertexGroups.size()
                      << " matrixGroups=" << geo.matrixGroups.size()
                      << " matrixIndices=" << geo.matrixIndices.size()
                      << std::endl;
            if (!geo.matrixIndices.empty()) {
                std::cout << "ModelDebug[" << name_ << "]: geoset=" << gi
                          << " matrixIndexValues=";
                for (size_t mi = 0; mi < geo.matrixIndices.size(); ++mi) {
                    if (mi > 0) {
                        std::cout << ",";
                    }
                    const int objectId = (int)geo.matrixIndices[mi];
                    std::cout << objectId;
                    auto nodeIt = debugNodesByObjectId.find(objectId);
                    if (nodeIt != debugNodesByObjectId.end() && nodeIt->second) {
                        std::cout << "(" << nodeIt->second->name
                                  << ",flags=0x" << std::hex << nodeIt->second->flags << std::dec
                                  << ")";
                    }
                }
                std::cout << std::endl;
            }
            if (geo.materialId >= 0 && geo.materialId < (int)sourceModel.materials.size()) {
                const auto& dbgMaterial = sourceModel.materials[geo.materialId];
                std::cout << "ModelDebug[" << name_ << "]: material=" << geo.materialId
                          << " layers=" << dbgMaterial.layers.size()
                          << " flags=" << dbgMaterial.flags << std::endl;
                for (size_t li = 0; li < dbgMaterial.layers.size(); ++li) {
                    const auto& dbgLayer = dbgMaterial.layers[li];
                    std::cout << "ModelDebug[" << name_ << "]: layer=" << li
                              << " textureId=" << dbgLayer.textureId
                              << " filterMode=" << dbgLayer.filterMode
                              << " flags=" << dbgLayer.flags
                              << " coordId=" << dbgLayer.coordId
                              << " alpha=" << dbgLayer.alpha;
                    if (dbgLayer.textureId >= 0 && dbgLayer.textureId < (int)sourceModel.textures.size()) {
                        const auto& dbgTex = sourceModel.textures[dbgLayer.textureId];
                        std::cout << " texPath=" << dbgTex.path
                                  << " replaceableId=" << dbgTex.replaceableId;
                    }
                    std::cout << std::endl;
                }
            }
        }
        const MdxGeosetAnim* geosetAnim = nullptr;
        for (const auto& ga : sourceModel.geosetAnims) {
            if (ga.geosetId == gi) {
                geosetAnim = &ga;
                break;
            }
        }

        // Determine if this geoset needs skinning (has matrix groups)
        bool isSkinned = !geo.vertexGroups.empty() && !geo.matrixGroups.empty();

        const MdxMaterial* material = nullptr;
        if (geo.materialId >= 0 && geo.materialId < (int)sourceModel.materials.size()) {
            material = &sourceModel.materials[geo.materialId];
        }
        int passCount = (material && !material->layers.empty()) ? (int)material->layers.size() : 1;

        for (int pass = 0; pass < passCount; ++pass) {
            const MdxLayer* layer = (material && !material->layers.empty()) ? &material->layers[pass] : nullptr;

            sg_image texHandle = {0};
            sg_view texView = {0};
            int replaceableId = 0;
            std::string replaceablePath;
            int textureId = -1;
            int textureAnimationId = -1;
            const MdxTimeline* textureIdTimeline = nullptr;
            int coordId = layer ? std::max(0, layer->coordId) : 0;
            if (layer) {
                textureId = layer->textureId;
                textureAnimationId = layer->textureAnimationId;
                textureIdTimeline = FindTimeline(layer->timelines, TAG_KMTF);
                if (textureId >= 0 && textureId < (int)modelTextures_.size()) {
                    texHandle = modelTextures_[textureId].handle;
                    texView = modelTextures_[textureId].view;
                    replaceableId = modelTextures_[textureId].replaceableId;
                    replaceablePath = modelTextures_[textureId].replaceablePath;
                    if (textureId >= 0 && textureId < (int)sourceModel.textures.size()
                        && IsEmbeddedSplatTexturePath(sourceModel.textures[textureId].path)) {
                        continue;
                    }
                }
            }
            const std::vector<float>* uvSource = &geo.uvs;
            if (!geo.uvSets.empty() && coordId >= 0 && coordId < (int)geo.uvSets.size()) {
                uvSource = &geo.uvSets[coordId];
            }
            uint32_t sequenceHintMask = BatchSequenceHint_None;
            if (layer && layer->textureId >= 0 && layer->textureId < (int)sourceModel.textures.size()) {
                AppendSequenceHintsFromTexturePath(sequenceHintMask, ToLowerCopy(sourceModel.textures[layer->textureId].path));
            }
            for (uint16_t objectId : geo.matrixIndices) {
                const auto nodeNameIt = nodeNamesLowerByObjectId.find((int)objectId);
                if (nodeNameIt != nodeNamesLowerByObjectId.end()) {
                    AppendSequenceHintsFromNodeName(sequenceHintMask, nodeNameIt->second);
                }
            }

            if (isSkinned && hasAnimations_) {
                BuildSkinnedBatch(geo, gi, uvSource, sourceModel, mpq);
                if (!batches_.empty()) {
                    auto& batch = batches_.back();
                    batch.textureHandle = texHandle;
                    batch.textureView = texView;
                    batch.hasTexture = (texHandle.id != 0);
                    batch.textureId = textureId;
                    batch.textureAnimationId = textureAnimationId;
                    if (textureIdTimeline) {
                        batch.textureIdTimeline = *textureIdTimeline;
                        batch.hasTextureIdTimeline = true;
                    }
                    batch.replaceableId = replaceableId;
                    batch.replaceablePath = replaceablePath;
                    batch.sampler = sg_make_sampler(&smp);
                    batch.materialId = geo.materialId;
                    batch.geosetIndex = gi;
                    batch.materialLayerIndex = pass;
                    if (layer) {
                        batch.filterMode = layer->filterMode;
                        batch.layerFlags = layer->flags;
                        batch.layerAlpha = layer->alpha;
                    }
                    batch.sequenceHintMask = sequenceHintMask;
                    if (geosetAnim) {
                        batch.geosetAlpha = geosetAnim->alpha;
                        batch.geosetColor[0] = geosetAnim->color[0];
                        batch.geosetColor[1] = geosetAnim->color[1];
                        batch.geosetColor[2] = geosetAnim->color[2];
                    }
                }
            } else {
                int vc = (int)geo.vertices.size() / 3;
                std::vector<VertexStatic> verts(vc);
                for (int i = 0; i < vc; i++) {
                    float mx = geo.vertices[i*3];
                    float my = geo.vertices[i*3+1];
                    float mz = geo.vertices[i*3+2];
                    if (cliffMode) {
                        verts[i].px = my;  verts[i].py = mz;  verts[i].pz = -mx;
                        float nnx = geo.normals.empty() ? 0 : geo.normals[i*3];
                        float nny = geo.normals.empty() ? 0 : geo.normals[i*3+1];
                        float nnz = geo.normals.empty() ? 0 : geo.normals[i*3+2];
                        verts[i].nx = nny; verts[i].ny = nnz; verts[i].nz = -nnx;
                    } else {
                        verts[i].px = mx;  verts[i].py = mz;  verts[i].pz = -my;
                        if (!geo.normals.empty()) {
                            verts[i].nx = geo.normals[i*3];
                            verts[i].ny = geo.normals[i*3+2];
                            verts[i].nz = -geo.normals[i*3+1];
                        } else { verts[i].nx = 0; verts[i].ny = 1; verts[i].nz = 0; }
                    }
                    if (uvSource && uvSource->size() >= size_t(i * 2 + 2)) {
                        verts[i].tu = (*uvSource)[i*2];
                        verts[i].tv = (*uvSource)[i*2+1];
                    } else { verts[i].tu = 0; verts[i].tv = 0; }
                }

                bool invalidGeometry = false;
                const char* invalidReason = "";
                float maxAbsCoord = 0.0f;
                for (const auto& v : verts) {
                    if (!std::isfinite(v.px) || !std::isfinite(v.py) || !std::isfinite(v.pz)) {
                        invalidGeometry = true;
                        invalidReason = "non-finite vertex";
                        break;
                    }
                    maxAbsCoord = std::max(maxAbsCoord, std::fabs(v.px));
                    maxAbsCoord = std::max(maxAbsCoord, std::fabs(v.py));
                    maxAbsCoord = std::max(maxAbsCoord, std::fabs(v.pz));
                }
                if (!invalidGeometry && maxAbsCoord > 100000.0f) {
                    invalidGeometry = true;
                    invalidReason = "extreme vertex";
                }
                if (!invalidGeometry) {
                    for (unsigned index : geo.indices) {
                        if (index >= (unsigned)vc) {
                            invalidGeometry = true;
                            invalidReason = "index out of range";
                            break;
                        }
                    }
                }
                if (invalidGeometry) {
                    std::cerr << "MdxViewer: skipped invalid static batch model='" << name_
                              << "' geoset=" << gi
                              << " pass=" << pass
                              << " reason=" << invalidReason
                              << " vertices=" << vc
                              << " indices=" << geo.indices.size()
                              << " maxAbs=" << maxAbsCoord
                              << std::endl;
                    continue;
                }

                Batch batch;
                batch.indexCount = (int)geo.indices.size();
                batch.textureHandle = texHandle;
                batch.textureView = texView;
                batch.hasTexture = (texHandle.id != 0);
                batch.textureId = textureId;
                batch.textureAnimationId = textureAnimationId;
                if (textureIdTimeline) {
                    batch.textureIdTimeline = *textureIdTimeline;
                    batch.hasTextureIdTimeline = true;
                }
                batch.replaceableId = replaceableId;
                batch.replaceablePath = replaceablePath;
                batch.sampler = sg_make_sampler(&smp);
                batch.skinned = false;
                batch.materialId = geo.materialId;
                batch.geosetIndex = gi;
                batch.materialLayerIndex = pass;
                if (layer) {
                    batch.filterMode = layer->filterMode;
                    batch.layerFlags = layer->flags;
                    batch.layerAlpha = layer->alpha;
                }
                batch.sequenceHintMask = sequenceHintMask;
                if (geosetAnim) {
                    batch.geosetAlpha = geosetAnim->alpha;
                    batch.geosetColor[0] = geosetAnim->color[0];
                    batch.geosetColor[1] = geosetAnim->color[1];
                    batch.geosetColor[2] = geosetAnim->color[2];
                }

                sg_buffer_desc vbd = {};
                vbd.usage.vertex_buffer = true; vbd.usage.immutable = true;
                vbd.data.ptr = verts.data(); vbd.data.size = verts.size() * sizeof(VertexStatic);
                batch.vbo = sg_make_buffer(&vbd);

                sg_buffer_desc ibd = {};
                ibd.usage.index_buffer = true; ibd.usage.immutable = true;
                ibd.data.ptr = geo.indices.data(); ibd.data.size = geo.indices.size() * sizeof(unsigned);
                batch.ebo = sg_make_buffer(&ibd);

                batches_.push_back(batch);
            }
        }
    }
    if (traceLoad) {
        std::fprintf(stderr, "MdxViewer::Load geosets ready name=%s batches=%zu\n",
            name_.c_str(), batches_.size());
    }

    mpq_ = mpq;
    center_ = {
        (sourceModel.boundsMin[0] + sourceModel.boundsMax[0]) * 0.5f,
        (sourceModel.boundsMin[2] + sourceModel.boundsMax[2]) * 0.5f,
        -(sourceModel.boundsMin[1] + sourceModel.boundsMax[1]) * 0.5f
    };
    radius_ = sourceModel.boundsRadius > 0 ? sourceModel.boundsRadius : 100.0f;

    hasParticleEmitter1_ = false;
    particleEmitter1Spawns_.clear();
    particleEmitter1States_.clear();
    if (enableParticles && !sourceModel.particleEmitters1.empty()) {
        MdxResetParticleEmitter1RuntimeStates(sourceModel, particleEmitter1States_);
        hasParticleEmitter1_ = true;
    }

    if (enableParticles && !sourceModel.particleEmitters2.empty()) {
        particles_.Init(sourceModel, mpq);
        particles_.SetWorldOffset(Vec3{0.0f, 0.0f, 0.0f});
        hasParticles_ = true;
    } else {
        hasParticles_ = false;
    }

    if (enableParticles && !sourceModel.ribbonEmitters.empty()) {
        ribbons_.Init(sourceModel, mpq);
        ribbons_.SetWorldOffset(Vec3{0.0f, 0.0f, 0.0f});
        hasRibbons_ = true;
    } else {
        hasRibbons_ = false;
    }

    triggeredEventCount_ = 0;
    resolvedEventEffectCount_ = 0;
    resolvedEventEffects_.clear();
    eventEffectResolverLoaded_ = false;
    if (mpq && !sourceModel.eventObjects.empty()) {
        eventEffectResolverLoaded_ = eventEffectResolver_.Load(*mpq);
    }

    std::cout << "MdxViewer: " << name_ << " loaded (batches=" << batches_.size()
              << ", animated=" << hasAnimations_ << ", particles=" << hasParticles_ << ")" << std::endl;
    if (ToLowerCopy(name_).find("peasant") != std::string::npos) {
        std::cout << "WalkDiag[Model]: " << name_ << std::endl;
        for (size_t i = 0; i < sourceModel.sequences.size(); ++i) {
            const auto& seq = sourceModel.sequences[i];
            const std::string lowerName = ToLowerCopy(seq.name);
            if (lowerName.find("walk") != std::string::npos) {
                std::cout << "  seq[" << i << "] name=" << seq.name
                          << " moveSpeed=" << seq.moveSpeed
                          << " flags=" << seq.flags
                          << std::endl;
            }
        }
    }
    if (traceLoad) {
        std::fprintf(stderr, "MdxViewer::Load end name=%s\n", name_.c_str());
        std::fflush(stderr);
    }
    return !batches_.empty();
}

bool MdxViewer::PlayStandSequence() {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int defaultSequenceIndex = FindBestDefaultStandSequenceIndex(*modelSource_);
    if (defaultSequenceIndex < 0) {
        return false;
    }
    return SetSequencePreservingWalk(defaultSequenceIndex, true, 1.0f);
}

bool MdxViewer::PlayStandSequenceWithTags(const std::vector<std::string>& secondaryTags) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int sequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "stand", secondaryTags);
    if (sequenceIndex < 0) {
        return false;
    }
    return SetSequencePreservingWalk(sequenceIndex, true, 1.0f);
}

bool MdxViewer::PlayStandSequenceWithTag(const char* secondaryTag) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return PlayStandSequenceWithTags(tags);
}

bool MdxViewer::PlayWalkSequence() {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int walkSequenceIndex = FindBestWalkSequenceIndex(*modelSource_);
    if (walkSequenceIndex < 0) {
        return false;
    }
    return SetSequencePreservingWalk(walkSequenceIndex, true, 1.0f);
}

bool MdxViewer::PlayWalkSequenceWithTags(const std::vector<std::string>& secondaryTags) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int walkSequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "walk", secondaryTags);
    if (walkSequenceIndex < 0) {
        return false;
    }
    return SetSequencePreservingWalk(walkSequenceIndex, true, 1.0f);
}

bool MdxViewer::PlayWalkSequenceWithTag(const char* secondaryTag) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return PlayWalkSequenceWithTags(tags);
}

bool MdxViewer::PlayWalkSequenceAtSpeed(float currentMovementSpeed) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int walkSequenceIndex = FindBestWalkSequenceIndex(*modelSource_);
    if (walkSequenceIndex < 0) {
        return false;
    }
    const float speedRatio = ComputeWalkSequenceSpeedRatio(*modelSource_, walkSequenceIndex,
        currentMovementSpeed, animationSpeedScale_);
    return SetSequencePreservingWalk(walkSequenceIndex, true, speedRatio);
}

bool MdxViewer::PlayWalkSequenceWithTagsAtSpeed(const std::vector<std::string>& secondaryTags,
                                                float currentMovementSpeed) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int walkSequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "walk", secondaryTags);
    if (walkSequenceIndex < 0) {
        return false;
    }
    const float speedRatio = ComputeWalkSequenceSpeedRatio(*modelSource_, walkSequenceIndex,
        currentMovementSpeed, animationSpeedScale_);
    return SetSequencePreservingWalk(walkSequenceIndex, true, speedRatio);
}

bool MdxViewer::PlayWalkSequenceWithTagAtSpeed(const char* secondaryTag, float currentMovementSpeed) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return PlayWalkSequenceWithTagsAtSpeed(tags, currentMovementSpeed);
}

bool MdxViewer::PlayWalkSequenceAtSpeedRatio(float speedRatio) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int walkSequenceIndex = FindBestWalkSequenceIndex(*modelSource_);
    if (walkSequenceIndex < 0) {
        return false;
    }
    return SetSequencePreservingWalk(walkSequenceIndex, true, std::clamp(speedRatio, 0.1f, 4.0f));
}

bool MdxViewer::PlayWalkSequenceWithTagsAtSpeedRatio(const std::vector<std::string>& secondaryTags,
                                                     float speedRatio) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int walkSequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "walk", secondaryTags);
    if (walkSequenceIndex < 0) {
        return false;
    }
    return SetSequencePreservingWalk(walkSequenceIndex, true, std::clamp(speedRatio, 0.1f, 4.0f));
}

bool MdxViewer::PlayWalkSequenceWithTagAtSpeedRatio(const char* secondaryTag, float speedRatio) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return PlayWalkSequenceWithTagsAtSpeedRatio(tags, speedRatio);
}

bool MdxViewer::PlayAttackSequence() {
    return PlayAttackSequenceWithTags({});
}

bool MdxViewer::PlayAttackSequenceWithTags(const std::vector<std::string>& secondaryTags) {
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int attackSequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "attack", secondaryTags);
    if (attackSequenceIndex < 0) {
        return false;
    }
    pendingStandSequenceAfterCurrent_ = true;
    return SetSequencePreservingWalk(attackSequenceIndex, false, 1.0f);
}

bool MdxViewer::PlayAttackSequenceWithDuration(float durationSeconds) {
    return PlayAttackSequenceWithTagsAndDuration({}, durationSeconds);
}

bool MdxViewer::PlayAttackSequenceWithTagsAndDuration(const std::vector<std::string>& secondaryTags,
                                                      float durationSeconds) {
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int attackSequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "attack", secondaryTags);
    if (attackSequenceIndex < 0) {
        return false;
    }
    float speedRatio = 1.0f;
    if (durationSeconds > 0.0f) {
        const auto& seq = modelSource_->sequences[attackSequenceIndex];
        const float sequenceDurationSeconds = std::max(0.001f, (float)(seq.endFrame - seq.startFrame) / 1000.0f);
        speedRatio = std::clamp(sequenceDurationSeconds / durationSeconds, 0.1f, 4.0f);
    }
    pendingStandSequenceAfterCurrent_ = true;
    return SetSequencePreservingWalk(attackSequenceIndex, false, speedRatio);
}

bool MdxViewer::PlaySpellSequence(float durationSeconds) {
    return PlaySpellSequenceWithTags({}, durationSeconds);
}

bool MdxViewer::PlaySpellSequenceWithTags(const std::vector<std::string>& secondaryTags, float durationSeconds) {
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int sequenceIndex = MdxSelectSpellSequenceIndex(*modelSource_, secondaryTags);
    if (sequenceIndex < 0) {
        return false;
    }
    const float speedRatio = ComputeSequenceDurationSpeedRatio(*modelSource_, sequenceIndex, durationSeconds);
    pendingStandSequenceAfterCurrent_ = true;
    return SetSequencePreservingWalk(sequenceIndex, false, speedRatio);
}

bool MdxViewer::PlaySpellSequenceWithTag(const char* secondaryTag, float durationSeconds) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return PlaySpellSequenceWithTags(tags, durationSeconds);
}

bool MdxViewer::PlayChannelSequence(float durationSeconds) {
    return PlayChannelSequenceWithTags({}, durationSeconds);
}

bool MdxViewer::PlayChannelSequenceWithTags(const std::vector<std::string>& secondaryTags, float durationSeconds) {
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    int sequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "channel", secondaryTags);
    if (sequenceIndex < 0) {
        sequenceIndex = FindBestSequenceIndexByPrimaryAndTags(*modelSource_, "spell", secondaryTags);
    }
    if (sequenceIndex < 0) {
        return false;
    }
    const float speedRatio = ComputeSequenceDurationSpeedRatio(*modelSource_, sequenceIndex, durationSeconds);
    pendingStandSequenceAfterCurrent_ = false;
    return SetSequencePreservingWalk(sequenceIndex, false, speedRatio);
}

bool MdxViewer::PlayChannelSequenceWithTag(const char* secondaryTag, float durationSeconds) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return PlayChannelSequenceWithTags(tags, durationSeconds);
}

bool MdxViewer::PlayBirthThenStandSequence() {
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int birthSequenceIndex = FindBestBirthSequenceIndex(*modelSource_);
    if (birthSequenceIndex < 0) {
        return PlayStandSequence();
    }
    pendingStandSequenceAfterCurrent_ = SetSequencePreservingWalk(birthSequenceIndex, false, 1.0f);
    return pendingStandSequenceAfterCurrent_;
}

bool MdxViewer::PlayDeathSequence() {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int deathSequenceIndex = FindBestDeathSequenceIndex(*modelSource_);
    if (deathSequenceIndex < 0) {
        return false;
    }
    return SetSequencePreservingWalk(deathSequenceIndex, false, 1.0f);
}

bool MdxViewer::PlayDecaySequenceWithTags(const std::vector<std::string>& secondaryTags, float durationSeconds) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    std::vector<std::string> tags = secondaryTags;
    const char* tagPtr = nullptr;
    if (!tags.empty()) {
        tagPtr = tags.front().c_str();
    }
    const int sequenceIndex = FindBestDecaySequenceIndex(*modelSource_, tagPtr);
    if (sequenceIndex < 0) {
        return false;
    }
    const float speedRatio = ComputeSequenceDurationSpeedRatio(*modelSource_, sequenceIndex, durationSeconds);
    return SetSequencePreservingWalk(sequenceIndex, false, speedRatio);
}

bool MdxViewer::PlayDecaySequenceWithTag(const char* secondaryTag, float durationSeconds) {
    std::vector<std::string> tags;
    if (secondaryTag && secondaryTag[0]) {
        tags.push_back(secondaryTag);
    }
    return PlayDecaySequenceWithTags(tags, durationSeconds);
}

bool MdxViewer::PlayDissipateSequence(float durationSeconds) {
    pendingStandSequenceAfterCurrent_ = false;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int sequenceIndex = FindBestDissipateSequenceIndex(*modelSource_);
    if (sequenceIndex < 0) {
        return false;
    }
    const float speedRatio = ComputeSequenceDurationSpeedRatio(*modelSource_, sequenceIndex, durationSeconds);
    return SetSequencePreservingWalk(sequenceIndex, false, speedRatio);
}

bool MdxViewer::SetSequencePreservingWalk(int sequenceIndex, bool loop, float speed) {
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    if (sequenceIndex < 0 || sequenceIndex >= (int)modelSource_->sequences.size()) {
        return false;
    }

    const bool nextIsWalk = SequenceHasPrimaryToken(modelSource_->sequences[(size_t)sequenceIndex], "walk");
    if (currentSequenceIsWalk_ && !nextIsWalk) {
        lastWalkSequenceProgress_ = animator_.GetSequenceProgress();
    }

    const bool played = animator_.SetSequence(sequenceIndex, loop, speed);
    if (!played) {
        return false;
    }

    if (nextIsWalk && !currentSequenceIsWalk_ && lastWalkSequenceProgress_ >= 0.0f) {
        animator_.SetFrameByRatio(std::clamp(lastWalkSequenceProgress_, 0.0f, 1.0f));
    }
    currentSequenceIsWalk_ = nextIsWalk;
    return true;
}

bool MdxViewer::PlaySequenceByName(const char* sequenceName, bool loop, float speed,
                                   bool queueStandAfterCurrent) {
    pendingStandSequenceAfterCurrent_ = queueStandAfterCurrent && !loop;
    if (!initializedAnimator_ || !sequenceName || !sequenceName[0]) {
        return false;
    }
    const bool played = animator_.PlaySequence(sequenceName, loop, speed);
    if (!played) {
        pendingStandSequenceAfterCurrent_ = false;
    }
    return played;
}

bool MdxViewer::PlaySequenceByIndex(int index, bool loop, float speed, bool queueStandAfterCurrent) {
    pendingStandSequenceAfterCurrent_ = queueStandAfterCurrent && !loop;
    if (!initializedAnimator_) {
        return false;
    }
    const bool played = SetSequencePreservingWalk(index, loop, speed);
    if (!played) {
        pendingStandSequenceAfterCurrent_ = false;
    }
    return played;
}

bool MdxViewer::PlaySequenceByPrimaryAndTags(const char* primaryTag,
                                             const std::vector<std::string>& secondaryTags,
                                             bool loop,
                                             float speed,
                                             bool queueStandAfterCurrent,
                                             bool restartIfSame) {
    pendingStandSequenceAfterCurrent_ = queueStandAfterCurrent && !loop;
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }

    const int sequenceIndex = MdxSelectSequenceIndexByPrimaryAndTags(
        *modelSource_, (primaryTag && primaryTag[0]) ? primaryTag : nullptr, secondaryTags);
    if (sequenceIndex < 0) {
        pendingStandSequenceAfterCurrent_ = false;
        return false;
    }
    if (!restartIfSame && animator_.GetCurrentSequence() == sequenceIndex) {
        return true;
    }

    const bool played = SetSequencePreservingWalk(sequenceIndex, loop, speed);
    if (!played) {
        pendingStandSequenceAfterCurrent_ = false;
    }
    return played;
}

bool MdxViewer::IsPlayingSequenceByPrimaryAndTags(const char* primaryTag,
                                                  const std::vector<std::string>& secondaryTags) const {
    if (!initializedAnimator_ || !modelSource_) {
        return false;
    }
    const int sequenceIndex = MdxSelectSequenceIndexByPrimaryAndTags(
        *modelSource_, (primaryTag && primaryTag[0]) ? primaryTag : nullptr, secondaryTags);
    return sequenceIndex >= 0 && animator_.GetCurrentSequence() == sequenceIndex;
}

bool MdxViewer::SetCurrentSequenceFrameByRatio(float ratio) {
    if (!initializedAnimator_) {
        return false;
    }
    return animator_.SetFrameByRatio(ratio);
}

bool MdxViewer::IsPlayingSequenceWithToken(const char* token) const {
    if (!initializedAnimator_ || !modelSource_ || !token) {
        return false;
    }
    const int sequenceIndex = animator_.GetCurrentSequence();
    if (sequenceIndex < 0 || sequenceIndex >= (int)modelSource_->sequences.size()) {
        return false;
    }
    return SequenceNameContains(ToLowerCopy(modelSource_->sequences[sequenceIndex].name), token);
}

bool MdxViewer::IsCurrentSequenceFinished() const {
    return initializedAnimator_ && animator_.IsSequenceFinished();
}

bool MdxViewer::IsCurrentSequenceLooping() const {
    return initializedAnimator_ && animator_.IsSequenceLooping();
}

bool MdxViewer::GetSequenceName(int index, std::string& outName) const {
    outName.clear();
    if (!modelSource_ || index < 0 || index >= (int)modelSource_->sequences.size()) {
        return false;
    }
    outName = modelSource_->sequences[index].name;
    return true;
}

float MdxViewer::GetSequenceDurationSeconds(int index) const {
    return modelSource_ ? MdxSequenceDurationSeconds(*modelSource_, index) : 0.0f;
}

void MdxViewer::BuildSkinnedBatch(const MdxGeoset& geo, int geosetIdx,
                                   const std::vector<float>* uvSource,
                                   const MdxModel& model, MpqManager* mpq) {
    (void)model; (void)mpq;
    (void)geosetIdx;

    std::vector<MdxSkinnedVertex> verts;
    if (!MdxBuildSkinnedVertices(
            geo,
            uvSource,
            [this](int objectId) { return animator_.GetBoneIndexForObjectId(objectId); },
            verts,
            nullptr)) {
        return;
    }
    const int vc = (int)verts.size();

    bool invalidGeometry = false;
    const char* invalidReason = "";
    float maxAbsCoord = 0.0f;
    for (const auto& v : verts) {
        if (!std::isfinite(v.px) || !std::isfinite(v.py) || !std::isfinite(v.pz)) {
            invalidGeometry = true;
            invalidReason = "non-finite vertex";
            break;
        }
        maxAbsCoord = std::max(maxAbsCoord, std::fabs(v.px));
        maxAbsCoord = std::max(maxAbsCoord, std::fabs(v.py));
        maxAbsCoord = std::max(maxAbsCoord, std::fabs(v.pz));
    }
    if (!invalidGeometry && maxAbsCoord > 100000.0f) {
        invalidGeometry = true;
        invalidReason = "extreme vertex";
    }
    if (!invalidGeometry) {
        for (unsigned index : geo.indices) {
            if (index >= (unsigned)vc) {
                invalidGeometry = true;
                invalidReason = "index out of range";
                break;
            }
        }
    }
    if (invalidGeometry) {
        std::cerr << "MdxViewer: skipped invalid skinned batch model='" << name_
                  << "' geoset=" << geosetIdx
                  << " reason=" << invalidReason
                  << " vertices=" << vc
                  << " indices=" << geo.indices.size()
                  << " maxAbs=" << maxAbsCoord
                  << std::endl;
        return;
    }

    Batch batch;
    batch.indexCount = (int)geo.indices.size();
    batch.skinned = true;
    batch.materialId = geo.materialId;

    sg_buffer_desc vbd = {};
    vbd.usage.vertex_buffer = true; vbd.usage.immutable = true;
    vbd.data.ptr = verts.data(); vbd.data.size = verts.size() * sizeof(MdxSkinnedVertex);
    batch.vbo = sg_make_buffer(&vbd);

    sg_buffer_desc ibd = {};
    ibd.usage.index_buffer = true; ibd.usage.immutable = true;
    ibd.data.ptr = geo.indices.data(); ibd.data.size = geo.indices.size() * sizeof(unsigned);
    batch.ebo = sg_make_buffer(&ibd);

    batches_.push_back(batch);
}

void MdxViewer::Update(float dt) {
    elapsedMs_ += std::max(0.0f, dt) * 1000.0f;
    particleEmitter1Spawns_.clear();
    if (initializedAnimator_) {
        animator_.Update(dt);
        ProcessTriggeredEvents();
        if (pendingStandSequenceAfterCurrent_ && animator_.IsSequenceFinished()) {
            PlayStandSequence();
        }
    }
    if (hasAnimatedMaterials_) {
        UpdateAnimatedStates();
    }
    if (hasParticles_)
        particles_.Update(dt, particles_.GetWorldOffset(), modelSource_,
                          initializedAnimator_ ? &animator_ : nullptr,
                          initializedAnimator_ ? animator_.GetCurrentFrame() : 0,
                          elapsedMs_);
    if (hasRibbons_)
        ribbons_.Update(dt, ribbons_.GetWorldOffset(), modelSource_,
                        initializedAnimator_ ? &animator_ : nullptr,
                        initializedAnimator_ ? animator_.GetCurrentFrame() : 0,
                        elapsedMs_);
    if (hasParticleEmitter1_ && modelSource_) {
        MdxCollectParticleEmitter1Spawns(*modelSource_,
                                         initializedAnimator_ ? &animator_ : nullptr,
                                         initializedAnimator_ ? animator_.GetCurrentFrame() : 0,
                                         elapsedMs_, dt,
                                         particleEmitter1States_,
                                         particleEmitter1Spawns_);
    }
}

void MdxViewer::Update(float dt, const Mat4& view) {
    elapsedMs_ += std::max(0.0f, dt) * 1000.0f;
    particleEmitter1Spawns_.clear();
    if (hasAnimations_) {
        Vec3 cameraForward = {-view.m[2], -view.m[6], -view.m[10]};
        animator_.SetCameraForward(cameraForward);
    }
    if (initializedAnimator_) {
        animator_.Update(dt);
        ProcessTriggeredEvents();
        if (pendingStandSequenceAfterCurrent_ && animator_.IsSequenceFinished()) {
            PlayStandSequence();
        }
    }
    if (hasAnimatedMaterials_) {
        UpdateAnimatedStates();
    }
    if (hasParticles_)
        particles_.Update(dt, particles_.GetWorldOffset(), modelSource_,
                          initializedAnimator_ ? &animator_ : nullptr,
                          initializedAnimator_ ? animator_.GetCurrentFrame() : 0,
                          elapsedMs_);
    if (hasRibbons_)
        ribbons_.Update(dt, ribbons_.GetWorldOffset(), modelSource_,
                        initializedAnimator_ ? &animator_ : nullptr,
                        initializedAnimator_ ? animator_.GetCurrentFrame() : 0,
                        elapsedMs_);
    if (hasParticleEmitter1_ && modelSource_) {
        MdxCollectParticleEmitter1Spawns(*modelSource_,
                                         initializedAnimator_ ? &animator_ : nullptr,
                                         initializedAnimator_ ? animator_.GetCurrentFrame() : 0,
                                         elapsedMs_, dt,
                                         particleEmitter1States_,
                                         particleEmitter1Spawns_);
    }
}

void MdxViewer::ProcessTriggeredEvents() {
    resolvedEventEffects_.clear();
    const std::vector<MdxEventTrigger>& triggeredEvents = animator_.GetTriggeredEvents();
    if (triggeredEvents.empty()) {
        return;
    }

    triggeredEventCount_ += (int)triggeredEvents.size();
    if (!eventEffectResolverLoaded_) {
        return;
    }

    for (const MdxEventTrigger& trigger : triggeredEvents) {
        MdxEventEffect effect = eventEffectResolver_.Resolve(trigger);
        if (effect.resolved) {
            effect.hasLocalTransform = trigger.hasLocalTransform;
            effect.localTransform = trigger.localTransform;
            effect.localPosition = trigger.localPosition;
            effect.localScale = trigger.localScale;
            ++resolvedEventEffectCount_;
            resolvedEventEffects_.push_back(std::move(effect));
        }
    }
}

void MdxViewer::InitializeAnimatedStates() {
    if (!modelSource_) {
        animatedGeosetStates_.clear();
        animatedLayerAlphas_.clear();
        return;
    }

    animatedGeosetStates_.assign(modelSource_->geosets.size(), AnimatedGeosetState{});
    for (size_t gi = 0; gi < modelSource_->geosets.size(); ++gi) {
        auto& state = animatedGeosetStates_[gi];
        for (const auto& geosetAnim : modelSource_->geosetAnims) {
            if (geosetAnim.geosetId == (int)gi) {
                state.alpha = geosetAnim.alpha;
                state.color[0] = geosetAnim.color[0];
                state.color[1] = geosetAnim.color[1];
                state.color[2] = geosetAnim.color[2];
                break;
            }
        }
    }

    animatedLayerAlphas_.clear();
    animatedLayerAlphas_.reserve(modelSource_->materials.size());
    for (const auto& material : modelSource_->materials) {
        std::vector<float> materialLayers(material.layers.size(), 1.0f);
        for (size_t li = 0; li < material.layers.size(); ++li) {
            materialLayers[li] = material.layers[li].alpha;
        }
        animatedLayerAlphas_.push_back(std::move(materialLayers));
    }
}

void MdxViewer::UpdateAnimatedStates() {
    if (!modelSource_ || modelSource_->sequences.empty()) {
        return;
    }

    if (animatedGeosetStates_.size() != modelSource_->geosets.size()
        || animatedLayerAlphas_.size() != modelSource_->materials.size()) {
        InitializeAnimatedStates();
    }

    const uint32_t frame = animator_.GetCurrentFrame();
    const int currentSequence = initializedAnimator_ ? animator_.GetCurrentSequence() : -1;
    auto timelineAppliesToCurrentSequence = [&](const MdxTimeline& timeline) {
        if (timeline.globalSequenceId >= 0 || !modelSource_
            || currentSequence < 0 || currentSequence >= (int)modelSource_->sequences.size()) {
            return true;
        }
        const auto& sequence = modelSource_->sequences[(size_t)currentSequence];
        for (uint32_t keyFrame : timeline.frames) {
            if (keyFrame >= sequence.startFrame && keyFrame <= sequence.endFrame) {
                return true;
            }
        }
        return false;
    };

    for (size_t gi = 0; gi < modelSource_->geosets.size(); ++gi) {
        auto& state = animatedGeosetStates_[gi];
        state.alpha = 1.0f;
        state.color[0] = 1.0f;
        state.color[1] = 1.0f;
        state.color[2] = 1.0f;

        for (const auto& geosetAnim : modelSource_->geosetAnims) {
            if (geosetAnim.geosetId != (int)gi) {
                continue;
            }
            state.alpha = geosetAnim.alpha;
            state.color[0] = geosetAnim.color[0];
            state.color[1] = geosetAnim.color[1];
            state.color[2] = geosetAnim.color[2];

            float sampled[4] = {};
            for (const auto& timeline : geosetAnim.timelines) {
                if (!timelineAppliesToCurrentSequence(timeline)) {
                    continue;
                }
                if (timeline.tag == TAG_KGAO
                    && SampleTimelineForModel(timeline, modelSource_, frame, elapsedMs_, sampled, 1,
                                              geosetAnim.alpha, currentSequence)) {
                    state.alpha = sampled[0];
                }
                else if (timeline.tag == TAG_KGAC
                    && SampleTimelineForModel(timeline, modelSource_, frame, elapsedMs_, sampled, 3,
                                              1.0f, currentSequence)) {
                    MdxConvertAnimatedColorBgrToRgb(sampled, state.color);
                }
            }
            break;
        }
    }

    for (size_t mi = 0; mi < modelSource_->materials.size(); ++mi) {
        const auto& material = modelSource_->materials[mi];
        auto& materialLayers = animatedLayerAlphas_[mi];
        if (materialLayers.size() != material.layers.size()) {
            materialLayers.assign(material.layers.size(), 1.0f);
        }
        for (size_t li = 0; li < material.layers.size(); ++li) {
            const auto& layer = material.layers[li];
            materialLayers[li] = layer.alpha;
            float sampled = layer.alpha;
            for (const auto& timeline : layer.timelines) {
                if (!timelineAppliesToCurrentSequence(timeline)) {
                    continue;
                }
                if (timeline.tag == TAG_KMTA
                    && SampleTimelineForModel(timeline, modelSource_, frame, elapsedMs_, &sampled, 1,
                                              layer.alpha, currentSequence)) {
                    materialLayers[li] = sampled;
                    break;
                }
            }
        }
    }
}

void MdxViewer::Render(const Mat4& view, const Mat4& proj) {
    RenderAt(center_, view, proj, 1.0f);
}

void MdxViewer::RenderAt(const Vec3& pos, const Mat4& view, const Mat4& proj, float scale,
                         int player, float yawRadians, float alpha) {
    RenderAt(pos, view, proj, scale, player, yawRadians, alpha, true);
}

sg_image MdxViewer::GetPlayerTexture(Batch& batch, int player) {
    if (player == batch.cachedPlayer && batch.playerTexHandle.id)
        return batch.playerTexHandle;
    if (batch.replaceablePath.empty()) return {0};
    if (player < 0) player = 0;
    if (player > 23) player = 23;
    if (batch.playerTexView.id) {
        sg_destroy_view(batch.playerTexView);
        batch.playerTexView = {0};
    }
    if (batch.playerTexHandle.id) {
        sg_destroy_image(batch.playerTexHandle);
        batch.playerTexHandle = {0};
    }
    std::string fullPath = "ReplaceableTextures\\" + batch.replaceablePath
        + ReplaceableIds::GetIdString(player) + ".blp";
    sg_image tex = LoadTextureFromMpq(mpq_, fullPath);
    if (tex.id) {
        sg_view_desc vd = {};
        vd.texture.image = tex;
        batch.playerTexView = sg_make_view(&vd);
    }
    batch.playerTexHandle = tex;
    batch.cachedPlayer = player;
    return tex;
}

void MdxViewer::RenderAt(const Vec3& pos, const Mat4& view, const Mat4& proj, float scale,
                         int player, float yawRadians, float alpha, bool depthTest) {
    RenderAtScale(pos, view, proj, Vec3(scale, scale, scale), player, yawRadians, alpha, depthTest);
}

void MdxViewer::RenderAtScale(const Vec3& pos, const Mat4& view, const Mat4& proj, const Vec3& scale,
                              int player, float yawRadians, float alpha, bool depthTest) {
    Mat4 model = Mat4::Identity();
    float c = std::cos(yawRadians), s = std::sin(yawRadians);
    model.m[0] = c * scale.x;  model.m[2] = -s * scale.x;
    model.m[5] = scale.y;
    model.m[8] = s * scale.z;  model.m[10] = c * scale.z;
    model.m[12] = pos.x; model.m[13] = pos.y; model.m[14] = pos.z;
    RenderWithModelMatrix(model, view, proj, player, alpha, depthTest);
}

void MdxViewer::RenderWithModelMatrix(const Mat4& model, const Mat4& view, const Mat4& proj,
                                      int player, float alpha, bool depthTest) {
    if (batches_.empty() || (!sharedShader_.id && !sharedSkinnedShader_.id)) return;
    const float sx = std::sqrt(model.m[0] * model.m[0] + model.m[1] * model.m[1] + model.m[2] * model.m[2]);
    const float sy = std::sqrt(model.m[4] * model.m[4] + model.m[5] * model.m[5] + model.m[6] * model.m[6]);
    const float sz = std::sqrt(model.m[8] * model.m[8] + model.m[9] * model.m[9] + model.m[10] * model.m[10]);
    animationSpeedScale_ = std::max(0.001f, std::max(sx, std::max(sy, sz)));
    auto isValidBuffer = [](sg_buffer buffer) {
        return buffer.id && sg_query_buffer_state(buffer) == SG_RESOURCESTATE_VALID;
    };
    auto isValidImage = [](sg_image image) {
        return image.id && sg_query_image_state(image) == SG_RESOURCESTATE_VALID;
    };
    auto isValidView = [](sg_view viewHandle) {
        if (viewHandle.id && ((viewHandle.id & 0xFFFFu) >= 4096u)) {
            std::fprintf(stderr, "MdxViewer suspicious view handle: 0x%08x\n", viewHandle.id);
            return false;
        }
        return viewHandle.id && sg_query_view_state(viewHandle) == SG_RESOURCESTATE_VALID;
    };
    auto isValidSampler = [](sg_sampler samplerHandle) {
        return samplerHandle.id && sg_query_sampler_state(samplerHandle) == SG_RESOURCESTATE_VALID;
    };
    const std::string currentSequenceName = (initializedAnimator_ && modelSource_
        && animator_.GetCurrentSequence() >= 0
        && animator_.GetCurrentSequence() < (int)modelSource_->sequences.size())
        ? ToLowerCopy(modelSource_->sequences[animator_.GetCurrentSequence()].name)
        : std::string();
    const bool currentSequenceIsBirth = SequenceNameContains(currentSequenceName, "birth");
    const bool currentSequenceIsDeath = SequenceNameContains(currentSequenceName, "death");
    const bool currentSequenceIsDecay = SequenceNameContains(currentSequenceName, "decay")
        || SequenceNameContains(currentSequenceName, "corpse");
    auto shouldRenderBatchForCurrentSequence = [&](const Batch& batch) {
        if ((batch.sequenceHintMask & BatchSequenceHint_Birth) != 0 && !currentSequenceIsBirth) {
            return false;
        }
        if ((batch.sequenceHintMask & BatchSequenceHint_Death) != 0 && !currentSequenceIsDeath && !currentSequenceIsDecay) {
            return false;
        }
        if ((batch.sequenceHintMask & BatchSequenceHint_Decay) != 0 && !currentSequenceIsDeath && !currentSequenceIsDecay) {
            return false;
        }
        return true;
    };
    Mat4 mvp = proj * view * model;

    struct VSBlock { float MVP[16]; float Model[16]; };
    struct VSBlockSkinned { float MVP[16]; float Model[16]; int boneCount; };
    struct FSBlock {
        float color[4];
        float uvTransRot[4];
        float texFlags[4];
        float fogColor[4];
        float fogParams[4];
    };

    VSBlock vsU; std::memcpy(vsU.MVP, mvp.Data(), 64); std::memcpy(vsU.Model, model.Data(), 64);
    VSBlockSkinned vsUS;
    std::memcpy(vsUS.MVP, mvp.Data(), 64);
    std::memcpy(vsUS.Model, model.Data(), 64);
    vsUS.boneCount = animator_.GetBoneCount();
    FSBlock fsU = {};
    fsU.color[0] = 1.0f; fsU.color[1] = 1.0f; fsU.color[2] = 1.0f; fsU.color[3] = alpha;
    fsU.uvTransRot[0] = 0.0f;
    fsU.uvTransRot[1] = 0.0f;
    fsU.uvTransRot[2] = 0.0f;
    fsU.uvTransRot[3] = 1.0f;
    fsU.texFlags[0] = 0.0f;
    fsU.texFlags[1] = 0.0f;
    fsU.texFlags[2] = 1.0f;
    fsU.texFlags[3] = 1.0f;
    fsU.fogColor[0] = fogColor_[0];
    fsU.fogColor[1] = fogColor_[1];
    fsU.fogColor[2] = fogColor_[2];
    fsU.fogColor[3] = 1.0f;
    fsU.fogParams[0] = fogEnabled_ ? 1.0f : 0.0f;
    fsU.fogParams[1] = fogStart_;
    fsU.fogParams[2] = fogEnd_;
    fsU.fogParams[3] = 0.0f;
    const uint32_t currentFrame = initializedAnimator_ ? animator_.GetCurrentFrame() : 0;
    const int currentSequenceIndex = initializedAnimator_ ? animator_.GetCurrentSequence() : -1;

    sg_sampler linearSmp = {0};
    sg_sampler_desc smpDesc = {};
    smpDesc.min_filter = SG_FILTER_LINEAR;
    smpDesc.mag_filter = SG_FILTER_LINEAR;
    const sg_view fallbackTexView = isValidView(sharedWhiteTextureView_) ? sharedWhiteTextureView_ : sg_view{0};
    const sg_sampler fallbackSampler = isValidSampler(sharedSampler_) ? sharedSampler_ : sg_sampler{0};

    int skippedBySequenceHint = 0;
    int skippedInvalidTexture = 0;
    int submittedBatches = 0;
    for (auto& batch : batches_) {
        if (!isValidBuffer(batch.vbo) || !isValidBuffer(batch.ebo)) {
            continue;
        }
        if (!shouldRenderBatchForCurrentSequence(batch)) {
            ++skippedBySequenceHint;
            continue;
        }
        int activeTextureId = batch.textureId;
        if (batch.hasTextureIdTimeline && !batch.textureIdTimeline.frames.empty()) {
            float sampledTextureId = (float)activeTextureId;
            if (SampleTimelineForModel(batch.textureIdTimeline, modelSource_, currentFrame, elapsedMs_,
                    &sampledTextureId, 1, (float)activeTextureId, currentSequenceIndex)) {
                activeTextureId = (int)std::lround(sampledTextureId);
            }
        }

        sg_image tex = batch.textureHandle;
        sg_view texView = batch.textureView;
        int activeReplaceableId = batch.replaceableId;
        std::string activeReplaceablePath = batch.replaceablePath;
        if (activeTextureId >= 0 && activeTextureId < (int)modelTextures_.size()) {
            const auto& textureSlot = modelTextures_[activeTextureId];
            tex = textureSlot.handle;
            texView = textureSlot.view;
            activeReplaceableId = textureSlot.replaceableId;
            activeReplaceablePath = textureSlot.replaceablePath;
        }
        bool hasBoundTexture = isValidImage(tex) && isValidView(texView);
        const int originalReplaceableId = batch.replaceableId;
        const std::string originalReplaceablePath = batch.replaceablePath;
        batch.replaceableId = activeReplaceableId;
        batch.replaceablePath = activeReplaceablePath;
        if (activeReplaceableId == 1 || activeReplaceableId == 2) {
            sg_image pt = GetPlayerTexture(batch, player);
            if (isValidImage(pt)) {
                tex = pt;
                if (isValidView(batch.playerTexView)) {
                    texView = batch.playerTexView;
                    hasBoundTexture = true;
                }
            }
        }
        batch.replaceableId = originalReplaceableId;
        batch.replaceablePath = originalReplaceablePath;

        if (!isValidImage(tex)) {
            tex = sharedWhiteTexture_;
        }
        if (!isValidView(texView)) {
            texView = fallbackTexView;
        }
        sg_sampler diffuseSampler = isValidSampler(batch.sampler) ? batch.sampler : fallbackSampler;

        fsU.uvTransRot[0] = 0.0f;
        fsU.uvTransRot[1] = 0.0f;
        fsU.uvTransRot[2] = 0.0f;
        fsU.uvTransRot[3] = 1.0f;
        fsU.texFlags[2] = 1.0f;
        fsU.texFlags[3] = 1.0f;
        if (modelSource_
            && batch.textureAnimationId >= 0
            && batch.textureAnimationId < (int)modelSource_->texAnims.size()) {
            const auto& texAnim = modelSource_->texAnims[(size_t)batch.textureAnimationId];
            if (const MdxTimeline* tl = FindTimeline(texAnim.timelines, TAG_KTAT)) {
                float sampled[3] = {0.0f, 0.0f, 0.0f};
                if (SampleTimelineForModel(*tl, modelSource_, currentFrame, elapsedMs_, sampled, 3,
                                           0.0f, currentSequenceIndex)) {
                    fsU.uvTransRot[0] = sampled[0];
                    fsU.uvTransRot[1] = sampled[1];
                }
            }
            if (const MdxTimeline* tl = FindTimeline(texAnim.timelines, TAG_KTAR)) {
                float sampled[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                if (SampleTimelineForModel(*tl, modelSource_, currentFrame, elapsedMs_, sampled, 4,
                                           0.0f, currentSequenceIndex)) {
                    fsU.uvTransRot[2] = sampled[2];
                    fsU.uvTransRot[3] = sampled[3];
                }
            }
            if (const MdxTimeline* tl = FindTimeline(texAnim.timelines, TAG_KTAS)) {
                float sampled[3] = {1.0f, 1.0f, 1.0f};
                if (SampleTimelineForModel(*tl, modelSource_, currentFrame, elapsedMs_, sampled, 3,
                                           1.0f, currentSequenceIndex)) {
                    fsU.texFlags[2] = std::isfinite(sampled[0]) ? sampled[0] : 1.0f;
                    fsU.texFlags[3] = std::isfinite(sampled[1]) ? sampled[1] : 1.0f;
                }
            }
        }

        fsU.texFlags[0] = hasBoundTexture ? 1.0f : 0.0f;
        fsU.texFlags[1] = (float)batch.filterMode;
        float geosetColorR = batch.geosetColor[0];
        float geosetColorG = batch.geosetColor[1];
        float geosetColorB = batch.geosetColor[2];
        float geosetAlpha = batch.geosetAlpha;
        if (batch.geosetIndex >= 0 && batch.geosetIndex < (int)animatedGeosetStates_.size()) {
            const auto& state = animatedGeosetStates_[batch.geosetIndex];
            geosetColorR = state.color[0];
            geosetColorG = state.color[1];
            geosetColorB = state.color[2];
            geosetAlpha = state.alpha;
        }
        float layerAlpha = batch.layerAlpha;
        if (batch.materialId >= 0 && batch.materialId < (int)animatedLayerAlphas_.size()) {
            const auto& materialLayers = animatedLayerAlphas_[batch.materialId];
            if (batch.materialLayerIndex >= 0 && batch.materialLayerIndex < (int)materialLayers.size()) {
                layerAlpha = materialLayers[batch.materialLayerIndex];
            }
        }
        fsU.color[0] = geosetColorR;
        fsU.color[1] = geosetColorG;
        fsU.color[2] = geosetColorB;
        fsU.color[3] = alpha * layerAlpha * geosetAlpha;
        const bool alphaBlend = batch.filterMode == 2;
        const bool additiveBlend = batch.filterMode == 3;
        const bool addAlphaBlend = batch.filterMode == 4;
        const bool depthEnabled = depthTest && ((batch.layerFlags & 0x40) == 0);
        const bool noDepthWrite = (batch.layerFlags & 0x80) != 0;

        if (batch.skinned && sharedSkinnedShader_.id && sharedSkinnedPipeline_.id) {
            sg_image boneTex = animator_.GetBoneTexture();
            if (!isValidImage(boneTex)) continue;

            sg_pipeline selectedPipeline = {0};
            if (depthEnabled) {
                if (additiveBlend) {
                    selectedPipeline = sharedSkinnedAdditivePipeline_;
                } else if (addAlphaBlend) {
                    selectedPipeline = sharedSkinnedAddAlphaPipeline_;
                } else if (alphaBlend) {
                    selectedPipeline = sharedSkinnedBlendPipeline_;
                } else {
                    selectedPipeline = noDepthWrite ? sharedSkinnedNoDepthWritePipeline_ : sharedSkinnedPipeline_;
                }
            } else {
                if (additiveBlend) {
                    selectedPipeline = sharedOverlaySkinnedAdditivePipeline_;
                } else if (addAlphaBlend) {
                    selectedPipeline = sharedOverlaySkinnedAddAlphaPipeline_;
                } else if (alphaBlend) {
                    selectedPipeline = sharedOverlaySkinnedBlendPipeline_;
                } else {
                    selectedPipeline = sharedOverlaySkinnedPipeline_;
                }
            }
            sg_apply_pipeline(selectedPipeline);
            sg_apply_uniforms(0, sg_rng(vsUS));
            sg_apply_uniforms(1, sg_rng(fsU));

            sg_bindings bind = {};
            bind.vertex_buffers[0] = batch.vbo;
            bind.index_buffer = batch.ebo;
            if (!linearSmp.id) linearSmp = sg_make_sampler(&smpDesc);
            if (boneTex.id != cachedBoneTexId_) {
                if (cachedBoneView_.id) sg_destroy_view(cachedBoneView_);
                sg_view_desc vd = {};
                vd.texture.image = boneTex;
                cachedBoneView_ = sg_make_view(&vd);
                cachedBoneTexId_ = boneTex.id;
            }
            if (!isValidView(cachedBoneView_) || !isValidSampler(linearSmp)) {
                continue;
            }
            bind.views[0] = cachedBoneView_;
            bind.samplers[0] = linearSmp;
            // Diffuse texture (FS slot 1)
            bind.views[1] = texView;
            bind.samplers[1] = diffuseSampler.id ? diffuseSampler : linearSmp;
            sg_apply_bindings(&bind);
            sg_draw(0, batch.indexCount, 1);
            ++submittedBatches;
        } else if (sharedShader_.id && sharedPipeline_.id) {
            sg_pipeline selectedPipeline = {0};
            if (depthEnabled) {
                if (additiveBlend) {
                    selectedPipeline = sharedAdditivePipeline_;
                } else if (addAlphaBlend) {
                    selectedPipeline = sharedAddAlphaPipeline_;
                } else if (alphaBlend) {
                    selectedPipeline = sharedBlendPipeline_;
                } else {
                    selectedPipeline = noDepthWrite ? sharedNoDepthWritePipeline_ : sharedPipeline_;
                }
            } else {
                if (additiveBlend) {
                    selectedPipeline = sharedOverlayAdditivePipeline_;
                } else if (addAlphaBlend) {
                    selectedPipeline = sharedOverlayAddAlphaPipeline_;
                } else if (alphaBlend) {
                    selectedPipeline = sharedOverlayBlendPipeline_;
                } else {
                    selectedPipeline = sharedOverlayPipeline_;
                }
            }
            sg_apply_pipeline(selectedPipeline);
            sg_apply_uniforms(0, sg_rng(vsU));
            sg_apply_uniforms(1, sg_rng(fsU));

            sg_bindings bind = {};
            bind.vertex_buffers[0] = batch.vbo;
            bind.index_buffer = batch.ebo;
            bind.views[0] = texView;
            bind.samplers[0] = diffuseSampler;
            if (!isValidView(bind.views[0]) || !isValidSampler(bind.samplers[0])) {
                ++skippedInvalidTexture;
                continue;
            }
            sg_apply_bindings(&bind);
            sg_draw(0, batch.indexCount, 1);
            ++submittedBatches;
        }
    }
    if (linearSmp.id) sg_destroy_sampler(linearSmp);

    if (hasParticles_) {
        particles_.SetWorldOffset(Vec3{model.m[12], model.m[13], model.m[14]});
        particles_.Render(view, proj);
    }
    if (hasRibbons_) {
        ribbons_.SetWorldOffset(Vec3{model.m[12], model.m[13], model.m[14]});
        ribbons_.Render(view, proj);
    }
}

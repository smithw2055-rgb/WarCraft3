#include "mdx_sequence_select.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace {

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return value;
}

std::string TrimAsciiWhitespace(const std::string& value) {
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

std::string GetSequenceBaseNameLower(const std::string& sequenceName) {
    const size_t dash = sequenceName.find('-');
    const std::string baseName = (dash == std::string::npos)
        ? sequenceName
        : sequenceName.substr(0, dash);
    return ToLowerCopy(TrimAsciiWhitespace(baseName));
}

const std::array<const char*, 53> kKnownSecondaryTagNames = {{
    "alternate", "alternateex", "bone", "chain", "channel", "complete", "critical", "defend",
    "drain", "eattree", "fast", "fill", "flail", "flesh", "fifth", "fire",
    "first", "five", "four", "fourth", "gold", "hit", "large", "left",
    "light", "looping", "lumber", "medium", "moderate", "off", "one", "puke",
    "ready", "right", "second", "severe", "slam", "small", "spiked", "spin",
    "spell", "cinematic", "swim", "talk", "third", "three", "throw", "two",
    "turn", "victory", "work", "wounded", "upgrade"
}};

int GetKnownSecondaryTagOrdinal(const std::string& token) {
    for (int i = 0; i < (int)kKnownSecondaryTagNames.size(); ++i) {
        if (token == kKnownSecondaryTagNames[(size_t)i]) {
            return i;
        }
    }
    return -1;
}

bool IsPrimarySequenceToken(const std::string& token) {
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

std::vector<std::string> TokenizeSequenceNameLower(const std::string& sequenceName) {
    std::vector<std::string> tokens;
    std::string token;
    token.reserve(sequenceName.size());
    for (unsigned char ch : sequenceName) {
        if (std::isalnum(ch) != 0) {
            token.push_back((char)std::tolower(ch));
        } else if (!token.empty()) {
            tokens.push_back(token);
            token.clear();
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

void SortSecondaryTagsByOrdinal(std::vector<std::string>& tags) {
    std::sort(tags.begin(), tags.end(), [](const std::string& a, const std::string& b) {
        const int aOrdinal = GetKnownSecondaryTagOrdinal(a);
        const int bOrdinal = GetKnownSecondaryTagOrdinal(b);
        if (aOrdinal != bOrdinal) {
            return aOrdinal < bOrdinal;
        }
        return a < b;
    });
}

std::vector<std::string> NormalizeKnownSecondaryTags(const std::vector<std::string>& inputTags) {
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

void AppendUniqueTag(std::vector<std::string>& tags, const char* tag) {
    if (!tag || !tag[0]) {
        return;
    }
    std::string lower = ToLowerCopy(tag);
    if (std::find(tags.begin(), tags.end(), lower) == tags.end()) {
        tags.push_back(std::move(lower));
    }
}

std::vector<std::string> ExtractKnownSequenceSecondaryTags(const std::string& sequenceName) {
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

bool SequenceHasPrimaryToken(const MdxSequence& sequence, const std::string& primaryToken) {
    if (primaryToken.empty()) {
        return true;
    }
    const auto tokens = TokenizeSequenceNameLower(sequence.name);
    return std::find(tokens.begin(), tokens.end(), primaryToken) != tokens.end();
}

int ComputeRequestedSecondaryTagMatchRank(
    const std::vector<std::string>& requestedTags,
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

int ComputeSecondaryTagOrdinalRank(const std::vector<std::string>& candidateTags) {
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

bool IsBetterSequenceSelectionScore(
    const SequenceSelectionScore& candidate,
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

} // namespace

int MdxSelectSequenceIndexByPrimaryAndTags(
    const MdxModel& model,
    const char* primaryToken,
    const std::vector<std::string>& secondaryTags) {
    const std::string primary = primaryToken ? ToLowerCopy(std::string(primaryToken)) : std::string();
    const auto requestedTags = NormalizeKnownSecondaryTags(secondaryTags);
    bool foundCandidate = false;
    SequenceSelectionScore bestScore;
    for (int i = 0; i < (int)model.sequences.size(); ++i) {
        const auto& sequence = model.sequences[(size_t)i];
        if (!SequenceHasPrimaryToken(sequence, primary)) {
            continue;
        }

        const auto candidateTags = ExtractKnownSequenceSecondaryTags(sequence.name);
        SequenceSelectionScore candidateScore;
        candidateScore.exactRequested = candidateTags == requestedTags;
        candidateScore.matchRank = ComputeRequestedSecondaryTagMatchRank(requestedTags, candidateTags);
        candidateScore.positiveMatch = candidateScore.matchRank > 0;
        candidateScore.secondaryCount = (int)candidateTags.size();
        candidateScore.exactBase = GetSequenceBaseNameLower(sequence.name) == primary;
        candidateScore.ordinalRank = ComputeSecondaryTagOrdinalRank(candidateTags);
        candidateScore.index = i;

        if (!foundCandidate || IsBetterSequenceSelectionScore(candidateScore, bestScore)) {
            bestScore = candidateScore;
            foundCandidate = true;
        }
    }
    return foundCandidate ? bestScore.index : -1;
}

int MdxSelectSpellSequenceIndex(
    const MdxModel& model,
    const std::vector<std::string>& secondaryTags) {
    std::vector<std::string> spellTags = secondaryTags;
    AppendUniqueTag(spellTags, "spell");

    int sequenceIndex = MdxSelectSequenceIndexByPrimaryAndTags(model, "spell", spellTags);
    if (sequenceIndex >= 0) {
        return sequenceIndex;
    }

    sequenceIndex = MdxSelectSequenceIndexByPrimaryAndTags(model, nullptr, spellTags);
    if (sequenceIndex >= 0) {
        return sequenceIndex;
    }

    return MdxSelectSequenceIndexByPrimaryAndTags(model, "spell", secondaryTags);
}

float MdxSequenceDurationSeconds(const MdxModel& model, int sequenceIndex) {
    if (sequenceIndex < 0 || sequenceIndex >= (int)model.sequences.size()) {
        return 0.0f;
    }
    const MdxSequence& sequence = model.sequences[(size_t)sequenceIndex];
    if (sequence.endFrame <= sequence.startFrame) {
        return 0.0f;
    }
    return (float)(sequence.endFrame - sequence.startFrame) / 1000.0f;
}

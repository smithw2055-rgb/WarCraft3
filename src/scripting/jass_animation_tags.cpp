#include "scripting/jass_animation_tags.h"

#include <algorithm>
#include <cctype>

namespace {

const char* kPrimaryAnimationTags[] = {
    "attack", "birth", "death", "decay", "dissipate", "morph", "portrait", "sleep", "stand", "walk"
};

const char* kSecondaryAnimationTags[] = {
    "alternate", "alternateex", "bone", "chain", "channel", "complete", "critical", "defend",
    "drain", "eattree", "fast", "fill", "flail", "flesh", "fifth", "fire", "first", "five",
    "four", "fourth", "gold", "hit", "large", "left", "light", "looping", "lumber", "medium",
    "moderate", "off", "one", "puke", "ready", "right", "second", "severe", "slam", "small",
    "spiked", "spin", "spell", "cinematic", "swim", "talk", "third", "three", "throw", "two",
    "turn", "victory", "work", "wounded", "upgrade"
};

} // namespace

const char* JassAnimationPrimaryTagName(int ordinal) {
    if (ordinal < 0 || ordinal >= (int)(sizeof(kPrimaryAnimationTags) / sizeof(kPrimaryAnimationTags[0]))) {
        return "";
    }
    return kPrimaryAnimationTags[ordinal];
}

const char* JassAnimationSecondaryTagName(int ordinal) {
    if (ordinal < 0 || ordinal >= (int)(sizeof(kSecondaryAnimationTags) / sizeof(kSecondaryAnimationTags[0]))) {
        return "";
    }
    return kSecondaryAnimationTags[ordinal];
}

int JassFindAnimationPrimaryTagOrdinal(const std::string& lowerToken) {
    for (int i = 0; i < (int)(sizeof(kPrimaryAnimationTags) / sizeof(kPrimaryAnimationTags[0])); ++i) {
        if (lowerToken == kPrimaryAnimationTags[i]) return i;
    }
    return -1;
}

int JassFindAnimationSecondaryTagOrdinal(const std::string& lowerToken) {
    if (lowerToken == "1") return JassFindAnimationSecondaryTagOrdinal("first");
    if (lowerToken == "2") return JassFindAnimationSecondaryTagOrdinal("second");
    if (lowerToken == "3") return JassFindAnimationSecondaryTagOrdinal("third");
    if (lowerToken == "4") return JassFindAnimationSecondaryTagOrdinal("fourth");
    if (lowerToken == "5") return JassFindAnimationSecondaryTagOrdinal("fifth");
    for (int i = 0; i < (int)(sizeof(kSecondaryAnimationTags) / sizeof(kSecondaryAnimationTags[0])); ++i) {
        if (lowerToken == kSecondaryAnimationTags[i]) return i;
    }
    return -1;
}

void JassPopulateAnimationTagsFromSelector(const std::string& animationSelector,
                                           std::vector<int>& primaryOrdinals,
                                           std::vector<int>& secondaryOrdinals) {
    primaryOrdinals.clear();
    secondaryOrdinals.clear();

    std::string token;
    auto flushToken = [&]() {
        if (token.empty()) return;
        std::transform(token.begin(), token.end(), token.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        const int primary = JassFindAnimationPrimaryTagOrdinal(token);
        if (primary >= 0) {
            if (std::find(primaryOrdinals.begin(), primaryOrdinals.end(), primary) == primaryOrdinals.end()) {
                primaryOrdinals.push_back(primary);
            }
        } else {
            const int secondary = JassFindAnimationSecondaryTagOrdinal(token);
            if (secondary >= 0
                && std::find(secondaryOrdinals.begin(), secondaryOrdinals.end(), secondary) == secondaryOrdinals.end()) {
                secondaryOrdinals.push_back(secondary);
            }
        }
        token.clear();
    };

    for (char ch : animationSelector) {
        if (std::isalnum((unsigned char)ch)) {
            token.push_back(ch);
        } else {
            flushToken();
        }
    }
    flushToken();
}

#pragma once
#include <string>
#include <unordered_map>

namespace Variations {

struct StringHash {
    size_t operator()(const std::string& s) const {
        size_t h = 0;
        for (char c : s) h = h * 31 + c;
        return h;
    }
};

inline const std::unordered_map<std::string, int, StringHash>& GetCliffVars() {
    static const std::unordered_map<std::string, int, StringHash> vars = {
        {"AAAB", 1}, {"AAAC", 1}, {"AABA", 1}, {"AABB", 2}, {"AABC", 0},
        {"AACA", 1}, {"AACB", 0}, {"AACC", 1}, {"ABAA", 1}, {"ABAB", 1},
        {"ABAC", 0}, {"ABBA", 2}, {"ABBB", 1}, {"ABBC", 0}, {"ABCA", 0},
        {"ABCB", 0}, {"ABCC", 0}, {"ACAA", 1}, {"ACAB", 0}, {"ACAC", 1},
        {"ACBA", 0}, {"ACBB", 0}, {"ACBC", 0}, {"ACCA", 1}, {"ACCB", 0},
        {"ACCC", 1}, {"BAAA", 1}, {"BAAB", 1}, {"BAAC", 0}, {"BABA", 1},
        {"BABB", 1}, {"BABC", 0}, {"BACA", 0}, {"BACB", 0}, {"BACC", 0},
        {"BBAA", 1}, {"BBAB", 1}, {"BBAC", 0}, {"BBBA", 1}, {"BBCA", 0},
        {"BCAA", 0}, {"BCAB", 0}, {"BCAC", 0}, {"BCBA", 0}, {"BCCA", 0},
        {"CAAA", 1}, {"CAAB", 0}, {"CAAC", 1}, {"CABA", 0}, {"CABB", 0},
        {"CABC", 0}, {"CACA", 1}, {"CACB", 0}, {"CACC", 1}, {"CBAA", 0},
        {"CBAB", 0}, {"CBAC", 0}, {"CBBA", 0}, {"CBCA", 0}, {"CCAA", 1},
        {"CCAB", 0}, {"CCAC", 1}, {"CCBA", 0}, {"CCCA", 1},
    };
    return vars;
}

inline const std::unordered_map<std::string, int, StringHash>& GetCityCliffVars() {
    static const std::unordered_map<std::string, int, StringHash> vars = {
        {"AAAB", 2}, {"AAAC", 1}, {"AABA", 1}, {"AABB", 3}, {"AABC", 0},
        {"AACA", 1}, {"AACB", 0}, {"AACC", 3}, {"ABAA", 1}, {"ABAB", 2},
        {"ABAC", 0}, {"ABBA", 3}, {"ABBB", 0}, {"ABBC", 0}, {"ABCA", 0},
        {"ABCB", 0}, {"ABCC", 0}, {"ACAA", 1}, {"ACAB", 0}, {"ACAC", 2},
        {"ACBA", 0}, {"ACBB", 0}, {"ACBC", 0}, {"ACCA", 3}, {"ACCB", 0},
        {"ACCC", 1}, {"BAAA", 1}, {"BAAB", 3}, {"BAAC", 0}, {"BABA", 2},
        {"BABB", 0}, {"BABC", 0}, {"BACA", 0}, {"BACB", 0}, {"BACC", 0},
        {"BBAA", 3}, {"BBAB", 1}, {"BBAC", 0}, {"BBBA", 1}, {"BBCA", 0},
        {"BCAA", 0}, {"BCAB", 0}, {"BCAC", 0}, {"BCBA", 0}, {"BCCA", 0},
        {"CAAA", 1}, {"CAAB", 0}, {"CAAC", 3}, {"CABA", 0}, {"CABB", 0},
        {"CABC", 0}, {"CACA", 2}, {"CACB", 0}, {"CACC", 1}, {"CBAA", 0},
        {"CBAB", 0}, {"CBAC", 0}, {"CBBA", 0}, {"CBCA", 0}, {"CCAA", 3},
        {"CCAB", 0}, {"CCAC", 1}, {"CCBA", 0}, {"CCCA", 1},
    };
    return vars;
}

inline int GetCliffVariation(const std::string& dir, const std::string& tag, int variation) {
    int maxVar = 0;
    if (dir == "Cliffs") {
        auto& vars = GetCliffVars();
        auto it = vars.find(tag);
        if (it != vars.end()) maxVar = it->second;
    } else {
        auto& vars = GetCityCliffVars();
        auto it = vars.find(tag);
        if (it != vars.end()) maxVar = it->second;
    }
    if (variation < maxVar) return variation;
    return variation % (maxVar + 1);
}

} // namespace Variations

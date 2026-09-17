#include "scripting/jass_animation_tags.h"

#include <algorithm>
#include <string>
#include <vector>

#define TEST_ASSERT(expr) do { if (!(expr)) return false; } while (0)

bool TestJassAnimationTagOrdinalsMatchReference() {
    TEST_ASSERT(std::string(JassAnimationPrimaryTagName(0)) == "attack");
    TEST_ASSERT(std::string(JassAnimationPrimaryTagName(8)) == "stand");
    TEST_ASSERT(std::string(JassAnimationPrimaryTagName(9)) == "walk");
    TEST_ASSERT(std::string(JassAnimationPrimaryTagName(10)).empty());

    TEST_ASSERT(std::string(JassAnimationSecondaryTagName(4)) == "channel");
    TEST_ASSERT(std::string(JassAnimationSecondaryTagName(20)) == "gold");
    TEST_ASSERT(std::string(JassAnimationSecondaryTagName(40)) == "spell");
    TEST_ASSERT(std::string(JassAnimationSecondaryTagName(50)) == "work");
    TEST_ASSERT(std::string(JassAnimationSecondaryTagName(53)).empty());
    return true;
}

bool TestJassPopulateAnimationTagsFromSelector() {
    std::vector<int> primary;
    std::vector<int> secondary;
    JassPopulateAnimationTagsFromSelector("Attack Spell Channel Alternate - 2", primary, secondary);

    TEST_ASSERT(primary.size() == 1);
    TEST_ASSERT(primary[0] == JassFindAnimationPrimaryTagOrdinal("attack"));
    TEST_ASSERT(std::find(secondary.begin(), secondary.end(), JassFindAnimationSecondaryTagOrdinal("spell")) != secondary.end());
    TEST_ASSERT(std::find(secondary.begin(), secondary.end(), JassFindAnimationSecondaryTagOrdinal("channel")) != secondary.end());
    TEST_ASSERT(std::find(secondary.begin(), secondary.end(), JassFindAnimationSecondaryTagOrdinal("alternate")) != secondary.end());
    TEST_ASSERT(std::find(secondary.begin(), secondary.end(), JassFindAnimationSecondaryTagOrdinal("second")) != secondary.end());
    return true;
}

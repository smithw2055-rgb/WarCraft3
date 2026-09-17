// war3map.w3a/w3q/w3h object-mod pipeline tests (ref object data).
// Uses the checked-in tests/data/mini_mods.w3x fixture (custom Devotion
// Aura / human melee attack / Defend buff) so the test exercises only the
// stable MPQ read path.
#include <iostream>

#include "data/abilitydata.h"
#include "data/upgradedata.h"
#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

bool OpenResources(MpqManager& mpq) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    const char* archives[] = {"War3Patch.mpq", "War3xLocal.mpq", "war3x.mpq", "war3.mpq"};
    for (auto a : archives) mpq.Open(res + a);
    return mpq.ArchiveCount() > 0;
}

std::string FixtureMap() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/mini_mods.w3x";
}

} // namespace

bool TestAbilityAndBuffMapMods() {
    MpqManager mpq;
    TEST_ASSERT(OpenResources(mpq));

    // Baseline: stock data has the base ability/buff, no custom ids.
    CAbilityData base = CAbilityData::LoadFromMpq(mpq);
    TEST_ASSERT(base.HasAbility(War3ID("AHad")));
    TEST_ASSERT(!base.HasAbility(War3ID("A99X")));
    TEST_ASSERT(base.GetBuff(War3ID("Bdef")) != nullptr);

    CAbilityData modded = CAbilityData::LoadFromMpq(mpq, FixtureMap());

    // Original-entry override: AHad cooldown 0 -> 9.
    const CAbilityDefFull* orig = modded.GetAbility(War3ID("AHad"));
    TEST_ASSERT(orig != nullptr);
    TEST_ASSERT(orig->cooldown == 9.0f);
    // Custom clone: AHad -> A99X with cooldown 5 and mana 75.
    const CAbilityDefFull* custom = modded.GetAbility(War3ID("A99X"));
    TEST_ASSERT(custom != nullptr);
    TEST_ASSERT(custom->cooldown == 5.0f);
    TEST_ASSERT(custom->manaCost == 75);
    // Custom buff clone: Bdef -> B99X with new icon art.
    const CBuffDefFull* buff = modded.GetBuff(War3ID("B99X"));
    TEST_ASSERT(buff != nullptr);
    TEST_ASSERT(buff->iconArt == "TestIcon.blp");
    return true;
}

bool TestUpgradeMapModsAndBaseCosts() {
    MpqManager mpq;
    TEST_ASSERT(OpenResources(mpq));

    // Base table must be populated from the SLK (89 stock upgrades) with
    // base+mod*(level-1) costs — previously the TXT-profile parse read
    // nothing and every upgrade cost 0.
    CUpgradeData base = CUpgradeData::LoadFromMpq(mpq);
    TEST_ASSERT(base.HasUpgrade(War3ID("Rhme")));
    const CUpgradeType* rhme = base.GetUpgrade(War3ID("Rhme"));
    TEST_ASSERT(rhme != nullptr);
    TEST_ASSERT(rhme->maxLevel == 3);
    TEST_ASSERT(rhme->goldCost == 100);
    TEST_ASSERT(rhme->goldPerLevel.size() == 3);
    TEST_ASSERT(rhme->goldPerLevel[1] == 175); // 100 + 75*(2-1)

    CUpgradeData modded = CUpgradeData::LoadFromMpq(mpq, FixtureMap());

    // Custom upgrade clone: Rhme -> R99X with goldbase 999.
    TEST_ASSERT(modded.HasUpgrade(War3ID("R99X")));
    const CUpgradeType* custom = modded.GetUpgrade(War3ID("R99X"));
    TEST_ASSERT(custom != nullptr);
    TEST_ASSERT(custom->goldCost == 999);
    TEST_ASSERT(!custom->name.empty());
    return true;
}

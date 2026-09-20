// Ability level-data tests: per-level combat fields parse from retail
// AbilityData.slk, and CUnit::AddAbility applies them over the
// hardcoded constructor defaults (re-applied on hero skill level-up).
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/abilitydata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells.h"
#include "ecs/abilities/spells3.h"
#include "ecs/abilities/spells5.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakeLevelType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::NONE,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeLevelType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

void LoadAbilityData(MpqManager& mpq, CAbilityData& abilityData) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    abilityData = CAbilityData::LoadFromMpq(mpq);
}

} // namespace

bool TestAbilityLevelsParse() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    const CAbilityDefFull* bolt = abilityData.GetAbility(War3ID("AHtb"));
    TEST_ASSERT(bolt != nullptr);
    TEST_ASSERT(bolt->levels == 3);
    TEST_ASSERT(bolt->CooldownAt(1) == 9.0f);
    TEST_ASSERT(bolt->CooldownAt(3) == 9.0f);
    TEST_ASSERT(bolt->ManaCostAt(1) == 75);
    TEST_ASSERT(bolt->RangeAt(2) == 600.0f);

    // Starfall's mana cost steps down per level (ref 100/75/50).
    const CAbilityDefFull* starfall = abilityData.GetAbility(War3ID("AEst"));
    TEST_ASSERT(starfall != nullptr);
    TEST_ASSERT(starfall->levels == 3);
    TEST_ASSERT(starfall->ManaCostAt(1) == 100);
    TEST_ASSERT(starfall->ManaCostAt(2) == 75);
    TEST_ASSERT(starfall->ManaCostAt(3) == 50);
    // Out-of-range levels clamp instead of reading past the end.
    TEST_ASSERT(starfall->ManaCostAt(9) == 50);
    TEST_ASSERT(starfall->ManaCostAt(0) == 100);
    return true;
}

bool TestAbilityLevelsApplyOnAdd() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* unit = AdoptAt(sim, 1501, 0, 0, 0);

    // Holy Light: hardcoded range 600 gives way to retail 800.
    unit->AddAbility(sim, std::make_unique<CAbilityHolyLight>(15001));
    auto* holy = dynamic_cast<CAbilityHolyLight*>(unit->GetAbility(War3ID("AHhb")));
    TEST_ASSERT(holy != nullptr);
    TEST_ASSERT(holy->GetCooldown() == 5.0f);
    TEST_ASSERT(holy->GetManaCost() == 65);
    TEST_ASSERT(holy->GetCastRange() == 800.0f);

    // Death and Decay: every hardcoded number loses to retail.
    unit->AddAbility(sim, std::make_unique<CAbilityDeathAndDecay>(15002));
    auto* decay = dynamic_cast<CAbilityDeathAndDecay*>(unit->GetAbility(War3ID("AUdd")));
    TEST_ASSERT(decay != nullptr);
    TEST_ASSERT(decay->GetCooldown() == 150.0f);
    TEST_ASSERT(decay->GetManaCost() == 250);
    TEST_ASSERT(decay->GetCastRange() == 1000.0f);
    return true;
}

bool TestAbilityLevelsFollowSkillLevel() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CAbilityData abilityData;
    LoadAbilityData(mpq, abilityData);

    CSimulation sim;
    AddPlayers(sim);
    sim.SetAbilityData(&abilityData);
    auto* unit = AdoptAt(sim, 1511, 0, 0, 0);

    unit->AddAbility(sim, std::make_unique<CAbilityStarfall>(15101));
    auto* starfall = dynamic_cast<CAbilityStarfall*>(unit->GetAbility(War3ID("AEst")));
    TEST_ASSERT(starfall != nullptr);
    TEST_ASSERT(starfall->GetManaCost() == 100);
    // Hero skill level-up re-applies the new level's row (ref).
    starfall->SetLevel(3);
    TEST_ASSERT(starfall->GetManaCost() == 50);
    return true;
}

bool TestAbilityLevelsFallbackWithoutData() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1521, 0, 0, 0);

    // No ability data loaded: hardcoded defaults stand.
    unit->AddAbility(sim, std::make_unique<CAbilityHolyLight>(15201));
    auto* holy = dynamic_cast<CAbilityHolyLight*>(unit->GetAbility(War3ID("AHhb")));
    TEST_ASSERT(holy != nullptr);
    TEST_ASSERT(holy->GetCastRange() == 600.0f);
    TEST_ASSERT(holy->GetManaCost() == 65);
    return true;
}

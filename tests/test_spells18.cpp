// Batch-18 (abilities) tests: whirlwind, immolation, force of nature,
// owl scout, war eagle, factory, kaboom, phoenix fire. Summon tests run
// without unit data (graceful nullptr path); damage/buff tests use
// adopted units only.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells18.h"
#include "ecs/abilities/spells3.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
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

CUnitType* MakeSpell18Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell18Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

} // namespace

bool TestWhirlwindSpins() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1801, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1802, 1, 100, 0);
    auto* friend_ = AdoptAt(sim, 1803, 0, 100, 0);
    Settle(sim);
    CAbilityWhirlwind spell(18001);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("BOww")) != nullptr);
    TEST_ASSERT(caster->IsMagicImmune());
    TEST_ASSERT(caster->AreAttacksDisabled());
    const float foeBefore = foe->GetLife();
    const float friendBefore = friend_->GetLife();
    for (int i = 0; i < 60; i++) sim.Update(); // 1s periodic ticks land
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(friend_->GetLife() == friendBefore); // allies spared
    caster->RemoveBuff(War3ID("BOww"), sim);
    TEST_ASSERT(!caster->IsMagicImmune());
    TEST_ASSERT(!caster->AreAttacksDisabled());
    return true;
}

bool TestImmolationBurnsAndDrains() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1811, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1812, 1, 100, 0);
    Settle(sim);
    caster->SetMana(500.0f);
    CAbilityImmolation spell(18101);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("BEim")) != nullptr);
    const float foeBefore = foe->GetLife();
    const float manaBefore = caster->GetMana();
    for (int i = 0; i < 60; i++) sim.Update();
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(caster->GetMana() < manaBefore); // upkeep paid
    return true;
}

bool TestSummonsWithoutDataAreGraceful() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1821, 0, 0, 0);
    Settle(sim);
    // No unit data registered: summons must fail gracefully, not crash.
    bool fired = false;
    sim.SetUnitSummonedCallback([&](int, int) { fired = true; });
    CAbilityForceOfNature nature(18201);
    nature.DoEffect(sim, caster, 200, 0);
    CAbilityOwlScout owl(18202);
    owl.DoEffect(sim, caster);
    CAbilitySummonHawk hawk(18203);
    hawk.DoEffect(sim, caster);
    CAbilityFactory factory(18204);
    factory.DoEffect(sim, caster, 200, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(!fired);
    TEST_ASSERT(!caster->IsDead());
    return true;
}

bool TestKaboomDetonates() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1831, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1832, 1, 100, 0);
    auto* friend_ = AdoptAt(sim, 1833, 0, 100, 0);
    Settle(sim);
    caster->AddAbility(sim, std::make_unique<CAbilityKaboom>(18301));
    auto* kaboom = dynamic_cast<CAbilityKaboom*>(caster->GetAbility(War3ID("Asds")));
    TEST_ASSERT(kaboom != nullptr);
    const float foeBefore = foe->GetLife();
    const float friendBefore = friend_->GetLife();
    kaboom->DoEffect(sim, caster, 100, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(caster->IsDead()); // suicidal by design
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(friend_->GetLife() == friendBefore); // allies spared
    return true;
}

bool TestPhoenixFireScorches() {
    CSimulation sim;
    AddPlayers(sim);
    auto* bearer = AdoptAt(sim, 1841, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1842, 1, 100, 0);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    bearer->AddAbility(sim, std::make_unique<CAbilityPhoenixFire>(18401));
    TEST_ASSERT(bearer->GetBuff(War3ID("BPxf")) != nullptr);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    for (int i = 0; i < 60; i++) sim.Update();
    TEST_ASSERT(foe->GetLife() < foeBefore);
    return true;
}

bool TestStarfallCodeIsAesf() {
    // Retail AEsf is Starfall; AEst is the Owl Scout (ref + retail order
    // list: scout=852181/AEst, starfall=852183/AEsf).
    auto starfall = AbilityFactory::CreateAbility(18501, War3ID("AEsf"));
    TEST_ASSERT(starfall != nullptr);
    TEST_ASSERT(starfall->GetCode() == War3ID("AEsf"));
    auto owl = AbilityFactory::CreateAbility(18502, War3ID("AEst"));
    TEST_ASSERT(owl != nullptr);
    TEST_ASSERT(owl->GetCode() == War3ID("AEst"));
    return true;
}

bool TestSpells18FactoryCoverage() {
    const char* codes[] = {"AOww", "AEim", "AEfn", "AEst",
                           "ANsw", "ANfy", "Asds", "Apxf", "AEsf"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(18600, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

// Batch-14 tests: pocket factory spawner, lava-split, soul burn,
// quilbeast, frost breath, disease cloud, pulverize, forked lightning.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/auras.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells14.h"
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

CUnitType* MakeSpell14Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell14Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

void LoadUnitData(MpqManager& mpq, CGameplayConstants& gc, CUnitData& unitData) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    unitData = CUnitData::LoadFromMpq(mpq, gc);
}

int CountType(CSimulation& sim, War3ID type, int player) {
    int n = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == type &&
            unit->GetPlayerIndex() == player) {
            n++;
        }
    }
    return n;
}

} // namespace

bool TestPocketFactoryBuilds() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("nfac"))) return true;
    if (!unitData.HasUnitType(War3ID("ncgb"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    CAbilityPocketFactory spell(14001);
    spell.DoEffect(sim, caster, 200, 0);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    TEST_ASSERT(CountType(sim, War3ID("nfac"), 0) == 1);
    // 12s spawn interval at 20 ticks/s: 260 ticks build a goblin.
    for (int i = 0; i < 260; i++) sim.Update();
    TEST_ASSERT(CountType(sim, War3ID("ncgb"), 0) >= 1);
    // Goblin output is born volatile.
    bool volatileFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("ncgb") &&
            unit->GetBuff(War3ID("Bexp")) != nullptr) {
            volatileFound = true;
        }
    }
    TEST_ASSERT(volatileFound);
    return true;
}

bool TestLavaSpawnSplits() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("nlv1"))) return true;
    if (!unitData.HasUnitType(War3ID("nlv2"))) return true;
    if (!unitData.HasUnitType(War3ID("nlv3"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    CAbilityLavaSpawn spell(14101);
    spell.DoEffect(sim, caster);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    TEST_ASSERT(CountType(sim, War3ID("nlv1"), 0) == 1);
    // Kill the big one: two mediums break out.
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && unit->GetTypeId() == War3ID("nlv1")) {
            unit->Damage(sim, caster, false, false, CAttackType::CHAOS,
                         CDamageType::NORMAL, "", 99999.0f, 0);
        }
    }
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(CountType(sim, War3ID("nlv2"), 0) == 2);
    // Kill the mediums: four smalls break out, and stop there.
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("nlv2")) {
            unit->Damage(sim, caster, false, false, CAttackType::CHAOS,
                         CDamageType::NORMAL, "", 99999.0f, 0);
        }
    }
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(CountType(sim, War3ID("nlv3"), 0) == 4);
    return true;
}

bool TestSoulBurnBurns() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1421, 0, 0, 0);
    auto* target = AdoptAt(sim, 1422, 1, 100, 0);
    Settle(sim);
    const float before = target->GetLife();
    CAbilitySoulBurn spell(14201);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() < before);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    const float afterStrike = target->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // burn ticks land
    TEST_ASSERT(target->GetLife() < afterStrike);
    return true;
}

bool TestQuilbeastSummons() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("nqb1"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    CAbilityQuilbeast spell(14301);
    spell.DoEffect(sim, caster);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    TEST_ASSERT(CountType(sim, War3ID("nqb1"), 0) == 1);
    return true;
}

bool TestFrostBreathChills() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1441, 0, 0, 0);
    auto* target = AdoptAt(sim, 1442, 1, 100, 0);
    auto* neighbor = AdoptAt(sim, 1443, 1, 160, 0);
    auto* ally = AdoptAt(sim, 1444, 0, 100, 50);
    Settle(sim);
    const float targetBefore = target->GetLife();
    const float neighborBefore = neighbor->GetLife();
    CAbilityFrostBreath spell(14401);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() < targetBefore);
    TEST_ASSERT(neighbor->GetLife() < neighborBefore);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    return true;
}

bool TestDiseaseCloudSickens() {
    CSimulation sim;
    AddPlayers(sim);
    auto* bearer = AdoptAt(sim, 1451, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1452, 1, 100, 0);
    auto* ally = AdoptAt(sim, 1453, 0, 100, 100);
    Settle(sim);
    bearer->AddAbility(sim, std::make_unique<CAbilityDiseaseCloud>(14501));
    for (int i = 0; i < 5; i++) sim.Update(); // aura applies
    TEST_ASSERT(foe->GetBuff(War3ID("Bapl")) != nullptr);
    TEST_ASSERT(ally->GetBuff(War3ID("Bapl")) == nullptr);
    const float afterApply = foe->GetLife();
    for (int i = 0; i < 30; i++) sim.Update(); // disease ticks land
    TEST_ASSERT(foe->GetLife() < afterApply);
    return true;
}

bool TestPulverizeSlams() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 1461, 0, 0, 0);
    auto* victim = AdoptAt(sim, 1462, 1, 100, 0);
    auto* neighbor = AdoptAt(sim, 1463, 1, 150, 0);
    Settle(sim);
    auto pulverize = std::make_unique<CAbilityPulverize>(14601);
    pulverize->SetChance(1.0f);
    attacker->AddAbility(sim, std::move(pulverize));
    const float neighborBefore = neighbor->GetLife();
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    TEST_ASSERT(neighbor->GetLife() < neighborBefore); // slam lands
    return true;
}

bool TestForkedLightningChains() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1471, 0, 0, 0);
    auto* first = AdoptAt(sim, 1472, 1, 100, 0);
    auto* second = AdoptAt(sim, 1473, 1, 150, 0); // strictly nearest to first
    auto* far = AdoptAt(sim, 1474, 1, 5000, 5000);
    Settle(sim);
    const float firstBefore = first->GetLife();
    const float secondBefore = second->GetLife();
    CAbilityForkedLightning spell(14701);
    spell.DoEffect(sim, caster, first);
    TEST_ASSERT(first->GetLife() < firstBefore);
    TEST_ASSERT(second->GetLife() < secondBefore); // short chain reaches
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    return true;
}

bool TestSpells14FactoryCoverage() {
    const char* codes[] = {"ANsy", "ANlm", "ANsb", "ANsq",
                           "Afsh", "Aapl", "Awar", "ANfl"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(14800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

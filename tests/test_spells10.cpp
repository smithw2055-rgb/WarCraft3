// Batch-10 tests: cleaving attack, searing arrows, ensnare, ancestral
// spirit, witch-doctor healing ward, unstable concoction, abolish
// magic, cluster rockets.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells10.h"
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
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " #a " vs " #b "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell10Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell10Type(), player,
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

} // namespace

bool TestCleavingAttackSplashes() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 1001, 0, 0, 0);
    auto* victim = AdoptAt(sim, 1002, 1, 100, 0);
    auto* neighbor = AdoptAt(sim, 1003, 1, 150, 0);
    auto* ally = AdoptAt(sim, 1004, 0, 150, 100);
    auto* far = AdoptAt(sim, 1005, 1, 5000, 5000);
    Settle(sim);
    attacker->AddAbility(sim, std::make_unique<CAbilityCleavingAttack>(10001));
    const float victimBefore = victim->GetLife();
    const float neighborBefore = neighbor->GetLife();
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    const float victimLoss = victimBefore - victim->GetLife();
    const float neighborLoss = neighborBefore - neighbor->GetLife();
    TEST_ASSERT(victimLoss > 0.0f);
    TEST_NEAR(neighborLoss, victimLoss * 0.5f, 0.05f); // half splashes out
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(attacker->GetLife() == attacker->GetMaxLife());
    return true;
}

bool TestSearingArrowsProc() {
    CSimulation sim;
    AddPlayers(sim);
    auto* plain = AdoptAt(sim, 1011, 0, 0, 0);
    auto* holder = AdoptAt(sim, 1012, 0, 0, 100);
    auto* targetA = AdoptAt(sim, 1013, 1, 100, 0);
    auto* targetB = AdoptAt(sim, 1014, 1, 100, 100);
    Settle(sim);
    holder->AddAbility(sim, std::make_unique<CAbilitySearingArrows>(10101));
    const float beforeA = targetA->GetLife();
    const float beforeB = targetB->GetLife();
    targetA->Damage(sim, plain, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 100.0f, 0);
    targetB->Damage(sim, holder, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR((beforeB - targetB->GetLife()) - (beforeA - targetA->GetLife()),
              20.0f, 0.01f);
    return true;
}

bool TestEnsnareRoots() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1021, 0, 0, 0);
    auto* target = AdoptAt(sim, 1022, 1, 100, 0);
    Settle(sim);
    TEST_ASSERT(!target->IsRooted());
    CAbilityEnsnare spell(10201);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bena")) != nullptr);
    TEST_ASSERT(target->IsRooted());
    target->RemoveBuff(War3ID("Bena"), sim); // expiry frees the unit
    TEST_ASSERT(!target->IsRooted());
    return true;
}

bool TestAncestralSpiritRevives() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("otau"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("otau"), 0, 0, 0, 0);
    auto* fallen = sim.CreateUnit(War3ID("otau"), 1, 100, 0, 0);
    auto* grunt = sim.CreateUnit(War3ID("hfoo"), 1, 200, 0, 0);
    TEST_ASSERT(caster && fallen && grunt);
    Settle(sim);
    fallen->Damage(sim, caster, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 99999.0f, 0);
    grunt->Damage(sim, caster, false, false, CAttackType::CHAOS,
                  CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(fallen->IsDead() && grunt->IsDead());
    CAbilityAncestralSpirit spell(10301);
    spell.DoEffect(sim, caster, grunt); // not a tauren: no answer
    TEST_ASSERT(grunt->IsDead());
    spell.DoEffect(sim, caster, fallen);
    TEST_ASSERT(!fallen->IsDead());
    TEST_ASSERT(fallen->GetPlayerIndex() == 0); // risen for the caster
    return true;
}

bool TestHealingWardUnitHeals() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("ohwd"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* ally = sim.CreateUnit(War3ID("hfoo"), 0, 100, 100, 0);
    TEST_ASSERT(caster && ally);
    Settle(sim);
    ally->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", ally->GetMaxLife() * 0.5f, 0);
    TEST_ASSERT(!ally->IsDead());
    CAbilityHealingWardUnit spell(10401);
    spell.DoEffect(sim, caster, 100, 100);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    bool wardFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("ohwd")) {
            wardFound = true;
        }
    }
    TEST_ASSERT(wardFound);
    const float afterPlace = ally->GetLife();
    for (int i = 0; i < 30; i++) sim.Update(); // ward ticks land
    TEST_ASSERT(ally->GetLife() > afterPlace);
    return true;
}

bool TestUnstableConcoctionBlasts() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1051, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1052, 1, 100, 0);
    auto* ally = AdoptAt(sim, 1053, 0, 100, 100);
    auto* far = AdoptAt(sim, 1054, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    const float casterBefore = caster->GetLife();
    CAbilityUnstableConcoction spell(10501);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(caster->GetLife() < casterBefore); // volatile brew burns all
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    return true;
}

bool TestAbolishMagicStripsArea() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1061, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1062, 1, 100, 100);
    Settle(sim);
    foe->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) != nullptr);
    CAbilityAbolishMagic spell(10601);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) == nullptr);
    return true;
}

bool TestClusterRocketsBlast() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1071, 0, 0, 0);
    auto* left = AdoptAt(sim, 1072, 1, 25, 0);
    auto* right = AdoptAt(sim, 1073, 1, 175, 0);
    auto* far = AdoptAt(sim, 1074, 1, 5000, 5000);
    Settle(sim);
    const float leftBefore = left->GetLife();
    const float rightBefore = right->GetLife();
    CAbilityClusterRockets spell(10701);
    spell.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(left->GetLife() < leftBefore); // west cluster
    TEST_ASSERT(right->GetLife() < rightBefore); // east cluster
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestSpells10FactoryCoverage() {
    const char* codes[] = {"ANca", "AHfa", "Aens", "Aast",
                           "Ahwd", "Auco", "Aadm", "ANcs"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(10800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

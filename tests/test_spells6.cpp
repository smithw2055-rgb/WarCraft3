// Batch-6 tests: alchemist kit (heal spray, acid bomb, chemical rage,
// transmute) plus Bloodlust, Purge, Lightning Shield, Raise Dead.
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
#include "ecs/abilities/spells6.h"
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

CUnitType* MakeSpell6Type(bool mech = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    static std::vector<std::unique_ptr<CUnitType>> keepMech;
    auto& store = mech ? keepMech : keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    std::unordered_set<CUnitClassification> classes;
    if (mech) classes.insert(CUnitClassification::MECHANICAL);
    store.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        classes,
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
    return store.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y, bool mech = false) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell6Type(mech), player,
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

} // namespace

bool TestHealingSprayHealsAllies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 601, 0, 0, 0);
    auto* ally = AdoptAt(sim, 602, 0, 100, 100);
    auto* foe = AdoptAt(sim, 603, 1, 150, 100);
    Settle(sim);
    ally->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", 5000.0f, 0);
    foe->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                CDamageType::NORMAL, "", 5000.0f, 0);
    const float allyBefore = ally->GetLife();
    const float foeBefore = foe->GetLife();
    CAbilityHealingSpray spell(6001);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(ally->GetLife() > allyBefore);
    TEST_ASSERT(foe->GetLife() == foeBefore); // enemies get nothing
    return true;
}

bool TestAcidBombCurses() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 611, 0, 0, 0);
    auto* target = AdoptAt(sim, 612, 1, 100, 0);
    Settle(sim);
    const int baseArmor = target->GetTotalDefense();
    CAbilityAcidBomb spell(6101);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bacb")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    TEST_ASSERT(target->GetTotalDefense() == baseArmor - 4);
    const float afterBomb = target->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // poison ticks land
    TEST_ASSERT(target->GetLife() < afterBomb);
    return true;
}

bool TestChemicalRageHastes() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 621, 0, 0, 0);
    Settle(sim);
    const float speedBefore = caster->GetSpeed();
    CAbilityChemicalRage spell(6201);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("Bblo")) != nullptr);
    TEST_ASSERT(caster->GetSpeed() > speedBefore);
    return true;
}

bool TestTransmutePaysGold() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 631, 0, 0, 0);
    auto* victim = AdoptAt(sim, 632, 1, 100, 0);
    auto* ally = AdoptAt(sim, 633, 0, 200, 0);
    Settle(sim);
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 0);
    CAbilityTransmute spell(6301);
    spell.DoEffect(sim, caster, ally); // allies are not convertible
    TEST_ASSERT(!ally->IsDead());
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 0);
    spell.DoEffect(sim, caster, victim);
    TEST_ASSERT(victim->IsDead());
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 200);
    return true;
}

bool TestBloodlustCastHastes() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 641, 0, 0, 0);
    auto* ally = AdoptAt(sim, 642, 0, 100, 0);
    auto* mech = AdoptAt(sim, 643, 0, 200, 0, true);
    Settle(sim);
    const float speedBefore = ally->GetSpeed();
    CAbilityBloodlustCast spell(6401);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("Bblo")) != nullptr);
    TEST_ASSERT(ally->GetSpeed() > speedBefore);
    spell.DoEffect(sim, caster, mech); // mechanical units refuse haste
    TEST_ASSERT(mech->GetBuff(War3ID("Bblo")) == nullptr);
    return true;
}

bool TestPurgeDispelsAndSlows() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 651, 0, 0, 0);
    auto* target = AdoptAt(sim, 652, 1, 100, 0);
    Settle(sim);
    target->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    TEST_ASSERT(target->GetBuff(War3ID("Bblo")) != nullptr);
    CAbilityPurge spell(6501);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bblo")) == nullptr); // dispelled
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr); // slowed
    return true;
}

bool TestPurgeBurnsSummons() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 661, 0, 0, 0);
    auto* summoned = AdoptAt(sim, 662, 1, 100, 0);
    Settle(sim);
    summoned->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("AOsf"), 120.0f), sim);
    TEST_ASSERT(summoned->IsSummoned());
    const float before = summoned->GetLife();
    CAbilityPurge spell(6601);
    spell.DoEffect(sim, caster, summoned);
    TEST_ASSERT(summoned->GetLife() < before); // summon burn landed
    return true;
}

bool TestLightningShieldZaps() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 671, 0, 0, 0);
    auto* bearer = AdoptAt(sim, 672, 0, 100, 0);
    auto* foe = AdoptAt(sim, 673, 1, 150, 0);
    auto* ally = AdoptAt(sim, 674, 0, 150, 100);
    auto* far = AdoptAt(sim, 675, 1, 5000, 5000);
    Settle(sim);
    CAbilityLightningShield spell(6701);
    spell.DoEffect(sim, caster, bearer);
    TEST_ASSERT(bearer->GetBuff(War3ID("Blsh")) != nullptr);
    const float foeBefore = foe->GetLife();
    for (int i = 0; i < 30; i++) sim.Update(); // shield ticks land
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(bearer->GetLife() == bearer->GetMaxLife());
    return true;
}

bool TestRaiseDeadSkeletons() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("uske"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* fallen1 = sim.CreateUnit(War3ID("hfoo"), 1, 100, 0, 0);
    auto* fallen2 = sim.CreateUnit(War3ID("hfoo"), 1, 200, 0, 0);
    TEST_ASSERT(caster && fallen1 && fallen2);
    Settle(sim);
    for (auto* fallen : {fallen1, fallen2}) {
        fallen->Damage(sim, caster, false, false, CAttackType::CHAOS,
                       CDamageType::NORMAL, "", 99999.0f, 0);
    }
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(fallen1->IsDead() && fallen2->IsDead());
    CAbilityRaiseDead spell(6801);
    spell.DoEffect(sim, caster);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    int skeletons = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("uske") &&
            unit->GetPlayerIndex() == 0) {
            skeletons++;
        }
    }
    TEST_ASSERT(skeletons == 2);
    return true;
}

bool TestSpells6FactoryCoverage() {
    const char* codes[] = {"ANhs", "ANab", "ANcr", "ANtm",
                           "Ablo", "Aprg", "Alsh", "Arai"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(6900, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

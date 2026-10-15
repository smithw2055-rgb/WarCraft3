// Batch-9 tests: orb attack modifiers, illusion/lightning wands,
// ankh reincarnation, beast scroll, healing ward.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells3.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells9.h"
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

CUnitType* MakeSpell9Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell9Type(), player,
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

bool TestOrbOfFrostChills() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 901, 0, 0, 0);
    auto* target = AdoptAt(sim, 902, 1, 100, 0);
    Settle(sim);
    attacker->AddAbility(sim, std::make_unique<CAbilityOrbOfFrost>(9001));
    const float speedBefore = target->GetSpeed();
    target->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(target->GetSpeed() < speedBefore);
    return true;
}

bool TestOrbOfCorruptionStrips() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 911, 0, 0, 0);
    auto* target = AdoptAt(sim, 912, 1, 100, 0);
    Settle(sim);
    attacker->AddAbility(sim, std::make_unique<CAbilityOrbOfCorruption>(9101));
    const int armorBefore = target->GetTotalDefense();
    target->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(target->GetBuff(War3ID("AIcb")) != nullptr);
    TEST_ASSERT(target->GetTotalDefense() == armorBefore - 3);
    return true;
}

bool TestOrbOfFireProcs() {
    CSimulation sim;
    AddPlayers(sim);
    auto* plain = AdoptAt(sim, 921, 0, 0, 0);
    auto* holder = AdoptAt(sim, 922, 0, 0, 100);
    auto* targetA = AdoptAt(sim, 923, 1, 100, 0);
    auto* targetB = AdoptAt(sim, 924, 1, 100, 100);
    Settle(sim);
    holder->AddAbility(sim, std::make_unique<CAbilityOrbOfFire>(9201));
    const float beforeA = targetA->GetLife();
    const float beforeB = targetB->GetLife();
    targetA->Damage(sim, plain, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 100.0f, 0);
    targetB->Damage(sim, holder, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 100.0f, 0);
    const float lossPlain = beforeA - targetA->GetLife();
    const float lossOrb = beforeB - targetB->GetLife();
    TEST_ASSERT(lossOrb > lossPlain);
    TEST_NEAR(lossOrb - lossPlain, 15.0f, 0.01f); // fire proc lands on top
    return true;
}

bool TestAttackModifiersDoNotStack() {
    // Retail orb rule via the replacement slot: the second installed
    // modifier replaces the first (no double proc); removing it leaves
    // the slot empty (the first is not restored).
    CSimulation sim;
    AddPlayers(sim);
    auto* holder = AdoptAt(sim, 925, 0, 0, 0);
    auto* target = AdoptAt(sim, 926, 1, 100, 0);
    Settle(sim);
    holder->AddAbility(sim, std::make_unique<CAbilityOrbOfFrost>(9202));
    TEST_ASSERT(holder->GetAttackModifier(0) != nullptr);
    holder->AddAbility(sim, std::make_unique<CAbilityOrbOfFire>(9203));
    const float before = target->GetLife();
    target->Damage(sim, holder, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(before - target->GetLife(), 115.0f, 0.01f); // fire only
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) == nullptr); // frost gone
    // Removing the winner empties the slot; the replaced frost is gone.
    TEST_ASSERT(holder->RemoveAbility(sim, War3ID("AIfb")));
    TEST_ASSERT(holder->GetAttackModifier(0) == nullptr);
    auto* fresh = AdoptAt(sim, 927, 1, 100, 100);
    Settle(sim);
    const float before2 = fresh->GetLife();
    fresh->Damage(sim, holder, true, false, CAttackType::NORMAL,
                  CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(before2 - fresh->GetLife(), 100.0f, 0.01f);
    TEST_ASSERT(fresh->GetBuff(War3ID("Bslo")) == nullptr);
    return true;
}

bool TestAttackModifierRemovedWithAbility() {
    CSimulation sim;
    AddPlayers(sim);
    auto* holder = AdoptAt(sim, 928, 0, 0, 0);
    auto* first = AdoptAt(sim, 929, 1, 100, 0);
    Settle(sim);
    holder->AddAbility(sim, std::make_unique<CAbilityOrbOfFrost>(9204));
    first->Damage(sim, holder, true, false, CAttackType::NORMAL,
                  CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(first->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(holder->RemoveAbility(sim, War3ID("AIob")));
    auto* second = AdoptAt(sim, 930, 1, 100, 100);
    Settle(sim);
    second->Damage(sim, holder, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(second->GetBuff(War3ID("Bslo")) == nullptr);
    return true;
}

bool TestIllusionCopiesTarget() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* target = sim.CreateUnit(War3ID("hfoo"), 1, 100, 0, 0);
    TEST_ASSERT(caster && target);
    Settle(sim);
    CAbilityIllusion spell(9301);
    spell.DoEffect(sim, caster, target);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    int copies = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("hfoo") &&
            unit->GetPlayerIndex() == 0 && unit != caster && unit->IsSummoned()) {
            copies++;
        }
    }
    TEST_ASSERT(copies == 1);
    return true;
}

bool TestLightningShieldWandZaps() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 941, 0, 0, 0);
    auto* target = AdoptAt(sim, 942, 1, 100, 0);
    Settle(sim);
    CAbilityLightningShieldWand spell(9401);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Blsh")) != nullptr);
    return true;
}

bool TestAnkhReincarnates() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* bearer = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* killer = sim.CreateUnit(War3ID("hfoo"), 1, 100, 0, 0);
    TEST_ASSERT(bearer && killer);
    Settle(sim);
    bearer->AddAbility(sim, std::make_unique<CAbilityReincarnation>(
        9501, War3ID("AIrc"), 0));
    bearer->Damage(sim, killer, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(!bearer->IsDead()); // ankh brought the bearer back
    TEST_ASSERT(bearer->GetLife() > 0.0f);
    return true;
}

bool TestBeastScrollRallies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 961, 0, 0, 0);
    auto* near = AdoptAt(sim, 962, 0, 100, 0);
    auto* foe = AdoptAt(sim, 963, 1, 100, 100);
    Settle(sim);
    const int nearBefore = near->GetBonusDamage();
    const int foeBefore = foe->GetBonusDamage();
    CAbilityBeastScroll spell(9601);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(near->GetBonusDamage() == nearBefore + 4);
    TEST_ASSERT(foe->GetBonusDamage() == foeBefore);
    return true;
}

bool TestHealingWardHeals() {
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
    CAbilityHealingWard spell(9701);
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

bool TestSpells9FactoryCoverage() {
    const char* codes[] = {"AIob", "AIcb", "AIfb", "AIil",
                           "AIls", "AIrc", "AIrr", "AIhw"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(9800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

// Batch-11 tests: stealth system (wind walk, invisibility, permanent
// invisibility, shadowmeld, dust reveal) plus berserk, summon bear,
// battle roar.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_attack.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells11.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
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

CUnitType* MakeSpell11Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell11Type(), player,
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

bool TestWindWalkHidesAndBreaks() {
    CSimulation sim;
    AddPlayers(sim);
    auto* walker = AdoptAt(sim, 1101, 0, 0, 0);
    auto* plain = AdoptAt(sim, 1102, 0, 0, 100);
    auto* targetA = AdoptAt(sim, 1103, 1, 100, 0);
    auto* targetB = AdoptAt(sim, 1104, 1, 100, 100);
    Settle(sim);
    CAbilityWindWalk spell(11001);
    spell.DoEffect(sim, walker);
    TEST_ASSERT(walker->GetBuff(War3ID("BOwk")) != nullptr);
    TEST_ASSERT(walker->IsInvisibleTo(targetA));
    TEST_ASSERT(!walker->IsInvisibleTo(plain)); // own side still sees
    // Attack orders cannot be issued against the unseen.
    CAbilityAttack attack(11002);
    TEST_ASSERT(!attack.CheckCanTarget(sim, targetA, OrderIds::ATTACK, walker));
    TEST_ASSERT(attack.CheckCanTarget(sim, targetA, OrderIds::ATTACK, plain));
    // Landing a hit breaks the walk with a backstab bonus on top.
    const float beforeA = targetA->GetLife();
    const float beforeB = targetB->GetLife();
    targetA->Damage(sim, plain, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 100.0f, 0);
    targetB->Damage(sim, walker, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(walker->GetBuff(War3ID("BOwk")) == nullptr);
    TEST_ASSERT(!walker->IsInvisibleTo(targetA));
    TEST_NEAR((beforeB - targetB->GetLife()) - (beforeA - targetA->GetLife()),
              40.0f, 0.01f);
    return true;
}

bool TestSorceressInvisibility() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1111, 0, 0, 0);
    auto* ally = AdoptAt(sim, 1112, 0, 100, 0);
    auto* foe = AdoptAt(sim, 1113, 1, 200, 0);
    Settle(sim);
    CAbilityInvisibility spell(11101);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("BOwk")) != nullptr);
    TEST_ASSERT(ally->IsInvisibleTo(foe));
    return true;
}

bool TestPermanentInvisibility() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1121, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1122, 1, 200, 0);
    Settle(sim);
    TEST_ASSERT(!unit->IsInvisibleTo(foe));
    unit->AddAbility(sim, std::make_unique<CAbilityPermanentInvisibility>(11201));
    TEST_ASSERT(unit->IsInvisibleTo(foe));
    TEST_ASSERT(unit->RemoveAbility(sim, War3ID("Apiv")));
    TEST_ASSERT(!unit->IsInvisibleTo(foe));
    return true;
}

bool TestShadowmeldNeedsStillness() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1131, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1132, 1, 200, 0);
    Settle(sim);
    unit->AddAbility(sim, std::make_unique<CAbilityShadowmeld>(11301));
    Settle(sim); // no movement: the meld settles in
    TEST_ASSERT(unit->IsInvisibleTo(foe));
    unit->SetPosition(500.0f, 500.0f);
    sim.Update(); // movement breaks the meld
    TEST_ASSERT(!unit->IsInvisibleTo(foe));
    return true;
}

bool TestDustRevealsArea() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1141, 0, 0, 0);
    auto* hidden = AdoptAt(sim, 1142, 1, 100, 100);
    auto* far = AdoptAt(sim, 1143, 1, 5000, 5000);
    Settle(sim);
    hidden->AddAbility(sim, std::make_unique<CAbilityPermanentInvisibility>(11401));
    TEST_ASSERT(hidden->IsInvisibleTo(caster));
    CAbilityDustOfAppearance spell(11402);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(hidden->GetBuff(War3ID("Bfae")) != nullptr);
    TEST_ASSERT(!hidden->IsInvisibleTo(caster)); // dusted: targetable again
    TEST_ASSERT(far->GetBuff(War3ID("Bfae")) == nullptr);
    return true;
}

bool TestFaerieFireReveals() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1151, 0, 0, 0);
    auto* hidden = AdoptAt(sim, 1152, 1, 100, 0);
    Settle(sim);
    hidden->AddAbility(sim, std::make_unique<CAbilityPermanentInvisibility>(11501));
    TEST_ASSERT(hidden->IsInvisibleTo(caster));
    // Faerie Fire's reveal half now has a system to counter (ref note).
    hidden->AddBuff(std::make_unique<CBuffArmor>(
        War3ID("Bfae"), 20.0f, -2, false), sim);
    TEST_ASSERT(!hidden->IsInvisibleTo(caster));
    return true;
}

bool TestBerserkHastesAndSoftens() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 1161, 1, 0, 0);
    auto* plain = AdoptAt(sim, 1162, 0, 100, 0);
    auto* wild = AdoptAt(sim, 1163, 0, 200, 0);
    Settle(sim);
    CAbilityBerserk spell(11601);
    spell.DoEffect(sim, wild);
    TEST_ASSERT(wild->GetBuff(War3ID("Babr")) != nullptr);
    TEST_NEAR(wild->GetAttackSpeedMultiplier(), 1.4f, 0.001f);
    const float plainBefore = plain->GetLife();
    const float wildBefore = wild->GetLife();
    plain->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                  CDamageType::NORMAL, "", 200.0f, 0);
    wild->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                 CDamageType::NORMAL, "", 200.0f, 0);
    TEST_NEAR((wildBefore - wild->GetLife()) / (plainBefore - plain->GetLife()),
              1.25f, 0.01f);
    wild->RemoveBuff(War3ID("Babr"), sim);
    TEST_NEAR(wild->GetAttackSpeedMultiplier(), 1.0f, 0.001f);
    return true;
}

bool TestSummonBearStands() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("ngz1"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    CAbilitySummonBear spell(11701);
    spell.DoEffect(sim, caster);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    bool bearFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("ngz1") &&
            unit->GetPlayerIndex() == 0) {
            bearFound = true;
        }
    }
    TEST_ASSERT(bearFound);
    return true;
}

bool TestBattleRoarRallies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1181, 0, 0, 0);
    auto* near = AdoptAt(sim, 1182, 0, 100, 0);
    auto* foe = AdoptAt(sim, 1183, 1, 100, 100);
    Settle(sim);
    const int nearBefore = near->GetBonusDamage();
    const int foeBefore = foe->GetBonusDamage();
    CAbilityBattleRoar spell(11801);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(near->GetBonusDamage() == nearBefore + 6);
    TEST_ASSERT(foe->GetBonusDamage() == foeBefore);
    return true;
}

bool TestSpells11FactoryCoverage() {
    const char* codes[] = {"AOwk", "Aivs", "Apiv", "Ashm",
                           "AItb", "Absk", "ANsg", "ANbr"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(11900, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

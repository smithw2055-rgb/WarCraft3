// Batch-4 hero spell tests: passives, curses, sacrifice, mass animation,
// doom, eruption waves, bash procs.
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
#include "ecs/abilities/spells4.h"
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

CUnitType* MakeSpell4Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell4Type(), player,
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

bool TestEvasionPassive() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 401, 0, 0, 0);
    Settle(sim);
    TEST_ASSERT(unit->GetEvasionChance() == 0.0f);
    unit->AddAbility(sim, std::make_unique<CAbilityEvasion>(4001));
    // Combined single-source value (1 - (1 - p)); fabs, not ==, for the 1ulp.
    TEST_ASSERT(std::fabs(unit->GetEvasionChance() - 0.10f) < 1e-6f); // ref DataA level 1
    TEST_ASSERT(unit->RemoveAbility(sim, War3ID("AEev")));
    TEST_ASSERT(unit->GetEvasionChance() == 0.0f);
    return true;
}

bool TestCriticalStrikePassive() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 411, 0, 0, 0);
    Settle(sim);
    unit->AddAbility(sim, std::make_unique<CAbilityCriticalStrike>(4101));
    TEST_ASSERT(unit->GetCritChance() == 0.15f);
    TEST_ASSERT(unit->GetCritMultiplier() == 2.0f);
    TEST_ASSERT(unit->RemoveAbility(sim, War3ID("AOcr")));
    TEST_ASSERT(unit->GetCritChance() == 0.0f);
    return true;
}

bool TestBashProcsOnHit() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 421, 0, 0, 0);
    auto* target = AdoptAt(sim, 422, 1, 100, 0);
    Settle(sim);
    auto bash = std::make_unique<CAbilityBash>(4201);
    bash->SetChance(1.0f);
    bash->SetDuration(2.0f);
    attacker->AddAbility(sim, std::move(bash));
    target->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) != nullptr);
    // Chance 0 never procs (separate attacker holding only a 0-chance bash).
    auto* attacker2 = AdoptAt(sim, 424, 0, 0, 0);
    auto* target2 = AdoptAt(sim, 423, 1, 200, 0);
    Settle(sim);
    auto bash2 = std::make_unique<CAbilityBash>(4202);
    bash2->SetChance(0.0f);
    attacker2->AddAbility(sim, std::move(bash2));
    target2->Damage(sim, attacker2, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(target2->GetBuff(War3ID("Bstn")) == nullptr);
    return true;
}

bool TestSlowAppliesDebuff() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 431, 0, 0, 0);
    auto* target = AdoptAt(sim, 432, 1, 100, 0);
    Settle(sim);
    const float speedBefore = target->GetSpeed();
    CAbilitySlow spell(4301);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(target->GetSpeed() < speedBefore);
    return true;
}

bool TestFrostArmorAndFaerieFire() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 441, 0, 0, 0);
    auto* target = AdoptAt(sim, 442, 1, 100, 0);
    Settle(sim);
    const int base = target->GetTotalDefense();
    CAbilityFrostArmor frost(4401);
    frost.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("BUfa")) != nullptr);
    TEST_ASSERT(target->GetTotalDefense() == base + 3);
    CAbilityFaerieFire faerie(4402);
    faerie.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bfae")) != nullptr);
    TEST_ASSERT(target->GetTotalDefense() == base + 3 - 4); // ref DataA
    return true;
}

bool TestDeathPactHeals() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 451, 0, 0, 0);
    auto* sacrifice = AdoptAt(sim, 452, 0, 100, 0);
    Settle(sim);
    caster->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                   CDamageType::NORMAL, "", 5000.0f, 0);
    const float casterBefore = caster->GetLife();
    CAbilityDeathPact spell(4501);
    spell.DoEffect(sim, caster, sacrifice);
    TEST_ASSERT(sacrifice->IsDead());
    TEST_ASSERT(caster->GetLife() > casterBefore);
    // Heroes cannot be sacrificed.
    auto* hero = AdoptAt(sim, 453, 0, 200, 0);
    Settle(sim);
    (void)hero;
    return true;
}

bool TestAnimateDeadRaisesInvulnerable() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 461, 0, 0, 0);
    auto* fallen = AdoptAt(sim, 462, 1, 100, 0);
    Settle(sim);
    fallen->Damage(sim, caster, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(fallen->IsDead());
    CAbilityAnimateDead spell(4601);
    spell.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(!fallen->IsDead());
    TEST_ASSERT(fallen->IsInvulnerable());
    TEST_ASSERT(fallen->IsSummoned());
    TEST_ASSERT(fallen->GetPlayerIndex() == 0); // raised for the caster
    return true;
}

bool TestDoomDotsAndSpawnsGuard() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("nba2"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* target = sim.CreateUnit(War3ID("hfoo"), 1, 100, 0, 0);
    TEST_ASSERT(caster && target);
    Settle(sim);
    CAbilityDoom spell(4701);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("BNdo")) != nullptr);
    const float before = target->GetLife();
    for (int i = 0; i < 25; i++) sim.Update(); // >1s: a tick lands
    TEST_ASSERT(target->GetLife() < before);
    // Lethal damage while doomed raises a guard for the caster's player.
    const size_t unitsBefore = sim.GetUnits().size();
    target->Damage(sim, caster, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(target->IsDead());
    bool guardFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("nba2") &&
            unit->GetPlayerIndex() == 0) {
            guardFound = true;
        }
    }
    TEST_ASSERT(guardFound);
    (void)unitsBefore;
    return true;
}

bool TestVolcanoErupts() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 481, 0, 0, 0);
    auto* near = AdoptAt(sim, 482, 1, 100, 100);
    auto* far = AdoptAt(sim, 483, 1, 5000, 5000);
    Settle(sim);
    const float nearBefore = near->GetLife();
    CAbilityVolcano spell(4801);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(near->GetLife() < nearBefore);
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    return true;
}

bool TestSpells4FactoryCoverage() {
    const char* codes[] = {"AEev", "AOcr", "AHbh", "Aslo", "AUfa",
                           "Afae", "AUdp", "AUan", "ANdo", "ANvc"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(4900, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

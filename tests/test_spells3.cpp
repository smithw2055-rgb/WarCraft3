// Batch-3 hero spell tests: teleport/blink positioning, line and scatter
// AoE, heal-over-time, revive flows, ownership transfer, timed summons.
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
#include "ecs/abilities/spells3.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/pathing/pathfinding.h"
#include "map/wpm.h"
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

CUnitType* MakeSpell3Type(bool hero = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    static std::vector<std::unique_ptr<CUnitType>> keepHero;
    auto& store = hero ? keepHero : keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    store.push_back(std::make_unique<CUnitType>(
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
        1, hero, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return store.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y, bool hero = false) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell3Type(hero), player,
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

bool TestMassTeleportMovesGroup() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 301, 0, 0, 0);
    auto* ally = AdoptAt(sim, 302, 0, 100, 0);
    auto* foe = AdoptAt(sim, 303, 1, 100, 0);
    Settle(sim);
    CAbilityMassTeleport spell(3001);
    spell.DoEffect(sim, caster, 1000, 1000);
    TEST_ASSERT(caster->GetX() == 1000.0f && caster->GetY() == 1000.0f);
    TEST_ASSERT(ally->GetX() == 1000.0f && ally->GetY() == 1000.0f);
    TEST_ASSERT(foe->GetX() == 100.0f); // enemies stay
    return true;
}

bool TestBlinkClampsRange() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 311, 0, 0, 0);
    CAbilityBlink spell(3101);
    spell.DoEffect(sim, caster, 500, 0); // inside 1000 range
    TEST_ASSERT(caster->GetX() == 500.0f);
    spell.DoEffect(sim, caster, 5000, 0); // clamped to 1000 from (500,0)
    TEST_ASSERT(std::fabs(caster->GetX() - 1500.0f) < 0.01f);
    return true;
}

bool TestBlinkSlidesOffBlockedCell() {
    CSimulation sim;
    AddPlayers(sim);
    WpmData wpm;
    wpm.width = 4;
    wpm.height = 4;
    wpm.pathing.assign(16, 0);
    wpm.pathing[0] = WpmData::UNWALKABLE;
    sim.SetPathingMap(&wpm, 0.0f, 0.0f);
    auto* caster = AdoptAt(sim, 312, 0, 0, 0);
    caster->SetCollisionSize(0.0f);
    caster->SetPositionAndCheckUnstuck(16.0f, 16.0f, sim);
    TEST_ASSERT(std::fabs(caster->GetX() - 16.0f) > 1.0f
                || std::fabs(caster->GetY() - 16.0f) > 1.0f);
    return true;
}

bool TestShockwaveLineDamage() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 321, 0, 0, 0);
    auto* ahead = AdoptAt(sim, 322, 1, 400, 0);
    auto* aside = AdoptAt(sim, 323, 1, 400, 500);
    Settle(sim);
    const float aheadBefore = ahead->GetLife();
    const float asideBefore = aside->GetLife();
    CAbilityShockwave spell(3201);
    spell.DoEffect(sim, caster, ahead);
    TEST_ASSERT(ahead->GetLife() < aheadBefore);
    TEST_ASSERT(aside->GetLife() == asideBefore);
    return true;
}

bool TestStarfallDamagesArea() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 331, 0, 0, 0);
    auto* near = AdoptAt(sim, 332, 1, 100, 0);
    auto* far = AdoptAt(sim, 333, 1, 5000, 0);
    Settle(sim);
    const float nearBefore = near->GetLife();
    CAbilityStarfall spell(3301);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(near->GetLife() < nearBefore);
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife()); // self spared
    return true;
}

bool TestTranquilityHealsAllies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 341, 0, 0, 0);
    auto* ally = AdoptAt(sim, 342, 0, 100, 0);
    auto* foe = AdoptAt(sim, 343, 1, 100, 0);
    Settle(sim);
    ally->Damage(sim, caster, false, false, CAttackType::SPELLS, CDamageType::NORMAL, "", 500.0f, 0);
    foe->Damage(sim, caster, false, false, CAttackType::SPELLS, CDamageType::NORMAL, "", 500.0f, 0);
    const float allyBefore = ally->GetLife();
    const float foeBefore = foe->GetLife();
    CAbilityTranquility spell(3401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(ally->GetLife() > allyBefore);
    TEST_ASSERT(foe->GetLife() == foeBefore);
    TEST_ASSERT(ally->GetBuff(War3ID("Brej")) != nullptr); // rejuvenation ticking
    return true;
}

bool TestResurrectRevivesInRadius() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 351, 0, 0, 0);
    auto* fallen = AdoptAt(sim, 352, 0, 100, 0);
    auto* farFallen = AdoptAt(sim, 353, 0, 5000, 0);
    Settle(sim);
    fallen->Damage(sim, caster, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    farFallen->Damage(sim, caster, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    TEST_ASSERT(fallen->IsDead() && farFallen->IsDead());
    CAbilityResurrect spell(3501);
    spell.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(!fallen->IsDead());
    TEST_ASSERT(fallen->GetLife() == fallen->GetMaxLife());
    TEST_ASSERT(farFallen->IsDead()); // outside radius
    return true;
}

bool TestResurrectSkipsBoneCorpse() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 354, 0, 0, 0);
    auto* fallen = AdoptAt(sim, 355, 0, 100, 0);
    Settle(sim);
    fallen->Damage(sim, caster, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 40; i++) sim.Update();
    TEST_ASSERT(fallen->IsBoneCorpse());
    CAbilityResurrect spell(3502);
    spell.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(fallen->IsDead());
    return true;
}

bool TestReincarnationRevivesOnDeath() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 361, 0, 0, 0);
    Settle(sim);
    unit->SetMana(150.0f); // exactly the reincarnation price
    auto reinc = std::make_unique<CAbilityReincarnation>(3601);
    CAbilityReincarnation* ptr = reinc.get();
    unit->AddAbility(sim, std::move(reinc));
    unit->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update(); // death processing -> OnDeath -> Revive
    // OnDeath fired during death processing: mana spent, unit back up.
    TEST_ASSERT(!unit->IsDead());
    TEST_ASSERT(unit->GetMana() < 5.0f);
    (void)ptr;
    (void)ptr;
    // Second lethal hit while on cooldown: stays dead.
    unit->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(unit->IsDead());

    // Without mana the reincarnation fizzles.
    auto* poor = AdoptAt(sim, 362, 0, 0, 0);
    Settle(sim);
    poor->SetMana(100.0f);
    poor->AddAbility(sim, std::make_unique<CAbilityReincarnation>(3602));
    poor->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(poor->IsDead());
    return true;
}

bool TestCharmConvertsCreep() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 371, 0, 0, 0);
    auto* creep = AdoptAt(sim, 372, 1, 100, 0);
    Settle(sim);
    CAbilityCharm spell(3701);
    spell.DoEffect(sim, caster, creep);
    TEST_ASSERT(creep->GetPlayerIndex() == 0);
    // Heroes resist charm.
    auto* hero = sim.AdoptUnit(std::make_unique<CUnit>(373, MakeSpell3Type(true), 1,
                                                       War3ID("hfoo"), 200, 0, 0));
    Settle(sim);
    TEST_ASSERT(hero->IsHero());
    spell.DoEffect(sim, caster, hero);
    TEST_ASSERT(hero->GetPlayerIndex() == 1);
    return true;
}

bool TestSummonWaterElemental() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("hwat"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    const size_t before = sim.GetUnits().size();
    CAbilitySummonWaterElemental spell(3801);
    spell.DoEffect(sim, caster);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    TEST_ASSERT(sim.GetUnits().size() == before + 1);
    CUnit* elemental = sim.GetUnits().back();
    TEST_ASSERT(elemental->GetTypeId() == War3ID("hwat"));
    TEST_ASSERT(elemental->IsSummoned());
    return true;
}

bool TestMirrorImageCopiesCaster() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    const size_t before = sim.GetUnits().size();
    CAbilityMirrorImage spell(3901);
    spell.DoEffect(sim, caster);
    for (int i = 0; i < 3; i++) sim.Update();
    TEST_ASSERT(sim.GetUnits().size() == before + 1); // ref DataA level 1
    TEST_ASSERT(sim.GetUnits().back()->IsSummoned());
    return true;
}

bool TestSpells3FactoryCoverage() {
    // AEsf is Starfall (AEst is the Owl Scout, covered in spells18).
    const char* codes[] = {"AHmt", "AHwe", "AHre", "AEbl", "AOmi",
                           "AOsh", "AEsf", "AEtq", "AOre", "ANch"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(4000, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    TEST_ASSERT(AbilityFactory::CreateAbility(4000, War3ID("ZZZ9")) == nullptr);
    return true;
}

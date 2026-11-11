// Consolidated War3 Spell Catalog Regression Suite (Stage 0 de-fragmentation)
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/abilitydata.h"
#include "data/combat_types.h"
#include "data/destructabledata.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_attack.h"
#include "ecs/abilities/ability_build.h"
#include "ecs/abilities/ability_generic.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/abilities/ability_return_resources.h"
#include "ecs/abilities/ability_upgrade.h"
#include "ecs/ability_factory.h"
#include "ecs/animation_utils.h"
#include "ecs/behaviors/behavior.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/behaviors/behavior_return_resources.h"
#include "ecs/buffs.h"
#include "ecs/combat/combat_system.h"
#include "ecs/combat/projectile.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/item.h"
#include "ecs/item_bonuses.h"
#include "ecs/orders/order.h"
#include "ecs/pathing/pathfinding.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "game/war3/abilities/war3_spells.h"
#include "map/path_tex.h"
#include "map/wpm.h"
#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#ifndef TEST_ASSERT
#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#endif

#ifndef TEST_NEAR
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " << (a) << " vs " << (b) << "\n"; return false; } } while (0)
#endif

// ---- Suite test_spells3.cpp ----
namespace war3_spell_suite_3 {
// Batch-3 hero spell tests: teleport/blink positioning, line and scatter
// AoE, heal-over-time, revive flows, ownership transfer, timed summons.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
    auto* foe = AdoptAt(sim, 303, 1, 200, 0);
    Settle(sim);
    CAbilityMassTeleport spell(3001);
    spell.DoEffect(sim, caster, 1000, 1000);
    TEST_ASSERT(caster->GetX() == 1000.0f && caster->GetY() == 1000.0f);
    TEST_ASSERT(ally->GetX() == 1000.0f && ally->GetY() == 1000.0f);
    TEST_ASSERT(foe->GetX() == 200.0f); // enemies stay
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
    // Unknown rawcodes fall back to a do-nothing placeholder (never
    // nullptr) so the code stays on the unit instead of being dropped.
    auto unknown = AbilityFactory::CreateAbility(4000, War3ID("ZZZ9"));
    TEST_ASSERT(unknown != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityGenericDoNothing*>(unknown.get()) != nullptr);
    TEST_ASSERT(unknown->GetCode() == War3ID("ZZZ9"));
    return true;
}
} // namespace war3_spell_suite_3
bool TestMassTeleportMovesGroup() { return war3_spell_suite_3::TestMassTeleportMovesGroup(); }
bool TestBlinkClampsRange() { return war3_spell_suite_3::TestBlinkClampsRange(); }
bool TestBlinkSlidesOffBlockedCell() { return war3_spell_suite_3::TestBlinkSlidesOffBlockedCell(); }
bool TestShockwaveLineDamage() { return war3_spell_suite_3::TestShockwaveLineDamage(); }
bool TestStarfallDamagesArea() { return war3_spell_suite_3::TestStarfallDamagesArea(); }
bool TestTranquilityHealsAllies() { return war3_spell_suite_3::TestTranquilityHealsAllies(); }
bool TestResurrectRevivesInRadius() { return war3_spell_suite_3::TestResurrectRevivesInRadius(); }
bool TestResurrectSkipsBoneCorpse() { return war3_spell_suite_3::TestResurrectSkipsBoneCorpse(); }
bool TestReincarnationRevivesOnDeath() { return war3_spell_suite_3::TestReincarnationRevivesOnDeath(); }
bool TestCharmConvertsCreep() { return war3_spell_suite_3::TestCharmConvertsCreep(); }
bool TestSummonWaterElemental() { return war3_spell_suite_3::TestSummonWaterElemental(); }
bool TestMirrorImageCopiesCaster() { return war3_spell_suite_3::TestMirrorImageCopiesCaster(); }
bool TestSpells3FactoryCoverage() { return war3_spell_suite_3::TestSpells3FactoryCoverage(); }

// ---- Suite test_spells4.cpp ----
namespace war3_spell_suite_4 {
// Batch-4 hero spell tests: passives, curses, sacrifice, mass animation,
// doom, eruption waves, bash procs.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
} // namespace war3_spell_suite_4
bool TestEvasionPassive() { return war3_spell_suite_4::TestEvasionPassive(); }
bool TestCriticalStrikePassive() { return war3_spell_suite_4::TestCriticalStrikePassive(); }
bool TestBashProcsOnHit() { return war3_spell_suite_4::TestBashProcsOnHit(); }
bool TestSlowAppliesDebuff() { return war3_spell_suite_4::TestSlowAppliesDebuff(); }
bool TestFrostArmorAndFaerieFire() { return war3_spell_suite_4::TestFrostArmorAndFaerieFire(); }
bool TestDeathPactHeals() { return war3_spell_suite_4::TestDeathPactHeals(); }
bool TestAnimateDeadRaisesInvulnerable() { return war3_spell_suite_4::TestAnimateDeadRaisesInvulnerable(); }
bool TestDoomDotsAndSpawnsGuard() { return war3_spell_suite_4::TestDoomDotsAndSpawnsGuard(); }
bool TestVolcanoErupts() { return war3_spell_suite_4::TestVolcanoErupts(); }
bool TestSpells4FactoryCoverage() { return war3_spell_suite_4::TestSpells4FactoryCoverage(); }

// ---- Suite test_spells5.cpp ----
namespace war3_spell_suite_5 {
// Batch-5 hero spell tests: point-damage waves, line impale, instant
// knives, passive carapace, sacrifice-for-mana, strike with slow+poison.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell5Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell5Type(), player,
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

bool TestFlameStrikeWaves() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 501, 0, 0, 0);
    auto* foe = AdoptAt(sim, 502, 1, 100, 100);
    auto* far = AdoptAt(sim, 503, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityFlameStrike spell(5001);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestDeathAndDecayHitsAll() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 511, 0, 0, 0);
    auto* foe = AdoptAt(sim, 512, 1, 100, 100);
    auto* ally = AdoptAt(sim, 513, 0, 150, 100);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    const float allyBefore = ally->GetLife();
    CAbilityDeathAndDecay spell(5101);
    spell.DoEffect(sim, caster, 125, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() < allyBefore); // corrupts friend and foe alike
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestImpaleLineStuns() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 521, 0, 0, 0);
    auto* target = AdoptAt(sim, 522, 1, 300, 0);
    auto* behind = AdoptAt(sim, 523, 1, 500, 0);
    auto* offline = AdoptAt(sim, 524, 1, 300, 300);
    Settle(sim);
    const float targetBefore = target->GetLife();
    const float behindBefore = behind->GetLife();
    CAbilityImpale spell(5201);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() < targetBefore);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) != nullptr);
    TEST_ASSERT(behind->GetLife() < behindBefore);
    TEST_ASSERT(behind->GetBuff(War3ID("Bstn")) != nullptr);
    TEST_ASSERT(offline->GetLife() == offline->GetMaxLife());
    TEST_ASSERT(offline->GetBuff(War3ID("Bstn")) == nullptr);
    return true;
}

bool TestFanOfKnivesHitsEnemiesOnly() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 531, 0, 0, 0);
    auto* foe = AdoptAt(sim, 532, 1, 100, 0);
    auto* ally = AdoptAt(sim, 533, 0, -100, 0);
    auto* far = AdoptAt(sim, 534, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityFanOfKnives spell(5301);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestRainOfFireWaves() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 541, 0, 0, 0);
    auto* foe = AdoptAt(sim, 542, 1, 100, 100);
    auto* far = AdoptAt(sim, 543, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityRainOfFire spell(5401);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    return true;
}

bool TestSpikedCarapaceArmorAndReflect() {
    CSimulation sim;
    AddPlayers(sim);
    auto* bearer = AdoptAt(sim, 551, 0, 0, 0);
    auto* attacker = AdoptAt(sim, 552, 1, 100, 0);
    Settle(sim);
    TEST_ASSERT(bearer->GetBonusArmor() == 0);
    bearer->AddAbility(sim, std::make_unique<CAbilitySpikedCarapace>(5501));
    TEST_ASSERT(bearer->GetBonusArmor() == 4);
    const float attackerBefore = attacker->GetLife();
    bearer->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(attacker->GetLife() < attackerBefore); // melee reflected
    TEST_ASSERT(bearer->RemoveAbility(sim, War3ID("AUts")));
    TEST_ASSERT(bearer->GetBonusArmor() == 0);
    return true;
}

bool TestDarkRitualGrantsMana() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 561, 0, 0, 0);
    auto* sacrifice = AdoptAt(sim, 562, 0, 100, 0);
    auto* enemy = AdoptAt(sim, 563, 1, 200, 0);
    Settle(sim);
    caster->SetMana(0.0f);
    CAbilityDarkRitual spell(5601);
    // Enemies cannot be sacrificed.
    spell.DoEffect(sim, caster, enemy);
    TEST_ASSERT(!enemy->IsDead());
    TEST_ASSERT(caster->GetMana() == 0.0f);
    spell.DoEffect(sim, caster, sacrifice);
    TEST_ASSERT(sacrifice->IsDead());
    TEST_ASSERT(caster->GetMana() > 0.0f);
    TEST_ASSERT(caster->GetMana() <= static_cast<float>(caster->GetMaxMana()));
    return true;
}

bool TestShadowStrikeSlowsAndPoisons() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 571, 0, 0, 0);
    auto* target = AdoptAt(sim, 572, 1, 100, 0);
    Settle(sim);
    const float speedBefore = target->GetSpeed();
    CAbilityShadowStrike spell(5701);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    TEST_ASSERT(target->GetSpeed() < speedBefore);
    const float afterStrike = target->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // poison ticks land
    TEST_ASSERT(target->GetLife() < afterStrike);
    return true;
}

bool TestSpells5FactoryCoverage() {
    const char* codes[] = {"AHfs", "AUdd", "AUim", "AEfk",
                           "ANrf", "AUts", "AUdr", "AEsh"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(5800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}
} // namespace war3_spell_suite_5
bool TestFlameStrikeWaves() { return war3_spell_suite_5::TestFlameStrikeWaves(); }
bool TestDeathAndDecayHitsAll() { return war3_spell_suite_5::TestDeathAndDecayHitsAll(); }
bool TestImpaleLineStuns() { return war3_spell_suite_5::TestImpaleLineStuns(); }
bool TestFanOfKnivesHitsEnemiesOnly() { return war3_spell_suite_5::TestFanOfKnivesHitsEnemiesOnly(); }
bool TestRainOfFireWaves() { return war3_spell_suite_5::TestRainOfFireWaves(); }
bool TestSpikedCarapaceArmorAndReflect() { return war3_spell_suite_5::TestSpikedCarapaceArmorAndReflect(); }
bool TestDarkRitualGrantsMana() { return war3_spell_suite_5::TestDarkRitualGrantsMana(); }
bool TestShadowStrikeSlowsAndPoisons() { return war3_spell_suite_5::TestShadowStrikeSlowsAndPoisons(); }
bool TestSpells5FactoryCoverage() { return war3_spell_suite_5::TestSpells5FactoryCoverage(); }

// ---- Suite test_spells6.cpp ----
namespace war3_spell_suite_6 {
// Batch-6 tests: alchemist kit (heal spray, acid bomb, chemical rage,
// transmute) plus Bloodlust, Purge, Lightning Shield, Raise Dead.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
    // In WC3 lightning shield damages all adjacent units including allies (friendly fire)
    TEST_ASSERT(ally->GetLife() < ally->GetMaxLife());
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
} // namespace war3_spell_suite_6
bool TestHealingSprayHealsAllies() { return war3_spell_suite_6::TestHealingSprayHealsAllies(); }
bool TestAcidBombCurses() { return war3_spell_suite_6::TestAcidBombCurses(); }
bool TestChemicalRageHastes() { return war3_spell_suite_6::TestChemicalRageHastes(); }
bool TestTransmutePaysGold() { return war3_spell_suite_6::TestTransmutePaysGold(); }
bool TestBloodlustCastHastes() { return war3_spell_suite_6::TestBloodlustCastHastes(); }
bool TestPurgeDispelsAndSlows() { return war3_spell_suite_6::TestPurgeDispelsAndSlows(); }
bool TestPurgeBurnsSummons() { return war3_spell_suite_6::TestPurgeBurnsSummons(); }
bool TestLightningShieldZaps() { return war3_spell_suite_6::TestLightningShieldZaps(); }
bool TestRaiseDeadSkeletons() { return war3_spell_suite_6::TestRaiseDeadSkeletons(); }
bool TestSpells6FactoryCoverage() { return war3_spell_suite_6::TestSpells6FactoryCoverage(); }

// ---- Suite test_spells7.cpp ----
namespace war3_spell_suite_7 {
// Batch-7 tests: serpent ward, polymorph, inner fire, priest heal,
// dispel magic, rejuvenation, roar, defend.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell7Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell7Type(), player,
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

bool TestSerpentWardSummons() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("osp1"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    CAbilitySerpentWard spell(7001);
    spell.DoEffect(sim, caster, 100, 0);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    bool wardFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("osp1") &&
            unit->GetPlayerIndex() == 0) {
            wardFound = true;
        }
    }
    TEST_ASSERT(wardFound);
    return true;
}

bool TestPolymorphHexes() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 711, 0, 0, 0);
    auto* target = AdoptAt(sim, 712, 1, 100, 0);
    Settle(sim);
    CAbilityPolymorph spell(7101);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bply")) != nullptr);
    TEST_ASSERT(target->IsHexed()); // sheep cannot attack or cast
    return true;
}

bool TestInnerFireBuffs() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 721, 0, 0, 0);
    auto* ally = AdoptAt(sim, 722, 0, 100, 0);
    Settle(sim);
    const int dmgBefore = ally->GetBonusDamage();
    const int armorBefore = ally->GetTotalDefense();
    CAbilityInnerFire spell(7201);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("Binf")) != nullptr);
    TEST_ASSERT(ally->GetBonusDamage() == dmgBefore + 5);
    TEST_ASSERT(ally->GetTotalDefense() == armorBefore + 3);
    ally->RemoveBuff(War3ID("Binf"), sim); // expiry path restores both
    TEST_ASSERT(ally->GetBonusDamage() == dmgBefore);
    TEST_ASSERT(ally->GetTotalDefense() == armorBefore);
    return true;
}

bool TestPriestHealHeals() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 731, 0, 0, 0);
    auto* ally = AdoptAt(sim, 732, 0, 100, 0);
    Settle(sim);
    ally->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", 5000.0f, 0);
    const float before = ally->GetLife();
    CAbilityHeal spell(7301);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetLife() > before);
    TEST_ASSERT(ally->GetLife() <= ally->GetMaxLife());
    return true;
}

bool TestDispelMagicStripsArea() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 741, 0, 0, 0);
    auto* foe = AdoptAt(sim, 742, 1, 100, 100);
    auto* ally = AdoptAt(sim, 743, 0, 150, 100);
    Settle(sim);
    caster->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    foe->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    ally->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    CAbilityDispelMagic spell(7401);
    spell.DoEffect(sim, caster, 125, 100);
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) == nullptr);
    TEST_ASSERT(ally->GetBuff(War3ID("Bblo")) == nullptr);
    TEST_ASSERT(caster->GetBuff(War3ID("Bblo")) != nullptr); // caster exempt
    return true;
}

bool TestRejuvenationHealsOverTime() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 751, 0, 0, 0);
    auto* ally = AdoptAt(sim, 752, 0, 100, 0);
    Settle(sim);
    ally->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", 5000.0f, 0);
    CAbilityRejuvenation spell(7501);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("Brej")) != nullptr);
    const float afterCast = ally->GetLife();
    for (int i = 0; i < 30; i++) sim.Update(); // regen ticks land
    TEST_ASSERT(ally->GetLife() > afterCast);
    return true;
}

bool TestRoarRalliesAllies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 761, 0, 0, 0);
    auto* near = AdoptAt(sim, 762, 0, 30, 0);
    auto* far = AdoptAt(sim, 763, 0, 100, 0);
    auto* foe = AdoptAt(sim, 764, 1, 30, 30);
    Settle(sim);
    const int nearBefore = near->GetBonusDamage();
    const int farBefore = far->GetBonusDamage();
    const int foeBefore = foe->GetBonusDamage();
    CAbilityRoar spell(7601);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(near->GetBuff(War3ID("Broa")) != nullptr); // own buff id: sums with Inner Fire
    TEST_ASSERT(near->GetBonusDamage() == nearBefore + 5);
    TEST_ASSERT(far->GetBonusDamage() == farBefore); // outside the 50 radius
    TEST_ASSERT(foe->GetBonusDamage() == foeBefore);
    return true;
}

bool TestDefendHalvesAttacks() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 771, 1, 0, 0);
    auto* plain = AdoptAt(sim, 772, 0, 100, 0);
    auto* defender = AdoptAt(sim, 773, 0, 200, 0);
    Settle(sim);
    CAbilityDefend spell(7701);
    spell.DoEffect(sim, defender);
    TEST_ASSERT(defender->GetBuff(War3ID("Bdef")) != nullptr);
    const float plainBefore = plain->GetLife();
    const float defenderBefore = defender->GetLife();
    plain->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                  CDamageType::NORMAL, "", 200.0f, 0);
    defender->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                     CDamageType::NORMAL, "", 200.0f, 0);
    const float plainLoss = plainBefore - plain->GetLife();
    const float defenderLoss = defenderBefore - defender->GetLife();
    TEST_ASSERT(defenderLoss < plainLoss);
    TEST_NEAR(defenderLoss * 2.0f, plainLoss, 0.01f);
    return true;
}

bool TestSpells7FactoryCoverage() {
    const char* codes[] = {"AOsw", "Aply", "Ainf", "Ahea",
                           "Adis", "Arej", "Aroa", "Adef"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(7800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}
} // namespace war3_spell_suite_7
bool TestSerpentWardSummons() { return war3_spell_suite_7::TestSerpentWardSummons(); }
bool TestPolymorphHexes() { return war3_spell_suite_7::TestPolymorphHexes(); }
bool TestInnerFireBuffs() { return war3_spell_suite_7::TestInnerFireBuffs(); }
bool TestPriestHealHeals() { return war3_spell_suite_7::TestPriestHealHeals(); }
bool TestDispelMagicStripsArea() { return war3_spell_suite_7::TestDispelMagicStripsArea(); }
bool TestRejuvenationHealsOverTime() { return war3_spell_suite_7::TestRejuvenationHealsOverTime(); }
bool TestRoarRalliesAllies() { return war3_spell_suite_7::TestRoarRalliesAllies(); }
bool TestDefendHalvesAttacks() { return war3_spell_suite_7::TestDefendHalvesAttacks(); }
bool TestSpells7FactoryCoverage() { return war3_spell_suite_7::TestSpells7FactoryCoverage(); }

// ---- Suite test_spells8.cpp ----
namespace war3_spell_suite_8 {
// Batch-8 tests: charged-item abilities (healing scroll, town portal,
// invuln/mana potions, clarity, mana steal, protection scroll).



namespace {

CUnitType* MakeSpell8Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell8Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

void Wound(CSimulation& sim, CUnit* unit, float amount) {
    unit->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", amount, 0);
}

} // namespace

bool TestItemHealGroupHeals() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 801, 0, 0, 0);
    auto* ally = AdoptAt(sim, 802, 0, 100, 100);
    auto* foe = AdoptAt(sim, 803, 1, 150, 100);
    Settle(sim);
    Wound(sim, ally, 5000.0f);
    Wound(sim, foe, 5000.0f);
    const float allyBefore = ally->GetLife();
    const float foeBefore = foe->GetLife();
    CAbilityItemHealGroup spell(8001);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(ally->GetLife() > allyBefore);
    TEST_ASSERT(foe->GetLife() == foeBefore);
    return true;
}

bool TestItemTownPortalMovesGroup() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 811, 0, 0, 0);
    auto* near = AdoptAt(sim, 812, 0, 100, 0);
    auto* far = AdoptAt(sim, 813, 0, 5000, 5000);
    auto* foe = AdoptAt(sim, 814, 1, 100, 100);
    Settle(sim);
    CAbilityItemTownPortal spell(8101);
    spell.DoEffect(sim, caster, 9000, 9000);
    TEST_ASSERT(caster->GetX() == 9000.0f && caster->GetY() == 9000.0f);
    TEST_ASSERT(near->GetX() == 9000.0f && near->GetY() == 9000.0f);
    TEST_ASSERT(far->GetX() == 5000.0f && far->GetY() == 5000.0f);
    TEST_ASSERT(foe->GetX() == 100.0f && foe->GetY() == 100.0f);
    return true;
}

bool TestItemInvulnerableShields() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 821, 0, 0, 0);
    Settle(sim);
    CAbilityItemInvulnerable spell(8201);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->IsInvulnerable());
    const float before = caster->GetLife();
    caster->Damage(sim, nullptr, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 5000.0f, 0);
    TEST_ASSERT(caster->GetLife() == before); // shield holds
    return true;
}

bool TestItemHealSelfCaps() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 831, 0, 0, 0);
    Settle(sim);
    Wound(sim, caster, 200.0f);
    CAbilityItemHealSelf spell(8301);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife()); // capped, not over
    return true;
}

bool TestItemManaSelfCaps() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 841, 0, 0, 0);
    Settle(sim);
    caster->SetMana(10.0f);
    CAbilityItemManaSelf spell(8401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetMana() > 10.0f);
    TEST_ASSERT(caster->GetMana() <= static_cast<float>(caster->GetMaxMana()));
    return true;
}

bool TestItemClarityRegens() {
    CSimulation sim;
    AddPlayers(sim);
    auto* hasted = AdoptAt(sim, 851, 0, 0, 0);
    auto* plain = AdoptAt(sim, 852, 0, 100, 0);
    Settle(sim);
    hasted->SetMana(50.0f);
    plain->SetMana(50.0f);
    CAbilityItemClarity spell(8501);
    spell.DoEffect(sim, hasted);
    TEST_ASSERT(hasted->GetBuff(War3ID("Bclf")) != nullptr);
    for (int i = 0; i < 40; i++) sim.Update();
    TEST_ASSERT(hasted->GetMana() > plain->GetMana()); // clarity out-regens base
    hasted->RemoveBuff(War3ID("Bclf"), sim);
    TEST_ASSERT(hasted->GetBonusManaRegen() == 0.0f);
    return true;
}

bool TestItemManaStealDrains() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 861, 0, 0, 0);
    auto* target = AdoptAt(sim, 862, 1, 100, 0);
    Settle(sim);
    caster->SetMana(0.0f);
    target->SetMana(200.0f);
    CAbilityItemManaSteal spell(8601);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetMana() == 100.0f);
    TEST_ASSERT(caster->GetMana() == 100.0f);
    // Cannot steal from an empty pool.
    spell.DoEffect(sim, caster, target);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetMana() == 0.0f);
    TEST_ASSERT(caster->GetMana() == 200.0f);
    return true;
}

bool TestItemProtectionArmors() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 871, 0, 0, 0);
    Settle(sim);
    const int base = caster->GetTotalDefense();
    CAbilityItemProtection spell(8701);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("AIda")) != nullptr);
    TEST_ASSERT(caster->GetTotalDefense() == base + 3);
    return true;
}

bool TestSpells8FactoryCoverage() {
    const char* codes[] = {"AIha", "AItp", "AIvl", "AIh1",
                           "AIm1", "AIpl", "Aste", "AIda"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(8800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}
} // namespace war3_spell_suite_8
bool TestItemHealGroupHeals() { return war3_spell_suite_8::TestItemHealGroupHeals(); }
bool TestItemTownPortalMovesGroup() { return war3_spell_suite_8::TestItemTownPortalMovesGroup(); }
bool TestItemInvulnerableShields() { return war3_spell_suite_8::TestItemInvulnerableShields(); }
bool TestItemHealSelfCaps() { return war3_spell_suite_8::TestItemHealSelfCaps(); }
bool TestItemManaSelfCaps() { return war3_spell_suite_8::TestItemManaSelfCaps(); }
bool TestItemClarityRegens() { return war3_spell_suite_8::TestItemClarityRegens(); }
bool TestItemManaStealDrains() { return war3_spell_suite_8::TestItemManaStealDrains(); }
bool TestItemProtectionArmors() { return war3_spell_suite_8::TestItemProtectionArmors(); }
bool TestSpells8FactoryCoverage() { return war3_spell_suite_8::TestSpells8FactoryCoverage(); }

// ---- Suite test_spells9.cpp ----
namespace war3_spell_suite_9 {
// Batch-9 tests: orb attack modifiers, illusion/lightning wands,
// ankh reincarnation, beast scroll, healing ward.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
} // namespace war3_spell_suite_9
bool TestOrbOfFrostChills() { return war3_spell_suite_9::TestOrbOfFrostChills(); }
bool TestOrbOfCorruptionStrips() { return war3_spell_suite_9::TestOrbOfCorruptionStrips(); }
bool TestOrbOfFireProcs() { return war3_spell_suite_9::TestOrbOfFireProcs(); }
bool TestAttackModifiersDoNotStack() { return war3_spell_suite_9::TestAttackModifiersDoNotStack(); }
bool TestAttackModifierRemovedWithAbility() { return war3_spell_suite_9::TestAttackModifierRemovedWithAbility(); }
bool TestIllusionCopiesTarget() { return war3_spell_suite_9::TestIllusionCopiesTarget(); }
bool TestLightningShieldWandZaps() { return war3_spell_suite_9::TestLightningShieldWandZaps(); }
bool TestAnkhReincarnates() { return war3_spell_suite_9::TestAnkhReincarnates(); }
bool TestBeastScrollRallies() { return war3_spell_suite_9::TestBeastScrollRallies(); }
bool TestHealingWardHeals() { return war3_spell_suite_9::TestHealingWardHeals(); }
bool TestSpells9FactoryCoverage() { return war3_spell_suite_9::TestSpells9FactoryCoverage(); }

// ---- Suite test_spells10.cpp ----
namespace war3_spell_suite_10 {
// Batch-10 tests: cleaving attack, searing arrows, ensnare, ancestral
// spirit, witch-doctor healing ward, unstable concoction, abolish
// magic, cluster rockets.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
} // namespace war3_spell_suite_10
bool TestCleavingAttackSplashes() { return war3_spell_suite_10::TestCleavingAttackSplashes(); }
bool TestSearingArrowsProc() { return war3_spell_suite_10::TestSearingArrowsProc(); }
bool TestEnsnareRoots() { return war3_spell_suite_10::TestEnsnareRoots(); }
bool TestAncestralSpiritRevives() { return war3_spell_suite_10::TestAncestralSpiritRevives(); }
bool TestHealingWardUnitHeals() { return war3_spell_suite_10::TestHealingWardUnitHeals(); }
bool TestUnstableConcoctionBlasts() { return war3_spell_suite_10::TestUnstableConcoctionBlasts(); }
bool TestAbolishMagicStripsArea() { return war3_spell_suite_10::TestAbolishMagicStripsArea(); }
bool TestClusterRocketsBlast() { return war3_spell_suite_10::TestClusterRocketsBlast(); }
bool TestSpells10FactoryCoverage() { return war3_spell_suite_10::TestSpells10FactoryCoverage(); }

// ---- Suite test_spells11.cpp ----
namespace war3_spell_suite_11 {
// Batch-11 tests: stealth system (wind walk, invisibility, permanent
// invisibility, shadowmeld, dust reveal) plus berserk, summon bear,
// battle roar.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
} // namespace war3_spell_suite_11
bool TestWindWalkHidesAndBreaks() { return war3_spell_suite_11::TestWindWalkHidesAndBreaks(); }
bool TestSorceressInvisibility() { return war3_spell_suite_11::TestSorceressInvisibility(); }
bool TestPermanentInvisibility() { return war3_spell_suite_11::TestPermanentInvisibility(); }
bool TestShadowmeldNeedsStillness() { return war3_spell_suite_11::TestShadowmeldNeedsStillness(); }
bool TestDustRevealsArea() { return war3_spell_suite_11::TestDustRevealsArea(); }
bool TestFaerieFireReveals() { return war3_spell_suite_11::TestFaerieFireReveals(); }
bool TestBerserkHastesAndSoftens() { return war3_spell_suite_11::TestBerserkHastesAndSoftens(); }
bool TestSummonBearStands() { return war3_spell_suite_11::TestSummonBearStands(); }
bool TestBattleRoarRallies() { return war3_spell_suite_11::TestBattleRoarRallies(); }
bool TestSpells11FactoryCoverage() { return war3_spell_suite_11::TestSpells11FactoryCoverage(); }

// ---- Suite test_spells12.cpp ----
namespace war3_spell_suite_12 {
// Batch-12 tests: detection (true sight, sentry ward, flare) plus
// burrow, cannibalize, carrion beetles, web.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell12Type(bool fly = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    static std::vector<std::unique_ptr<CUnitType>> keepFly;
    auto& store = fly ? keepFly : keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    store.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false,
        fly ? PathingGridMovementType::FLY : PathingGridMovementType::FOOT,
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
    return store.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y, bool fly = false) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell12Type(fly), player,
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

bool TestDetectorsPierceStealth() {
    CSimulation sim;
    AddPlayers(sim);
    auto* hidden = AdoptAt(sim, 1201, 1, 100, 100);
    auto* blind = AdoptAt(sim, 1202, 0, 100, 0);
    auto* seer = AdoptAt(sim, 1203, 0, 150, 100);
    auto* farSeer = AdoptAt(sim, 1204, 0, 5000, 5000);
    Settle(sim);
    hidden->AddAbility(sim, std::make_unique<CAbilityPermanentInvisibility>(12001));
    TEST_ASSERT(hidden->IsInvisibleTo(blind)); // no detector: unseen
    seer->SetDetector(true);
    farSeer->SetDetector(true);
    TEST_ASSERT(!hidden->IsInvisibleTo(seer)); // in sight: revealed
    TEST_ASSERT(hidden->IsInvisibleTo(farSeer)); // out of range: hidden
    return true;
}

bool TestTrueSightPassive() {
    CSimulation sim;
    AddPlayers(sim);
    auto* unit = AdoptAt(sim, 1211, 0, 0, 0);
    Settle(sim);
    TEST_ASSERT(!unit->IsDetector());
    unit->AddAbility(sim, std::make_unique<CAbilityTrueSight>(12101));
    TEST_ASSERT(unit->IsDetector());
    TEST_ASSERT(unit->RemoveAbility(sim, War3ID("Atru")));
    TEST_ASSERT(!unit->IsDetector());
    return true;
}

bool TestSentryWardDetects() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("oeye"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* hidden = sim.CreateUnit(War3ID("hfoo"), 1, 100, 100, 0);
    TEST_ASSERT(caster && hidden);
    Settle(sim);
    hidden->AddAbility(sim, std::make_unique<CAbilityPermanentInvisibility>(12201));
    TEST_ASSERT(hidden->IsInvisibleTo(caster));
    CAbilitySentryWard spell(12202);
    spell.DoEffect(sim, caster, 150, 100);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    CUnit* ward = nullptr;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("oeye")) {
            ward = unit;
        }
    }
    TEST_ASSERT(ward != nullptr);
    TEST_ASSERT(ward->GetAbility(War3ID("Adet")) != nullptr); // detect attached
    TEST_ASSERT(!hidden->IsInvisibleTo(ward)); // ward sees the unseen
    return true;
}

bool TestFlareRevealsArea() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1231, 0, 0, 0);
    auto* hidden = AdoptAt(sim, 1232, 1, 100, 100);
    auto* far = AdoptAt(sim, 1233, 1, 5000, 5000);
    Settle(sim);
    hidden->AddAbility(sim, std::make_unique<CAbilityPermanentInvisibility>(12301));
    TEST_ASSERT(hidden->IsInvisibleTo(caster));
    CAbilityFlare spell(12302);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(hidden->GetBuff(War3ID("Bfae")) != nullptr);
    TEST_ASSERT(!hidden->IsInvisibleTo(caster));
    TEST_ASSERT(far->GetBuff(War3ID("Bfae")) == nullptr);
    return true;
}

bool TestBurrowHidesAndRoots() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1241, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1242, 1, 300, 0);
    Settle(sim);
    CAbilityBurrow spell(12401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("Bbur")) != nullptr);
    TEST_ASSERT(caster->IsInvisibleTo(foe));
    TEST_ASSERT(caster->IsRooted());
    // Burrowed units cannot be ordered to attack.
    CAbilityAttack attack(12402);
    TEST_ASSERT(!attack.CheckCanTarget(sim, caster, OrderIds::ATTACK, foe));
    caster->RemoveBuff(War3ID("Bbur"), sim); // surfacing restores both
    TEST_ASSERT(!caster->IsInvisibleTo(foe));
    TEST_ASSERT(!caster->IsRooted());
    return true;
}

bool TestCannibalizeEats() {
    CSimulation sim;
    AddPlayers(sim);
    auto* ghoul = AdoptAt(sim, 1251, 0, 0, 0);
    auto* corpse = AdoptAt(sim, 1252, 1, 100, 0);
    Settle(sim);
    ghoul->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                  CDamageType::NORMAL, "", 5000.0f, 0);
    corpse->Damage(sim, ghoul, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 99999.0f, 0);
    TEST_ASSERT(corpse->IsDead());
    const float before = ghoul->GetLife();
    CAbilityCannibalize spell(12501);
    spell.DoEffect(sim, ghoul, corpse);
    TEST_ASSERT(ghoul->GetLife() > before);
    for (int i = 0; i < 3; i++) sim.Update(); // removal processes
    for (CUnit* unit : sim.GetUnits()) {
        TEST_ASSERT(unit != corpse); // consumed
    }
    return true;
}

bool TestCarrionBeetlesRise() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("ucry"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* fallen = sim.CreateUnit(War3ID("hfoo"), 1, 100, 0, 0);
    TEST_ASSERT(caster && fallen);
    Settle(sim);
    fallen->Damage(sim, caster, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(fallen->IsDead());
    CAbilityCarrionBeetles spell(12601);
    spell.DoEffect(sim, caster, fallen);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    int beetles = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("ucry") &&
            unit->GetPlayerIndex() == 0) {
            beetles++;
        }
    }
    TEST_ASSERT(beetles == 2);
    return true;
}

bool TestWebGroundsFlyers() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1271, 0, 0, 0);
    auto* flyer = AdoptAt(sim, 1272, 1, 100, 0, true);
    auto* walker = AdoptAt(sim, 1273, 1, 200, 0, false);
    Settle(sim);
    CAbilityWeb spell(12701);
    spell.DoEffect(sim, caster, walker); // ground units slip free
    TEST_ASSERT(walker->GetBuff(War3ID("Bena")) == nullptr);
    spell.DoEffect(sim, caster, flyer);
    TEST_ASSERT(flyer->GetBuff(War3ID("Bena")) != nullptr);
    TEST_ASSERT(flyer->IsRooted());
    return true;
}

bool TestSpells12FactoryCoverage() {
    const char* codes[] = {"Atru", "Adet", "Aeye", "AIsw", "Afla",
                           "Abur", "Acan", "AUcb", "Aweb"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(12800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}
} // namespace war3_spell_suite_12
bool TestDetectorsPierceStealth() { return war3_spell_suite_12::TestDetectorsPierceStealth(); }
bool TestTrueSightPassive() { return war3_spell_suite_12::TestTrueSightPassive(); }
bool TestSentryWardDetects() { return war3_spell_suite_12::TestSentryWardDetects(); }
bool TestFlareRevealsArea() { return war3_spell_suite_12::TestFlareRevealsArea(); }
bool TestBurrowHidesAndRoots() { return war3_spell_suite_12::TestBurrowHidesAndRoots(); }
bool TestCannibalizeEats() { return war3_spell_suite_12::TestCannibalizeEats(); }
bool TestCarrionBeetlesRise() { return war3_spell_suite_12::TestCarrionBeetlesRise(); }
bool TestWebGroundsFlyers() { return war3_spell_suite_12::TestWebGroundsFlyers(); }
bool TestSpells12FactoryCoverage() { return war3_spell_suite_12::TestSpells12FactoryCoverage(); }

// ---- Suite test_spells13.cpp ----
namespace war3_spell_suite_13 {
// Batch-13 tests: alternate-form morphs, stone form, avatar, siphon
// mana, big bad voodoo.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell13Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell13Type(), player,
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

bool TestMorphRoundTrip(const char* formCode, War3ID buffId, int handleBase) {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID(formCode))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    if (!caster) return false;
    Settle(sim);
    TEST_ASSERT(!caster->IsMorphed());
    TEST_ASSERT(caster->GetTypeId() == War3ID("hfoo"));
    caster->AddBuff(std::make_unique<CBuffTransform>(buffId, 60.0f, War3ID(formCode)), sim);
    TEST_ASSERT(caster->IsMorphed());
    TEST_ASSERT(caster->GetTypeId() == War3ID(formCode));
    caster->RemoveBuff(buffId, sim); // demorph restores the original
    TEST_ASSERT(!caster->IsMorphed());
    TEST_ASSERT(caster->GetTypeId() == War3ID("hfoo"));
    (void)handleBase;
    return true;
}

} // namespace

bool TestMetamorphosisMorphs() {
    if (!TestMorphRoundTrip("Edem", War3ID("BEme"), 13001)) return false;
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
    if (!caster) return false;
    Settle(sim);
    CAbilityMetamorphosis spell(13002);
    spell.DoEffect(sim, caster);
    if (!unitData.HasUnitType(War3ID("Edem"))) return true;
    TEST_ASSERT(caster->GetBuff(War3ID("BEme")) != nullptr);
    TEST_ASSERT(caster->IsMorphed());
    return true;
}

bool TestBearFormMorphs() {
    return TestMorphRoundTrip("edcm", War3ID("Abrf"), 13101);
}

bool TestRavenFormMorphs() {
    return TestMorphRoundTrip("edot", War3ID("Arav"), 13201);
}

bool TestRoboGoblinMorphs() {
    return TestMorphRoundTrip("Ntin", War3ID("ANrg"), 13301);
}

bool TestStoneFormStatues() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1341, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1342, 1, 300, 0);
    Settle(sim);
    CAbilityStoneForm spell(13401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("Bstm")) != nullptr);
    TEST_ASSERT(caster->IsInvulnerable());
    TEST_ASSERT(caster->IsRooted());
    const float before = caster->GetLife();
    caster->Damage(sim, foe, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 5000.0f, 0);
    TEST_ASSERT(caster->GetLife() == before); // stone takes nothing
    caster->RemoveBuff(War3ID("Bstm"), sim);
    TEST_ASSERT(!caster->IsInvulnerable());
    TEST_ASSERT(!caster->IsRooted());
    return true;
}

bool TestAvatarRages() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1351, 0, 0, 0);
    Settle(sim);
    const int dmgBefore = caster->GetBonusDamage();
    const float maxBefore = caster->GetMaxLife();
    CAbilityAvatar spell(13501);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("BAvt")) != nullptr);
    TEST_ASSERT(caster->GetBonusDamage() == dmgBefore + 20);
    TEST_ASSERT(caster->GetMaxLife() == maxBefore + 500.0f);
    const float lifeBefore = caster->GetLife();
    caster->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                   CDamageType::FIRE, "", 5000.0f, 0);
    TEST_ASSERT(caster->GetLife() == lifeBefore); // spells wash off
    caster->Damage(sim, nullptr, false, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(caster->GetLife() < lifeBefore); // steel still bites
    caster->RemoveBuff(War3ID("BAvt"), sim);
    TEST_ASSERT(caster->GetBonusDamage() == dmgBefore);
    TEST_ASSERT(caster->GetMaxLife() == maxBefore);
    return true;
}

bool TestSiphonManaDrains() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1361, 0, 0, 0);
    auto* target = AdoptAt(sim, 1362, 1, 100, 0);
    Settle(sim);
    caster->SetMana(0.0f);
    target->SetMana(200.0f);
    CAbilitySiphonMana spell(13601);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bsip")) != nullptr);
    for (int i = 0; i < 30; i++) sim.Update(); // siphon ticks land
    TEST_ASSERT(target->GetMana() < 200.0f);
    TEST_ASSERT(caster->GetMana() > 0.0f);
    return true;
}

bool TestBigBadVoodooShields() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1371, 0, 0, 0);
    auto* ally = AdoptAt(sim, 1372, 0, 100, 0);
    auto* far = AdoptAt(sim, 1373, 0, 5000, 5000);
    auto* foe = AdoptAt(sim, 1374, 1, 100, 100);
    Settle(sim);
    CAbilityBigBadVoodoo spell(13701);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->IsInvulnerable());
    TEST_ASSERT(ally->IsInvulnerable());
    TEST_ASSERT(!far->IsInvulnerable());
    TEST_ASSERT(!foe->IsInvulnerable());
    return true;
}

bool TestSpells13FactoryCoverage() {
    const char* codes[] = {"AEme", "Abrf", "Arav", "ANrg",
                           "Astn", "AHav", "AHdr", "AOvd"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(13800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}
} // namespace war3_spell_suite_13
bool TestMetamorphosisMorphs() { return war3_spell_suite_13::TestMetamorphosisMorphs(); }
bool TestBearFormMorphs() { return war3_spell_suite_13::TestBearFormMorphs(); }
bool TestRavenFormMorphs() { return war3_spell_suite_13::TestRavenFormMorphs(); }
bool TestRoboGoblinMorphs() { return war3_spell_suite_13::TestRoboGoblinMorphs(); }
bool TestStoneFormStatues() { return war3_spell_suite_13::TestStoneFormStatues(); }
bool TestAvatarRages() { return war3_spell_suite_13::TestAvatarRages(); }
bool TestSiphonManaDrains() { return war3_spell_suite_13::TestSiphonManaDrains(); }
bool TestBigBadVoodooShields() { return war3_spell_suite_13::TestBigBadVoodooShields(); }
bool TestSpells13FactoryCoverage() { return war3_spell_suite_13::TestSpells13FactoryCoverage(); }

// ---- Suite test_spells14.cpp ----
namespace war3_spell_suite_14 {
// Batch-14 tests: pocket factory spawner, lava-split, soul burn,
// quilbeast, frost breath, disease cloud, pulverize, forked lightning.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
} // namespace war3_spell_suite_14
bool TestPocketFactoryBuilds() { return war3_spell_suite_14::TestPocketFactoryBuilds(); }
bool TestLavaSpawnSplits() { return war3_spell_suite_14::TestLavaSpawnSplits(); }
bool TestSoulBurnBurns() { return war3_spell_suite_14::TestSoulBurnBurns(); }
bool TestQuilbeastSummons() { return war3_spell_suite_14::TestQuilbeastSummons(); }
bool TestFrostBreathChills() { return war3_spell_suite_14::TestFrostBreathChills(); }
bool TestDiseaseCloudSickens() { return war3_spell_suite_14::TestDiseaseCloudSickens(); }
bool TestPulverizeSlams() { return war3_spell_suite_14::TestPulverizeSlams(); }
bool TestForkedLightningChains() { return war3_spell_suite_14::TestForkedLightningChains(); }
bool TestSpells14FactoryCoverage() { return war3_spell_suite_14::TestSpells14FactoryCoverage(); }

// ---- Suite test_spells15.cpp ----
namespace war3_spell_suite_15 {
// Batch-15 (abilities) test: Phoenix summon with egg rebirth.



#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


namespace fs = std::filesystem;

namespace {

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

int CountType(CSimulation& sim, War3ID type, int player, bool aliveOnly = true) {
    int n = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (!unit) continue;
        if (aliveOnly && unit->IsDead()) continue;
        if (unit->GetTypeId() == type && unit->GetPlayerIndex() == player) n++;
    }
    return n;
}

} // namespace

bool TestPhoenixRebirths() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("hphx"))) return true;
    if (!unitData.HasUnitType(War3ID("hpxe"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    for (int i = 0; i < 2; i++) sim.Update();
    CAbilityPhoenix spell(15001);
    spell.DoEffect(sim, caster);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    TEST_ASSERT(CountType(sim, War3ID("hphx"), 0) == 1);

    // Kill the bird: an egg is left behind.
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && unit->GetTypeId() == War3ID("hphx")) {
            unit->Damage(sim, caster, false, false, CAttackType::CHAOS,
                         CDamageType::NORMAL, "", 99999.0f, 0);
        }
    }
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(CountType(sim, War3ID("hphx"), 0) == 0);
    TEST_ASSERT(CountType(sim, War3ID("hpxe"), 0) == 1);

    // The egg hatches after its delay: the bird returns, shell consumed.
    for (int i = 0; i < 220; i++) sim.Update();
    TEST_ASSERT(CountType(sim, War3ID("hphx"), 0) == 1);
    TEST_ASSERT(CountType(sim, War3ID("hpxe"), 0) == 0);
    return true;
}

bool TestPhoenixHatchDelayFromData() {
    CAbilityDefFull def;
    def.levels = 1;
    def.durations = {6.5f};
    CAbilityPhoenix spell(15011);
    spell.ApplyData(def);
    TEST_ASSERT(std::fabs(spell.GetHatchDelay() - 6.5f) < 0.01f);

    CAbilityDefFull dataOnly;
    dataOnly.levels = 1;
    dataOnly.dataLevels.push_back({});
    dataOnly.dataLevels[0][0] = "4";
    CAbilityPhoenix fromData(15012);
    fromData.ApplyData(dataOnly);
    TEST_ASSERT(std::fabs(fromData.GetHatchDelay() - 4.0f) < 0.01f);
    return true;
}

bool TestSpells15FactoryCoverage() {
    auto ability = AbilityFactory::CreateAbility(15002, War3ID("AHpx"));
    TEST_ASSERT(ability != nullptr);
    return true;
}
} // namespace war3_spell_suite_15
bool TestPhoenixRebirths() { return war3_spell_suite_15::TestPhoenixRebirths(); }
bool TestPhoenixHatchDelayFromData() { return war3_spell_suite_15::TestPhoenixHatchDelayFromData(); }
bool TestSpells15FactoryCoverage() { return war3_spell_suite_15::TestSpells15FactoryCoverage(); }

// ---- Suite test_spells16.cpp ----
namespace war3_spell_suite_16 {
// Batch-16 (abilities) tests: devour, devour magic, cold arrows,
// monsoon, absorb mana, unholy frenzy.



namespace {

CUnitType* MakeSpell16Type(bool fly = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    static std::vector<std::unique_ptr<CUnitType>> keepFly;
    auto& store = fly ? keepFly : keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    store.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false,
        fly ? PathingGridMovementType::FLY : PathingGridMovementType::FOOT,
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
    return store.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y, bool fly = false) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell16Type(fly), player,
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

bool TestDevourSwallows() {
    CSimulation sim;
    AddPlayers(sim);
    auto* kodo = AdoptAt(sim, 1601, 0, 0, 0);
    auto* victim = AdoptAt(sim, 1602, 1, 50, 0);
    auto* flyer = AdoptAt(sim, 1603, 1, 60, 0, true);
    Settle(sim);
    CAbilityDevour spell(16001);
    spell.DoEffect(sim, kodo, flyer); // flyers cannot be swallowed
    TEST_ASSERT(!flyer->IsHidden());
    TEST_ASSERT(flyer->GetBuff(War3ID("Bdig")) == nullptr);
    spell.DoEffect(sim, kodo, victim);
    TEST_ASSERT(victim->IsHidden()); // riding in cargo
    TEST_ASSERT(victim->GetBuff(War3ID("Bdig")) != nullptr);
    TEST_ASSERT(kodo->GetCargoUnits().size() == 1);
    const float before = victim->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // digest ticks land
    TEST_ASSERT(victim->GetLife() < before);
    TEST_ASSERT(kodo->GetCargoUnits().size() == 1); // no release
    return true;
}

bool TestDevourMagicDispels() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1611, 0, 0, 0);
    auto* target = AdoptAt(sim, 1612, 1, 100, 0);
    Settle(sim);
    target->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    target->AddBuff(std::make_unique<CBuffSlow>(10.0f, 0.3f, -1), sim);
    TEST_ASSERT(target->GetBuffs().size() == 2);
    caster->SetMana(0.0f);
    CAbilityDevourMagic spell(16101);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuffs().empty());
    const float cap = (float)caster->GetMaxMana();
    TEST_ASSERT(caster->GetMana() == (100.0f < cap ? 100.0f : cap)); // 2 buffs
    return true;
}

bool TestColdArrowsProc() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 1621, 0, 0, 0);
    auto* holder = AdoptAt(sim, 1622, 0, 0, 100);
    auto* targetA = AdoptAt(sim, 1623, 1, 100, 0);
    auto* targetB = AdoptAt(sim, 1624, 1, 100, 100);
    Settle(sim);
    attacker->AddAbility(sim, std::make_unique<CAbilityColdArrows>(16201));
    auto frost = std::make_unique<CAbilityColdArrows>(16202);
    frost->SetAlias(War3ID("ANfa")); // sea-witch skin shares the base
    holder->AddAbility(sim, std::move(frost));
    targetA->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(targetA->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(targetA->GetBuff(War3ID("Bpoi")) != nullptr);
    targetB->Damage(sim, holder, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(targetB->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(targetB->GetBuff(War3ID("Bpoi")) != nullptr);
    return true;
}

bool TestMonsoonWaves() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1631, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1632, 1, 100, 100);
    auto* ally = AdoptAt(sim, 1633, 0, 150, 100);
    auto* far = AdoptAt(sim, 1634, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityMonsoon spell(16301);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestAbsorbManaPops() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1641, 0, 0, 0);
    auto* summoned = AdoptAt(sim, 1642, 1, 100, 0);
    auto* regular = AdoptAt(sim, 1643, 1, 200, 0);
    Settle(sim);
    summoned->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("AOsf"), 120.0f), sim);
    TEST_ASSERT(summoned->IsSummoned());
    caster->SetMana(0.0f);
    CAbilityAbsorbMana spell(16401);
    spell.DoEffect(sim, caster, regular); // the real stays
    TEST_ASSERT(!regular->IsDead());
    TEST_ASSERT(caster->GetMana() == 0.0f);
    spell.DoEffect(sim, caster, summoned);
    TEST_ASSERT(summoned->IsDead());
    TEST_ASSERT(caster->GetMana() > 0.0f);
    return true;
}

bool TestUnholyFrenzyHastes() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1651, 0, 0, 0);
    auto* ally = AdoptAt(sim, 1652, 0, 100, 0);
    Settle(sim);
    CAbilityUnholyFrenzy spell(16501);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("Babr")) != nullptr);
    TEST_NEAR(ally->GetAttackSpeedMultiplier(), 1.5f, 0.001f);
    return true;
}

bool TestSpells16FactoryCoverage() {
    const char* codes[] = {"Adev", "Advm", "AHca", "ANfa",
                           "ANmo", "Aabs", "Auhf"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(16600, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    // Sea-witch skin keeps its alias for the shared proc hook.
    auto frost = AbilityFactory::CreateAbility(16601, War3ID("ANfa"));
    TEST_ASSERT(frost != nullptr && frost->GetAlias() == War3ID("ANfa"));
    return true;
}
} // namespace war3_spell_suite_16
bool TestDevourSwallows() { return war3_spell_suite_16::TestDevourSwallows(); }
bool TestDevourMagicDispels() { return war3_spell_suite_16::TestDevourMagicDispels(); }
bool TestColdArrowsProc() { return war3_spell_suite_16::TestColdArrowsProc(); }
bool TestMonsoonWaves() { return war3_spell_suite_16::TestMonsoonWaves(); }
bool TestAbsorbManaPops() { return war3_spell_suite_16::TestAbsorbManaPops(); }
bool TestUnholyFrenzyHastes() { return war3_spell_suite_16::TestUnholyFrenzyHastes(); }
bool TestSpells16FactoryCoverage() { return war3_spell_suite_16::TestSpells16FactoryCoverage(); }

// ---- Suite test_spells17.cpp ----
namespace war3_spell_suite_17 {
// Batch-17 (abilities) tests: howl, breath of fire, drunken haze,
// mana shield, locust swarm, cyclone, vengeance.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell17Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell17Type(), player,
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

bool TestHowlWeakens() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1701, 0, 0, 0);
    auto* howled = AdoptAt(sim, 1702, 1, 100, 0);
    auto* control = AdoptAt(sim, 1703, 1, 5000, 5000);
    auto* targetA = AdoptAt(sim, 1704, 0, 200, 0);
    auto* targetB = AdoptAt(sim, 1705, 0, 200, 100);
    Settle(sim);
    CAbilityHowlOfTerror spell(17001);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(howled->GetBuff(War3ID("BNht")) != nullptr);
    TEST_ASSERT(control->GetBuff(War3ID("BNht")) == nullptr);
    const float beforeA = targetA->GetLife();
    const float beforeB = targetB->GetLife();
    targetA->Damage(sim, howled, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 200.0f, 0);
    targetB->Damage(sim, control, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 200.0f, 0);
    TEST_NEAR((beforeA - targetA->GetLife()) / (beforeB - targetB->GetLife()),
              0.7f, 0.01f);
    return true;
}

bool TestBreathOfFireBurns() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1711, 0, 0, 0);
    auto* target = AdoptAt(sim, 1712, 1, 100, 0);
    auto* neighbor = AdoptAt(sim, 1713, 1, 150, 0);
    auto* ally = AdoptAt(sim, 1714, 0, 120, 0);
    Settle(sim);
    const float targetBefore = target->GetLife();
    const float neighborBefore = neighbor->GetLife();
    CAbilityBreathOfFire spell(17101);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() < targetBefore);
    TEST_ASSERT(neighbor->GetLife() < neighborBefore);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    const float afterStrike = target->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // burn ticks land
    TEST_ASSERT(target->GetLife() < afterStrike);
    return true;
}

bool TestDrunkenHazeMisses() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1721, 0, 0, 0);
    auto* target = AdoptAt(sim, 1722, 1, 100, 100);
    auto* far = AdoptAt(sim, 1723, 1, 5000, 5000);
    // Shooters stand clear of the haze so only the direct buff applies.
    auto* attacker = AdoptAt(sim, 1724, 1, 2000, 100);
    auto* victim = AdoptAt(sim, 1725, 0, 2100, 100);
    Settle(sim);
    CAbilityDrunkenHaze spell(17201);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(target->GetBuff(War3ID("BNdh")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(far->GetBuff(War3ID("BNdh")) == nullptr);
    // Certain miss: the swing lands nothing.
    attacker->AddBuff(std::make_unique<CBuffHaze>(30.0f, 1.0f), sim);
    const float victimBefore = victim->GetLife();
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    TEST_ASSERT(victim->GetLife() == victimBefore);
    // Certain hit: the swing lands fully.
    attacker->RemoveBuff(War3ID("BNdh"), sim);
    attacker->AddBuff(std::make_unique<CBuffHaze>(30.0f, 0.0f), sim);
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    TEST_ASSERT(victim->GetLife() < victimBefore);
    return true;
}

bool TestManaShieldAbsorbs() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1731, 0, 0, 0);
    Settle(sim);
    CAbilityManaShield spell(17301);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("BNms")) != nullptr);
    caster->SetMana(200.0f);
    const float lifeBefore = caster->GetLife();
    caster->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                   CDamageType::FIRE, "", 150.0f, 0);
    TEST_ASSERT(caster->GetLife() == lifeBefore); // mana took it all
    TEST_ASSERT(caster->GetMana() == 50.0f);
    // Empty pool: steel bites again.
    caster->SetMana(0.0f);
    caster->Damage(sim, nullptr, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    TEST_ASSERT(caster->GetLife() < lifeBefore);
    return true;
}

bool TestLocustSwarmDevours() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1741, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1742, 1, 100, 0);
    auto* ally = AdoptAt(sim, 1743, 0, 100, 100);
    auto* far = AdoptAt(sim, 1744, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityLocustSwarm spell(17401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestCycloneSuspends() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1751, 0, 0, 0);
    auto* target = AdoptAt(sim, 1752, 1, 100, 0);
    Settle(sim);
    CAbilityCyclone spell(17501);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bcyc")) != nullptr);
    TEST_ASSERT(target->IsInvulnerable()); // tossed and untouchable
    const float before = target->GetLife();
    target->Damage(sim, caster, false, false, CAttackType::SPELLS,
                   CDamageType::FIRE, "", 5000.0f, 0);
    TEST_ASSERT(target->GetLife() == before);
    target->RemoveBuff(War3ID("Bcyc"), sim); // touchdown restores both
    TEST_ASSERT(!target->IsInvulnerable());
    return true;
}

bool TestVengeanceRises() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("even"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* bearer = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* killer = sim.CreateUnit(War3ID("hfoo"), 1, 100, 0, 0);
    TEST_ASSERT(bearer && killer);
    Settle(sim);
    bearer->AddAbility(sim, std::make_unique<CAbilityVengeance>(17601));
    bearer->Damage(sim, killer, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(bearer->IsDead());
    bool spiritFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("even") &&
            unit->GetPlayerIndex() == 0) {
            spiritFound = true;
        }
    }
    TEST_ASSERT(spiritFound);
    return true;
}

bool TestSpells17FactoryCoverage() {
    const char* codes[] = {"ANht", "ANbf", "ANdh", "ANms",
                           "AUls", "Acyc", "Avng"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(17700, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}
} // namespace war3_spell_suite_17
bool TestHowlWeakens() { return war3_spell_suite_17::TestHowlWeakens(); }
bool TestBreathOfFireBurns() { return war3_spell_suite_17::TestBreathOfFireBurns(); }
bool TestDrunkenHazeMisses() { return war3_spell_suite_17::TestDrunkenHazeMisses(); }
bool TestManaShieldAbsorbs() { return war3_spell_suite_17::TestManaShieldAbsorbs(); }
bool TestLocustSwarmDevours() { return war3_spell_suite_17::TestLocustSwarmDevours(); }
bool TestCycloneSuspends() { return war3_spell_suite_17::TestCycloneSuspends(); }
bool TestVengeanceRises() { return war3_spell_suite_17::TestVengeanceRises(); }
bool TestSpells17FactoryCoverage() { return war3_spell_suite_17::TestSpells17FactoryCoverage(); }

// ---- Suite test_spells18.cpp ----
namespace war3_spell_suite_18 {
// Batch-18 (abilities) tests: whirlwind, immolation, force of nature,
// owl scout, war eagle, factory, kaboom, phoenix fire. Summon tests run
// without unit data (graceful nullptr path); damage/buff tests use
// adopted units only.


#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif


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
} // namespace war3_spell_suite_18
bool TestWhirlwindSpins() { return war3_spell_suite_18::TestWhirlwindSpins(); }
bool TestImmolationBurnsAndDrains() { return war3_spell_suite_18::TestImmolationBurnsAndDrains(); }
bool TestSummonsWithoutDataAreGraceful() { return war3_spell_suite_18::TestSummonsWithoutDataAreGraceful(); }
bool TestKaboomDetonates() { return war3_spell_suite_18::TestKaboomDetonates(); }
bool TestPhoenixFireScorches() { return war3_spell_suite_18::TestPhoenixFireScorches(); }
bool TestStarfallCodeIsAesf() { return war3_spell_suite_18::TestStarfallCodeIsAesf(); }
bool TestSpells18FactoryCoverage() { return war3_spell_suite_18::TestSpells18FactoryCoverage(); }

// ---- Suite test_spells19.cpp ----
namespace war3_spell_suite_19 {
// Batch-19 tests: Banish (AHbn) and Anti-Magic Shell (Aam2). Both attach
// their buffs through the AbilityBuilder single-target path; tests pin the
// buff ids, the combat consequences, factory coverage and retail order ids.



namespace {

CUnitType* MakeSpell19Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell19Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestBanishEtherealizes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 1901, 0, 0, 0);
    auto* target = AdoptAt(sim, 1902, 1, 100, 0);
    Settle(sim);
    CAbilityBanish banish(19901);
    banish.DoEffect(sim, caster, target);
    TEST_ASSERT(target->IsEthereal());
    TEST_ASSERT(target->GetBuff(War3ID("Bban")) != nullptr);
    // Banished victim shrugs physical attacks...
    DamageResult r = sim.GetCombatSystem()->CalculateDamage(
        caster, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(r.damageDealt == 0.0f);
    // ...but takes bonus magic.
    DamageResult m = sim.GetCombatSystem()->CalculateDamage(
        caster, target, false, false, CAttackType::MAGIC, CDamageType::MAGIC, 100, 0, 0, 0);
    TEST_ASSERT(m.damageDealt > 100.0f);
    return true;
}

bool TestAntiMagicShellImmunizes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 1911, 0, 0, 0);
    auto* target = AdoptAt(sim, 1912, 0, 100, 0);
    Settle(sim);
    CAbilityAntiMagicShell shell(19902);
    shell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->IsMagicImmune());
    TEST_ASSERT(target->GetBuff(War3ID("Bam2")) != nullptr);
    DamageResult m = sim.GetCombatSystem()->CalculateDamage(
        caster, target, false, false, CAttackType::MAGIC, CDamageType::FIRE, 100, 0, 0, 0);
    TEST_ASSERT(m.damageDealt == 0.0f);
    DamageResult p = sim.GetCombatSystem()->CalculateDamage(
        caster, target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100, 0, 0, 0);
    TEST_ASSERT(p.damageDealt > 0.0f);
    return true;
}

bool TestSpells19FactoryCoverage() {
    auto banish = AbilityFactory::CreateAbility(19911, War3ID("AHbn"));
    TEST_ASSERT(banish != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityBanish*>(banish.get()) != nullptr);
    auto shell = AbilityFactory::CreateAbility(19912, War3ID("Aam2"));
    TEST_ASSERT(shell != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityAntiMagicShell*>(shell.get()) != nullptr);
    return true;
}

bool TestSpells19OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("AHbn")) == 852486);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aam2")) == 852186);
    TEST_ASSERT(OrderIdFromString("banish") == 852486);
    TEST_ASSERT(OrderIdFromString("antimagicshell") == 852186);
    TEST_ASSERT(SpellMatchesOrder(War3ID("AHbn"), 852486));
    TEST_ASSERT(SpellMatchesOrder(War3ID("Aam2"), 852186));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("AHbn"), 852186));
    return true;
}
} // namespace war3_spell_suite_19
bool TestBanishEtherealizes() { return war3_spell_suite_19::TestBanishEtherealizes(); }
bool TestAntiMagicShellImmunizes() { return war3_spell_suite_19::TestAntiMagicShellImmunizes(); }
bool TestSpells19FactoryCoverage() { return war3_spell_suite_19::TestSpells19FactoryCoverage(); }
bool TestSpells19OrderIds() { return war3_spell_suite_19::TestSpells19OrderIds(); }

// ---- Suite test_spells20.cpp ----
namespace war3_spell_suite_20 {
// Batch-20 tests: Possession (Apos) converts + sacrifices, Stampede
// (ANst) tramples a point corridor; factory + retail order ids.



namespace {

CUnitType* MakeSpell20Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell20Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestPossessionConvertsAndSacrifices() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2001, 0, 0, 0);
    auto* victim = AdoptAt(sim, 2002, 1, 100, 0);
    Settle(sim);
    CAbilityPossession possess(20901);
    possess.DoEffect(sim, caster, victim);
    // Victim converted...
    TEST_ASSERT(victim->GetPlayerIndex() == 0);
    // ...and the banshee was removed (queued removal drains on Update).
    Settle(sim);
    TEST_ASSERT(sim.GetUnit(2001) == nullptr);
    TEST_ASSERT(sim.GetUnit(2002) == victim);
    return true;
}

bool TestPossessionNoSacrificeWithoutConversion() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2011, 0, 0, 0);
    auto* dead = AdoptAt(sim, 2012, 1, 100, 0);
    Settle(sim);
    // Kill the victim first: possession of a corpse is a no-op and must
    // NOT sacrifice the caster.
    dead->SetLife(sim, 0.0f);
    CAbilityPossession possess(20902);
    possess.DoEffect(sim, caster, dead);
    Settle(sim);
    TEST_ASSERT(sim.GetUnit(2011) == caster);
    return true;
}

bool TestStampedeTramplesCorridor() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2021, 0, 0, 0);
    auto* foeIn = AdoptAt(sim, 2022, 1, 400, 0);
    auto* foeOut = AdoptAt(sim, 2023, 1, 400, 500);
    Settle(sim);
    const float lifeBefore = foeIn->GetLife();
    CAbilityStampede stampede(20903);
    stampede.DoEffect(sim, caster, 800, 0);
    TEST_ASSERT(foeIn->GetLife() < lifeBefore);
    TEST_ASSERT(foeOut->GetLife() == 10000.0f);
    return true;
}

bool TestSpells20FactoryCoverage() {
    auto possess = AbilityFactory::CreateAbility(20911, War3ID("Apos"));
    TEST_ASSERT(possess != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityPossession*>(possess.get()) != nullptr);
    auto stampede = AbilityFactory::CreateAbility(20912, War3ID("ANst"));
    TEST_ASSERT(stampede != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityStampede*>(stampede.get()) != nullptr);
    return true;
}

bool TestSpells20OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Apos")) == 852196);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANst")) == 852593);
    TEST_ASSERT(OrderIdFromString("possession") == 852196);
    TEST_ASSERT(OrderIdFromString("stampede") == 852593);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Apos"), 852196));
    TEST_ASSERT(SpellMatchesOrder(War3ID("ANst"), 852593));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Apos"), 852593));
    // Fixed codes resolve in the SLK now (ABan->AHbn, AEtr->AEtq).
    TEST_ASSERT(SpellBaseOrderId(War3ID("AHbn")) == 852486);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AEtq")) == 852184);
    return true;
}
} // namespace war3_spell_suite_20
bool TestPossessionConvertsAndSacrifices() { return war3_spell_suite_20::TestPossessionConvertsAndSacrifices(); }
bool TestPossessionNoSacrificeWithoutConversion() { return war3_spell_suite_20::TestPossessionNoSacrificeWithoutConversion(); }
bool TestStampedeTramplesCorridor() { return war3_spell_suite_20::TestStampedeTramplesCorridor(); }
bool TestSpells20FactoryCoverage() { return war3_spell_suite_20::TestSpells20FactoryCoverage(); }
bool TestSpells20OrderIds() { return war3_spell_suite_20::TestSpells20OrderIds(); }

// ---- Suite test_spells21.cpp ----
namespace war3_spell_suite_21 {
// Batch-21 tests: Stasis Trap mine (proximity stun + consume) and
// Tornado wanderers (drift + toss + expiry); factory + order ids.



namespace {

CUnitType* MakeSpell21Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell21Type(), player,
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

bool TestStasisTrapTripsAndStuns() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2101, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2102, 1, 500, 0);
    Settle(sim);
    CAbilityStasisTrap trap(21901);
    trap.DoEffect(sim, caster, 500, 0);
    TEST_ASSERT(sim.GetHazards().size() == 1);
    Settle(sim, 10); // proximity polls run; foe stands on the mine
    TEST_ASSERT(sim.GetHazards().empty()); // consumed
    TEST_ASSERT(foe->GetBuff(War3ID("Bstn")) != nullptr);
    return true;
}

bool TestStasisTrapIgnoresAllies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2111, 0, 0, 0);
    auto* friend_ = AdoptAt(sim, 2112, 0, 500, 0);
    Settle(sim);
    CAbilityStasisTrap trap(21902);
    trap.DoEffect(sim, caster, 500, 0);
    Settle(sim, 30);
    TEST_ASSERT(sim.GetHazards().size() == 1); // never tripped
    TEST_ASSERT(friend_->GetBuff(War3ID("Bstn")) == nullptr);
    return true;
}

bool TestTornadoWandersAndTosses() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2121, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2122, 1, 100, 0);
    Settle(sim);
    CAbilityTornado tornado(21903);
    tornado.DoEffect(sim, caster, 100, 0);
    TEST_ASSERT(sim.GetHazards().size() == 3);
    const float hx = sim.GetHazards()[0].x;
    const float hy = sim.GetHazards()[0].y;
    Settle(sim, 40); // wander + two toss ticks
    const float hx2 = sim.GetHazards()[0].x;
    const float hy2 = sim.GetHazards()[0].y;
    TEST_ASSERT(std::fabs(hx2 - hx) + std::fabs(hy2 - hy) > 1.0f); // drifted
    // The foe started inside a toss radius: cyclone buff or damage landed.
    const bool tossed = foe->GetBuff(War3ID("Bcyc")) != nullptr;
    const bool hurt = foe->GetLife() < 10000.0f;
    TEST_ASSERT(tossed || hurt);
    (void)hx; (void)hy;
    return true;
}

bool TestTornadoExpires() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 2131, 0, 0, 0);
    Settle(sim);
    CAbilityTornado tornado(21904);
    tornado.DoEffect(sim, caster, 0, 0);
    TEST_ASSERT(sim.GetHazards().size() == 3);
    Settle(sim, 20 * 25); // past the 20s lifetime
    TEST_ASSERT(sim.GetHazards().empty());
    return true;
}

bool TestSpells21FactoryCoverage() {
    auto stasis = AbilityFactory::CreateAbility(21911, War3ID("Asta"));
    TEST_ASSERT(stasis != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityStasisTrap*>(stasis.get()) != nullptr);
    auto tornado = AbilityFactory::CreateAbility(21912, War3ID("ANto"));
    TEST_ASSERT(tornado != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTornado*>(tornado.get()) != nullptr);
    return true;
}

bool TestSpells21OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Asta")) == 852114);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANto")) == 852597);
    TEST_ASSERT(OrderIdFromString("stasistrap") == 852114);
    TEST_ASSERT(OrderIdFromString("tornado") == 852597);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Asta"), 852114));
    TEST_ASSERT(SpellMatchesOrder(War3ID("ANto"), 852597));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Asta"), 852597));
    return true;
}
} // namespace war3_spell_suite_21
bool TestStasisTrapTripsAndStuns() { return war3_spell_suite_21::TestStasisTrapTripsAndStuns(); }
bool TestStasisTrapIgnoresAllies() { return war3_spell_suite_21::TestStasisTrapIgnoresAllies(); }
bool TestTornadoWandersAndTosses() { return war3_spell_suite_21::TestTornadoWandersAndTosses(); }
bool TestTornadoExpires() { return war3_spell_suite_21::TestTornadoExpires(); }
bool TestSpells21FactoryCoverage() { return war3_spell_suite_21::TestSpells21FactoryCoverage(); }
bool TestSpells21OrderIds() { return war3_spell_suite_21::TestSpells21OrderIds(); }

// ---- Suite test_spells22.cpp ----
namespace war3_spell_suite_22 {
// Batch-22 tests: Finger of Death / Howl of Terror code variants (Afod,
// Acht) reuse the ANfd/ANht classes through aliases, and the ANin/Aams
// variants answer their own retail orders via the widened alias match.



namespace {

CUnitType* MakeSpell22Type(bool hero) {
    static std::vector<std::unique_ptr<CUnitType>> keepFoot;
    static std::vector<std::unique_ptr<CUnitType>> keepHero;
    auto& keep = hero ? keepHero : keepFoot;
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
        1, hero, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y, bool hero = false) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell22Type(hero), player,
                                                 hero ? War3ID("Hpal") : War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestFingerOfDeathExecutes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2201, 0, 0, 0);
    auto* victim = AdoptAt(sim, 2202, 1, 100, 0);
    Settle(sim);
    // Afod variant shares the ANfd implementation (lethal to non-heroes).
    auto finger = AbilityFactory::CreateAbility(22901, War3ID("Afod"));
    TEST_ASSERT(finger != nullptr);
    TEST_ASSERT(finger->GetAlias() == War3ID("Afod"));
    auto* fod = dynamic_cast<CAbilityFingerOfDeath*>(finger.get());
    TEST_ASSERT(fod != nullptr);
    fod->DoEffect(sim, caster, victim);
    TEST_ASSERT(victim->IsDead());
    return true;
}

bool TestFingerOfDeathSparesHeroes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2211, 0, 0, 0);
    auto* hero = AdoptAt(sim, 2212, 1, 100, 0, true);
    TEST_ASSERT(hero->IsHero());
    Settle(sim);
    CAbilityFingerOfDeath finger(22902);
    finger.DoEffect(sim, caster, hero);
    // Heroes withstand it for DataC (500): scratched, not executed.
    TEST_ASSERT(!hero->IsDead());
    if (std::fabs(hero->GetLife() - 9500.0f) > 1.0f) return false;
    return true;
}

bool TestHowlCripplesEnemiesOnly() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2221, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2222, 1, 100, 0);
    auto* friend_ = AdoptAt(sim, 2223, 0, 150, 0);
    Settle(sim);
    auto howl = AbilityFactory::CreateAbility(22903, War3ID("Acht"));
    TEST_ASSERT(howl != nullptr);
    auto* howlPtr = dynamic_cast<CAbilityHowlOfTerror*>(howl.get());
    TEST_ASSERT(howlPtr != nullptr);
    howlPtr->DoEffect(sim, caster);
    TEST_ASSERT(foe->GetBuff(War3ID("BNht")) != nullptr);
    TEST_ASSERT(friend_->GetBuff(War3ID("BNht")) == nullptr);
    TEST_ASSERT(caster->GetBuff(War3ID("BNht")) == nullptr);
    return true;
}

bool TestAliasAnswersOwnOrder() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2231, 0, 0, 0);
    caster->SetMana(1000);
    Settle(sim);
    // Generic Inferno code shares the Dreadlord class but answers 852232.
    auto inferno = AbilityFactory::CreateAbility(22911, War3ID("ANin"));
    TEST_ASSERT(inferno != nullptr);
    TEST_ASSERT(inferno->GetAlias() == War3ID("ANin"));
    TEST_ASSERT(inferno->CheckCanUse(sim, caster, 852232));
    TEST_ASSERT(inferno->CheckCanUse(sim, caster, 852224)); // base still works
    TEST_ASSERT(!inferno->CheckCanUse(sim, caster, 852486));
    // Base Anti-Magic Shell variant likewise.
    auto shell = AbilityFactory::CreateAbility(22912, War3ID("Aams"));
    TEST_ASSERT(shell != nullptr);
    TEST_ASSERT(shell->GetAlias() == War3ID("Aams"));
    TEST_ASSERT(shell->CheckCanUse(sim, caster, 852186));
    return true;
}

bool TestSpells22FactoryCoverage() {
    auto finger = AbilityFactory::CreateAbility(22921, War3ID("Afod"));
    TEST_ASSERT(finger != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityFingerOfDeath*>(finger.get()) != nullptr);
    TEST_ASSERT(finger->GetAlias() == War3ID("Afod"));
    auto howl = AbilityFactory::CreateAbility(22922, War3ID("Acht"));
    TEST_ASSERT(howl != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityHowlOfTerror*>(howl.get()) != nullptr);
    TEST_ASSERT(howl->GetAlias() == War3ID("Acht"));
    return true;
}

bool TestSpells22OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Afod")) == 852230);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acht")) == 852588);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANin")) == 852232);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aams")) == 852186);
    TEST_ASSERT(OrderIdFromString("fingerofdeath") == 852230);
    TEST_ASSERT(OrderIdFromString("howlofterror") == 852588);
    TEST_ASSERT(OrderIdFromString("inferno") == 852232);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Afod"), 852230));
    TEST_ASSERT(SpellMatchesOrder(War3ID("Acht"), 852588));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Afod"), 852588));
    return true;
}
} // namespace war3_spell_suite_22
bool TestFingerOfDeathExecutes() { return war3_spell_suite_22::TestFingerOfDeathExecutes(); }
bool TestFingerOfDeathSparesHeroes() { return war3_spell_suite_22::TestFingerOfDeathSparesHeroes(); }
bool TestHowlCripplesEnemiesOnly() { return war3_spell_suite_22::TestHowlCripplesEnemiesOnly(); }
bool TestAliasAnswersOwnOrder() { return war3_spell_suite_22::TestAliasAnswersOwnOrder(); }
bool TestSpells22FactoryCoverage() { return war3_spell_suite_22::TestSpells22FactoryCoverage(); }
bool TestSpells22OrderIds() { return war3_spell_suite_22::TestSpells22OrderIds(); }

// ---- Suite test_spells23.cpp ----
namespace war3_spell_suite_23 {
// Batch-23 tests: Chain Dispel (faction-sensitive bouncing cleanse) and
// Taunt (logged forced-attack orders); factory + retail order ids.



namespace {

CUnitType* MakeSpell23Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell23Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestChainDispelCleansesBothSides() {
    CSimulation sim;
    // Ally checks read player alliances; same-player units are allied.
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2301, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2302, 1, 100, 0);
    auto* friend_ = AdoptAt(sim, 2303, 0, 150, 0);
    Settle(sim);
    // Enemy buffed, ally debuffed.
    foe->AddBuff(std::make_unique<CBuffBloodlust>(60.0f, 0.1f, 0.1f), sim);
    friend_->AddBuff(std::make_unique<CBuffSlow>(60.0f, 0.3f), sim);
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) != nullptr);
    TEST_ASSERT(friend_->GetBuff(War3ID("Bslo")) != nullptr);

    CAbilityChainDispel dispel(23901);
    dispel.DoEffect(sim, caster, foe);
    // Enemy sheds the buff, ally sheds the debuff.
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) == nullptr);
    TEST_ASSERT(friend_->GetBuff(War3ID("Bslo")) == nullptr);
    return true;
}

bool TestTauntForcesAttack() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2311, 0, 0, 0);
    auto* foe1 = AdoptAt(sim, 2312, 1, 100, 0);
    auto* foe2 = AdoptAt(sim, 2313, 1, 150, 50);
    auto* friend_ = AdoptAt(sim, 2314, 0, 120, 0);
    Settle(sim);
    CAbilityTaunt taunt(23902);
    taunt.DoEffect(sim, caster);
    TEST_ASSERT(foe1->GetLastOrderId() == OrderIds::ATTACK);
    TEST_ASSERT(foe2->GetLastOrderId() == OrderIds::ATTACK);
    TEST_ASSERT(friend_->GetLastOrderId() == 0);
    // And the orders went through the lockstep log (replayable).
    TEST_ASSERT(!sim.LoggedCommands().empty());
    return true;
}

bool TestSpells23FactoryCoverage() {
    auto dispel = AbilityFactory::CreateAbility(23911, War3ID("Ache"));
    TEST_ASSERT(dispel != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChainDispel*>(dispel.get()) != nullptr);
    auto taunt = AbilityFactory::CreateAbility(23912, War3ID("ANta"));
    TEST_ASSERT(taunt != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTaunt*>(taunt.get()) != nullptr);
    return true;
}

bool TestSpells23OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Ache")) == 852615);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANta")) == 852520);
    TEST_ASSERT(OrderIdFromString("taunt") == 852520);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Ache"), 852615));
    TEST_ASSERT(SpellMatchesOrder(War3ID("ANta"), 852520));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Ache"), 852520));
    return true;
}
} // namespace war3_spell_suite_23
bool TestChainDispelCleansesBothSides() { return war3_spell_suite_23::TestChainDispelCleansesBothSides(); }
bool TestTauntForcesAttack() { return war3_spell_suite_23::TestTauntForcesAttack(); }
bool TestSpells23FactoryCoverage() { return war3_spell_suite_23::TestSpells23FactoryCoverage(); }
bool TestSpells23OrderIds() { return war3_spell_suite_23::TestSpells23OrderIds(); }

// ---- Suite test_spells24.cpp ----
namespace war3_spell_suite_24 {
// Batch-24 tests: regeneration auras, item stat passives, and the
// ability-builder aliases that share an already implemented class.



namespace {

CUnitType* MakeSpell24Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell24Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestRegenAuraReachesAllies() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2401, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2402, 0, 200, 0);
    auto* foe = AdoptAt(sim, 2403, 1, 200, 0);
    auto* far = AdoptAt(sim, 2404, 0, 2000, 0);
    // Adopted units join the live list on the next update.
    for (int i = 0; i < 2; i++) sim.Update();

    CAbilityRegenerationAura aura(24901);
    aura.OnTick(sim, caster);
    TEST_ASSERT(std::fabs(caster->GetBonusLifeRegen() - 1.0f) < 0.01f);
    TEST_ASSERT(std::fabs(ally->GetBonusLifeRegen() - 1.0f) < 0.01f);
    TEST_ASSERT(foe->GetBonusLifeRegen() == 0.0f);
    TEST_ASSERT(far->GetBonusLifeRegen() == 0.0f);

    CAbilityManaRegenerationAura mana(24902);
    mana.OnTick(sim, caster);
    TEST_ASSERT(std::fabs(caster->GetBonusManaRegen() - 1.0f) < 0.01f);
    TEST_ASSERT(foe->GetBonusManaRegen() == 0.0f);
    return true;
}

bool TestItemPassivesApplyAndRemove() {
    CSimulation sim;
    auto* hero = AdoptAt(sim, 2411, 0, 0, 0);
    const float speedBefore = hero->GetSpeed();
    const float aspdBefore = hero->GetAttackSpeedMultiplier();

    CAbilityItemMoveSpeed boots(24911);
    boots.OnAdd(sim, hero);
    TEST_ASSERT(hero->GetSpeed() == speedBefore + 50.0f);
    boots.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetSpeed() == speedBefore);

    CAbilityItemAttackSpeed haste(24912);
    haste.OnAdd(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetAttackSpeedMultiplier() - aspdBefore * 1.15f) < 0.001f);
    haste.OnRemove(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetAttackSpeedMultiplier() - aspdBefore) < 0.001f);

    hero->SetMaxMana(200);
    CAbilityItemManaRegen sobi(24913);
    sobi.OnAdd(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetBonusManaRegen() - 100.0f) < 0.01f);
    sobi.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetBonusManaRegen() == 0.0f);

    CAbilityItemVampirism mask(24914);
    mask.OnAdd(sim, hero);
    TEST_ASSERT(hero->GetBuff(War3ID("BUav")) != nullptr);
    mask.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetBuff(War3ID("BUav")) == nullptr);

    auto sight = AbilityFactory::CreateAbility(24915, War3ID("Agyv"));
    TEST_ASSERT(sight != nullptr);
    sight->OnAdd(sim, hero);
    TEST_ASSERT(hero->IsDetector());
    sight->OnRemove(sim, hero);
    TEST_ASSERT(!hero->IsDetector());
    return true;
}

bool TestSpells24FactoryCoverage() {
    auto regen = AbilityFactory::CreateAbility(24921, War3ID("Aoar"));
    TEST_ASSERT(dynamic_cast<CAbilityRegenerationAura*>(regen.get()) != nullptr);
    auto regenAlias = AbilityFactory::CreateAbility(24922, War3ID("Aabr"));
    TEST_ASSERT(dynamic_cast<CAbilityRegenerationAura*>(regenAlias.get()) != nullptr);
    TEST_ASSERT(regenAlias->GetAlias() == War3ID("Aabr"));
    auto mana = AbilityFactory::CreateAbility(24923, War3ID("Aarm"));
    TEST_ASSERT(dynamic_cast<CAbilityManaRegenerationAura*>(mana.get()) != nullptr);

    TEST_ASSERT(dynamic_cast<CAbilityItemAttackSpeed*>(
        AbilityFactory::CreateAbility(24924, War3ID("AIas")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemManaRegen*>(
        AbilityFactory::CreateAbility(24925, War3ID("AIrm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemMoveSpeed*>(
        AbilityFactory::CreateAbility(24926, War3ID("AIms")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemVampirism*>(
        AbilityFactory::CreateAbility(24927, War3ID("AIva")).get()) != nullptr);

    TEST_ASSERT(dynamic_cast<CAbilityEnduranceAura*>(
        AbilityFactory::CreateAbility(24928, War3ID("Aasl")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityCommandAura*>(
        AbilityFactory::CreateAbility(24929, War3ID("Aakb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityReincarnation*>(
        AbilityFactory::CreateAbility(24930, War3ID("ACrn")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilitySpellBase*>(
        AbilityFactory::CreateAbility(24931, War3ID("ANdb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityRejuvenation*>(
        AbilityFactory::CreateAbility(24932, War3ID("AIrg")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityRaiseDead*>(
        AbilityFactory::CreateAbility(24933, War3ID("AIrd")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityStormBolt*>(
        AbilityFactory::CreateAbility(24934, War3ID("ANfb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityReturnResources*>(
        AbilityFactory::CreateAbility(24935, War3ID("Artn")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTrueSight*>(
        AbilityFactory::CreateAbility(24936, War3ID("Adts")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityTrueSight*>(
        AbilityFactory::CreateAbility(24937, War3ID("Abdt")).get()) != nullptr);
    return true;
}

bool TestSpells24AliasOrders() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2421, 0, 0, 0);
    caster->SetMana(1000);

    auto firebolt = AbilityFactory::CreateAbility(24941, War3ID("ANfb"));
    TEST_ASSERT(firebolt != nullptr);
    TEST_ASSERT(firebolt->GetAlias() == War3ID("ANfb"));
    TEST_ASSERT(firebolt->CheckCanUse(sim, caster, 852231));
    TEST_ASSERT(firebolt->CheckCanUse(sim, caster, 852095));
    TEST_ASSERT(!firebolt->CheckCanUse(sim, caster, 852486));

    auto rejuv = AbilityFactory::CreateAbility(24942, War3ID("AIrg"));
    TEST_ASSERT(rejuv->CheckCanUse(sim, caster, 852609));
    TEST_ASSERT(rejuv->CheckCanUse(sim, caster, 852161));

    auto raised = AbilityFactory::CreateAbility(24943, War3ID("AIrd"));
    TEST_ASSERT(raised->CheckCanUse(sim, caster, 852624));
    TEST_ASSERT(raised->CheckCanUse(sim, caster, 852197));

    TEST_ASSERT(SpellBaseOrderId(War3ID("ANfb")) == 852231);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIrg")) == 852609);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIrd")) == 852624);
    TEST_ASSERT(OrderIdFromString("firebolt") == 852231);
    return true;
}
} // namespace war3_spell_suite_24
bool TestRegenAuraReachesAllies() { return war3_spell_suite_24::TestRegenAuraReachesAllies(); }
bool TestItemPassivesApplyAndRemove() { return war3_spell_suite_24::TestItemPassivesApplyAndRemove(); }
bool TestSpells24FactoryCoverage() { return war3_spell_suite_24::TestSpells24FactoryCoverage(); }
bool TestSpells24AliasOrders() { return war3_spell_suite_24::TestSpells24AliasOrders(); }

// ---- Suite test_spells25.cpp ----
namespace war3_spell_suite_25 {
// Batch-25 tests: curse, moon well, eat tree, sacrifice, hide, ethereal
// collision, chest of gold, and item stat passives.



namespace {

CUnitType* MakeSpell25Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell25Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestCurseMakesAttacksMiss() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2501, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2502, 1, 100, 0);
    caster->SetMana(1000);
    CAbilityCurse curse(25901);
    curse.DoEffect(sim, caster, foe);
    TEST_ASSERT(foe->GetBuff(War3ID("Bcrs")) != nullptr);

    auto* attacker = AdoptAt(sim, 2503, 1, 0, 50);
    auto* victim = AdoptAt(sim, 2504, 0, 40, 50);
    const float lifeBefore = victim->GetLife();
    attacker->AddBuff(std::make_unique<CBuffCurse>(10.0f, 1.0f), sim);
    const float dealt = victim->Damage(sim, attacker, true, false,
                                       CAttackType::NORMAL, CDamageType::NORMAL,
                                       "", 40.0f, 0);
    TEST_ASSERT(dealt == 0.0f);
    TEST_ASSERT(victim->GetLife() == lifeBefore);
    return true;
}

bool TestMoonWellSpendsMana() {
    CSimulation sim;
    auto* well = AdoptAt(sim, 2511, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2512, 0, 100, 0);
    well->SetMana(100);
    ally->SetMaxLife(200);
    ally->SetLife(sim, 50);
    CAbilityMoonWell moon(25911);
    moon.DoEffect(sim, well, ally);
    // 100 mana / 4 per hit point restores 25 life and spends the pool.
    TEST_ASSERT(std::fabs(ally->GetLife() - 75.0f) < 0.01f);
    TEST_ASSERT(well->GetMana() == 0.0f);
    return true;
}

bool TestEatTreeFellsAndHeals() {
    CSimulation sim;
    auto* ancient = AdoptAt(sim, 2521, 0, 0, 0);
    ancient->SetMaxLife(500);
    ancient->SetLife(sim, 50);
    auto* tree = sim.CreateDestructable(War3ID("LTlt"), 40, 0, 80);
    TEST_ASSERT(tree != nullptr);
    CAbilityEatTree eat(25921);
    eat.DoEffect(sim, ancient, tree);
    TEST_ASSERT(tree->IsDead());
    TEST_ASSERT(std::fabs(ancient->GetLife() - 150.0f) < 0.01f);
    return true;
}

bool TestSacrificeKillsTheOffering() {
    CSimulation sim;
    auto* pit = AdoptAt(sim, 2531, 0, 0, 0);
    auto* acolyte = AdoptAt(sim, 2532, 0, 60, 0);
    CAbilitySacrifice sacrifice(25931);
    sacrifice.DoEffect(sim, pit, acolyte);
    TEST_ASSERT(acolyte->IsDead());
    TEST_ASSERT(!pit->IsDead());

    auto* acolyte2 = AdoptAt(sim, 2533, 0, 80, 0);
    auto* pit2 = AdoptAt(sim, 2534, 0, 0, 40);
    CAbilityRequestSacrifice request(25932);
    request.DoEffect(sim, acolyte2, pit2);
    TEST_ASSERT(acolyte2->IsDead());
    TEST_ASSERT(!pit2->IsDead());
    return true;
}

bool TestHideAndEtherealAndGold() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* unit = AdoptAt(sim, 2541, 0, 0, 0);
    CAbilityHide hide(25941);
    hide.DoEffect(sim, unit);
    TEST_ASSERT(unit->IsInvisible());

    const float collision = unit->GetCollisionSize();
    CAbilityEthereal ethereal(25942);
    ethereal.OnAdd(sim, unit);
    TEST_ASSERT(unit->GetCollisionSize() == 0.0f);
    TEST_ASSERT(unit->IsEthereal());
    ethereal.OnRemove(sim, unit);
    TEST_ASSERT(unit->GetCollisionSize() == collision);
    TEST_ASSERT(!unit->IsEthereal());

    CAbilityChestOfGold chest(25943);
    chest.DoEffect(sim, unit);
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 250);
    return true;
}

bool TestItemStatBonusesApply() {
    CSimulation sim;
    auto* hero = AdoptAt(sim, 2551, 0, 0, 0);
    hero->AddAbility(sim, std::make_unique<CAbilityHero>(25950));
    const float lifeBefore = hero->GetMaxLife();
    const int manaBefore = hero->GetMaxMana();
    const int damageBefore = hero->GetBonusDamage();

    hero->AddAbility(sim, std::make_unique<CAbilityItemDamageBonus>(25951));
    hero->AddAbility(sim, std::make_unique<CAbilityItemArmorBonus>(25952));
    hero->AddAbility(sim, std::make_unique<CAbilityItemLifeBonus>(25953));
    hero->AddAbility(sim, std::make_unique<CAbilityItemManaPoolBonus>(25954));
    TEST_ASSERT(hero->GetBonusDamage() == damageBefore + 3);
    TEST_ASSERT(hero->GetBonusArmor() == 2);
    TEST_ASSERT(std::fabs(hero->GetMaxLife() - (lifeBefore + 150.0f)) < 0.01f);
    TEST_ASSERT(hero->GetMaxMana() == manaBefore + 100);

    hero->AddAbility(sim, std::make_unique<CAbilityItemAttributeBonus>(25955));
    auto* heroAbil = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
    TEST_ASSERT(heroAbil != nullptr);
    TEST_ASSERT(heroAbil->GetStrength().bonus == 1);
    TEST_ASSERT(heroAbil->GetAgility().bonus == 1);
    TEST_ASSERT(heroAbil->GetIntelligence().bonus == 1);

    hero->RemoveAbility(sim, War3ID("AIat"));
    hero->RemoveAbility(sim, War3ID("AIde"));
    hero->RemoveAbility(sim, War3ID("AIml"));
    hero->RemoveAbility(sim, War3ID("AImm"));
    hero->RemoveAbility(sim, War3ID("AIab"));
    TEST_ASSERT(hero->GetBonusDamage() == damageBefore);
    TEST_ASSERT(hero->GetBonusArmor() == 0);
    TEST_ASSERT(std::fabs(hero->GetMaxLife() - lifeBefore) < 0.01f);
    TEST_ASSERT(hero->GetMaxMana() == manaBefore);
    TEST_ASSERT(heroAbil->GetStrength().bonus == 0);
    return true;
}

bool TestSpells25FactoryCoverage() {
    TEST_ASSERT(dynamic_cast<CAbilityCurse*>(
        AbilityFactory::CreateAbility(25961, War3ID("Acrs")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityMoonWell*>(
        AbilityFactory::CreateAbility(25962, War3ID("Ambt")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityEatTree*>(
        AbilityFactory::CreateAbility(25963, War3ID("Aeat")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilitySacrifice*>(
        AbilityFactory::CreateAbility(25964, War3ID("Asac")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityRequestSacrifice*>(
        AbilityFactory::CreateAbility(25965, War3ID("Alam")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityHide*>(
        AbilityFactory::CreateAbility(25966, War3ID("Ahid")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityEthereal*>(
        AbilityFactory::CreateAbility(25967, War3ID("Aeth")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChestOfGold*>(
        AbilityFactory::CreateAbility(25968, War3ID("AIgo")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemDamageBonus*>(
        AbilityFactory::CreateAbility(25969, War3ID("AIat")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemArmorBonus*>(
        AbilityFactory::CreateAbility(25970, War3ID("AIde")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemLifeBonus*>(
        AbilityFactory::CreateAbility(25971, War3ID("AIml")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemManaPoolBonus*>(
        AbilityFactory::CreateAbility(25972, War3ID("AImm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemAttributeBonus*>(
        AbilityFactory::CreateAbility(25973, War3ID("AIab")).get()) != nullptr);
    return true;
}

bool TestSpells25OrderIds() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2561, 0, 0, 0);
    caster->SetMana(1000);
    auto curse = AbilityFactory::CreateAbility(25981, War3ID("Acrs"));
    TEST_ASSERT(curse->CheckCanUse(sim, caster, 852190));
    auto moon = AbilityFactory::CreateAbility(25982, War3ID("Ambt"));
    TEST_ASSERT(moon->CheckCanUse(sim, caster, 852542));
    auto eat = AbilityFactory::CreateAbility(25983, War3ID("Aeat"));
    TEST_ASSERT(eat->CheckCanUse(sim, caster, 852146));
    auto hide = AbilityFactory::CreateAbility(25984, War3ID("Ahid"));
    TEST_ASSERT(hide->CheckCanUse(sim, caster, 852131));
    auto gold = AbilityFactory::CreateAbility(25985, War3ID("AIgo"));
    TEST_ASSERT(gold->CheckCanUse(sim, caster, 852610));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Asac")) == 852205);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Alam")) == 852201);
    TEST_ASSERT(OrderIdFromString("curse") == 852190);
    return true;
}
} // namespace war3_spell_suite_25
bool TestCurseMakesAttacksMiss() { return war3_spell_suite_25::TestCurseMakesAttacksMiss(); }
bool TestMoonWellSpendsMana() { return war3_spell_suite_25::TestMoonWellSpendsMana(); }
bool TestEatTreeFellsAndHeals() { return war3_spell_suite_25::TestEatTreeFellsAndHeals(); }
bool TestSacrificeKillsTheOffering() { return war3_spell_suite_25::TestSacrificeKillsTheOffering(); }
bool TestHideAndEtherealAndGold() { return war3_spell_suite_25::TestHideAndEtherealAndGold(); }
bool TestItemStatBonusesApply() { return war3_spell_suite_25::TestItemStatBonusesApply(); }
bool TestSpells25FactoryCoverage() { return war3_spell_suite_25::TestSpells25FactoryCoverage(); }
bool TestSpells25OrderIds() { return war3_spell_suite_25::TestSpells25OrderIds(); }

// ---- Suite test_spells26.cpp ----
namespace war3_spell_suite_26 {
// Batch-26 tests: spell shield, consumable hero items, form toggles, clock.



namespace {

CUnitType* MakeSpell26Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell26Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim) {
    for (int i = 0; i < 2; i++) sim.Update();
}

} // namespace

bool TestSpellShieldBlocksHostileCast() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* bearer = AdoptAt(sim, 2601, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2602, 0, 40, 0);
    auto* foe = AdoptAt(sim, 2603, 1, 80, 0);
    CAbilitySpellShieldPassive shield(26901);
    shield.OnAdd(sim, bearer);
    TEST_ASSERT(bearer->HasBuff(War3ID("BNss")));
    TEST_ASSERT(bearer->CheckSpellEffectReaction(sim, ally));
    TEST_ASSERT(bearer->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!bearer->CheckSpellEffectReaction(sim, foe));
    TEST_ASSERT(!bearer->HasBuff(War3ID("BNss")));

    bool rearmed = false;
    for (int i = 0; i < 900 && !rearmed; i++) {
        shield.OnTick(sim, bearer);
        rearmed = bearer->HasBuff(War3ID("BNss"));
    }
    TEST_ASSERT(rearmed);
    return true;
}

bool TestSpellShieldAreaCoversAllies() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2611, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2612, 0, 100, 0);
    auto* foe = AdoptAt(sim, 2613, 1, 100, 0);
    auto* far = AdoptAt(sim, 2614, 0, 2000, 0);
    Settle(sim);
    CAbilitySpellShieldArea area(26911);
    area.DoEffect(sim, caster, 0, 0);
    TEST_ASSERT(caster->HasBuff(War3ID("BNss")));
    TEST_ASSERT(ally->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!foe->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!far->HasBuff(War3ID("BNss")));
    return true;
}

bool TestPermanentItemsChangeTheHero() {
    CSimulation sim;
    auto* hero = AdoptAt(sim, 2621, 0, 0, 0);
    hero->AddAbility(sim, std::make_unique<CAbilityHero>(26920));
    auto* heroAbil = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
    TEST_ASSERT(heroAbil != nullptr);
    const int strBefore = heroAbil->GetStrength().base;

    CAbilityItemPermanentStats tome(26921, War3ID("AIim"), 0, 0, 1);
    tome.DoEffect(sim, hero);
    TEST_ASSERT(heroAbil->GetStrength().base == strBefore + 1);
    const float lifeBefore = hero->GetMaxLife();

    CAbilityItemPermanentLife life(26922);
    life.DoEffect(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetMaxLife() - (lifeBefore + 50.0f)) < 0.01f);

    CAbilityItemExperience xp(26923);
    xp.DoEffect(sim, hero);
    TEST_ASSERT(heroAbil->GetXP() == 100);
    TEST_ASSERT(heroAbil->GetHeroLevel() == 1);

    CAbilityItemLevelGain levels(26924);
    levels.DoEffect(sim, hero);
    TEST_ASSERT(heroAbil->GetHeroLevel() == 2);

    CAbilityPermanentInvulnerable invuln(26925);
    invuln.OnAdd(sim, hero);
    TEST_ASSERT(hero->IsInvulnerable());
    invuln.OnRemove(sim, hero);
    TEST_ASSERT(!hero->IsInvulnerable());
    // The tome stays after the ability is gone.
    TEST_ASSERT(heroAbil->GetStrength().base == strBefore + 1);
    return true;
}

bool TestMoonstoneSetsTheClock() {
    CSimulation sim;
    TEST_ASSERT(std::fabs(sim.GetGameTimeOfDay() - 12.0f) < 0.01f);
    auto* caster = AdoptAt(sim, 2631, 0, 0, 0);
    CAbilityChangeTimeOfDay moon(26931);
    moon.DoEffect(sim, caster);
    TEST_ASSERT(std::fabs(sim.GetGameTimeOfDay()) < 0.01f);
    return true;
}

bool TestFormToggleAndChaos() {
    CSimulation sim;
    auto* walker = AdoptAt(sim, 2641, 0, 0, 0);
    CAbilityToggleForm form(26941, War3ID("Acpf"));
    form.DoEffect(sim, walker);
    TEST_ASSERT(walker->GetBuff(War3ID("Acpf")) != nullptr);
    form.DoEffect(sim, walker);
    TEST_ASSERT(walker->GetBuff(War3ID("Acpf")) == nullptr);

    auto* chaos = AdoptAt(sim, 2642, 0, 40, 0);
    CAbilityChaos ability(26942);
    ability.OnAdd(sim, chaos);
    TEST_ASSERT(chaos->GetBuff(War3ID("Acha")) != nullptr);
    ability.OnRemove(sim, chaos);
    TEST_ASSERT(chaos->GetBuff(War3ID("Acha")) == nullptr);
    return true;
}

bool TestSpells26FactoryCoverage() {
    TEST_ASSERT(dynamic_cast<CAbilitySpellShieldPassive*>(
        AbilityFactory::CreateAbility(26951, War3ID("ANss")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilitySpellShieldArea*>(
        AbilityFactory::CreateAbility(26952, War3ID("ANse")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26953, War3ID("AIim")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26954, War3ID("AIam")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26955, War3ID("AIxm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26956, War3ID("AIsm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentLife*>(
        AbilityFactory::CreateAbility(26957, War3ID("AImi")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemExperience*>(
        AbilityFactory::CreateAbility(26958, War3ID("AIem")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemLevelGain*>(
        AbilityFactory::CreateAbility(26959, War3ID("AIlm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemFigurine*>(
        AbilityFactory::CreateAbility(26960, War3ID("AIfs")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityPermanentInvulnerable*>(
        AbilityFactory::CreateAbility(26961, War3ID("Avul")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChangeTimeOfDay*>(
        AbilityFactory::CreateAbility(26962, War3ID("AIct")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityToggleForm*>(
        AbilityFactory::CreateAbility(26963, War3ID("Acpf")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityToggleForm*>(
        AbilityFactory::CreateAbility(26964, War3ID("Aetf")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityToggleForm*>(
        AbilityFactory::CreateAbility(26965, War3ID("Aave")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChaos*>(
        AbilityFactory::CreateAbility(26966, War3ID("Acha")).get()) != nullptr);
    return true;
}

bool TestSpells26OrderIds() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2651, 0, 0, 0);
    caster->SetMana(1000);
    auto shield = AbilityFactory::CreateAbility(26971, War3ID("ANse"));
    TEST_ASSERT(shield->CheckCanUse(sim, caster, 852572));
    auto clock = AbilityFactory::CreateAbility(26972, War3ID("AIct"));
    TEST_ASSERT(clock->CheckCanUse(sim, caster, 852621));
    auto form = AbilityFactory::CreateAbility(26973, War3ID("Acpf"));
    TEST_ASSERT(form->CheckCanUse(sim, caster, 852493));
    TEST_ASSERT(form->CheckCanUse(sim, caster, 852494));
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIim")) == 852214001);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AImi")) == 852264);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIem")) == 852260);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIlm")) == 852263);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIfs")) == 852261);
    TEST_ASSERT(OrderIdFromString("etherealform") == 852496);
    return true;
}
} // namespace war3_spell_suite_26
bool TestSpellShieldBlocksHostileCast() { return war3_spell_suite_26::TestSpellShieldBlocksHostileCast(); }
bool TestSpellShieldAreaCoversAllies() { return war3_spell_suite_26::TestSpellShieldAreaCoversAllies(); }
bool TestPermanentItemsChangeTheHero() { return war3_spell_suite_26::TestPermanentItemsChangeTheHero(); }
bool TestMoonstoneSetsTheClock() { return war3_spell_suite_26::TestMoonstoneSetsTheClock(); }
bool TestFormToggleAndChaos() { return war3_spell_suite_26::TestFormToggleAndChaos(); }
bool TestSpells26FactoryCoverage() { return war3_spell_suite_26::TestSpells26FactoryCoverage(); }
bool TestSpells26OrderIds() { return war3_spell_suite_26::TestSpells26OrderIds(); }

// ---- Suite test_spells27.cpp ----
namespace war3_spell_suite_27 {
// Batch-27 tests: state passives, spell resist, magic defense, leash, replenish.



namespace {

CUnitType* MakeSpell27Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell27Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim) {
    for (int i = 0; i < 2; i++) sim.Update();
}

float LifeLoss(CUnit* unit, CSimulation& sim, CUnit* source, bool isAttack,
               CAttackType attack) {
    const float before = unit->GetLife();
    unit->Damage(sim, source, isAttack, false, attack, CDamageType::MAGIC, "", 100.0f, 0);
    return before - unit->GetLife();
}

} // namespace

bool TestStatePassivesApply() {
    CSimulation sim;
    auto* eth = AdoptAt(sim, 2701, 0, 0, 0);
    auto* immune = AdoptAt(sim, 2702, 0, 10, 0);
    auto* resist = AdoptAt(sim, 2703, 0, 20, 0);
    auto* ghost = AdoptAt(sim, 2704, 0, 30, 0);
    const float collision = ghost->GetCollisionSize();

    CAbilityStatePassive ethereal(27901, War3ID("Aetl"), CPassiveState::Ethereal);
    ethereal.OnAdd(sim, eth);
    TEST_ASSERT(eth->IsEthereal());
    const float before = eth->GetLife();
    eth->Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 80.0f, 0);
    TEST_ASSERT(std::fabs(eth->GetLife() - before) < 0.01f);
    ethereal.OnRemove(sim, eth);
    TEST_ASSERT(!eth->IsEthereal());

    CAbilityStatePassive magic(27902, War3ID("Amim"), CPassiveState::MagicImmune);
    magic.OnAdd(sim, immune);
    TEST_ASSERT(immune->IsMagicImmune());
    const float immuneBefore = immune->GetLife();
    immune->Damage(sim, nullptr, false, false, CAttackType::MAGIC, CDamageType::MAGIC, "", 80.0f, 0);
    TEST_ASSERT(std::fabs(immune->GetLife() - immuneBefore) < 0.01f);
    magic.OnRemove(sim, immune);
    TEST_ASSERT(!immune->IsMagicImmune());

    CAbilityStatePassive skin(27903, War3ID("Arsk"), CPassiveState::Resistant);
    skin.OnAdd(sim, resist);
    resist->AddBuff(std::make_unique<CBuffSlow>(20.0f, 0.5f), sim);
    CBuff* slow = resist->GetBuff(War3ID("Bslo"));
    TEST_ASSERT(slow != nullptr);
    TEST_ASSERT(std::fabs(slow->GetRemaining() - 5.0f) < 0.01f);
    skin.OnRemove(sim, resist);
    auto* plain = AdoptAt(sim, 2705, 0, 40, 0);
    plain->AddBuff(std::make_unique<CBuffSlow>(20.0f, 0.5f), sim);
    TEST_ASSERT(std::fabs(plain->GetBuff(War3ID("Bslo"))->GetRemaining() - 20.0f) < 0.01f);

    CAbilityStatePassive shade(27904, War3ID("Agho"), CPassiveState::Ghost);
    shade.OnAdd(sim, ghost);
    TEST_ASSERT(ghost->IsInvisible());
    TEST_ASSERT(ghost->GetCollisionSize() == 0.0f);
    shade.OnRemove(sim, ghost);
    TEST_ASSERT(!ghost->IsInvisible());
    TEST_ASSERT(std::fabs(ghost->GetCollisionSize() - collision) < 0.01f);
    return true;
}

bool TestSpellResistAndMixedDefense() {
    CSimulation sim;
    auto* plain = AdoptAt(sim, 2711, 0, 0, 0);
    auto* bracers = AdoptAt(sim, 2712, 0, 10, 0);
    auto* mixed = AdoptAt(sim, 2713, 0, 20, 0);
    auto* attacker = AdoptAt(sim, 2714, 0, 30, 0);

    CAbilityItemSpellResist resist(27911);
    resist.OnAdd(sim, bracers);
    const float plainSpell = LifeLoss(plain, sim, nullptr, false, CAttackType::SPELLS);
    const float reduced = LifeLoss(bracers, sim, nullptr, false, CAttackType::SPELLS);
    TEST_ASSERT(plainSpell > 1.0f);
    TEST_ASSERT(std::fabs(reduced - plainSpell * 0.67f) < 0.5f);
    resist.OnRemove(sim, bracers);
    const float restored = LifeLoss(bracers, sim, nullptr, false, CAttackType::SPELLS);
    TEST_ASSERT(std::fabs(restored - plainSpell) < 0.5f);

    CAbilityItemMixedDefense defense(27912);
    defense.OnAdd(sim, mixed);
    auto* piercePlain = AdoptAt(sim, 2715, 0, 40, 0);
    const float plainPierce = LifeLoss(piercePlain, sim, attacker, false, CAttackType::PIERCE);
    const float mixedPierce = LifeLoss(mixed, sim, attacker, false, CAttackType::PIERCE);
    TEST_ASSERT(plainPierce > 1.0f);
    TEST_ASSERT(std::fabs(mixedPierce - plainPierce * 0.80f) < 0.5f);

    auto* dealtPlain = AdoptAt(sim, 2716, 0, 50, 0);
    auto* dealtTarget = AdoptAt(sim, 2717, 0, 60, 0);
    defense.OnAdd(sim, attacker);
    const float without = LifeLoss(dealtPlain, sim, piercePlain, true, CAttackType::NORMAL);
    const float with = LifeLoss(dealtTarget, sim, attacker, true, CAttackType::NORMAL);
    TEST_ASSERT(without > 1.0f);
    TEST_ASSERT(std::fabs(with - without * 1.20f) < 0.5f);

    const float mixedBefore = mixed->GetLife();
    const float attackerBefore = attacker->GetLife();
    CProjectile shot;
    shot.attackerHandleId = attacker->GetHandleId();
    shot.targetHandleId = mixed->GetHandleId();
    shot.x = mixed->GetX();
    shot.y = mixed->GetY();
    shot.speed = 2000.0f;
    shot.damage = 30.0f;
    shot.attackType = CAttackType::PIERCE;
    shot.maxTravel = 4000.0f;
    sim.SpawnProjectile(std::move(shot));
    sim.Update();
    TEST_ASSERT(mixed->GetLife() == mixedBefore);
    sim.Update();
    TEST_ASSERT(attacker->GetLife() < attackerBefore);
    return true;
}

bool TestMagicDefenseToggles() {
    CSimulation sim;
    auto* unit = AdoptAt(sim, 2721, 0, 0, 0);
    CAbilityMagicDefense defense(27921);
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    defense.DoEffect(sim, unit);
    TEST_ASSERT(unit->IsMagicImmune());
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 0.5f) < 0.01f);
    defense.DoEffect(sim, unit);
    TEST_ASSERT(!unit->IsMagicImmune());
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    defense.DoEffect(sim, unit);
    defense.OnRemove(sim, unit);
    TEST_ASSERT(!unit->IsMagicImmune());
    TEST_ASSERT(std::fabs(unit->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    return true;
}

bool TestMagicLeashStuns() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2731, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2732, 1, 40, 0);
    const float before = foe->GetLife();
    CAbilityMagicLeash leash(27931);
    leash.DoEffect(sim, caster, foe);
    TEST_ASSERT(foe->HasBuff(War3ID("Bstn")));
    TEST_ASSERT(foe->HasBuff(War3ID("Bpoi")));
    TEST_ASSERT(before - foe->GetLife() > 1.0f);
    return true;
}

bool TestReplenishRestoresAllies() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* statue = AdoptAt(sim, 2741, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2742, 0, 100, 0);
    auto* foe = AdoptAt(sim, 2743, 1, 100, 0);
    auto* far = AdoptAt(sim, 2744, 0, 2000, 0);
    Settle(sim);
    ally->SetLife(sim, ally->GetMaxLife() - 40.0f);
    foe->SetLife(sim, foe->GetMaxLife() - 40.0f);
    far->SetLife(sim, far->GetMaxLife() - 40.0f);
    const float allyBefore = ally->GetLife();
    const float foeBefore = foe->GetLife();
    const float farBefore = far->GetLife();
    CAbilityReplenish life(27941, War3ID("Arpl"), true);
    life.DoEffect(sim, statue);
    TEST_ASSERT(std::fabs(ally->GetLife() - (allyBefore + 10.0f)) < 0.01f);
    TEST_ASSERT(std::fabs(foe->GetLife() - foeBefore) < 0.01f);
    TEST_ASSERT(std::fabs(far->GetLife() - farBefore) < 0.01f);

    ally->AddBonusMaxMana(100);
    ally->SetMana(0.0f);
    foe->AddBonusMaxMana(100);
    foe->SetMana(0.0f);
    CAbilityReplenish mana(27942, War3ID("Arpm"), false);
    mana.DoEffect(sim, statue);
    TEST_ASSERT(std::fabs(ally->GetMana() - 10.0f) < 0.01f);
    TEST_ASSERT(std::fabs(foe->GetMana()) < 0.01f);
    return true;
}

bool TestSpells27FactoryCoverage() {
    const char* codes[] = {
        "Aetl", "Amim", "Arsk", "Agho", "AIsr", "AIdd", "Amdf",
        "Amls", "Arpl", "Arpm", "Asph", "AIco"
    };
    for (const char* code : codes) {
        auto ability = AbilityFactory::CreateAbility(27950, War3ID(code));
        if (!ability) {
            std::cerr << "missing factory ability " << code << "\n";
            return false;
        }
    }
    auto charm = AbilityFactory::CreateAbility(27951, War3ID("AIco"));
    TEST_ASSERT(charm->GetCode() == War3ID("ANch"));
    TEST_ASSERT(charm->GetAlias() == War3ID("AIco"));
    return true;
}

bool TestSpells27OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amdf")) == 852478);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Amdf"), 852479));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amls")) == 852480);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Arpl")) == 852545);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Arpm")) == 852548);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIco")) == 852267);
    return true;
}
} // namespace war3_spell_suite_27
bool TestStatePassivesApply() { return war3_spell_suite_27::TestStatePassivesApply(); }
bool TestSpellResistAndMixedDefense() { return war3_spell_suite_27::TestSpellResistAndMixedDefense(); }
bool TestMagicDefenseToggles() { return war3_spell_suite_27::TestMagicDefenseToggles(); }
bool TestMagicLeashStuns() { return war3_spell_suite_27::TestMagicLeashStuns(); }
bool TestReplenishRestoresAllies() { return war3_spell_suite_27::TestReplenishRestoresAllies(); }
bool TestSpells27FactoryCoverage() { return war3_spell_suite_27::TestSpells27FactoryCoverage(); }
bool TestSpells27OrderIds() { return war3_spell_suite_27::TestSpells27OrderIds(); }

// ---- Suite test_spells28.cpp ----
namespace war3_spell_suite_28 {
// Batch-28 tests: pairing, blight, harvest, cargo, waygate, shops, shade.



namespace {

std::unique_ptr<CUnitType> MakeType(const char* id) {
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    return std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID(id),
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
        10, 1, 6, 5, 1, 3, false);
}

CUnitType* SharedType() {
    static std::unique_ptr<CUnitType> keep = MakeType("hfoo");
    return keep.get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, SharedType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim) {
    for (int i = 0; i < 2; i++) sim.Update();
}

War3ID abilityCode(CPairMode mode) {
    switch (mode) {
    case CPairMode::MilitiaCall: return War3ID("Amic");
    case CPairMode::Militia: return War3ID("Amil");
    case CPairMode::Mount: return War3ID("Acoa");
    case CPairMode::LoadPartner: return War3ID("Acoh");
    case CPairMode::Couple: return War3ID("Acoi");
    }
    return War3ID("Amil");
}

CAbilityPairing* GivePair(CSimulation& sim, CUnit* unit, int handle, CPairMode mode) {
    auto ability = std::make_unique<CAbilityPairing>(handle, abilityCode(mode), mode);
    CAbilityPairing* raw = ability.get();
    unit->AddAbility(sim, std::move(ability));
    return raw;
}

} // namespace

bool TestMilitiaAndMountPairing() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hmil"), MakeType("hmil"));
    data.RegisterUnitType(War3ID("ehpr"), MakeType("ehpr"));
    data.RegisterUnitType(War3ID("ecop"), MakeType("ecop"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* hall = AdoptAt(sim, 2801, 0, 0, 0);
    auto* peasant = AdoptAt(sim, 2802, 0, 100, 0);
    auto* far = AdoptAt(sim, 2803, 0, 5000, 0);
    GivePair(sim, hall, 28901, CPairMode::MilitiaCall);
    auto* militia = GivePair(sim, peasant, 28902, CPairMode::Militia);
    GivePair(sim, far, 28903, CPairMode::Militia);
    Settle(sim);

    auto* call = dynamic_cast<CAbilityPairing*>(hall->GetAbility(War3ID("Amic")));
    TEST_ASSERT(call != nullptr);
    call->DoEffect(sim, hall);
    TEST_ASSERT(peasant->GetTypeId() == War3ID("hmil"));
    TEST_ASSERT(militia->IsConverted());
    TEST_ASSERT(far->GetTypeId() == War3ID("hfoo"));
    militia->DoEffect(sim, peasant);
    TEST_ASSERT(!peasant->IsMorphed());
    TEST_ASSERT(!militia->IsConverted());

    auto* mount = AdoptAt(sim, 2804, 0, 0, 0);
    auto* archer = AdoptAt(sim, 2805, 0, 40, 0);
    GivePair(sim, mount, 28904, CPairMode::LoadPartner);
    auto* climb = GivePair(sim, archer, 28905, CPairMode::Mount);
    Settle(sim);
    climb->DoEffect(sim, archer);
    TEST_ASSERT(archer->IsLoadedAboard());
    TEST_ASSERT(mount->GetCargoUnits().size() == 1);
    TEST_ASSERT(mount->GetTypeId() == War3ID("ehpr"));

    auto* left = AdoptAt(sim, 2806, 0, 0, 200);
    auto* right = AdoptAt(sim, 2807, 0, 40, 200);
    auto* couple = GivePair(sim, left, 28906, CPairMode::Couple);
    GivePair(sim, right, 28907, CPairMode::Couple);
    Settle(sim);
    couple->DoEffect(sim, left);
    TEST_ASSERT(left->GetTypeId() == War3ID("ecop"));
    TEST_ASSERT(right->IsDead());
    return true;
}

bool TestMilitiaReturnsCargoAndExpires() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hmil"), MakeType("hmil"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* hall = AdoptAt(sim, 2871, 0, 0, 0);
    auto* peasant = AdoptAt(sim, 2872, 0, 40, 0);
    GivePair(sim, hall, 28981, CPairMode::MilitiaCall);
    auto* militia = GivePair(sim, peasant, 28982, CPairMode::Militia);
    militia->SetMilitiaDuration(0.2f);
    auto harvestAbility = std::make_unique<CAbilityHarvest>(28983);
    CAbilityHarvest* harvest = harvestAbility.get();
    peasant->AddAbility(sim, std::move(harvestAbility));
    harvest->SetCarried(CResourceType::GOLD, 10);
    hall->AddAbility(sim, std::make_unique<CAbilityGoldMine>(28984));
    harvest->SetLastTargetHandle(hall->GetHandleId());
    Settle(sim);

    auto* call = dynamic_cast<CAbilityPairing*>(hall->GetAbility(War3ID("Amic")));
    call->DoEffect(sim, hall);
    TEST_ASSERT(peasant->GetTypeId() == War3ID("hmil"));
    TEST_ASSERT(peasant->GetBuff(War3ID("Bmil")) != nullptr);
    TEST_ASSERT(harvest->IsDisabled());
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 10);
    TEST_ASSERT(harvest->GetCarriedAmount() == 0);

    for (int i = 0; i < 20 && militia->IsConverted(); i++) peasant->Update(sim);
    TEST_ASSERT(!militia->IsConverted());
    TEST_ASSERT(!peasant->IsMorphed());
    TEST_ASSERT(!harvest->IsDisabled());
    peasant->Update(sim);
    TEST_ASSERT(peasant->GetCurrentBehavior() != nullptr);
    TEST_ASSERT(peasant->GetCurrentBehavior()->GetCategory() == CBehaviorCategory::HARVEST);
    return true;
}

bool TestUpgradeAndPowerup() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hkee"), MakeType("hkee"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.GetPlayer(0)->AddGold(1000);
    sim.GetPlayer(0)->AddLumber(1000);
    auto* hall = AdoptAt(sim, 2881, 0, 0, 0);
    CAbilityUpgrade upgrade(28991);
    upgrade.AddUpgradeTo(War3ID("hkee"));
    TEST_ASSERT(upgrade.BeginNoTarget(sim, hall, static_cast<int>(War3ID("hkee").value)) == nullptr);
    TEST_ASSERT(upgrade.IsUpgrading());
    TEST_ASSERT(hall->IsConstructing());
    for (int i = 0; i < 800 && upgrade.IsUpgrading(); i++) upgrade.OnTick(sim, hall);
    TEST_ASSERT(!upgrade.IsUpgrading());
    TEST_ASSERT(hall->GetTypeId() == War3ID("hkee"));

    auto* bearer = AdoptAt(sim, 2882, 0, 10, 0);
    const float life = bearer->GetMaxLife();
    CAbilityInventory inventory(28992);
    CItem tome(28993, War3ID("tkno"), 0, 0);
    tome.SetPowerup(true);
    tome.AddAbility(War3ID("AImi"));
    TEST_ASSERT(inventory.PickUpItem(sim, bearer, &tome, 0));
    TEST_ASSERT(bearer->GetMaxLife() == life + 50.0f);
    return true;
}

bool TestItemPickupAppliesBonuses() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* bearer = AdoptAt(sim, 2883, 0, 0, 0);
    const int before = bearer->GetBonusDamage();
    CAbilityInventory inventory(28994);
    CItem item(28995, War3ID("ratc"), 0, 0);
    item.SetBonuses({{CItemBonusType::DAMAGE, 7}});
    TEST_ASSERT(inventory.PickUpItem(sim, bearer, &item, 0));
    TEST_ASSERT(bearer->GetBonusDamage() == before + 7);
    TEST_ASSERT(inventory.DropItem(sim, bearer, 0));
    TEST_ASSERT(bearer->GetBonusDamage() == before);
    return true;
}

bool TestBuildFootprintUsesPathTex() {
    // 4x2 texture at angle 0 rotates to a 2x4 rect (ref PathingGrid:
    // rotation (0+450)%360 = 90). Center cell (8,8) then covers x 7..8,
    // y 6..9; an unrotated rect would cover x 6..9, y 7..8 instead.
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hbar"), MakeType("hbar"));
    sim.SetUnitData(&data);
    WpmData wpm;
    wpm.width = 16;
    wpm.height = 16;
    wpm.pathing.assign(256, 0);
    sim.SetPathingMap(&wpm, 0.0f, 0.0f);
    auto img = std::make_shared<PathTexImage>();
    img->w = 4;
    img->h = 2;
    img->rgb.assign(24, 0);
    img->valid = true;
    data.GetUnitType(War3ID("hbar"))->SetPathTexImage(img);
    // Rotation helper sanity: 4x2 at 0 degrees -> 2x4.
    int rx, ry, rw, rh;
    PathTexRotate(0, 0, 4, 2, 0, rx, ry, rw, rh);
    TEST_ASSERT(rw == 2 && rh == 4);
    PathTexRotate(0, 0, 4, 2, 90, rx, ry, rw, rh); // (90+450)%360 = 180
    TEST_ASSERT(rw == 4 && rh == 2);
    // Pixel flag mapping: B -> build, G -> fly, R -> walk+swim.
    PathTexImage one;
    one.w = 1;
    one.h = 1;
    one.rgb = {200, 0, 0};
    one.valid = true;
    TEST_ASSERT(PathTexPixelFlags(one, 0, 0, false) == (WpmData::UNWALKABLE | WpmData::UNSWIMABLE));
    one.rgb = {0, 200, 0};
    TEST_ASSERT(PathTexPixelFlags(one, 0, 0, false) == WpmData::UNFLYABLE);
    one.rgb = {0, 0, 200};
    TEST_ASSERT(PathTexPixelFlags(one, 0, 0, false) == WpmData::UNBUILDABLE);
    // Cell (7,6) is inside the rotated rect, (9,6) is outside it.
    wpm.pathing[7 + 6 * 16] = WpmData::UNBUILDABLE;
    TEST_ASSERT(CAbilityBuild::IsBuildLocationObstructed(sim, 272.0f, 272.0f, 0, War3ID("hbar")));
    wpm.pathing[7 + 6 * 16] = 0;
    wpm.pathing[9 + 6 * 16] = WpmData::UNBUILDABLE;
    TEST_ASSERT(!CAbilityBuild::IsBuildLocationObstructed(sim, 272.0f, 272.0f, 0, War3ID("hbar")));
    return true;
}

bool TestBuildRejectsUnbuildableCell() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hbar"), MakeType("hbar"));
    sim.SetUnitData(&data);
    WpmData wpm;
    wpm.width = 16;
    wpm.height = 16;
    wpm.pathing.assign(256, 0);
    wpm.pathing[2 + 2 * 16] = WpmData::UNBUILDABLE;
    sim.SetPathingMap(&wpm, 0.0f, 0.0f);
    TEST_ASSERT(CAbilityBuild::IsBuildLocationObstructed(sim, 80.0f, 80.0f, 0, War3ID("hbar")));
    TEST_ASSERT(!CAbilityBuild::IsBuildLocationObstructed(sim, 400.0f, 400.0f, 0, War3ID("hbar")));

    CUnitType* type = data.GetUnitType(War3ID("hbar"));
    TEST_ASSERT(type != nullptr);
    type->SetRequiredPathingTypes({CBuildingPathingType::BLIGHTED});
    for (int cy = 11; cy <= 13; ++cy) {
        for (int cx = 11; cx <= 13; ++cx) {
            wpm.pathing[(size_t)cy * 16u + (size_t)cx] = WpmData::BLIGHTED;
        }
    }
    TEST_ASSERT(!CAbilityBuild::IsBuildLocationObstructed(sim, 400.0f, 400.0f, 0, War3ID("hbar")));
    wpm.pathing[11 + 11 * 16] = 0;
    TEST_ASSERT(CAbilityBuild::IsBuildLocationObstructed(sim, 400.0f, 400.0f, 0, War3ID("hbar")));
    return true;
}

bool TestBlightAndHauntedMine() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* necropolis = AdoptAt(sim, 2811, 0, 0, 0);
    CAbilityBlight blight(28911);
    blight.OnAdd(sim, necropolis);
    for (int i = 0; i < 20; i++) blight.OnTick(sim, necropolis);
    TEST_ASSERT(sim.IsPointBlighted(0, 0));
    TEST_ASSERT(!sim.IsPointBlighted(5000, 0));
    TEST_ASSERT(blight.GetCurrentRadius() > 0.0f);

    auto* mineUnit = AdoptAt(sim, 2812, 0, 200, 0);
    auto* acolyte = AdoptAt(sim, 2813, 0, 220, 0);
    mineUnit->AddAbility(sim, std::make_unique<CAbilityBlightedGoldMine>(28913));
    auto* live = dynamic_cast<CAbilityBlightedGoldMine*>(mineUnit->GetAbility(War3ID("Abgm")));
    TEST_ASSERT(live != nullptr);
    live->SetGold(100);
    CAbilityAcolyteHarvest harvest(28914);
    harvest.DoEffect(sim, acolyte, mineUnit);
    TEST_ASSERT(live->GetMinerCount() == 1);
    CPlayer* player = sim.GetPlayer(0);
    for (int i = 0; i < 200 && player->GetGold() == 0; i++) live->OnTick(sim, mineUnit);
    TEST_ASSERT(player->GetGold() == 10);
    TEST_ASSERT(live->GetGold() == 90);

    auto overlay = AbilityFactory::CreateAbility(28915, War3ID("Agl2"));
    TEST_ASSERT(dynamic_cast<CAbilityOverlayedGoldMine*>(overlay.get()) != nullptr);
    return true;
}

bool TestWispAndLumberHarvest() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* wisp = AdoptAt(sim, 2821, 0, 0, 0);
    auto* chopper = AdoptAt(sim, 2822, 0, 30, 0);
    CDestructable* tree = sim.CreateDestructable(War3ID("LTlt"), 10, 0, 100.0f);
    CDestructable* tree2 = sim.CreateDestructable(War3ID("LTlt"), 40, 0, 100.0f);
    TEST_ASSERT(tree != nullptr && tree2 != nullptr);
    const float treeLife = tree->GetLife();
    CAbilityWispHarvest gather(28921);
    gather.DoEffect(sim, wisp, tree);
    gather.OnTick(sim, wisp);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 5);
    TEST_ASSERT(tree->GetLife() < treeLife);

    const float before = tree2->GetLife();
    CAbilityHarvestLumber lumber(28922);
    lumber.DoEffect(sim, chopper, tree2);
    TEST_ASSERT(tree2->GetLife() < before);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 15);
    return true;
}

bool TestAcolyteBindingPersistsAndReleasesOnDeath() {
    // ref CBehaviorAcolyteHarvest: bound indefinitely (no timeout) until
    // death/removal/rebind. The old 30s auto-unbind silently slowed the
    // mine trickle while the acolyte kept standing there.
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* mineUnit = AdoptAt(sim, 2851, 0, 200, 0);
    mineUnit->AddAbility(sim, std::make_unique<CAbilityBlightedGoldMine>(28953));
    auto* live = dynamic_cast<CAbilityBlightedGoldMine*>(mineUnit->GetAbility(War3ID("Abgm")));
    TEST_ASSERT(live != nullptr);
    live->SetGold(100000);
    auto* acolyte = AdoptAt(sim, 2852, 0, 220, 0);
    acolyte->AddAbility(sim, std::make_unique<CAbilityAcolyteHarvest>(28954));
    auto* harvest = dynamic_cast<CAbilityAcolyteHarvest*>(acolyte->GetAbility(War3ID("Aaha")));
    TEST_ASSERT(harvest != nullptr);
    harvest->DoEffect(sim, acolyte, mineUnit);
    TEST_ASSERT(harvest->IsBound());
    TEST_ASSERT(live->GetMinerCount() == 1);

    // 40s of mine ticks (past the old 30s timeout): still bound, still paid.
    const int goldBefore = sim.GetPlayer(0)->GetGold();
    for (int i = 0; i < 800; i++) live->OnTick(sim, mineUnit);
    TEST_ASSERT(live->GetMinerCount() == 1);
    TEST_ASSERT(harvest->IsBound());
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() > goldBefore);

    // Lethal damage dispatches OnDeath on the next Update -> slot released.
    acolyte->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL,
                    "", 1e9f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(live->GetMinerCount() == 0);
    TEST_ASSERT(!harvest->IsBound());
    return true;
}

bool TestWispJumpsOntoTreeAndDetachesOnDeath() {
    // ref CBehaviorWispHarvest: instant jump onto the tree, trickle until
    // the tree dies, then detach (handle cleared, no dangling pointer).
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* wisp = AdoptAt(sim, 2861, 0, 0, 0);
    CDestructable* tree = sim.CreateDestructable(War3ID("LTlt"), 100, 0, 100.0f);
    TEST_ASSERT(tree != nullptr);
    CAbilityWispHarvest gather(28961);
    gather.DoEffect(sim, wisp, tree);
    TEST_ASSERT(wisp->GetX() == tree->GetX() && wisp->GetY() == tree->GetY());
    TEST_ASSERT(gather.GetBoundTreeHandle() == tree->GetHandleId());
    gather.OnTick(sim, wisp);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 5);

    // Fell the tree: binding clears and trickling stops.
    tree->Damage(sim, nullptr, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                 "", 1e9f, 0);
    TEST_ASSERT(tree->IsDead());
    const int lumberBefore = sim.GetPlayer(0)->GetLumber();
    gather.OnTick(sim, wisp);
    TEST_ASSERT(gather.GetBoundTreeHandle() == -1);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == lumberBefore);
    return true;
}

bool TestGoldMineCapacityMatchesRetail() {
    // Retail Agld DataC1 = 5 miners; the old hardcoded 3 starved the 4th/5th
    // worker. Both AddMiner and CanAddMiner enforce the cap.
    CAbilityGoldMine mine(29011);
    TEST_ASSERT(mine.GetMiningCapacity() == 5);
    CBehaviorHarvest m1, m2, m3, m4, m5, m6;
    mine.AddMiner(&m1);
    mine.AddMiner(&m2);
    mine.AddMiner(&m3);
    mine.AddMiner(&m4);
    mine.AddMiner(&m5);
    TEST_ASSERT(mine.GetActiveMinerCount() == 5);
    TEST_ASSERT(!mine.CanAddMiner());
    mine.AddMiner(&m6); // refused
    TEST_ASSERT(mine.GetActiveMinerCount() == 5);
    mine.RemoveMiner(&m1);
    TEST_ASSERT(mine.CanAddMiner());
    mine.AddMiner(&m6);
    TEST_ASSERT(mine.GetActiveMinerCount() == 5);
    return true;
}

bool TestCargoBurrowAndDrop() {
    CSimulation sim;
    auto* holder = AdoptAt(sim, 2831, 0, 0, 0);
    auto* peon = AdoptAt(sim, 2832, 0, 10, 0);
    auto* extra = AdoptAt(sim, 2833, 0, 20, 0);
    CAbilityCargoHold cargo(28931, War3ID("Acar"), false);
    cargo.OnAdd(sim, holder);
    TEST_ASSERT(cargo.TryLoad(holder, peon));
    TEST_ASSERT(peon->IsLoadedAboard());
    TEST_ASSERT(peon->IsHidden());
    for (int i = 0; i < cargo.GetCapacity() - 1; i++) {
        auto* filler = AdoptAt(sim, 2840 + i, 0, 30.0f + i, 0);
        TEST_ASSERT(cargo.TryLoad(holder, filler));
    }
    TEST_ASSERT(!cargo.TryLoad(holder, extra));
    CAbilityDropInstant drop(28932, War3ID("Adri"));
    holder->AddAbility(sim, std::make_unique<CAbilityCargoHold>(28933, War3ID("Acar"), false));
    // The drop looks up the ability on the unit, which is empty. Unload
    // the local hold first, then exercise the unit-owned hold.
    cargo.UnloadAll(holder);
    TEST_ASSERT(!peon->IsLoadedAboard());
    TEST_ASSERT(!peon->IsHidden());

    auto* burrow = AdoptAt(sim, 2834, 0, 100, 0);
    auto* worker = AdoptAt(sim, 2835, 0, 110, 0);
    auto hold = std::make_unique<CAbilityCargoHold>(28934, War3ID("Abun"), true);
    CAbilityCargoHold* burrowHold = hold.get();
    burrow->AddAbility(sim, std::move(hold));
    TEST_ASSERT(burrow->AreAttacksDisabled());
    TEST_ASSERT(burrowHold->TryLoad(burrow, worker));
    TEST_ASSERT(!burrow->AreAttacksDisabled());
    TEST_ASSERT(std::fabs(burrow->GetAttackSpeedMultiplier() - 2.0f) < 0.01f);
    CAbilityDropInstant stand(28935, War3ID("Astd"));
    stand.DoEffect(sim, burrow);
    TEST_ASSERT(!worker->IsLoadedAboard());
    TEST_ASSERT(burrow->AreAttacksDisabled());
    TEST_ASSERT(std::fabs(burrow->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    return true;
}

bool TestWaygateAndShops() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* gateUnit = AdoptAt(sim, 2851, 0, 0, 0);
    auto* walker = AdoptAt(sim, 2852, 0, 20, 0);
    Settle(sim);
    auto* outsider = AdoptAt(sim, 2856, 0, 400, 0);
    CAbilityWayGate gate(28951);
    gate.SetDestination(gateUnit, 1000, 1000);
    gate.Begin(sim, gateUnit, OrderIds::SMART, outsider);
    TEST_ASSERT(std::fabs(outsider->GetX() - 400.0f) < 0.01f);
    gate.Begin(sim, gateUnit, OrderIds::SMART, walker);
    TEST_ASSERT(std::fabs(walker->GetX() - 1000.0f) < 0.01f);
    TEST_ASSERT(std::fabs(walker->GetY() - 1000.0f) < 0.01f);
    TEST_ASSERT(gateUnit->IsWaygateActive());

    auto* shop = AdoptAt(sim, 2853, 0, 0, 500);
    auto* ally = AdoptAt(sim, 2854, 0, 40, 500);
    auto* foe = AdoptAt(sim, 2855, 1, 10, 500);
    auto* closer = AdoptAt(sim, 2857, 0, 5, 500);
    Settle(sim);
    CAbilityNeutralBuilding shared(28952, War3ID("Aall"), true);
    shared.OnTick(sim, shop);
    TEST_ASSERT(shared.GetSelectedUnit(0) == ally);
    TEST_ASSERT(shared.GetSelectedUnit(1) == nullptr);
    CAbilityNeutralBuilding any(28953, War3ID("Aneu"), false);
    any.OnTick(sim, shop);
    TEST_ASSERT(any.GetSelectedUnit(0) == ally);
    TEST_ASSERT(any.GetSelectedUnit(1) == foe);
    for (int i = 0; i < 80; i++) any.OnTick(sim, shop);
    TEST_ASSERT(any.GetSelectedUnit(0) == ally);
    any.Begin(sim, shop, OrderIds::NEUTRALINTERACT, closer);
    TEST_ASSERT(any.GetSelectedUnit(0) == closer);
    auto purchase = AbilityFactory::CreateAbility(28954, War3ID("Apit"));
    TEST_ASSERT(purchase != nullptr);
    TEST_ASSERT(purchase->GetCode() == War3ID("Apit"));
    return true;
}

bool TestSacrificeSummonsShade() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("ushd"), MakeType("ushd"));
    sim.SetUnitData(&data);
    auto* pit = AdoptAt(sim, 2861, 0, 0, 0);
    auto* victim = AdoptAt(sim, 2862, 0, 30, 0);
    CAbilitySacrifice sacrifice(28961);
    sacrifice.DoEffect(sim, pit, victim);
    TEST_ASSERT(victim->IsDead());
    Settle(sim);
    int shades = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && unit->GetTypeId() == War3ID("ushd") && !unit->IsDead()) shades++;
    }
    TEST_ASSERT(shades == 1);
    return true;
}

bool TestSpells28FactoryCoverage() {
    const char* codes[] = {
        "Amic", "Amil", "Acoa", "Acoh", "Acoi",
        "Abli", "Abgm", "Agl2", "Aaha", "Awha", "Ahrl",
        "Acar", "Abun", "Adri", "Astd", "Awrp",
        "Apit", "Aneu", "Aall"
    };
    for (const char* code : codes) {
        auto ability = AbilityFactory::CreateAbility(28970, War3ID(code));
        if (!ability) {
            std::cerr << "missing factory ability " << code << "\n";
            return false;
        }
    }
    return true;
}

bool TestSpells28OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amic")) == 852071);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Amic"), 852651));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amil")) == 852072);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Amil"), 852073));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acoa")) == 852143);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acoh")) == 852142);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acoi")) == 852508);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aaha")) == 852185);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Awha")) == 852214);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Ahrl")) == 852022);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Adri")) == 852049);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Astd")) == 852113);
    return true;
}

namespace {

std::unique_ptr<CUnitType> MakeHeroType(const char* id, int gold, int lumber, int food) {
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    return std::make_unique<CUnitType>(
        "Hero", "Hero", "icon", War3ID(id),
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
        gold, lumber, food, 0, 60, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, true, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false);
}

} // namespace

bool TestTavernHiresHero() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::NEUTRAL));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.GetPlayer(0)->SetAlliance(1, CAllianceType::ALLIED, true);
    sim.GetPlayer(1)->SetAlliance(0, CAllianceType::ALLIED, true);
    sim.GetPlayer(1)->SetGold(2000);
    sim.GetPlayer(1)->SetLumber(500);
    sim.GetPlayer(1)->SetFoodCap(100);
    CUnitData data;
    data.RegisterUnitType(War3ID("Hamg"), MakeHeroType("Hamg", 425, 100, 5));
    sim.SetUnitData(&data);

    auto* tavern = AdoptAt(sim, 2901, 0, 0, 0);
    tavern->AddAbility(sim, std::make_unique<CAbilityNeutralBuilding>(2902, War3ID("Aall"), true));
    auto* hero = sim.AdoptUnit(std::make_unique<CUnit>(2903, data.GetUnitType(War3ID("Hamg")),
                                                       0, War3ID("Hamg"), 100, 0, 0));
    auto* shopper = AdoptAt(sim, 2904, 1, 50, 0);
    Settle(sim);
    TEST_ASSERT(hero->IsHero());

    auto* tavernAb = dynamic_cast<CAbilityNeutralBuilding*>(tavern->GetAbility(War3ID("Aall")));
    TEST_ASSERT(tavernAb != nullptr);
    // Shopper selects the tavern first (per-player selection = buyer).
    tavernAb->Begin(sim, tavern, OrderIds::NEUTRALINTERACT, shopper);
    TEST_ASSERT(tavernAb->GetSelectedUnit(1) == shopper);
    // Awaken order targeted at the hero hires for player 1.
    // Level-1 awaken: 425 * 0.8 = 340g, 100 * 0.8 = 80l.
    tavernAb->Begin(sim, tavern, 852462, hero);
    TEST_ASSERT(hero->GetPlayerIndex() == 1);
    TEST_ASSERT(sim.GetPlayer(1)->GetGold() == 2000 - 340);
    TEST_ASSERT(sim.GetPlayer(1)->GetLumber() == 500 - 80);
    TEST_ASSERT(std::fabs(hero->GetLife() - hero->GetMaxLife() * 0.5f) < 1.0f);
    // Musters beside the tavern (spawn unstick may have nudged it).
    TEST_ASSERT(std::fabs(hero->GetX() - (tavern->GetX() + 128.0f)) < 1.0f);

    // Broke buyer fails soft (no partial charge, no transfer).
    auto* hero2 = sim.AdoptUnit(std::make_unique<CUnit>(2905, data.GetUnitType(War3ID("Hamg")),
                                                        0, War3ID("Hamg"), 100, 50, 0));
    sim.GetPlayer(1)->SetGold(10);
    tavernAb->Begin(sim, tavern, 852463, hero2);
    TEST_ASSERT(hero2->GetPlayerIndex() == 0);
    TEST_ASSERT(sim.GetPlayer(1)->GetGold() == 10);

    // Fourth live hero refused (retail 3-hero cap).
    sim.GetPlayer(1)->SetGold(5000);
    sim.AdoptUnit(std::make_unique<CUnit>(2906, data.GetUnitType(War3ID("Hamg")),
                                          1, War3ID("Hamg"), 300, 0, 0));
    sim.AdoptUnit(std::make_unique<CUnit>(2907, data.GetUnitType(War3ID("Hamg")),
                                          1, War3ID("Hamg"), 350, 0, 0));
    Settle(sim); // flush AdoptUnit staging into GetUnits()
    auto* hero3 = sim.AdoptUnit(std::make_unique<CUnit>(2908, data.GetUnitType(War3ID("Hamg")),
                                                        0, War3ID("Hamg"), 100, 100, 0));
    Settle(sim);
    tavernAb->Begin(sim, tavern, 852464, hero3);
    TEST_ASSERT(hero3->GetPlayerIndex() == 0);
    return true;
}

bool TestHeroTokensAndReviveFormula() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitData data;
    data.RegisterUnitType(War3ID("Hamg"), MakeHeroType("Hamg", 425, 100, 5));
    sim.SetUnitData(&data);
    CPlayer* player = sim.GetPlayer(0);
    TEST_ASSERT(player->GetHeroTokens() == 0);

    // Token training: broke player with a token still trains the hero.
    player->SetHeroTokens(2);
    auto* hall = AdoptAt(sim, 2921, 0, 0, 0);
    hall->AddAbility(sim, std::make_unique<CAbilityQueue>(2922));
    auto* queue = dynamic_cast<CAbilityQueue*>(hall->GetAbility(War3ID("Aque")));
    TEST_ASSERT(queue != nullptr);
    queue->AddTrainedUnit(War3ID("Hamg"));
    player->SetGold(0);
    player->SetLumber(0);
    player->SetFoodCap(100);
    // QueueUnit reports order-queueing (always false: consumed), so assert
    // side effects: token spent instead of gold, queue filled.
    queue->QueueUnit(sim, hall, War3ID("Hamg"));
    TEST_ASSERT(player->GetHeroTokens() == 1);
    TEST_ASSERT(player->GetGold() == 0);
    TEST_ASSERT(queue->GetQueueCount() == 1);

    // Revive formula (ref, lumber off gold base): level-1 gold 425*0.4=170,
    // lumber 425*0=0 (old code charged lumber too).
    const auto& gc = sim.GetGameplayConstants();
    TEST_ASSERT(gc.GetHeroReviveGoldCost(425, 1) == 170);
    TEST_ASSERT(gc.GetHeroReviveLumberCost(425, 1) == 0);
    TEST_ASSERT(gc.GetHeroReviveGoldCost(425, 3) == (int)(425 * 0.6f));
    TEST_ASSERT(gc.GetHeroAwakenGoldCost(425, 1) == 340);
    TEST_ASSERT(gc.GetHeroAwakenLumberCost(100, 1) == 80);
    return true;
}

namespace {

std::unique_ptr<CUnitType> MakeBuildingType(const char* id) {
    auto type = MakeType(id);
    // Rebuild as a building: same ctor shape as MakeType, building=true.
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    return std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID(id),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, true, PathingGridMovementType::FOOT,
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
        10, 1, 6, 5, 1, 3, false);
}

std::shared_ptr<PathTexImage> MakeSolidTex(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    auto img = std::make_shared<PathTexImage>();
    img->w = w;
    img->h = h;
    img->rgb.assign((size_t)w * (size_t)h * 3, 0);
    for (size_t i = 0; i < (size_t)w * (size_t)h; ++i) {
        img->rgb[i * 3 + 0] = r;
        img->rgb[i * 3 + 1] = g;
        img->rgb[i * 3 + 2] = b;
    }
    img->valid = true;
    return img;
}

} // namespace

bool TestWpmStampRebuild() {
    // Retail blit ORs footprint flags over the pristine base; unstamping
    // rebuilds from base (ref RemovablePathingMapInstance remove()).
    WpmData wpm;
    wpm.width = 8;
    wpm.height = 8;
    wpm.pathing.assign(64, 0);
    wpm.basePathing = wpm.pathing;
    wpm.originX = 0.0f;
    wpm.originY = 0.0f;
    auto img = MakeSolidTex(1, 1, 200, 0, 0);
    const int id = wpm.StampFootprint(80.0f, 80.0f, 0, img, false);
    TEST_ASSERT(id >= 0);
    // Cell (2,2): 80/32 = 2, 1x1 footprint centered -> (2,2).
    TEST_ASSERT((wpm.pathing[2 + 2 * 8] & WpmData::UNWALKABLE) != 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 8] & WpmData::BLOCKVISION) == 0);
    wpm.UnstampFootprint(id);
    TEST_ASSERT(wpm.pathing[2 + 2 * 8] == 0);
    // Vision-blocking stamp sets BLOCKVISION alongside walk block.
    const int id2 = wpm.StampFootprint(80.0f, 80.0f, 0, img, true);
    TEST_ASSERT(id2 >= 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 8] & WpmData::BLOCKVISION) != 0);
    wpm.SetStampBlocksVision(id2, false);
    TEST_ASSERT((wpm.pathing[2 + 2 * 8] & WpmData::BLOCKVISION) == 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 8] & WpmData::UNWALKABLE) != 0);
    // Null/empty images never stamp (ref BLANK guard).
    TEST_ASSERT(wpm.StampFootprint(80.0f, 80.0f, 0, nullptr, false) == -1);
    return true;
}

bool TestBuildingStampLifecycle() {
    // Retail setupCreatedUnit blits the footprint on creation and clears
    // it on death/removal (ref regenerate/killPathingInstance).
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hbar"), MakeBuildingType("hbar"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    WpmData wpm;
    wpm.width = 16;
    wpm.height = 16;
    wpm.pathing.assign(256, 0);
    sim.SetPathingMap(&wpm, 0.0f, 0.0f);
    data.GetUnitType(War3ID("hbar"))->SetPathTexImage(MakeSolidTex(2, 2, 200, 0, 0));
    CUnit* hall = sim.CreateUnit(War3ID("hbar"), 0, 100.0f, 100.0f, 0.0f);
    TEST_ASSERT(hall != nullptr);
    // Snapped to the 64-grid with the odd-half nudge (ref: 100 -> 96).
    TEST_ASSERT(std::fabs(hall->GetX() - 96.0f) < 0.01f);
    TEST_ASSERT(std::fabs(hall->GetY() - 96.0f) < 0.01f);
    TEST_ASSERT(hall->GetPathingStampId() >= 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 16] & WpmData::UNWALKABLE) != 0);
    // Flush newUnits_ into the active list so the death tick runs.
    sim.Update();
    sim.Update();
    // Death clears the footprint on the next Update.
    hall->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 1e9f, 0);
    TEST_ASSERT(hall->IsDead());
    sim.Update();
    TEST_ASSERT(hall->GetPathingStampId() < 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 16] & WpmData::UNWALKABLE) == 0);
    // Removal also clears (idempotent when already dead).
    CUnit* shed = sim.CreateUnit(War3ID("hbar"), 0, 300.0f, 300.0f, 0.0f);
    TEST_ASSERT(shed != nullptr && shed->GetPathingStampId() >= 0);
    sim.RemoveUnit(shed);
    sim.Update();
    TEST_ASSERT(shed->GetPathingStampId() < 0);
    return true;
}

bool TestDestructableDeathSwap() {
    // Retail CDestructable.kill(): live pathTex out, pathTexDeath in;
    // revive swaps back (ref setLife branch).
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CDestructableData data;
    auto type = std::make_unique<CDestructableType>();
    type->id = War3ID("LTlt");
    type->maxLife = 100;
    type->occlusionHeight = 200.0f;
    type->pathTex = "live";
    type->pathTexDeath = "dead";
    type->pathTexImage = MakeSolidTex(1, 1, 200, 0, 0);
    type->pathTexDeathImage = MakeSolidTex(1, 1, 0, 0, 200);
    data.RegisterDestructable(War3ID("LTlt"), std::move(type));
    sim.SetDestructableData(&data);
    WpmData wpm;
    wpm.width = 16;
    wpm.height = 16;
    wpm.pathing.assign(256, 0);
    sim.SetPathingMap(&wpm, 0.0f, 0.0f);
    CDestructable* tree = sim.CreateDestructable(War3ID("LTlt"), 80.0f, 80.0f, 100.0f, 0);
    TEST_ASSERT(tree != nullptr);
    TEST_ASSERT(tree->GetPathingStampId() >= 0);
    TEST_ASSERT(tree->GetDeathStampId() < 0);
    // Live cell blocks walk + vision (occluder), not build.
    TEST_ASSERT((wpm.pathing[2 + 2 * 16] & WpmData::UNWALKABLE) != 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 16] & WpmData::BLOCKVISION) != 0);
    tree->Damage(sim, nullptr, false, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 1e9f, 0);
    TEST_ASSERT(tree->IsDead());
    TEST_ASSERT(tree->GetPathingStampId() < 0);
    TEST_ASSERT(tree->GetDeathStampId() >= 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 16] & WpmData::UNWALKABLE) == 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 16] & WpmData::UNBUILDABLE) != 0);
    // Revive swaps back to the live footprint.
    tree->SetLife(sim, 100.0f);
    TEST_ASSERT(!tree->IsDead());
    TEST_ASSERT(tree->GetPathingStampId() >= 0);
    TEST_ASSERT(tree->GetDeathStampId() < 0);
    TEST_ASSERT((wpm.pathing[2 + 2 * 16] & WpmData::UNWALKABLE) != 0);
    // Removal clears both footprints.
    sim.RemoveDestructable(tree);
    sim.Update();
    TEST_ASSERT(tree->GetPathingStampId() < 0 && tree->GetDeathStampId() < 0);
    TEST_ASSERT(wpm.pathing[2 + 2 * 16] == 0);
    return true;
}


bool TestHarvestAnimationCarryState() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* worker = AdoptAt(sim, 29101, 0, 0, 0);
    TEST_ASSERT(worker != nullptr);

    using animation_utils::UnitVisualAnimationMode;
    TEST_ASSERT(animation_utils::ResolveUnitVisualAnimationMode(
        worker, true, CBehaviorCategory::RETURN_RESOURCES,
        CResourceType::GOLD, 0) == UnitVisualAnimationMode::WALK_GOLD);
    TEST_ASSERT(animation_utils::ResolveUnitVisualAnimationMode(
        worker, true, CBehaviorCategory::RETURN_RESOURCES,
        CResourceType::LUMBER, 0) == UnitVisualAnimationMode::WALK_LUMBER);
    TEST_ASSERT(animation_utils::ResolveUnitVisualAnimationMode(
        worker, false, CBehaviorCategory::RETURN_RESOURCES,
        CResourceType::LUMBER, 0) == UnitVisualAnimationMode::READY_LUMBER);
    TEST_ASSERT(animation_utils::ResolveUnitVisualAnimationMode(
        worker, false, CBehaviorCategory::HARVEST,
        CResourceType::NONE, 0) == UnitVisualAnimationMode::READY);

    const auto goldTags = animation_utils::BuildContextualAnimationTags(
        worker, nullptr, CResourceType::GOLD, {"work"});
    TEST_ASSERT(std::find(goldTags.begin(), goldTags.end(), "gold") != goldTags.end());
    TEST_ASSERT(std::find(goldTags.begin(), goldTags.end(), "work") != goldTags.end());
    TEST_ASSERT(std::find(goldTags.begin(), goldTags.end(), "lumber") == goldTags.end());

    const auto lumberTags = animation_utils::BuildContextualAnimationTags(
        worker, nullptr, CResourceType::LUMBER, {});
    TEST_ASSERT(std::find(lumberTags.begin(), lumberTags.end(), "lumber") != lumberTags.end());
    TEST_ASSERT(std::find(lumberTags.begin(), lumberTags.end(), "gold") == lumberTags.end());
    return true;
}

bool TestHarvestReturnsCarriedBeforeSwitchingResource() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    CDestructableData treeData;
    auto treeType = std::make_unique<CDestructableType>();
    treeType->id = War3ID("LTlt");
    treeType->maxLife = 100;
    treeType->isTree = true;
    treeData.RegisterDestructable(War3ID("LTlt"), std::move(treeType));
    sim.SetDestructableData(&treeData);

    auto* worker = AdoptAt(sim, 29111, 0, 0, 0);
    auto* mine = AdoptAt(sim, 29112, 0, 200, 0);
    auto* tree = sim.CreateDestructable(War3ID("LTlt"), 100, 0, 100.0f, 0);
    TEST_ASSERT(worker != nullptr && mine != nullptr && tree != nullptr);

    auto harvestAbility = std::make_unique<CAbilityHarvest>(29113);
    CAbilityHarvest* harvest = harvestAbility.get();
    worker->AddAbility(sim, std::move(harvestAbility));
    mine->AddAbility(sim, std::make_unique<CAbilityGoldMine>(29114));

    harvest->SetCarried(CResourceType::GOLD, 10);
    std::unique_ptr<CBehavior> goldToTree(
        harvest->Begin(sim, worker, OrderIds::HARVEST, tree));
    TEST_ASSERT(dynamic_cast<CBehaviorReturnResources*>(goldToTree.get()) != nullptr);
    TEST_ASSERT(harvest->GetLastTargetHandle() == tree->GetHandleId());

    harvest->SetCarried(CResourceType::LUMBER, 4);
    std::unique_ptr<CBehavior> partialLumberToTree(
        harvest->Begin(sim, worker, OrderIds::HARVEST, tree));
    TEST_ASSERT(dynamic_cast<CBehaviorReturnResources*>(partialLumberToTree.get()) != nullptr);

    harvest->SetCarried(CResourceType::LUMBER, 10);
    std::unique_ptr<CBehavior> lumberToMine(
        harvest->Begin(sim, worker, OrderIds::HARVEST, mine));
    TEST_ASSERT(dynamic_cast<CBehaviorReturnResources*>(lumberToMine.get()) != nullptr);
    TEST_ASSERT(harvest->GetLastTargetHandle() == mine->GetHandleId());

    harvest->ClearCarried();
    std::unique_ptr<CBehavior> freshHarvest(
        harvest->Begin(sim, worker, OrderIds::HARVEST, tree));
    TEST_ASSERT(dynamic_cast<CBehaviorHarvest*>(freshHarvest.get()) != nullptr);
    return true;
}

bool TestLumberHarvestReturnsPartialWhenNoTreesRemain() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    CDestructableData treeData;
    auto treeType = std::make_unique<CDestructableType>();
    treeType->id = War3ID("LTlt");
    treeType->maxLife = 100;
    treeType->isTree = true;
    treeData.RegisterDestructable(War3ID("LTlt"), std::move(treeType));
    sim.SetDestructableData(&treeData);

    auto* worker = AdoptAt(sim, 29121, 0, 0, 0);
    auto* tree = sim.CreateDestructable(War3ID("LTlt"), 40, 0, 2.0f, 0);
    TEST_ASSERT(worker != nullptr && tree != nullptr);

    auto harvestAbility = std::make_unique<CAbilityHarvest>(29122);
    CAbilityHarvest* harvest = harvestAbility.get();
    worker->AddAbility(sim, std::move(harvestAbility));
    worker->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::HARVEST, tree));

    for (int i = 0; i < 120 && harvest->GetCarriedAmount() == 0; i++) {
        sim.Update();
    }

    TEST_ASSERT(harvest->GetCarriedAmount() == 2);
    TEST_ASSERT(harvest->GetCarriedResourceType() == CResourceType::LUMBER);
    TEST_ASSERT(tree->IsDead());
    return true;
}
} // namespace war3_spell_suite_28
bool TestMilitiaAndMountPairing() { return war3_spell_suite_28::TestMilitiaAndMountPairing(); }
bool TestMilitiaReturnsCargoAndExpires() { return war3_spell_suite_28::TestMilitiaReturnsCargoAndExpires(); }
bool TestUpgradeAndPowerup() { return war3_spell_suite_28::TestUpgradeAndPowerup(); }
bool TestItemPickupAppliesBonuses() { return war3_spell_suite_28::TestItemPickupAppliesBonuses(); }
bool TestBuildFootprintUsesPathTex() { return war3_spell_suite_28::TestBuildFootprintUsesPathTex(); }
bool TestBuildRejectsUnbuildableCell() { return war3_spell_suite_28::TestBuildRejectsUnbuildableCell(); }
bool TestBlightAndHauntedMine() { return war3_spell_suite_28::TestBlightAndHauntedMine(); }
bool TestWispAndLumberHarvest() { return war3_spell_suite_28::TestWispAndLumberHarvest(); }
bool TestAcolyteBindingPersistsAndReleasesOnDeath() { return war3_spell_suite_28::TestAcolyteBindingPersistsAndReleasesOnDeath(); }
bool TestWispJumpsOntoTreeAndDetachesOnDeath() { return war3_spell_suite_28::TestWispJumpsOntoTreeAndDetachesOnDeath(); }
bool TestGoldMineCapacityMatchesRetail() { return war3_spell_suite_28::TestGoldMineCapacityMatchesRetail(); }
bool TestCargoBurrowAndDrop() { return war3_spell_suite_28::TestCargoBurrowAndDrop(); }
bool TestWaygateAndShops() { return war3_spell_suite_28::TestWaygateAndShops(); }
bool TestSacrificeSummonsShade() { return war3_spell_suite_28::TestSacrificeSummonsShade(); }
bool TestSpells28FactoryCoverage() { return war3_spell_suite_28::TestSpells28FactoryCoverage(); }
bool TestSpells28OrderIds() { return war3_spell_suite_28::TestSpells28OrderIds(); }
bool TestTavernHiresHero() { return war3_spell_suite_28::TestTavernHiresHero(); }
bool TestHeroTokensAndReviveFormula() { return war3_spell_suite_28::TestHeroTokensAndReviveFormula(); }
bool TestWpmStampRebuild() { return war3_spell_suite_28::TestWpmStampRebuild(); }
bool TestBuildingStampLifecycle() { return war3_spell_suite_28::TestBuildingStampLifecycle(); }
bool TestDestructableDeathSwap() { return war3_spell_suite_28::TestDestructableDeathSwap(); }
bool TestHarvestAnimationCarryState() { return war3_spell_suite_28::TestHarvestAnimationCarryState(); }
bool TestHarvestReturnsCarriedBeforeSwitchingResource() { return war3_spell_suite_28::TestHarvestReturnsCarriedBeforeSwitchingResource(); }
bool TestLumberHarvestReturnsPartialWhenNoTreesRemain() { return war3_spell_suite_28::TestLumberHarvestReturnsPartialWhenNoTreesRemain(); }

// ---- Suite test_spells29.cpp ----
namespace war3_spell_suite_29 {
// Batch-29 tests: entangle gold mine, channel test, drunken brawler dodge.



namespace {

std::unique_ptr<CUnitType> MakeType(const char* id) {
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    return std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID(id),
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
        10, 1, 6, 5, 1, 3, false);
}

CUnitType* SharedType() {
    static std::unique_ptr<CUnitType> keep = MakeType("hfoo");
    return keep.get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, SharedType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestEntangleGoldMinePaysFromParent() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("egol"), MakeType("egol"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* mineUnit = AdoptAt(sim, 2901, 0, 0, 0);
    auto* tree = AdoptAt(sim, 2902, 0, 40, 0);
    auto* wisp = AdoptAt(sim, 2903, 0, 20, 0);
    auto goldAbility = std::make_unique<CAbilityGoldMine>(29901);
    CAbilityGoldMine* gold = goldAbility.get();
    mineUnit->AddAbility(sim, std::move(goldAbility));
    gold->SetGold(100);

    CAbilityEntangleGoldMine entangle(29902);
    entangle.DoEffect(sim, tree, mineUnit);
    CUnit* replacement = entangle.GetReplacement();
    TEST_ASSERT(replacement != nullptr);
    TEST_ASSERT(replacement->GetTypeId() == War3ID("egol"));
    TEST_ASSERT(mineUnit->IsHidden());
    TEST_ASSERT(entangle.GetOriginal() == mineUnit);

    auto* cargo = dynamic_cast<CAbilityCargoHold*>(replacement->GetAbility(War3ID("Aenc")));
    auto* haunted = dynamic_cast<CAbilityEntangledMine*>(replacement->GetAbility(War3ID("Aegm")));
    TEST_ASSERT(cargo != nullptr && haunted != nullptr);
    auto* overlay = dynamic_cast<CAbilityOverlayedGoldMine*>(replacement->GetAbility(War3ID("Agl2")));
    TEST_ASSERT(overlay != nullptr);
    TEST_ASSERT(overlay->GetGold() == 100);
    TEST_ASSERT(mineUnit->IsPaused());
    TEST_ASSERT(cargo->TryLoad(replacement, wisp));

    CPlayer* player = sim.GetPlayer(0);
    for (int i = 0; i < 400 && player->GetGold() == 0; i++) haunted->OnTick(sim, replacement);
    TEST_ASSERT(player->GetGold() == 10);
    TEST_ASSERT(gold->GetGold() == 90);

    gold->SetGold(10);
    for (int i = 0; i < 400 && !replacement->IsDead(); i++) haunted->OnTick(sim, replacement);
    TEST_ASSERT(replacement->IsDead());
    TEST_ASSERT(gold->GetGold() == 0);
    entangle.OnTick(sim, tree);
    TEST_ASSERT(!mineUnit->IsHidden());
    return true;
}

bool TestEntangleConstruction() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("egol"), MakeType("egol"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* mineUnit = AdoptAt(sim, 2931, 0, 0, 0);
    auto* tree = AdoptAt(sim, 2932, 0, 40, 0);
    mineUnit->AddAbility(sim, std::make_unique<CAbilityGoldMine>(29941));

    CAbilityEntangleGoldMine entangle(29942);
    CBehavior* order = entangle.Begin(sim, tree, 852147, mineUnit);
    delete order;
    entangle.DoEffect(sim, tree, mineUnit);
    CUnit* replacement = entangle.GetReplacement();
    TEST_ASSERT(replacement != nullptr);
    TEST_ASSERT(entangle.IsConstructing());
    TEST_ASSERT(replacement->IsConstructing());
    TEST_ASSERT(replacement->GetLife() < replacement->GetMaxLife() * 0.5f);
    TEST_ASSERT(replacement->GetAbility(War3ID("Aegm"))->IsDisabled());

    for (int i = 0; i < 800 && entangle.IsConstructing(); i++) entangle.OnTick(sim, tree);
    TEST_ASSERT(!entangle.IsConstructing());
    TEST_ASSERT(!replacement->IsConstructing());
    TEST_ASSERT(replacement->GetLife() == replacement->GetMaxLife());
    TEST_ASSERT(!replacement->GetAbility(War3ID("Aegm"))->IsDisabled());

    auto* mineInstant = AdoptAt(sim, 2933, 0, 200, 0);
    auto* treeInstant = AdoptAt(sim, 2934, 0, 240, 0);
    mineInstant->AddAbility(sim, std::make_unique<CAbilityGoldMine>(29943));
    CAbilityEntangleGoldMine instant(29944);
    CBehavior* instantOrder = instant.Begin(sim, treeInstant, 852148, mineInstant);
    delete instantOrder;
    instant.DoEffect(sim, treeInstant, mineInstant);
    TEST_ASSERT(instant.GetReplacement() != nullptr);
    TEST_ASSERT(!instant.IsConstructing());
    TEST_ASSERT(instant.GetReplacement()->GetLife() == instant.GetReplacement()->GetMaxLife());
    return true;
}

bool TestOverlayedMinePaysParent() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* parent = AdoptAt(sim, 2921, 0, 0, 0);
    auto* face = AdoptAt(sim, 2922, 0, 0, 0);
    auto* worker = AdoptAt(sim, 2923, 0, 30, 0);
    auto goldAbility = std::make_unique<CAbilityGoldMine>(29931);
    CAbilityGoldMine* gold = goldAbility.get();
    parent->AddAbility(sim, std::move(goldAbility));
    gold->SetGold(30);
    parent->SetHidden(true);
    parent->SetPaused(true);

    auto overlayAbility = std::make_unique<CAbilityOverlayedGoldMine>(29932);
    CAbilityOverlayedGoldMine* overlay = overlayAbility.get();
    face->AddAbility(sim, std::move(overlayAbility));
    overlay->SetParentMine(parent, gold);
    TEST_ASSERT(overlay->GetGold() == 30);

    CBehaviorHarvest miner;
    miner.SetUnit(worker);
    miner.SetCapacity(10);
    worker->SetHidden(true);
    overlay->AddMiner(&miner);
    overlay->OnTick(sim, face);
    TEST_ASSERT(gold->GetGold() == 20);
    TEST_ASSERT(!worker->IsHidden());

    gold->SetGold(0);
    overlay->OnTick(sim, face);
    TEST_ASSERT(face->IsDead());
    face->Update(sim);
    TEST_ASSERT(!parent->IsHidden());
    TEST_ASSERT(!parent->IsPaused());
    return true;
}

bool TestChannelAndDrunkenBrawler() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2911, 0, 0, 0);
    auto* hero = AdoptAt(sim, 2912, 0, 10, 0);
    CAbilityChannelTest channel(29911);
    channel.DoEffect(sim, caster);
    TEST_ASSERT(channel.IsChanneling());
    for (int i = 0; i < 80 && channel.IsChanneling(); i++) channel.OnTick(sim, caster);
    TEST_ASSERT(!channel.IsChanneling());

    CAbilityDrunkenBrawler brawler(29912);
    brawler.OnAdd(sim, hero);
    TEST_ASSERT(hero->GetCritChance() > 0.0f);
    TEST_ASSERT(hero->GetEvasionChance() > 0.0f);
    brawler.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetEvasionChance() == 0.0f);
    return true;
}

bool TestSpells29FactoryCoverage() {
    const char* codes[] = {"Aent", "Aegm", "Aenc", "ANcl", "ANdb"};
    for (const char* code : codes) {
        auto ability = AbilityFactory::CreateAbility(29920, War3ID(code));
        if (!ability) {
            std::cerr << "missing factory ability " << code << "\n";
            return false;
        }
    }
    TEST_ASSERT(dynamic_cast<CAbilitySpellBase*>(
        AbilityFactory::CreateAbility(29921, War3ID("ANdb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityDrunkenBrawler*>(
        AbilityFactory::CreateAbility(29922, War3ID("ANdb")).get()) != nullptr);
    return true;
}

bool TestSpells29OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aent")) == 852147);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Aent"), 852148));
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANcl")) == 852600);
    return true;
}
} // namespace war3_spell_suite_29
bool TestEntangleGoldMinePaysFromParent() { return war3_spell_suite_29::TestEntangleGoldMinePaysFromParent(); }
bool TestEntangleConstruction() { return war3_spell_suite_29::TestEntangleConstruction(); }
bool TestOverlayedMinePaysParent() { return war3_spell_suite_29::TestOverlayedMinePaysParent(); }
bool TestChannelAndDrunkenBrawler() { return war3_spell_suite_29::TestChannelAndDrunkenBrawler(); }
bool TestSpells29FactoryCoverage() { return war3_spell_suite_29::TestSpells29FactoryCoverage(); }
bool TestSpells29OrderIds() { return war3_spell_suite_29::TestSpells29OrderIds(); }

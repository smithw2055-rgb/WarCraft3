// Batch-12 tests: detection (true sight, sentry ward, flare) plus
// burrow, cannibalize, carrion beetles, web.
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
#include "ecs/abilities/spells12.h"
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

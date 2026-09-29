// Batch-15 (abilities) test: Phoenix summon with egg rebirth.
#include <filesystem>
#include <iostream>
#include <memory>

#include <cmath>

#include "data/abilitydata.h"
#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells15.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

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

// P0-8 death sequence tests: worker popout, food refund, death vision,
// mid-construction death explosion (ref CUnit.kill).
#include <cmath>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/csimulation.h"
#include "ecs/fog_state.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

CUnitType* MakeDeathType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 0.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 300, 0, War3ID(),
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
        0.5f, 10.0f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

void Kill(CSimulation& sim, CUnit* victim, CUnit* killer = nullptr) {
    victim->Damage(sim, killer, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 1e9f, 0);
}

} // namespace

// Food is refunded at death (ref kill zeroes foodMade/foodUsed), not later
// at corpse removal.
bool TestDeathRefundsFoodAtDeath() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* unit = sim.AdoptUnit(
        std::make_unique<CUnit>(101, MakeDeathType(), 0, War3ID("hfoo"), 0, 0, 0));
    unit->SetFoodUsed(2);
    unit->SetFoodMade(3);
    sim.GetPlayer(0)->SetFoodCap(10);
    sim.Update(); // join: player foodUsed 0 -> 2
    TEST_ASSERT(sim.GetPlayer(0)->GetFoodUsed() == 2);
    Kill(sim, unit);
    sim.Update(); // death tick
    TEST_ASSERT(unit->IsDead());
    TEST_ASSERT(sim.GetPlayer(0)->GetFoodUsed() == 0);
    TEST_ASSERT(sim.GetPlayer(0)->GetFoodCap() == 7);
    TEST_ASSERT(unit->GetFoodUsed() == 0);
    TEST_ASSERT(unit->GetFoodMade() == 0);
    return true;
}

// A worker hidden inside a foundation pops back out when it dies
// (ref popoutWorker); the link is single-use.
bool TestDeathPopsOutWorker() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* hall = sim.AdoptUnit(
        std::make_unique<CUnit>(102, MakeDeathType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* peon = sim.AdoptUnit(
        std::make_unique<CUnit>(103, MakeDeathType(), 0, War3ID("hpea"), 0, 0, 0));
    sim.Update();
    peon->SetHidden(true);
    peon->SetPaused(true);
    peon->SetInvulnerable(true);
    hall->SetWorker(peon->GetHandleId(), true);
    Kill(sim, hall);
    sim.Update(); // death tick
    TEST_ASSERT(hall->IsDead());
    TEST_ASSERT(!peon->IsHidden());
    TEST_ASSERT(!peon->IsPaused());
    TEST_ASSERT(!peon->IsInvulnerable());
    TEST_ASSERT(hall->GetWorkerHandle() < 0);
    return true;
}

// Dying reveals the death spot to the owner and the killer for FogFlashTime
// (ref CUnitDeathVisionFogModifier), then the reveal expires.
bool TestDeathVisionRevealsOwnerAndKiller() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* victim = sim.AdoptUnit(
        std::make_unique<CUnit>(104, MakeDeathType(), 0, War3ID("hfoo"), 400, 0, 0));
    auto* killer = sim.AdoptUnit(
        std::make_unique<CUnit>(105, MakeDeathType(), 1, War3ID("hfoo"), 500, 0, 0));
    sim.Update();
    Kill(sim, victim, killer);
    sim.Update(); // death tick
    TEST_ASSERT(victim->IsDead());
    TEST_ASSERT(sim.GetFogSystem()->GetState(0, 400, 0) == CFogState::VISIBLE);
    TEST_ASSERT(sim.GetFogSystem()->GetState(1, 400, 0) == CFogState::VISIBLE);
    // FogFlashTime (3s = 60 ticks) passes: the flash lapses. The victim
    // had seen its own death spot while alive, so the cell settles back to
    // explored (FOGGED), not black mask.
    for (int i = 0; i < 70; i++) sim.Update();
    TEST_ASSERT(sim.GetFogSystem()->GetState(0, 400, 0) == CFogState::FOGGED);
    return true;
}

// A foundation killed mid-construction fires the death-explode hook with its
// explode buff id (ref kill constructing branch -> createDeathExplodeEffect).
bool TestConstructingDeathFiresExplode() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    int firedHandle = -1;
    War3ID firedBuff(0u);
    int fireCount = 0;
    sim.SetDeathExplodeCallback([&](int handle, War3ID buff) {
        firedHandle = handle;
        firedBuff = buff;
        fireCount++;
    });
    auto* site = sim.AdoptUnit(
        std::make_unique<CUnit>(106, MakeDeathType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update();
    site->SetConstructing(true);
    site->SetExplodeOnDeathBuffId(War3ID("Babr"));
    Kill(sim, site);
    sim.Update(); // death tick
    TEST_ASSERT(site->IsDead());
    TEST_ASSERT(fireCount == 1);
    TEST_ASSERT(firedHandle == site->GetHandleId());
    TEST_ASSERT(firedBuff == War3ID("Babr"));
    return true;
}

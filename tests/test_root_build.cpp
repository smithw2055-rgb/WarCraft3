// Root/Uproot + race build model tests (ref rooted combat, NE sacrifice).
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/abilities/ability_build.h"
#include "ecs/abilities/ability_root.h"
#include "ecs/behaviors/behavior_build.h"
#include "ecs/behaviors/behavior_move.h"
#include "ecs/csimulation.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeRootTestType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 300.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
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

} // namespace

bool TestRootUprootSwapAndHold() {
    CSimulation sim;
    CUnit ancient(5001, MakeRootTestType(), 0, War3ID("hfoo"), 0, 0, 0);
    ancient.AddAbility(sim, std::make_unique<CAbilityRoot>(601));
    ancient.AddAbility(sim, std::make_unique<CAbilityUproot>(602));

    auto* root = dynamic_cast<CAbilityRoot*>(ancient.GetAbility(War3ID("Aroo")));
    auto* uproot = dynamic_cast<CAbilityUproot*>(ancient.GetAbility(War3ID("Aro1")));
    TEST_ASSERT(root && uproot);
    TEST_ASSERT(!ancient.IsRooted());
    TEST_ASSERT(uproot->IsDisabled()); // hidden until rooted (ability swap)

    // Root: state set, card swapped.
    TEST_ASSERT(root->CheckCanTargetNoTarget(sim, &ancient, 0));
    root->BeginNoTarget(sim, &ancient, 0);
    TEST_ASSERT(ancient.IsRooted());
    TEST_ASSERT(root->IsDisabled());
    TEST_ASSERT(!uproot->IsDisabled());
    TEST_ASSERT(uproot->CheckCanTargetNoTarget(sim, &ancient, 0));
    TEST_ASSERT(!root->CheckCanTargetNoTarget(sim, &ancient, 0));

    // Rooted unit: move behavior completes immediately (holds position).
    CBehaviorMove move;
    move.SetUnit(&ancient);
    move.SetTarget(1000.0f, 1000.0f);
    move.Begin(sim);
    TEST_ASSERT(move.Update(sim) == true);
    TEST_ASSERT(std::fabs(ancient.GetX()) < 0.001f && std::fabs(ancient.GetY()) < 0.001f);

    // Uproot: state cleared, card swapped back.
    TEST_ASSERT(uproot->CheckCanTargetNoTarget(sim, &ancient, 0));
    uproot->BeginNoTarget(sim, &ancient, 0);
    TEST_ASSERT(!ancient.IsRooted());
    TEST_ASSERT(uproot->IsDisabled());
    TEST_ASSERT(!root->IsDisabled());
    return true;
}

bool TestNightElfBuildConsumesWisp() {
    CSimulation sim;
    CUnitType* type = MakeRootTestType();
    auto* wisp = sim.AdoptUnit(std::make_unique<CUnit>(5101, type, 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(wisp != nullptr);
    sim.Update();

    CBehaviorBuild build;
    build.SetUnit(wisp);
    build.SetBuildTarget(0.0f, 0.0f, War3ID("hfoo"), CBuildRace::NIGHT_ELF);
    build.Begin(sim);
    for (int i = 0; i < 10 && !build.Update(sim); i++) {
        sim.Update();
    }
    TEST_ASSERT(!build.IsActivelyBuilding());
    sim.Update(); // process removals
    // The wisp is sacrificed: gone from the simulation.
    TEST_ASSERT(sim.GetUnit(5101) == nullptr);
    return true;
}

bool TestHumanBuildPausesWhenWorkerLeaves() {
    CSimulation sim;
    CUnitType* type = MakeRootTestType();
    auto* peasant = sim.AdoptUnit(std::make_unique<CUnit>(5201, type, 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(peasant != nullptr);
    sim.Update();

    CBehaviorBuild build;
    build.SetUnit(peasant);
    build.SetBuildTarget(0.0f, 0.0f, War3ID("hfoo"), CBuildRace::HUMAN);
    build.Begin(sim);
    TEST_ASSERT(build.Update(sim) == false); // enters building phase at site
    TEST_ASSERT(build.IsActivelyBuilding());

    // Worker walks away: construction stalls, behavior stays active.
    peasant->SetPosition(1000.0f, 1000.0f);
    TEST_ASSERT(build.Update(sim) == false);
    TEST_ASSERT(build.IsActivelyBuilding());

    // Worker returns: construction resumes and completes (0 build time in
    // a dataless sim means immediate finish once unpaused).
    peasant->SetPosition(0.0f, 0.0f);
    TEST_ASSERT(build.Update(sim) == true);
    TEST_ASSERT(!build.IsActivelyBuilding());
    // Human worker survives.
    TEST_ASSERT(sim.GetUnit(5201) != nullptr);
    return true;
}

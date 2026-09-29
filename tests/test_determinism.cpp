// RNG determinism tests (lockstep foundation): every game-affecting roll
// flows through seeded LCG streams (sim + combat); random_device and
// std::rand stay banned from sim code. Two games from the same seed must
// roll identical sequences and hash identical states.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/csimulation.h"
#include "ecs/combat/combat_system.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeDetType() {
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

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) return JassValue::Null();
    return it->second(args);
}

} // namespace

bool TestSeedRngReproducesStreams() {
    CSimulation a, b;
    a.SeedRng(42);
    b.SeedRng(42);
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT(a.SimRand01() == b.SimRand01());
    }
    // First values of different seeds differ (LCG avalanche on high bits).
    CSimulation d, e;
    d.SeedRng(1);
    e.SeedRng(2);
    TEST_ASSERT(d.SimRand01() != e.SimRand01());
    return true;
}

bool TestCombatSequencesDeterministic() {
    CSimulation a, b;
    a.SeedRng(777);
    b.SeedRng(777);
    auto* aa = a.AdoptUnit(std::make_unique<CUnit>(2201, MakeDetType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* at = a.AdoptUnit(std::make_unique<CUnit>(2202, MakeDetType(), 1, War3ID("hfoo"), 50, 0, 0));
    auto* ba = b.AdoptUnit(std::make_unique<CUnit>(2201, MakeDetType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* bt = b.AdoptUnit(std::make_unique<CUnit>(2202, MakeDetType(), 1, War3ID("hfoo"), 50, 0, 0));
    a.Update();
    b.Update();
    // Exercise dice + evasion + crit in one call shape.
    at->SetEvasionChance(0.5f);
    bt->SetEvasionChance(0.5f);
    aa->SetCriticalStrike(0.5f, 2.0f);
    ba->SetCriticalStrike(0.5f, 2.0f);
    for (int i = 0; i < 32; i++) {
        DamageResult ra = a.GetCombatSystem()->CalculateDamage(
            aa, at, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 10, 0, 2, 6);
        DamageResult rb = b.GetCombatSystem()->CalculateDamage(
            ba, bt, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 10, 0, 2, 6);
        TEST_ASSERT(ra.damageDealt == rb.damageDealt);
        TEST_ASSERT(ra.isEvaded == rb.isEvaded);
        TEST_ASSERT(ra.isCritical == rb.isCritical);
        // Heal back so both stay alive identically.
        at->SetLife(a, 10000.0f);
        bt->SetLife(b, 10000.0f);
    }
    TEST_ASSERT(a.HashState() == b.HashState());
    return true;
}

bool TestJassRandomFollowsSeed() {
    CSimulation sim;
    JassVM vm;
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    CallNative(vm, "SetRandomSeed", {JassValue::Int(1234)});
    const int first = CallNative(vm, "GetRandomInt", {JassValue::Int(0), JassValue::Int(1000000)}).intVal;
    CallNative(vm, "SetRandomSeed", {JassValue::Int(1234)});
    const int second = CallNative(vm, "GetRandomInt", {JassValue::Int(0), JassValue::Int(1000000)}).intVal;
    TEST_ASSERT(first == second);
    const float rf = CallNative(vm, "GetRandomReal", {JassValue::Real(0), JassValue::Real(1)}).realVal;
    TEST_ASSERT(rf >= 0.0f && rf < 1.0f);
    return true;
}

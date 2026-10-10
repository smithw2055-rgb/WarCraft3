// Player ability availability, hero level stripping, hero skill events
// (ref setAbilityEnabled sweep, UnitStripHeroLevel, HERO_SKILL scope).
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/abilities/ability_attack.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace {

CUnitType* MakeAvailHeroType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Paladin", "Paladin", "icon", War3ID("Hpal"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 2, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::HERO,
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
        1, true, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

const char* kSkillEventScript = R"JASS(
globals
    integer skillFires = 0
    integer skillSeen = 0
    integer skillLevelSeen = 0
endglobals

function OnLearnSkill takes nothing returns nothing
    set skillFires = skillFires + 1
    set skillSeen = GetLearnedSkill()
    set skillLevelSeen = GetLearnedSkillLevel()
endfunction
)JASS";

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) {
        std::cerr << "missing native " << name << "\n";
        return JassValue::Null();
    }
    return it->second(args);
}

} // namespace

bool TestPlayerAbilityAvailable() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(9001, MakeAvailHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(unit);
    unit->AddAbility(sim, std::make_unique<CAbilityHero>(9002));
    unit->AddAbility(sim, std::make_unique<CAbilityAttack>(9003));
    sim.Update();
    const int ph = data->AllocHandle(JassHandleInfo::PLAYER, 0);
    const int attackCode = (int)War3ID("Aatk").value;
    auto* attack = unit->GetAbility(War3ID("Aatk"));
    auto* hero = unit->GetAbility(War3ID("AHer"));
    TEST_ASSERT(attack && hero);
    TEST_ASSERT(!attack->IsDisabled());

    // Disable: attack flagged PLAYER-disabled, hero untouched.
    CallNative(vm, "SetPlayerAbilityAvailable",
               {JassValue::Int(ph), JassValue::Int(attackCode), JassValue::Bool(false)});
    TEST_ASSERT(attack->IsDisabled());
    TEST_ASSERT(!hero->IsDisabled());
    // Re-enable clears only the PLAYER bit.
    CallNative(vm, "SetPlayerAbilityAvailable",
               {JassValue::Int(ph), JassValue::Int(attackCode), JassValue::Bool(true)});
    TEST_ASSERT(!attack->IsDisabled());
    // Bad player is a safe no-op.
    CallNative(vm, "SetPlayerAbilityAvailable",
               {JassValue::Int(999999), JassValue::Int(attackCode), JassValue::Bool(false)});
    TEST_ASSERT(!attack->IsDisabled());
    return true;
}

bool TestStripHeroLevel() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(9011, MakeAvailHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(unit);
    unit->AddAbility(sim, std::make_unique<CAbilityHero>(9012));
    auto* peon = sim.AdoptUnit(std::make_unique<CUnit>(9013, MakeAvailHeroType(), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(peon);
    sim.Update();
    const int uh = data->AllocHandle(JassHandleInfo::UNIT, 9011);
    const int peonHandle = data->AllocHandle(JassHandleInfo::UNIT, 9013);

    // Level 1 (and non-heroes / bad handles) refuse.
    TEST_ASSERT(!CallNative(vm, "UnitStripHeroLevel", {JassValue::Int(uh), JassValue::Int(2)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetHeroLevel", {JassValue::Int(uh)}).intVal == 1);
    TEST_ASSERT(!CallNative(vm, "UnitStripHeroLevel", {JassValue::Int(peonHandle), JassValue::Int(1)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "UnitStripHeroLevel", {JassValue::Int(999999), JassValue::Int(1)}).boolVal);

    // Level to 3 (500 XP), strip one: level 2 with threshold XP.
    CallNative(vm, "AddHeroXP", {JassValue::Int(uh), JassValue::Int(500), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "GetHeroLevel", {JassValue::Int(uh)}).intVal == 3);
    TEST_ASSERT(CallNative(vm, "UnitStripHeroLevel", {JassValue::Int(uh), JassValue::Int(1)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetHeroLevel", {JassValue::Int(uh)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "GetHeroXP", {JassValue::Int(uh)}).intVal == 200);
    // Over-strip clamps at 1.
    TEST_ASSERT(CallNative(vm, "UnitStripHeroLevel", {JassValue::Int(uh), JassValue::Int(9)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetHeroLevel", {JassValue::Int(uh)}).intVal == 1);
    return true;
}

bool TestHeroSkillEvent() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    TEST_ASSERT(vm.LoadScript(kSkillEventScript));
    auto data = natives.GetData();
    sim.SetHeroSkillCallback([&](int unitHandle, int skillId, int newLevel) {
        natives.FireHeroSkillEvent(unitHandle, skillId, newLevel);
    });

    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(9021, MakeAvailHeroType(), 0, War3ID("Hpal"), 0, 0, 0));
    TEST_ASSERT(unit);
    unit->AddAbility(sim, std::make_unique<CAbilityHero>(9022));
    unit->AddAbility(sim, std::make_unique<CAbilityAttack>(9023));
    auto* hero = dynamic_cast<CAbilityHero*>(unit->GetAbility(War3ID("AHer")));
    TEST_ASSERT(hero);
    hero->AddLearnableSkill(War3ID("Aatk"));
    sim.Update();
    const int uh = data->AllocHandle(JassHandleInfo::UNIT, 9021);

    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerRegisterUnitEvent",
               {JassValue::Int(trig), JassValue::Int(uh), JassValue::Int(79)});
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("OnLearnSkill")});

    // Attack starts at level 1; learning bumps to 2 and reports it.
    CallNative(vm, "SelectHeroSkill",
               {JassValue::Int(uh), JassValue::Int((int)War3ID("Aatk").value)});
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("skillFires").intVal == 1);
    TEST_ASSERT(vm.GetGlobal("skillSeen").intVal == (int)War3ID("Aatk").value);
    TEST_ASSERT(vm.GetGlobal("skillLevelSeen").intVal == 2);
    auto* attack = unit->GetAbility(War3ID("Aatk"));
    TEST_ASSERT(attack && attack->GetLevel() == 2);

    // Level the same skill again after granting a point: level 3 reported.
    hero->SetSkillPoints(1);
    CallNative(vm, "SelectHeroSkill",
               {JassValue::Int(uh), JassValue::Int((int)War3ID("Aatk").value)});
    natives.PumpQueuedThreads(1024);
    TEST_ASSERT(vm.GetGlobal("skillFires").intVal == 2);
    TEST_ASSERT(vm.GetGlobal("skillLevelSeen").intVal == 3);
    return true;
}

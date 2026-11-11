// Batch-4 native tests: unit type flags, permanent abilities, timed life
// apply/pause, sleep family, alarm/icon/progress flags, BuffEx
// counting/removal, delayed point damage, indicators, waygates.
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/abilities/ability_hero.h"
#include "game/war3/abilities/war3_spells.h"
#include "ecs/buffs.h"
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

CUnitType* MakeStateType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Footman", "Footman", "icon", War3ID("hfoo"),
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

struct Fixture {
    JassVM vm;
    CSimulation sim;
    std::unique_ptr<JassNatives> natives;
    std::shared_ptr<JassNativesData> data;
    CUnit* unit = nullptr;
    int handle = 0;

    Fixture() {
        sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
        unit = sim.AdoptUnit(std::make_unique<CUnit>(8501, MakeStateType(), 0, War3ID("hfoo"), 0, 0, 0));
        sim.Update();
        handle = data->AllocHandle(JassHandleInfo::UNIT, 8501);
    }
};

} // namespace

bool TestUnitTypeFlagsAndPermanent() {
    Fixture f;
    TEST_ASSERT(CallNative(f.vm, "UnitAddType", {JassValue::Int(f.handle), JassValue::Int(7)}).boolVal);
    TEST_ASSERT(f.unit->HasUnitTypeFlag(7));
    TEST_ASSERT(CallNative(f.vm, "UnitRemoveType", {JassValue::Int(f.handle), JassValue::Int(7)}).boolVal);
    TEST_ASSERT(!f.unit->HasUnitTypeFlag(7));
    TEST_ASSERT(!CallNative(f.vm, "UnitAddType", {JassValue::Int(999999), JassValue::Int(7)}).boolVal);

    f.unit->AddAbility(f.sim, std::make_unique<CAbilityHero>(8502));
    const int heroCode = ((int)'A' << 24) | ((int)'H' << 16) | ((int)'e' << 8) | (int)'r';
    TEST_ASSERT(CallNative(f.vm, "UnitMakeAbilityPermanent",
        {JassValue::Int(f.handle), JassValue::Bool(true), JassValue::Int(heroCode)}).boolVal);
    TEST_ASSERT(f.unit->GetAbility(War3ID("AHer"))->IsPermanent());
    TEST_ASSERT(!CallNative(f.vm, "UnitMakeAbilityPermanent",
        {JassValue::Int(f.handle), JassValue::Bool(true), JassValue::Int(0x58585858)}).boolVal);
    return true;
}

bool TestUnitTimedLifePause() {
    Fixture f;
    const int buffId = ((int)'B' << 24) | ((int)'H' << 16) | ((int)'w' << 8) | (int)'e';
    CallNative(f.vm, "UnitApplyTimedLife", {JassValue::Int(f.handle), JassValue::Int(buffId), JassValue::Real(2.0f)});
    TEST_ASSERT(f.unit->IsSummoned());
    // Pause freezes the countdown: 5s pass, still alive.
    CallNative(f.vm, "UnitPauseTimedLife", {JassValue::Int(f.handle), JassValue::Bool(true)});
    for (int i = 0; i < 100; i++) f.sim.Update();
    TEST_ASSERT(!f.unit->IsDead());
    // Resume: 2s timed life expires.
    CallNative(f.vm, "UnitPauseTimedLife", {JassValue::Int(f.handle), JassValue::Bool(false)});
    for (int i = 0; i < 100; i++) f.sim.Update();
    TEST_ASSERT(f.unit->IsDead());
    return true;
}

bool TestUnitSleepFamily() {
    Fixture f;
    TEST_ASSERT(CallNative(f.vm, "UnitCanSleep", {JassValue::Int(f.handle)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "UnitCanSleepPerm", {JassValue::Int(f.handle)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "UnitIsSleeping", {JassValue::Int(f.handle)}).boolVal);

    // Sleep lands while capable.
    CAbilitySleep sleep(8503);
    sleep.DoEffect(f.sim, nullptr, f.unit);
    TEST_ASSERT(CallNative(f.vm, "UnitIsSleeping", {JassValue::Int(f.handle)}).boolVal);
    CallNative(f.vm, "UnitWakeUp", {JassValue::Int(f.handle)});
    TEST_ASSERT(!CallNative(f.vm, "UnitIsSleeping", {JassValue::Int(f.handle)}).boolVal);

    // Capability removed: sleep no longer lands.
    CallNative(f.vm, "UnitAddSleep", {JassValue::Int(f.handle), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "UnitCanSleep", {JassValue::Int(f.handle)}).boolVal);
    sleep.DoEffect(f.sim, nullptr, f.unit);
    TEST_ASSERT(!CallNative(f.vm, "UnitIsSleeping", {JassValue::Int(f.handle)}).boolVal);
    CallNative(f.vm, "UnitAddSleepPerm", {JassValue::Int(f.handle), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(f.vm, "UnitCanSleepPerm", {JassValue::Int(f.handle)}).boolVal);
    return true;
}

bool TestUnitAlarmIconProgress() {
    Fixture f;
    TEST_ASSERT(CallNative(f.vm, "UnitIgnoreAlarm", {JassValue::Int(f.handle), JassValue::Bool(true)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "UnitIgnoreAlarmToggled", {JassValue::Int(f.handle)}).boolVal);
    TEST_ASSERT(f.unit->IsAlarmIgnored());
    CallNative(f.vm, "UnitSetUsesAltIcon", {JassValue::Int(f.handle), JassValue::Bool(true)});
    TEST_ASSERT(f.unit->UsesAltIcon());
    CallNative(f.vm, "UnitSetUpgradeProgress", {JassValue::Int(f.handle), JassValue::Int(50)});
    TEST_ASSERT(f.unit->GetUpgradeProgress() == 50.0f);
    // Build time is 30 in the fixture type: 50% -> 15 seconds stored.
    CallNative(f.vm, "UnitSetConstructionProgress", {JassValue::Int(f.handle), JassValue::Int(50)});
    TEST_ASSERT(f.unit->GetConstructionProgress() == 15.0f);
    TEST_ASSERT(!CallNative(f.vm, "WaygateIsActive", {JassValue::Int(f.handle)}).boolVal);
    f.unit->SetWaygateActive(true);
    TEST_ASSERT(CallNative(f.vm, "WaygateIsActive", {JassValue::Int(f.handle)}).boolVal);
    CallNative(f.vm, "CachePlayerHeroData", {JassValue::Int(0)});
    return true;
}

bool TestUnitBuffExFamily() {
    Fixture f;
    f.unit->AddBuff(std::make_unique<CBuffSlow>(30.0f, 0.2f, -1), f.sim);
    f.unit->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.1f, 0.1f), f.sim);
    auto ex = [&](bool pos, bool neg, bool timed) {
        return std::vector<JassValue>{JassValue::Int(f.handle), JassValue::Bool(pos), JassValue::Bool(neg),
                                      JassValue::Bool(true), JassValue::Bool(true), JassValue::Bool(timed),
                                      JassValue::Bool(true), JassValue::Bool(true)};
    };
    TEST_ASSERT(CallNative(f.vm, "UnitCountBuffsEx", ex(true, true, false)).intVal == 2);
    TEST_ASSERT(CallNative(f.vm, "UnitCountBuffsEx", ex(true, false, false)).intVal == 1);
    TEST_ASSERT(CallNative(f.vm, "UnitCountBuffsEx", ex(false, true, false)).intVal == 1);
    TEST_ASSERT(CallNative(f.vm, "UnitHasBuffsEx", ex(false, true, false)).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "UnitHasBuffsEx", ex(false, false, false)).boolVal);
    // Timed-life filter: none present yet.
    TEST_ASSERT(CallNative(f.vm, "UnitCountBuffsEx", ex(true, true, true)).intVal == 0);
    const int buffId = ((int)'B' << 24) | ((int)'H' << 16) | ((int)'w' << 8) | (int)'e';
    CallNative(f.vm, "UnitApplyTimedLife", {JassValue::Int(f.handle), JassValue::Int(buffId), JassValue::Real(60.0f)});
    TEST_ASSERT(CallNative(f.vm, "UnitCountBuffsEx", ex(true, true, true)).intVal == 1);

    CallNative(f.vm, "UnitRemoveBuffs", {JassValue::Int(f.handle), JassValue::Bool(true), JassValue::Bool(false)});
    TEST_ASSERT(CallNative(f.vm, "UnitCountBuffsEx", ex(true, true, false)).intVal == 2); // slow + timed life
    // Full purge uses timedLife=false (timedLife=true would filter to it).
    CallNative(f.vm, "UnitRemoveBuffsEx",
        {JassValue::Int(f.handle), JassValue::Bool(true), JassValue::Bool(true), JassValue::Bool(true),
         JassValue::Bool(true), JassValue::Bool(false), JassValue::Bool(true), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "UnitCountBuffsEx", ex(true, true, false)).intVal == 0);
    return true;
}

bool TestUnitDamagePointDelayed() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    auto* src = sim.AdoptUnit(std::make_unique<CUnit>(8511, MakeStateType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto* in = sim.AdoptUnit(std::make_unique<CUnit>(8512, MakeStateType(), 0, War3ID("hfoo"), 50, 0, 0));
    auto* out = sim.AdoptUnit(std::make_unique<CUnit>(8513, MakeStateType(), 0, War3ID("hfoo"), 5000, 0, 0));
    sim.Update();
    (void)src;
    const float inLife = in->GetLife(), outLife = out->GetLife();
    const int srcHandle = data->AllocHandle(JassHandleInfo::UNIT, 8511);
    auto strike = [&](float delay) {
        return CallNative(vm, "UnitDamagePoint",
            {JassValue::Int(srcHandle), JassValue::Real(delay), JassValue::Real(200),
             JassValue::Real(0), JassValue::Real(0), JassValue::Real(100),
             JassValue::Bool(true), JassValue::Bool(false), JassValue::Int(5), JassValue::Int(4),
             JassValue::Int(0)});
    };
    // Immediate strike: in-radius damaged (CHAOS attack), out-of-radius safe.
    TEST_ASSERT(strike(0.0f).boolVal);
    TEST_ASSERT(in->GetLife() < inLife);
    TEST_ASSERT(out->GetLife() == outLife);
    // Delayed strike: regen may raise life, but no 100-damage drop yet.
    const float inLife2 = in->GetLife();
    TEST_ASSERT(strike(1.0f).boolVal);
    for (int i = 0; i < 10; i++) sim.Update();
    TEST_ASSERT(in->GetLife() >= inLife2);
    TEST_ASSERT(in->GetLife() > inLife2 - 100.0f);
    for (int i = 0; i < 15; i++) sim.Update();
    TEST_ASSERT(in->GetLife() < inLife2);
    return true;
}

bool TestUnitIndicatorVisionFog() {
    Fixture f;
    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    CallNative(f.vm, "UnitAddIndicator",
        {JassValue::Int(f.handle), JassValue::Int(255), JassValue::Int(0), JassValue::Int(0), JassValue::Int(255)});
    {
        auto it = f.data->unitIndicators.find(8501);
        TEST_ASSERT(it != f.data->unitIndicators.end());
        TEST_ASSERT(it->second.red == 255 && it->second.green == 0 && it->second.alpha == 255);
    }
    CallNative(f.vm, "UnitShareVision", {JassValue::Int(f.handle), JassValue::Int(p0), JassValue::Bool(true)});
    TEST_ASSERT(f.data->unitSharedVision[8501].count(0) == 1);
    CallNative(f.vm, "UnitShareVision", {JassValue::Int(f.handle), JassValue::Int(p0), JassValue::Bool(false)});
    TEST_ASSERT(f.data->unitSharedVision[8501].count(0) == 0);
    CallNative(f.vm, "SetUnitFog",
        {JassValue::Real(1), JassValue::Real(2), JassValue::Real(3), JassValue::Real(4), JassValue::Real(5)});
    TEST_ASSERT(f.data->globalUnitFog.a == 1.0f && f.data->globalUnitFog.e == 5.0f);
    return true;
}

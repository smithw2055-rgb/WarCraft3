// Misc native tests: hero revive, ability levels, selection, food use,
// point values, proper names, XP suspend, unit defaults, destructables,
// campaign flags, fog requests (ref misc/unit/destructable API).
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/destructabledata.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_rally.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"
#include "scripting/JassNatives.h"
#include "scripting/JassVM.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n";         \
            return false;                                                   \
        }                                                                   \
    } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakeMiscType(bool hero) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        hero ? "Paladin" : "Footman", hero ? "Paladin" : "Footman", "icon",
        hero ? War3ID("Hpal") : War3ID("hfoo"),
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

    Fixture() {
        sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
        natives = std::make_unique<JassNatives>(vm, &sim);
        natives->RegisterAll();
        data = natives->GetData();
    }
};

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

} // namespace

bool TestReviveHero() {
    Fixture f;
    auto* hero = f.sim.AdoptUnit(std::make_unique<CUnit>(8901, MakeMiscType(true), 0, War3ID("Hpal"), 0, 0, 0));
    f.sim.Update();
    TEST_ASSERT(hero->IsHero());
    hero->Damage(f.sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL, "", 999999.0f, 0);
    TEST_ASSERT(hero->IsDead());
    const int hh = f.data->AllocHandle(JassHandleInfo::UNIT, 8901);
    // Non-hero / live / missing units refuse.
    auto* foot = f.sim.AdoptUnit(std::make_unique<CUnit>(8902, MakeMiscType(false), 0, War3ID("hfoo"), 0, 0, 0));
    f.sim.Update();
    (void)foot;
    const int fh = f.data->AllocHandle(JassHandleInfo::UNIT, 8902);
    TEST_ASSERT(!CallNative(f.vm, "ReviveHero", {JassValue::Int(fh), JassValue::Real(100), JassValue::Real(100), JassValue::Bool(false)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "ReviveHero", {JassValue::Int(999999), JassValue::Real(0), JassValue::Real(0), JassValue::Bool(false)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "ReviveHero", {JassValue::Int(hh), JassValue::Real(300), JassValue::Real(400), JassValue::Bool(false)}).boolVal);
    TEST_ASSERT(!hero->IsDead());
    TEST_ASSERT(hero->GetX() == 300.0f && hero->GetY() == 400.0f);
    TEST_ASSERT(hero->GetLife() == hero->GetMaxLife());
    return true;
}

bool TestAbilityLevelShift() {
    Fixture f;
    auto* hero = f.sim.AdoptUnit(std::make_unique<CUnit>(8911, MakeMiscType(true), 0, War3ID("Hpal"), 0, 0, 0));
    f.sim.Update();
    hero->AddAbility(f.sim, std::make_unique<CAbilityHero>(8912));
    const int hh = f.data->AllocHandle(JassHandleInfo::UNIT, 8911);
    const int code = ((int)'A' << 24) | ((int)'H' << 16) | ((int)'e' << 8) | (int)'r';
    TEST_ASSERT(CallNative(f.vm, "IncUnitAbilityLevel", {JassValue::Int(hh), JassValue::Int(code)}).intVal == 2);
    TEST_ASSERT(CallNative(f.vm, "DecUnitAbilityLevel", {JassValue::Int(hh), JassValue::Int(code)}).intVal == 1);
    TEST_ASSERT(CallNative(f.vm, "DecUnitAbilityLevel", {JassValue::Int(hh), JassValue::Int(code)}).intVal == 1); // floor
    TEST_ASSERT(CallNative(f.vm, "IncUnitAbilityLevel", {JassValue::Int(hh), JassValue::Int(0x58585858)}).intVal == 0);
    // Proper name + XP suspend + defaults ride the same hero.
    auto* ability = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
    TEST_ASSERT(ability != nullptr);
    ability->SetProperName("Uther");
    TEST_ASSERT(CallNative(f.vm, "GetHeroProperName", {JassValue::Int(hh)}).strVal == "Uther");
    TEST_ASSERT(!CallNative(f.vm, "IsSuspendedXP", {JassValue::Int(hh)}).boolVal);
    TEST_ASSERT(CallNative(f.vm, "GetUnitDefaultFlyHeight", {JassValue::Int(hh)}).realVal ==
                hero->GetUnitType()->GetDefaultFlyingHeight());
    TEST_ASSERT(CallNative(f.vm, "GetUnitDefaultTurnSpeed", {JassValue::Int(hh)}).realVal ==
                hero->GetUnitType()->GetTurnRate());
    return true;
}

bool TestSelectUnitAndFood() {
    Fixture f;
    auto* u = f.sim.AdoptUnit(std::make_unique<CUnit>(8921, MakeMiscType(false), 0, War3ID("hfoo"), 0, 0, 0));
    f.sim.Update();
    (void)u;
    const int uh = f.data->AllocHandle(JassHandleInfo::UNIT, 8921);
    CallNative(f.vm, "SelectUnit", {JassValue::Int(uh), JassValue::Bool(true)});
    TEST_ASSERT(f.data->selectedUnits.count(8921) == 1);
    const int grp = CallNative(f.vm, "CreateGroup", {}).intVal;
    CallNative(f.vm, "GroupEnumUnitsSelected", {JassValue::Int(grp)});
    TEST_ASSERT(CallNative(f.vm, "CountUnitsInGroup", {JassValue::Int(grp)}).intVal == 1);
    CallNative(f.vm, "SelectUnit", {JassValue::Int(uh), JassValue::Bool(false)});
    CallNative(f.vm, "GroupEnumUnitsSelected", {JassValue::Int(grp)});
    TEST_ASSERT(CallNative(f.vm, "CountUnitsInGroup", {JassValue::Int(grp)}).intVal == 0);

    CUnit* unit = f.sim.GetUnit(8921);
    unit->SetFoodUsed(2);
    CallNative(f.vm, "SetUnitUseFood", {JassValue::Int(uh), JassValue::Bool(false)});
    TEST_ASSERT(unit->GetFoodUsed() == 0);
    TEST_ASSERT(!unit->UsesFood());
    CallNative(f.vm, "SetUnitUseFood", {JassValue::Int(uh), JassValue::Bool(true)});
    TEST_ASSERT(unit->GetFoodUsed() == 2);
    return true;
}

bool TestDestructableNatives() {
    Fixture f;
    CDestructable* dest = f.sim.CreateDestructable(War3ID("LTlt"), 100, 200, 500);
    TEST_ASSERT(dest != nullptr);
    f.sim.Update();
    const int dh = f.data->AllocHandle(JassHandleInfo::DESTRUCTABLE, dest->GetHandleId());
    TEST_ASSERT(!CallNative(f.vm, "IsDestructableInvulnerable", {JassValue::Int(dh)}).boolVal);
    CallNative(f.vm, "SetDestructableInvulnerable", {JassValue::Int(dh), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "IsDestructableInvulnerable", {JassValue::Int(dh)}).boolVal);
    CallNative(f.vm, "ShowDestructable", {JassValue::Int(dh), JassValue::Bool(false)});
    TEST_ASSERT(dest->IsHidden());
    CallNative(f.vm, "ShowDestructable", {JassValue::Int(dh), JassValue::Bool(true)});
    TEST_ASSERT(!dest->IsHidden());
    CallNative(f.vm, "SetDestructableOccluderHeight", {JassValue::Int(dh), JassValue::Real(150)});
    TEST_ASSERT(dest->GetOccluderHeight() == 150.0f);
    TEST_ASSERT(CallNative(f.vm, "GetDestructableOccluderHeight", {JassValue::Int(dh)}).realVal == 150.0f);
    CallNative(f.vm, "SetDestructableAnimationSpeed", {JassValue::Int(dh), JassValue::Real(2)});
    TEST_ASSERT(dest->GetAnimationSpeed() == 2.0f);
    // Name needs destructable data; empty without it.
    TEST_ASSERT(CallNative(f.vm, "GetDestructableName", {JassValue::Int(dh)}).strVal.empty());
    if (ResourcesPresent()) {
        MpqManager mpq;
        const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
        for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
        CDestructableData data = CDestructableData::LoadFromMpq(mpq);
        f.sim.SetDestructableData(&data);
        const std::string name = CallNative(f.vm, "GetDestructableName", {JassValue::Int(dh)}).strVal;
        TEST_ASSERT(!name.empty());
    }
    return true;
}

bool TestCampaignAndFlowNatives() {
    Fixture f;
    CallNative(f.vm, "SetCampaignAvailable", {JassValue::Int(1), JassValue::Bool(true)});
    TEST_ASSERT(f.data->campaignAvailable[1]);
    CallNative(f.vm, "SetMissionAvailable", {JassValue::Int(1), JassValue::Int(2), JassValue::Bool(true)});
    const auto missionKey = std::make_pair(1, 2);
    TEST_ASSERT(f.data->missionAvailable[missionKey]);
    CallNative(f.vm, "SetEdCinematicAvailable", {JassValue::Int(1), JassValue::Bool(true)});
    CallNative(f.vm, "SetOpCinematicAvailable", {JassValue::Int(1), JassValue::Bool(false)});
    TEST_ASSERT(f.data->edCinematics[1] && !f.data->opCinematics[1]);
    CallNative(f.vm, "SetTutorialCleared", {JassValue::Bool(true)});
    TEST_ASSERT(f.data->tutorialCleared);
    CallNative(f.vm, "SetCustomCampaignButtonVisible", {JassValue::Int(3), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(f.vm, "GetCustomCampaignButtonVisible", {JassValue::Int(3)}).boolVal);
    TEST_ASSERT(!CallNative(f.vm, "GetCustomCampaignButtonVisible", {JassValue::Int(4)}).boolVal);
    CallNative(f.vm, "ForceCampaignSelectScreen", {});
    TEST_ASSERT(f.data->forceCampaignSelectScreen);
    CallNative(f.vm, "SetDefaultDifficulty", {JassValue::Int(2)});
    TEST_ASSERT(f.data->gameConfig.defaultDifficulty == 2);

    const int loc = CallNative(f.vm, "Location", {JassValue::Real(100), JassValue::Real(200)}).intVal;
    const int p0 = CallNative(f.vm, "Player", {JassValue::Int(0)}).intVal;
    CallNative(f.vm, "SetFogStateRadiusLoc",
        {JassValue::Int(p0), JassValue::Int(2), JassValue::Int(loc), JassValue::Real(512), JassValue::Bool(true)});
    TEST_ASSERT(f.data->fogRadiusRequests.size() == 1);
    TEST_ASSERT(f.data->fogRadiusRequests[0].radius == 512.0f);

    CallNative(f.vm, "ChangeLevel", {JassValue::Str("Maps\\FrozenThrone\\TurtleRock.w3x"), JassValue::Bool(false)});
    TEST_ASSERT(f.data->pendingChangeLevel == "Maps\\FrozenThrone\\TurtleRock.w3x");
    CallNative(f.vm, "DoNotSaveReplay", {});
    TEST_ASSERT(f.data->noReplaySave);
    CallNative(f.vm, "SyncSelections", {});
    return true;
}

bool TestUnitPointValue() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);

    JassVM vm;
    CSimulation sim;
    sim.SetUnitData(&unitData);
    JassNatives natives(vm, &sim);
    natives.RegisterAll();

    const int hfoo = ((int)'h' << 24) | ((int)'f' << 16) | ((int)'o' << 8) | (int)'o';
    const CUnitType* type = unitData.GetUnitType(War3ID("hfoo"));
    TEST_ASSERT(type != nullptr);
    TEST_ASSERT(CallNative(vm, "GetUnitPointValueByType", {JassValue::Int(hfoo)}).intVal == type->GetPointValue());
    return true;
}

bool TestAbilityIdConversion() {
    Fixture f;
    const int code = ((int)'A' << 24) | ((int)'H' << 16) | ((int)'b' << 8) | (int)'z';
    TEST_ASSERT(CallNative(f.vm, "AbilityId", {JassValue::Str("AHbz")}).intVal == code);
    TEST_ASSERT(CallNative(f.vm, "AbilityId2String", {JassValue::Int(code)}).strVal == "AHbz");
    TEST_ASSERT(CallNative(f.vm, "AbilityId", {}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "AbilityId2String", {}).strVal.empty());
    return true;
}

bool TestRallyTargetGetters() {
    Fixture f;
    auto* hall = f.sim.AdoptUnit(std::make_unique<CUnit>(9401, MakeMiscType(false), 0, War3ID("hfoo"), 0, 0, 0));
    auto* target = f.sim.AdoptUnit(std::make_unique<CUnit>(9402, MakeMiscType(false), 0, War3ID("hfoo"), 500, 0, 0));
    TEST_ASSERT(hall && target);
    hall->AddAbility(f.sim, std::make_unique<CAbilityRally>(9403));
    auto* rally = dynamic_cast<CAbilityRally*>(hall->GetAbility(War3ID("ARal")));
    TEST_ASSERT(rally);
    f.sim.Update();
    const int hallHandle = f.data->AllocHandle(JassHandleInfo::UNIT, 9401);

    // No rally set: both queries report null.
    TEST_ASSERT(CallNative(f.vm, "GetUnitRallyUnit", {JassValue::Int(hallHandle)}).intVal == 0);
    TEST_ASSERT(CallNative(f.vm, "GetUnitRallyDestructable", {JassValue::Int(hallHandle)}).intVal == 0);

    // Rally a unit: unit getter resolves, destructable getter stays null.
    rally->Begin(f.sim, hall, 0, target);
    const int rallyUnit = CallNative(f.vm, "GetUnitRallyUnit", {JassValue::Int(hallHandle)}).intVal;
    TEST_ASSERT(rallyUnit != 0);
    TEST_ASSERT(f.data->GetSimHandle(rallyUnit, JassHandleInfo::UNIT) == 9402);
    TEST_ASSERT(CallNative(f.vm, "GetUnitRallyDestructable", {JassValue::Int(hallHandle)}).intVal == 0);

    // Rally a destructable: roles flip.
    CDestructable* gate = f.sim.CreateDestructable(War3ID("ATtr"), 300, 0, 100.0f);
    TEST_ASSERT(gate);
    rally->Begin(f.sim, hall, 0, gate);
    TEST_ASSERT(CallNative(f.vm, "GetUnitRallyUnit", {JassValue::Int(hallHandle)}).intVal == 0);
    const int rallyDest = CallNative(f.vm, "GetUnitRallyDestructable", {JassValue::Int(hallHandle)}).intVal;
    TEST_ASSERT(rallyDest != 0);
    TEST_ASSERT(f.data->GetSimHandle(rallyDest, JassHandleInfo::DESTRUCTABLE) == gate->GetHandleId());

    // Bad handle is a safe null.
    TEST_ASSERT(CallNative(f.vm, "GetUnitRallyUnit", {JassValue::Int(999999)}).intVal == 0);
    return true;
}

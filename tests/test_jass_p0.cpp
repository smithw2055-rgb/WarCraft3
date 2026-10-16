// P0 JASS natives regression tests (ref Jass2.java): rawcode helpers,
// bit ops, unit range/alive, heal/resurrect/stop, group index/enum,
// dialog/region event getters, cooldowns, rally/movement stubs,
// typed unit counts, PolledWait.
#include <iostream>
#include <memory>
#include <vector>

#include "ecs/abilities/ability_queue.h"
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

JassValue CallNative(JassVM& vm, const std::string& name, const std::vector<JassValue>& args) {
    const auto& natives = vm.GetRegisteredNatives();
    auto it = natives.find(name);
    if (it == natives.end()) {
        std::cerr << "missing native " << name << "\n";
        return JassValue::Null();
    }
    return it->second(args);
}

CUnitType* MakeP0Type(const char* legacy, War3ID id) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        legacy, legacy, "icon", id,
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

} // namespace

bool TestJassP0Natives() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // Rawcode helpers.
    TEST_ASSERT(CallNative(vm, "FourCC", {JassValue::Str("hfoo")}).intVal == (int)War3ID("hfoo").value);
    TEST_ASSERT(CallNative(vm, "String2Rawcode", {JassValue::Str("Hpal")}).intVal == (int)War3ID("Hpal").value);
    TEST_ASSERT(CallNative(vm, "Rawcode2String", {JassValue::Int((int)War3ID("Hpal").value)}).strVal == "Hpal");
    TEST_ASSERT(CallNative(vm, "String2DamageType", {JassValue::Str("FIRE")}).intVal == 8);
    TEST_ASSERT(CallNative(vm, "String2DamageType", {JassValue::Str("bogus")}).intVal == 0);

    // Bit ops.
    TEST_ASSERT(CallNative(vm, "BlzBitAnd", {JassValue::Int(6), JassValue::Int(3)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "BlzBitOr", {JassValue::Int(6), JassValue::Int(3)}).intVal == 7);
    TEST_ASSERT(CallNative(vm, "BlzBitXor", {JassValue::Int(6), JassValue::Int(3)}).intVal == 5);

    // Units.
    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(7001, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(7002, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 100, 0, 0));
    TEST_ASSERT(a && b);
    sim.Update(); // flush AdoptUnit queue into GetUnits()
    const int ha = data->AllocHandle(JassHandleInfo::UNIT, a->GetHandleId());
    const int hb = data->AllocHandle(JassHandleInfo::UNIT, b->GetHandleId());
    TEST_ASSERT(CallNative(vm, "IsUnitInRange", {JassValue::Int(ha), JassValue::Int(hb), JassValue::Real(150)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsUnitInRange", {JassValue::Int(ha), JassValue::Int(hb), JassValue::Real(50)}).boolVal);
    TEST_ASSERT(CallNative(vm, "IsUnitInRangeXY", {JassValue::Int(ha), JassValue::Real(5), JassValue::Real(0), JassValue::Real(10)}).boolVal);
    TEST_ASSERT(CallNative(vm, "UnitAlive", {JassValue::Int(ha)}).boolVal);
    TEST_ASSERT(CallNative(vm, "WidgetAlive", {JassValue::Int(ha)}).boolVal);
    TEST_ASSERT(CallNative(vm, "IsWidgetAlive", {JassValue::Int(ha)}).boolVal);

    // Heal clamps at max life.
    a->SetLife(sim, 10.0f);
    CallNative(vm, "HealUnit", {JassValue::Int(ha), JassValue::Real(25)});
    TEST_ASSERT(a->GetLife() == 35.0f);

    // Defense / costs from type.
    TEST_ASSERT(CallNative(vm, "GetUnitDefense", {JassValue::Int(ha)}).realVal == (float)a->GetDefense());
    TEST_ASSERT(CallNative(vm, "BlzGetUnitArmor", {JassValue::Int(ha)}).realVal == (float)a->GetDefense());

    // Stop order is a no-op success path.
    CallNative(vm, "DoStopOrder", {JassValue::Int(ha)});
    CallNative(vm, "SendUnitBackToWork", {JassValue::Int(ha)});
    CallNative(vm, "UnitInstantReturnResources", {JassValue::Int(ha)});
    TEST_ASSERT(CallNative(vm, "UnitCanReach", {JassValue::Int(ha), JassValue::Int(hb), JassValue::Real(200)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "UnitCanReach", {JassValue::Int(ha), JassValue::Int(hb), JassValue::Real(10)}).boolVal);

    // Resurrect: kill then restore.
    a->SetLife(sim, 0.0f);
    TEST_ASSERT(a->IsDead());
    TEST_ASSERT(!CallNative(vm, "UnitAlive", {JassValue::Int(ha)}).boolVal);
    CallNative(vm, "ResurrectUnit", {JassValue::Int(ha)});
    TEST_ASSERT(!a->IsDead());

    // Groups.
    const int g = CallNative(vm, "CreateGroup", {}).intVal;
    CallNative(vm, "GroupAddUnit", {JassValue::Int(g), JassValue::Int(ha)});
    CallNative(vm, "GroupAddUnit", {JassValue::Int(g), JassValue::Int(hb)});
    TEST_ASSERT(CallNative(vm, "GroupGetSize", {JassValue::Int(g)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "BlzGroupGetSize", {JassValue::Int(g)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "GroupUnitAt", {JassValue::Int(g), JassValue::Int(0)}).intVal == ha);
    TEST_ASSERT(CallNative(vm, "BlzGroupUnitAt", {JassValue::Int(g), JassValue::Int(1)}).intVal == hb);
    TEST_ASSERT(CallNative(vm, "GroupUnitAt", {JassValue::Int(g), JassValue::Int(9)}).intVal == 0);
    const int g2 = CallNative(vm, "CreateGroup", {}).intVal;
    CallNative(vm, "GroupEnumUnitsInRangeOfUnit", {JassValue::Int(g2), JassValue::Int(ha), JassValue::Real(150)});
    TEST_ASSERT(CallNative(vm, "GroupGetSize", {JassValue::Int(g2)}).intVal == 2);
    const int g3 = CallNative(vm, "CreateGroup", {}).intVal;
    CallNative(vm, "GroupEnumUnitsInRangeOfUnitCounted",
               {JassValue::Int(g3), JassValue::Int(ha), JassValue::Real(500), JassValue::Str(""), JassValue::Int(1)});
    TEST_ASSERT(CallNative(vm, "GroupGetSize", {JassValue::Int(g3)}).intVal == 1);

    // Events default to null.
    TEST_ASSERT(CallNative(vm, "GetClickedButton", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetClickedDialog", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetTriggeringRegion", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetEventGameState", {}).intVal == 0);

    // Cooldowns start, report, decay, and clear.
    const int abCode = (int)War3ID("AHbz").value;
    CallNative(vm, "StartUnitAbilityCooldown", {JassValue::Int(ha), JassValue::Int(abCode), JassValue::Real(10)});
    TEST_ASSERT(CallNative(vm, "BlzGetUnitAbilityCooldownRemaining",
                           {JassValue::Int(ha), JassValue::Int(abCode)}).realVal == 10.0f);
    natives.UpdateTimers(4.0f);
    TEST_ASSERT(CallNative(vm, "GetUnitAbilityCooldownRemaining",
                           {JassValue::Int(ha), JassValue::Int(abCode)}).realVal == 6.0f);
    CallNative(vm, "BlzEndUnitAbilityCooldown", {JassValue::Int(ha), JassValue::Int(abCode)});
    TEST_ASSERT(CallNative(vm, "BlzGetUnitAbilityCooldownRemaining",
                           {JassValue::Int(ha), JassValue::Int(abCode)}).realVal == 0.0f);

    // Rally / movement stubs.
    TEST_ASSERT(CallNative(vm, "GetUnitRallyUnit", {JassValue::Int(ha)}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetUnitRallyDestructable", {JassValue::Int(ha)}).intVal == 0);
    CallNative(vm, "SetUnitMovementTypeNoCollision", {JassValue::Int(ha), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "IsUnitMovementDisabled", {JassValue::Int(ha)}).boolVal);
    CallNative(vm, "SetUnitMovementTypeNoCollision", {JassValue::Int(ha), JassValue::Bool(false)});
    TEST_ASSERT(!CallNative(vm, "IsUnitMovementDisabled", {JassValue::Int(ha)}).boolVal);

    // Typed counts by legacy name (both hfoo units belong to player 0).
    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;
    TEST_ASSERT(CallNative(vm, "GetPlayerTypedUnitCount",
                           {JassValue::Int(p0), JassValue::Str("hfoo"),
                            JassValue::Bool(true), JassValue::Bool(false)}).intVal == 2);
    return true;
}

bool TestJassP1Natives() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(7011, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(7012, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 100, 0, 0));
    TEST_ASSERT(a && b);
    sim.Update();
    const int ha = data->AllocHandle(JassHandleInfo::UNIT, a->GetHandleId());
    const int hb = data->AllocHandle(JassHandleInfo::UNIT, b->GetHandleId());

    // Ability-handle introspection on an orphan handle.
    const int abCode = (int)War3ID("AHbz").value;
    const int orph = CallNative(vm, "CreateAbility", {JassValue::Int(abCode)}).intVal;
    TEST_ASSERT(orph != 0);
    TEST_ASSERT(CallNative(vm, "GetAbilityLevel", {JassValue::Int(orph)}).intVal == 1);
    TEST_ASSERT(CallNative(vm, "GetAbilityAliasId", {JassValue::Int(orph)}).intVal == abCode);
    TEST_ASSERT(CallNative(vm, "GetAbilityCodeId", {JassValue::Int(orph)}).intVal == abCode);
    TEST_ASSERT(CallNative(vm, "AbilityId", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "AbilityId2String", {}).strVal.empty());
    TEST_ASSERT(!CallNative(vm, "IsAbilityPermanent", {JassValue::Int(orph)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsAbilityPhysical", {JassValue::Int(orph)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsAbilityUniversal", {JassValue::Int(orph)}).boolVal);
    CallNative(vm, "SetAbilityIconShowing", {JassValue::Int(orph)});
    CallNative(vm, "SetAbilityPermanent", {JassValue::Int(orph), JassValue::Bool(true)});
    CallNative(vm, "SetAbilityCastRange", {JassValue::Int(orph), JassValue::Real(500)});
    CallNative(vm, "SetAbilityDisabled", {JassValue::Int(ha), JassValue::Int(orph), JassValue::Bool(true)});

    // Unit-bound ability when the factory knows the rawcode.
    if (CallNative(vm, "UnitAddAbility", {JassValue::Int(ha), JassValue::Int(abCode)}).boolVal) {
        const int hab = CallNative(vm, "GetUnitAbility", {JassValue::Int(ha), JassValue::Int(abCode)}).intVal;
        TEST_ASSERT(hab != 0);
        TEST_ASSERT(CallNative(vm, "GetAbilityLevel", {JassValue::Int(hab)}).intVal >= 1);
        CallNative(vm, "SetAbilityPermanent", {JassValue::Int(hab), JassValue::Bool(true)});
        TEST_ASSERT(CallNative(vm, "IsAbilityPermanent", {JassValue::Int(hab)}).boolVal);
        CallNative(vm, "SetAbilityPermanent", {JassValue::Int(hab), JassValue::Bool(false)});
        TEST_ASSERT(!CallNative(vm, "IsAbilityPermanent", {JassValue::Int(hab)}).boolVal);
        CallNative(vm, "SetAbilityPhysical", {JassValue::Int(hab), JassValue::Bool(true)});
        TEST_ASSERT(CallNative(vm, "IsAbilityPhysical", {JassValue::Int(hab)}).boolVal);
        CallNative(vm, "SetAbilityUniversal", {JassValue::Int(hab), JassValue::Bool(true)});
        TEST_ASSERT(CallNative(vm, "IsAbilityUniversal", {JassValue::Int(hab)}).boolVal);
        CallNative(vm, "SetAbilityDisabled", {JassValue::Int(ha), JassValue::Int(hab), JassValue::Bool(true)});
        CallNative(vm, "SetAbilityDisabled", {JassValue::Int(ha), JassValue::Int(hab), JassValue::Bool(false)});
        CallNative(vm, "SetAbilityCastRange", {JassValue::Int(hab), JassValue::Real(900)});
    }

    // User data round-trips on the orphan handle.
    TEST_ASSERT(!CallNative(vm, "HasAbilityUserData", {JassValue::Int(orph), JassValue::Str("k")}).boolVal);
    TEST_ASSERT(!CallNative(vm, "SetAbilityUserDataInteger",
                            {JassValue::Int(orph), JassValue::Str("k"), JassValue::Int(7)}).boolVal);
    TEST_ASSERT(CallNative(vm, "SetAbilityUserDataInteger",
                           {JassValue::Int(orph), JassValue::Str("k"), JassValue::Int(8)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetAbilityUserDataInteger", {JassValue::Int(orph), JassValue::Str("k")}).intVal == 8);
    TEST_ASSERT(CallNative(vm, "GetAbilityUserData", {JassValue::Int(orph), JassValue::Str("k")}).intVal == 8);
    CallNative(vm, "SetAbilityUserDataBoolean", {JassValue::Int(orph), JassValue::Str("b"), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "GetAbilityUserDataBoolean", {JassValue::Int(orph), JassValue::Str("b")}).boolVal);
    CallNative(vm, "SetAbilityUserDataString",
               {JassValue::Int(orph), JassValue::Str("s"), JassValue::Str("hi")});
    TEST_ASSERT(CallNative(vm, "GetAbilityUserDataString", {JassValue::Int(orph), JassValue::Str("s")}).strVal == "hi");
    TEST_ASSERT(CallNative(vm, "HasAbilityUserData", {JassValue::Int(orph), JassValue::Str("k")}).boolVal);
    TEST_ASSERT(CallNative(vm, "FlushChildAbilityUserData", {JassValue::Int(orph), JassValue::Str("k")}).boolVal);
    TEST_ASSERT(!CallNative(vm, "HasAbilityUserData", {JassValue::Int(orph), JassValue::Str("k")}).boolVal);
    CallNative(vm, "FlushParentAbilityUserData", {JassValue::Int(orph)});
    TEST_ASSERT(!CallNative(vm, "HasAbilityUserData", {JassValue::Int(orph), JassValue::Str("s")}).boolVal);

    // Defense bonus folds into total defense.
    const int baseTotal = a->GetTotalDefense();
    CallNative(vm, "UnitAddDefenseBonus", {JassValue::Int(ha), JassValue::Real(5)});
    TEST_ASSERT(CallNative(vm, "UnitGetTemporaryDefenseBonus", {JassValue::Int(ha)}).realVal == 5.0f);
    TEST_ASSERT(a->GetTotalDefense() == baseTotal + 5);
    CallNative(vm, "UnitSetTemporaryDefenseBonus", {JassValue::Int(ha), JassValue::Real(2)});
    TEST_ASSERT(a->GetTotalDefense() == baseTotal + 2);

    // Cast timing mirrors the unit type.
    TEST_ASSERT(CallNative(vm, "GetUnitCastPoint", {JassValue::Int(ha)}).realVal ==
                a->GetUnitType()->GetCastPoint());
    TEST_ASSERT(CallNative(vm, "GetUnitCastBackswingPoint", {JassValue::Int(ha)}).realVal ==
                a->GetUnitType()->GetCastBackswingPoint());

    // Explode-on-death buff routing stores the id.
    CallNative(vm, "SetUnitExplodeOnDeathBuffId", {JassValue::Int(ha), JassValue::Int(abCode)});
    TEST_ASSERT(a->GetExplodeOnDeathBuffId().value == (uint32_t)abCode);
    CallNative(vm, "UnsetUnitExplodeOnDeathBuffId", {JassValue::Int(ha)});
    TEST_ASSERT(a->GetExplodeOnDeathBuffId().value == 0u);

    // Identity + order polling + aura recompute (no orders queued).
    TEST_ASSERT(CallNative(vm, "IsUnit", {JassValue::Int(ha), JassValue::Int(ha)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsUnit", {JassValue::Int(ha), JassValue::Int(hb)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "UnitPollNextOrderBehavior", {JassValue::Int(ha)}).boolVal);
    CallNative(vm, "RecomputeStatBonusesOnUnit", {JassValue::Int(ha), JassValue::Int(0)});

    // JASS projectile: homing hit deals stored damage.
    const float lifeBefore = b->GetLife();
    TEST_ASSERT(lifeBefore > 0.0f);
    const int proj = CallNative(vm, "CreateJassProjectile",
                                {JassValue::Int(ha), JassValue::Int((int)War3ID("Amsl").value),
                                 JassValue::Real(0), JassValue::Real(0), JassValue::Real(0),
                                 JassValue::Real(800), JassValue::Bool(true), JassValue::Int(hb)}).intVal;
    TEST_ASSERT(proj != 0);
    CallNative(vm, "SetProjectileDamage", {JassValue::Int(proj), JassValue::Real(100)});
    TEST_ASSERT(!CallNative(vm, "IsProjectileReflected", {JassValue::Int(proj)}).boolVal);
    CallNative(vm, "SetProjectileReflected", {JassValue::Int(proj), JassValue::Bool(true)});
    TEST_ASSERT(CallNative(vm, "IsProjectileReflected", {JassValue::Int(proj)}).boolVal);
    CallNative(vm, "SetProjectileReflected", {JassValue::Int(proj), JassValue::Bool(false)});
    for (int i = 0; i < 10 && b->GetLife() == lifeBefore; i++) natives.UpdateTimers(0.05f);
    TEST_ASSERT(b->GetLife() < lifeBefore);
    TEST_ASSERT(CallNative(vm, "GetProjectileX", {JassValue::Int(proj)}).realVal == 0.0f); // reaped

    // Point-targeted projectile fizzles without damage.
    const float lifeAfterHit = b->GetLife();
    const int p2 = CallNative(vm, "CreateUnitTargetedProjectile",
                              {JassValue::Int(ha), JassValue::Int(0), JassValue::Int(0),
                               JassValue::Int(ha), JassValue::Int(hb), JassValue::Int(hb),
                               JassValue::Int((int)War3ID("Amsl").value),
                               JassValue::Real(800), JassValue::Bool(false),
                               JassValue::Str(""), JassValue::Str("")}).intVal;
    TEST_ASSERT(p2 != 0);
    const int srcBack = CallNative(vm, "GetProjectileSource", {JassValue::Int(p2)}).intVal;
    TEST_ASSERT(srcBack != 0);
    CallNative(vm, "SetProjectileDone", {JassValue::Int(p2), JassValue::Bool(true)});
    for (int i = 0; i < 10; i++) natives.UpdateTimers(0.05f);
    TEST_ASSERT(b->GetLife() == lifeAfterHit);

    // Clock + setup constants.
    TEST_ASSERT(CallNative(vm, "GetSimulationStepTime", {}).realVal == CSimulation::SIM_STEP_TIME);
    TEST_ASSERT(CallNative(vm, "GetGameTurnTick", {}).intVal >= 0);
    TEST_ASSERT(CallNative(vm, "GetGamePlacement", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetResourceDensity", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetCreatureDensity", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetPlayerNeutralAggressive", {}).intVal == 24);
    TEST_ASSERT(CallNative(vm, "GetBJPlayerNeutralVictim", {}).intVal == 25);
    TEST_ASSERT(CallNative(vm, "GetBJPlayerNeutralExtra", {}).intVal == 26);
    TEST_ASSERT(CallNative(vm, "GetPlayerNeutralPassive", {}).intVal == 27);
    TEST_ASSERT(CallNative(vm, "GetBJMaxPlayers", {}).intVal == 24);
    TEST_ASSERT(CallNative(vm, "GetBJMaxPlayerSlots", {}).intVal == 28);
    TEST_ASSERT(CallNative(vm, "GetObjectName", {JassValue::Int(abCode)}).strVal.empty()); // no data tables
    CallNative(vm, "LogError", {JassValue::Str("p1-test-log")});
    CallNative(vm, "StartThread", {JassValue::Str("")});
    CallNative(vm, "StartAbilityBuilderThread", {JassValue::Str("")});
    CallNative(vm, "Sleep", {JassValue::Real(1), JassValue::Real(2)});

    // Spell scope defaults.
    TEST_ASSERT(CallNative(vm, "GetSpellAbilityOrderId", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetSpellAbilityOrderButton", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetSpellTargetType", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetTriggerCastId", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "GetAbilityTargetX", {JassValue::Int(hb)}).realVal == 100.0f);
    TEST_ASSERT(CallNative(vm, "GetAbilityTargetY", {JassValue::Int(hb)}).realVal == 0.0f);

    // Default-cooldown + display paths without abilities stay silent.
    CallNative(vm, "StartAbilityDefaultCooldown", {JassValue::Int(ha), JassValue::Int(orph)});
    CallNative(vm, "StartUnitAbilityDefaultCooldown", {JassValue::Int(ha), JassValue::Int(abCode)});
    CallNative(vm, "EndAbilityCooldown", {JassValue::Int(ha), JassValue::Int(orph)});
    TEST_ASSERT(CallNative(vm, "GetUnitAbilityCooldownLengthDisplay",
                           {JassValue::Int(ha), JassValue::Int(abCode)}).realVal == 0.0f);
    CallNative(vm, "SetPreferredSelectionReplacement", {JassValue::Int(ha), JassValue::Int(hb)});

    // Group union / subtraction (+Blz aliases).
    const int g = CallNative(vm, "CreateGroup", {}).intVal;
    const int g2 = CallNative(vm, "CreateGroup", {}).intVal;
    CallNative(vm, "GroupAddUnit", {JassValue::Int(g2), JassValue::Int(ha)});
    CallNative(vm, "GroupAddGroupFast", {JassValue::Int(g), JassValue::Int(g2)});
    TEST_ASSERT(CallNative(vm, "GroupGetSize", {JassValue::Int(g)}).intVal == 1);
    CallNative(vm, "BlzGroupAddGroupFast", {JassValue::Int(g), JassValue::Int(g2)});
    TEST_ASSERT(CallNative(vm, "GroupGetSize", {JassValue::Int(g)}).intVal == 1); // dedup
    CallNative(vm, "GroupRemoveGroupFast", {JassValue::Int(g), JassValue::Int(g2)});
    TEST_ASSERT(CallNative(vm, "GroupGetSize", {JassValue::Int(g)}).intVal == 0);
    CallNative(vm, "GroupAddGroupFast", {JassValue::Int(g), JassValue::Int(g2)});
    CallNative(vm, "BlzGroupRemoveGroupFast", {JassValue::Int(g), JassValue::Int(g2)});
    TEST_ASSERT(CallNative(vm, "GroupGetSize", {JassValue::Int(g)}).intVal == 0);

    // Facing + conversions + lookups + config text tags.
    CallNative(vm, "BlzSetUnitFacingEx", {JassValue::Int(ha), JassValue::Real(1.5)});
    TEST_ASSERT(a->GetFacing() == 1.5f);
    TEST_ASSERT(CallNative(vm, "ConvertTargetType", {JassValue::Int(6)}).intVal == 6);
    TEST_ASSERT(CallNative(vm, "ConvertResourceType", {JassValue::Int(2)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "ParseTargetType", {JassValue::Str("ground")}).intVal == 6);
    TEST_ASSERT(CallNative(vm, "ParseTargetType", {JassValue::Str("bogus")}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "String2NonStackingStatBonusType", {JassValue::Str("DEF")}).intVal == 5);
    TEST_ASSERT(CallNative(vm, "String2StateModType", {JassValue::Str("STUN")}).intVal == 3);
    const int tag = CallNative(vm, "CreateTextTagFromConfig",
                               {JassValue::Int(hb), JassValue::Int(0), JassValue::Str("+100")}).intVal;
    TEST_ASSERT(tag != 0);
    CallNative(vm, "SetTextTagCentered", {JassValue::Int(tag), JassValue::Bool(true)});
    const int tag2 = CallNative(vm, "CreateIntTextTagFromConfig",
                                {JassValue::Int(hb), JassValue::Int(0), JassValue::Int(42)}).intVal;
    TEST_ASSERT(tag2 != 0);
    CallNative(vm, "BlzCreateDestructableWithSkin",
               {JassValue::Int((int)War3ID("ATtr").value), JassValue::Real(0), JassValue::Real(0),
                JassValue::Real(0), JassValue::Real(1), JassValue::Int(0)});
    return true;
}

bool TestJassTargeting() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(7021, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(7022, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 100, 0, 0));
    auto* c = sim.AdoptUnit(std::make_unique<CUnit>(7023, MakeP0Type("hfoo", War3ID("hfoo")), 1, War3ID("hfoo"), 200, 0, 0));
    TEST_ASSERT(a && b && c);
    sim.Update();
    const int ha = data->AllocHandle(JassHandleInfo::UNIT, a->GetHandleId());
    const int hb = data->AllocHandle(JassHandleInfo::UNIT, b->GetHandleId());
    const int hc = data->AllocHandle(JassHandleInfo::UNIT, c->GetHandleId());

    // Set containers.
    const int set = CallNative(vm, "CreateTargetTypes", {}).intVal;
    TEST_ASSERT(set != 0);
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(set), JassValue::Int(6)}); // GROUND
    TEST_ASSERT(CallNative(vm, "TargetTypesContains", {JassValue::Int(set), JassValue::Int(6)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "TargetTypesContains", {JassValue::Int(set), JassValue::Int(5)}).boolVal);
    CallNative(vm, "TargetTypesRemove", {JassValue::Int(set), JassValue::Int(6)});
    TEST_ASSERT(!CallNative(vm, "TargetTypesContains", {JassValue::Int(set), JassValue::Int(6)}).boolVal);
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(set), JassValue::Int(6)});

    // Empty set is valid (ref); ground pair with GROUND allowed.
    const int empty = CallNative(vm, "CreateTargetTypes", {}).intVal;
    TEST_ASSERT(CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(empty)}).boolVal);
    TEST_ASSERT(CallNative(vm, "IsUnitValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(set)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetTargetError", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(set)}).strVal.empty());

    // Self gating.
    const int notSelf = CallNative(vm, "CreateTargetTypes", {}).intVal;
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(notSelf), JassValue::Int(15)}); // NOTSELF
    TEST_ASSERT(!CallNative(vm, "IsValidTarget", {JassValue::Int(ha), JassValue::Int(ha), JassValue::Int(notSelf)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetTargetError", {JassValue::Int(ha), JassValue::Int(ha), JassValue::Int(notSelf)}).strVal == "Notself");
    TEST_ASSERT(CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(notSelf)}).boolVal);

    // Ownership gating across players.
    const int owned = CallNative(vm, "CreateTargetTypes", {}).intVal;
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(owned), JassValue::Int(17)}); // PLAYERUNITS
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(owned), JassValue::Int(6)});
    TEST_ASSERT(CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(owned)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "IsValidTarget", {JassValue::Int(hc), JassValue::Int(ha), JassValue::Int(owned)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetTargetError", {JassValue::Int(hc), JassValue::Int(ha), JassValue::Int(owned)}).strVal == "Targetowned");

    // Hero gating on non-heroes.
    const int heroOnly = CallNative(vm, "CreateTargetTypes", {}).intVal;
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(heroOnly), JassValue::Int(7)}); // HERO
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(heroOnly), JassValue::Int(6)});
    TEST_ASSERT(!CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(heroOnly)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetTargetError",
                           {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(heroOnly)}).strVal == "Targethero");

    // Air-only set rejects ground units.
    const int airOnly = CallNative(vm, "CreateTargetTypes", {}).intVal;
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(airOnly), JassValue::Int(0)}); // AIR
    TEST_ASSERT(!CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(airOnly)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetTargetError", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(airOnly)}).strVal == "Noground");

    // Invulnerable units fail open sets.
    CallNative(vm, "SetUnitInvulnerable", {JassValue::Int(hb), JassValue::Bool(true)});
    TEST_ASSERT(!CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(empty)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetTargetError", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(empty)}).strVal == "Notinvulnerable");
    CallNative(vm, "SetUnitInvulnerable", {JassValue::Int(hb), JassValue::Bool(false)});
    TEST_ASSERT(CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(empty)}).boolVal);

    // Dead units fail sets without DEAD.
    b->SetLife(sim, 0.0f);
    TEST_ASSERT(b->IsDead());
    TEST_ASSERT(!CallNative(vm, "IsValidTarget", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(set)}).boolVal);
    TEST_ASSERT(!CallNative(vm, "GetTargetError", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(set)}).strVal.empty());
    CallNative(vm, "ResurrectUnit", {JassValue::Int(hb)});
    TEST_ASSERT(!b->IsDead());

    // Spell-shield reaction: first query consumes, second passes.
    // (Shield only answers hostile casters; allies bypass.)
    TEST_ASSERT(CallNative(vm, "CheckUnitForAbilityEffectReaction",
                           {JassValue::Int(hb), JassValue::Int(hc), JassValue::Int(0)}).boolVal);
    b->AddBuff(std::make_unique<CBuffSpellShield>(60.0f), sim);
    TEST_ASSERT(!CallNative(vm, "CheckUnitForAbilityEffectReaction",
                            {JassValue::Int(hb), JassValue::Int(hc), JassValue::Int(0)}).boolVal);
    TEST_ASSERT(CallNative(vm, "CheckUnitForAbilityEffectReaction",
                           {JassValue::Int(hb), JassValue::Int(hc), JassValue::Int(0)}).boolVal);
    TEST_ASSERT(CallNative(vm, "CheckUnitForAbilityEffectReaction",
                           {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(0)}).boolVal);

    // Interface errors share the message channel.
    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;
    CallNative(vm, "ShowInterfaceError", {JassValue::Int(p0), JassValue::Str("Not enough mana")});

    // Per-tick triggers fire through UpdateTimers with a real JASS action.
    TEST_ASSERT(vm.LoadScript("globals integer tickCount endglobals\n"
                              "function TickAct takes nothing returns nothing\n"
                              "set tickCount = tickCount + 1\n"
                              "endfunction\n"));
    const int trig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(trig), JassValue::Str("TickAct")});
    CallNative(vm, "TriggerRegisterOnTick", {JassValue::Int(trig)});
    TEST_ASSERT(data->onTickTriggers.count(trig) > 0);
    natives.UpdateTimers(0.05f);
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("tickCount").intVal == 1);
    natives.UpdateTimers(0.05f);
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("tickCount").intVal == 2);
    const int utrig = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(utrig), JassValue::Str("TickAct")});
    CallNative(vm, "TriggerRegisterOnUnitTick", {JassValue::Int(utrig), JassValue::Int(ha)});
    natives.UpdateTimers(0.05f);
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("tickCount").intVal == 4);
    CallNative(vm, "DestroyTrigger", {JassValue::Int(trig)});
    CallNative(vm, "DestroyTrigger", {JassValue::Int(utrig)});
    TEST_ASSERT(data->onTickTriggers.empty());
    TEST_ASSERT(data->onUnitTickTriggers.empty());
    natives.UpdateTimers(0.05f);
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("tickCount").intVal == 4);

    // ParseTargetTypes consumes a string list, skipping unknown names.
    const int sl = CallNative(vm, "CreateStringList", {}).intVal;
    CallNative(vm, "StringListAdd", {JassValue::Int(sl), JassValue::Str("ground")});
    CallNative(vm, "StringListAdd", {JassValue::Int(sl), JassValue::Str("enemy")});
    CallNative(vm, "StringListAdd", {JassValue::Int(sl), JassValue::Str("bogus")});
    const int parsed = CallNative(vm, "ParseTargetTypes", {JassValue::Int(sl)}).intVal;
    TEST_ASSERT(parsed != 0);
    TEST_ASSERT(CallNative(vm, "TargetTypesContains", {JassValue::Int(parsed), JassValue::Int(6)}).boolVal);
    TEST_ASSERT(CallNative(vm, "TargetTypesContains", {JassValue::Int(parsed), JassValue::Int(5)}).boolVal);
    // Cross-player pair with ENEMIES is valid; same-player allies are not.
    TEST_ASSERT(CallNative(vm, "IsValidTarget", {JassValue::Int(hc), JassValue::Int(ha), JassValue::Int(parsed)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetTargetError", {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(parsed)}).strVal == "Targetenemy");
    TEST_ASSERT(CallNative(vm, "ParseTargetTypes", {JassValue::Int(999999)}).intVal == 0);

    // Spell sounds without ability data fail soft (no audio id tracked).
    CallNative(vm, "UnitSpellSoundEffect", {JassValue::Int(ha), JassValue::Int((int)War3ID("AHbz").value)});
    CallNative(vm, "UnitLoopSpellSoundEffect", {JassValue::Int(ha), JassValue::Int((int)War3ID("AHbz").value)});
    TEST_ASSERT(data->loopedSpellSounds.empty());
    CallNative(vm, "UnitStopSpellSoundEffect", {JassValue::Int(ha), JassValue::Int((int)War3ID("AHbz").value)});
    return true;
}

bool TestJassFiles() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    // No lister injected (headless): graceful no-op.
    TEST_ASSERT(CallNative(vm, "GetEnumFilePath", {}).strVal.empty());
    CallNative(vm, "ForFiles", {JassValue::Str("Units\\"), JassValue::Str("Nope")});

    // Injected lister: prefix match, both separators, case-insensitive,
    // deduped, sorted; callback observes each path in order.
    data->SetFileLister([](const std::string& pattern) -> std::vector<std::string> {
        (void)pattern;
        return {"Units\\B.b", "units\\a.a", "UNITS\\B.B", "Other\\c.c"};
    });
    TEST_ASSERT(vm.LoadScript("globals integer fileCount\n"
                              "string lastPath\n"
                              "endglobals\n"
                              "function FileCb takes nothing returns nothing\n"
                              "set fileCount = fileCount + 1\n"
                              "set lastPath = GetEnumFilePath()\n"
                              "endfunction\n"));
    CallNative(vm, "ForFiles", {JassValue::Str("Units\\"), JassValue::Str("FileCb")});
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("fileCount").intVal == 2);
    // Byte-sorted ("U" < "u"), deduped case-insensitively: last is units\a.a.
    TEST_ASSERT(vm.GetGlobal("lastPath").strVal == "units\\a.a");
    // Enum path restores after the loop.
    TEST_ASSERT(CallNative(vm, "GetEnumFilePath", {}).strVal.empty());
    // Forward-slash prefix matches the same entries.
    CallNative(vm, "ForFiles", {JassValue::Str("units/"), JassValue::Str("FileCb")});
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("fileCount").intVal == 4);
    return true;
}

bool TestJassSmallBatch() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(7041, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(7042, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 100, 0, 0));
    TEST_ASSERT(a && b);
    sim.Update();
    const int ha = data->AllocHandle(JassHandleInfo::UNIT, a->GetHandleId());
    const int hb = data->AllocHandle(JassHandleInfo::UNIT, b->GetHandleId());
    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;

    // Tech research stacks on the current level.
    const int tech = (int)War3ID("Rhss").value;
    CallNative(vm, "AddPlayerTechResearched", {JassValue::Int(p0), JassValue::Int(tech), JassValue::Int(2)});
    TEST_ASSERT(CallNative(vm, "GetPlayerTechResearched", {JassValue::Int(p0), JassValue::Int(tech)}).intVal == 2);
    CallNative(vm, "AddPlayerTechResearched", {JassValue::Int(p0), JassValue::Int(tech), JassValue::Int(1)});
    TEST_ASSERT(CallNative(vm, "GetPlayerTechResearched", {JassValue::Int(p0), JassValue::Int(tech)}).intVal == 3);

    // Agent handle cells round-trip raw handle ints.
    const int ht = CallNative(vm, "InitHashtable", {}).intVal;
    CallNative(vm, "SaveUnitHandle", {JassValue::Int(ht), JassValue::Int(1), JassValue::Int(2), JassValue::Int(hb)});
    TEST_ASSERT(CallNative(vm, "LoadAgentHandle", {JassValue::Int(ht), JassValue::Int(1), JassValue::Int(2)}).intVal == hb);
    TEST_ASSERT(CallNative(vm, "WarsmashGetRawcode2String", {JassValue::Int((int)War3ID("Hpal").value)}).strVal == "Hpal");

    // Unit target error honors the targeted flag for visibility.
    const int set = CallNative(vm, "CreateTargetTypes", {}).intVal;
    CallNative(vm, "TargetTypesAdd", {JassValue::Int(set), JassValue::Int(6)});
    TEST_ASSERT(CallNative(vm, "GetUnitTargetError",
                           {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(set), JassValue::Bool(true)}).strVal.empty());
    CallNative(vm, "ShowUnit", {JassValue::Int(hb), JassValue::Bool(false)});
    TEST_ASSERT(b->IsHidden());
    TEST_ASSERT(CallNative(vm, "GetUnitTargetError",
                           {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(set), JassValue::Bool(true)}).strVal == "Targetunit");
    TEST_ASSERT(CallNative(vm, "GetUnitTargetError",
                           {JassValue::Int(hb), JassValue::Int(ha), JassValue::Int(set), JassValue::Bool(false)}).strVal.empty());
    CallNative(vm, "ShowUnit", {JassValue::Int(hb), JassValue::Bool(true)});

    // False time of day freezes then restores the suspend flag.
    TEST_ASSERT(!sim.IsTimeOfDaySuspended());
    CallNative(vm, "SetFalseTimeOfDay", {JassValue::Int(18), JassValue::Int(30), JassValue::Real(1.0f)});
    TEST_ASSERT(sim.GetGameTimeOfDay() == 18.5f);
    TEST_ASSERT(sim.IsTimeOfDaySuspended());
    natives.UpdateTimers(0.5f);
    TEST_ASSERT(sim.IsTimeOfDaySuspended());
    natives.UpdateTimers(0.5f);
    TEST_ASSERT(!sim.IsTimeOfDaySuspended());

    // Script loading parses, registers, and queues the entry point once.
    data->SetScriptReader([](const std::string& path) -> std::vector<uint8_t> {
        if (path != "Extra\\bonus.j") return {};
        const std::string src = "globals integer bonusLoaded endglobals\n"
                                "function BonusMain takes nothing returns nothing\n"
                                "set bonusLoaded = bonusLoaded + 1\n"
                                "endfunction\n";
        return std::vector<uint8_t>(src.begin(), src.end());
    });
    CallNative(vm, "LoadScriptFile",
               {JassValue::Str("Extra\\bonus.j"), JassValue::Str("BonusMain")});
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("bonusLoaded").intVal == 1);
    // Reload re-runs global initializers (reset to 0) without requeueing
    // an already-known entry point: still 0 after pumping.
    CallNative(vm, "LoadScriptFile",
               {JassValue::Str("Extra\\bonus.j"), JassValue::Str("BonusMain")});
    natives.PumpQueuedThreads();
    TEST_ASSERT(vm.GetGlobal("bonusLoaded").intVal == 0);
    CallNative(vm, "LoadScriptFile", {JassValue::Str("Missing\\no.j")});
    CallNative(vm, "Preloader", {JassValue::Str("Missing\\no.j")});
    return true;
}

bool TestJassEventFiring() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(7051, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0));
    auto* b = sim.AdoptUnit(std::make_unique<CUnit>(7052, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 100, 0, 0));
    TEST_ASSERT(a && b);
    sim.Update();
    const int ha = data->AllocHandle(JassHandleInfo::UNIT, a->GetHandleId());
    const int hb = data->AllocHandle(JassHandleInfo::UNIT, b->GetHandleId());
    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;
    const int p1 = CallNative(vm, "Player", {JassValue::Int(1)}).intVal;

    TEST_ASSERT(vm.LoadScript("globals integer evCount\n"
                              "integer watchVar\n"
                              "endglobals\n"
                              "function EvAct takes nothing returns nothing\n"
                              "set evCount = evCount + 1\n"
                              "endfunction\n"));
    auto base = [&]() { return vm.GetGlobal("evCount").intVal; };
    auto poll = [&]() {
        natives.UpdateTimers(0.05f);
        natives.PumpQueuedThreads();
    };

    // Unit life threshold: true on first poll, silent while held, refires.
    const int t1 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t1), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterUnitStateEvent",
               {JassValue::Int(t1), JassValue::Int(hb), JassValue::Int(0),
                JassValue::Int(4), JassValue::Real(50)});
    poll();
    TEST_ASSERT(base() == 1);
    poll();
    TEST_ASSERT(base() == 1);
    b->SetLife(sim, 10.0f);
    poll();
    TEST_ASSERT(base() == 1);
    b->SetLife(sim, 100.0f);
    poll();
    TEST_ASSERT(base() == 2);

    // Proximity entry with re-entry.
    const int t2 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t2), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterUnitInRange",
               {JassValue::Int(t2), JassValue::Int(ha), JassValue::Real(150), JassValue::Str("")});
    const int beforeRange = base();
    poll();
    TEST_ASSERT(base() == beforeRange + 1);
    CallNative(vm, "SetUnitPosition", {JassValue::Int(hb), JassValue::Real(1000), JassValue::Real(1000)});
    poll();
    TEST_ASSERT(base() == beforeRange + 1);
    CallNative(vm, "SetUnitPosition", {JassValue::Int(hb), JassValue::Real(100), JassValue::Real(0)});
    poll();
    TEST_ASSERT(base() == beforeRange + 2);

    // Player gold threshold.
    const int t3 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t3), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterPlayerStateEvent",
               {JassValue::Int(t3), JassValue::Int(p0), JassValue::Int(1),
                JassValue::Int(4), JassValue::Int(100)});
    const int beforeGold = base();
    poll();
    TEST_ASSERT(base() == beforeGold);
    CallNative(vm, "SetPlayerState", {JassValue::Int(p0), JassValue::Int(1), JassValue::Int(200)});
    poll();
    TEST_ASSERT(base() == beforeGold + 1);

    // Variable watch on a VM global.
    const int t4 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t4), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterVariableEvent",
               {JassValue::Int(t4), JassValue::Str("watchVar"), JassValue::Int(4), JassValue::Real(5)});
    const int beforeVar = base();
    poll();
    TEST_ASSERT(base() == beforeVar);
    vm.SetGlobal("watchVar", JassValue::Int(10));
    poll();
    TEST_ASSERT(base() == beforeVar + 1);

    // Time-of-day threshold drives the sim clock directly.
    const int t5 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t5), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterGameStateEvent",
               {JassValue::Int(t5), JassValue::Int(2), JassValue::Int(4), JassValue::Real(13)});
    const int beforeTod = base();
    poll();
    TEST_ASSERT(base() == beforeTod);
    sim.SetGameTimeOfDay(14.0f);
    poll();
    TEST_ASSERT(base() == beforeTod + 1);

    // Chat commands: substring match fires, exact miss does not.
    const int t6 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t6), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterPlayerChatEvent",
               {JassValue::Int(t6), JassValue::Int(p0), JassValue::Str("-repick"), JassValue::Bool(false)});
    const int beforeChat = base();
    natives.FireChatEvent(p0, "please -repick now");
    natives.PumpQueuedThreads();
    TEST_ASSERT(base() == beforeChat + 1);
    natives.FireChatEvent(p0, "hello");
    natives.PumpQueuedThreads();
    TEST_ASSERT(base() == beforeChat + 1);
    TEST_ASSERT(CallNative(vm, "GetEventPlayerChatString", {}).strVal == "hello");
    const int t7 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t7), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterPlayerChatEvent",
               {JassValue::Int(t7), JassValue::Int(p1), JassValue::Str("-repick"), JassValue::Bool(false)});
    natives.FireChatEvent(p0, "-repick");
    natives.PumpQueuedThreads();
    TEST_ASSERT(base() == beforeChat + 2); // t6 only; t7 listens to p1

    // Alliance change fires listeners on either side.
    const int t8 = CallNative(vm, "CreateTrigger", {}).intVal;
    CallNative(vm, "TriggerAddAction", {JassValue::Int(t8), JassValue::Str("EvAct")});
    CallNative(vm, "TriggerRegisterPlayerAllianceChange", {JassValue::Int(t8), JassValue::Int(p1)});
    const int beforeAlly = base();
    CallNative(vm, "SetPlayerAlliance",
               {JassValue::Int(p0), JassValue::Int(p1), JassValue::Int(3), JassValue::Bool(true)});
    natives.PumpQueuedThreads();
    TEST_ASSERT(base() == beforeAlly + 1);
    return true;
}

bool TestJassBonus() {
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();

    auto* a = sim.AdoptUnit(std::make_unique<CUnit>(7031, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(a != nullptr);
    sim.Update();
    const int ha = data->AllocHandle(JassHandleInfo::UNIT, a->GetHandleId());
    const int baseArmor = a->GetTotalDefense();

    // Aura-slot bonus (DEF ordinal 5): lands in the armor slot, removal undoes.
    const int def = CallNative(vm, "CreateNonStackingStatBonus",
                               {JassValue::Int(5), JassValue::Str("dev"), JassValue::Real(3)}).intVal;
    TEST_ASSERT(def != 0);
    TEST_ASSERT(CallNative(vm, "GetNonStackingStatBonusType", {JassValue::Int(def)}).intVal == 5);
    CallNative(vm, "AddUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(def)});
    TEST_ASSERT(a->GetTotalDefense() == baseArmor + 3);
    CallNative(vm, "UpdateNonStackingStatBonus", {JassValue::Int(def), JassValue::Real(4)});
    TEST_ASSERT(a->GetTotalDefense() == baseArmor + 4);
    CallNative(vm, "RemoveUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(def)});
    TEST_ASSERT(a->GetTotalDefense() == baseArmor);

    // MAXHP bonus (ordinal 18): strongest wins across records.
    const float baseMax = a->GetMaxLife();
    const int hp1 = CallNative(vm, "CreateNonStackingStatBonus",
                               {JassValue::Int(18), JassValue::Str("a"), JassValue::Real(50)}).intVal;
    const int hp2 = CallNative(vm, "CreateNonStackingStatBonus",
                               {JassValue::Int(18), JassValue::Str("b"), JassValue::Real(120)}).intVal;
    CallNative(vm, "AddUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(hp1)});
    TEST_ASSERT(a->GetMaxLife() == baseMax + 50.0f);
    CallNative(vm, "AddUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(hp2)});
    TEST_ASSERT(a->GetMaxLife() == baseMax + 120.0f);
    CallNative(vm, "RemoveUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(hp2)});
    TEST_ASSERT(a->GetMaxLife() == baseMax + 50.0f);
    CallNative(vm, "RemoveUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(hp1)});
    TEST_ASSERT(a->GetMaxLife() == baseMax);

    // Stored-only type (THORNS ordinal 16): records without mutators.
    const int th = CallNative(vm, "CreateNonStackingStatBonus",
                              {JassValue::Int(16), JassValue::Str("t"), JassValue::Real(1)}).intVal;
    CallNative(vm, "AddUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(th)});
    CallNative(vm, "RemoveUnitNonStackingStatBonus", {JassValue::Int(ha), JassValue::Int(th)});

    // State mods: OR-recompute over active records, removal restores.
    TEST_ASSERT(!a->IsInvulnerable());
    const int sm1 = CallNative(vm, "CreateStateMod", {JassValue::Int(17), JassValue::Int(1)}).intVal; // INVULNERABLE
    const int sm2 = CallNative(vm, "CreateStateMod", {JassValue::Int(17), JassValue::Int(0)}).intVal;
    CallNative(vm, "AddUnitStateMod", {JassValue::Int(ha), JassValue::Int(sm1)});
    TEST_ASSERT(a->IsInvulnerable());
    CallNative(vm, "AddUnitStateMod", {JassValue::Int(ha), JassValue::Int(sm2)});
    TEST_ASSERT(a->IsInvulnerable());
    CallNative(vm, "RemoveUnitStateMod", {JassValue::Int(ha), JassValue::Int(sm1)});
    TEST_ASSERT(!a->IsInvulnerable());
    CallNative(vm, "RemoveUnitStateMod", {JassValue::Int(ha), JassValue::Int(sm2)});
    CallNative(vm, "UpdateStateMod", {JassValue::Int(sm1), JassValue::Int(1)});
    CallNative(vm, "AddUnitStateMod", {JassValue::Int(ha), JassValue::Int(sm1)});
    TEST_ASSERT(a->IsInvulnerable());
    CallNative(vm, "RecomputeStateModsOnUnit", {JassValue::Int(ha), JassValue::Int(17)});
    TEST_ASSERT(a->IsInvulnerable());
    CallNative(vm, "RemoveUnitStateMod", {JassValue::Int(ha), JassValue::Int(sm1)});
    TEST_ASSERT(!a->IsInvulnerable());

    // Ethereal flag mapping.
    const int eth = CallNative(vm, "CreateStateMod", {JassValue::Int(0), JassValue::Int(1)}).intVal;
    CallNative(vm, "AddUnitStateMod", {JassValue::Int(ha), JassValue::Int(eth)});
    TEST_ASSERT(a->IsEthereal());
    CallNative(vm, "RemoveUnitStateMod", {JassValue::Int(ha), JassValue::Int(eth)});
    TEST_ASSERT(!a->IsEthereal());

    // Training queues production on a factory with a queue ability.
    // (No unit data/gold in this harness, so QueueUnit declines: the
    // native must find the queue and fail soft, not crash.)
    a->AddAbility(sim, std::make_unique<CAbilityQueue>(8001));
    auto* queue = dynamic_cast<CAbilityQueue*>(a->GetAbility(War3ID("Aque")));
    TEST_ASSERT(queue != nullptr);
    const int before = queue->GetQueueCount();
    CallNative(vm, "StartTrainingUnit", {JassValue::Int(ha), JassValue::Int((int)War3ID("hfoo").value)});
    TEST_ASSERT(queue->GetQueueCount() == before);
    CallNative(vm, "StartTrainingUnit", {JassValue::Int(999999), JassValue::Int((int)War3ID("hfoo").value)});

    // Containers.
    const int hl = CallNative(vm, "CreateHandleList", {}).intVal;
    TEST_ASSERT(hl != 0);
    CallNative(vm, "HandleListAdd", {JassValue::Int(hl), JassValue::Int(ha)});
    CallNative(vm, "HandleListAdd", {JassValue::Int(hl), JassValue::Str("x")});
    TEST_ASSERT(CallNative(vm, "HandleListSize", {JassValue::Int(hl)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "HandleListGet", {JassValue::Int(hl), JassValue::Int(0)}).intVal == ha);
    CallNative(vm, "HandleListRemove", {JassValue::Int(hl), JassValue::Int(ha)});
    TEST_ASSERT(CallNative(vm, "HandleListSize", {JassValue::Int(hl)}).intVal == 1);
    const int sl = CallNative(vm, "CreateStringList", {}).intVal;
    CallNative(vm, "StringListAdd", {JassValue::Int(sl), JassValue::Str("one")});
    CallNative(vm, "StringListAdd", {JassValue::Int(sl), JassValue::Str("two")});
    TEST_ASSERT(CallNative(vm, "StringListSize", {JassValue::Int(sl)}).intVal == 2);
    TEST_ASSERT(CallNative(vm, "StringListGet", {JassValue::Int(sl), JassValue::Int(1)}).strVal == "two");
    CallNative(vm, "StringListRemove", {JassValue::Int(sl), JassValue::Str("one")});
    TEST_ASSERT(CallNative(vm, "StringListGet", {JassValue::Int(sl), JassValue::Int(0)}).strVal == "two");
    const int ie = CallNative(vm, "IntExpr", {JassValue::Str("TickAct")}).intVal;
    TEST_ASSERT(ie != 0);
    CallNative(vm, "DestroyIntExpr", {JassValue::Int(ie)});

    // StringHash must match Java String.hashCode() (retail/ref parity, not
    // std::hash): golden values from Java semantics (31*h+c per char).
    TEST_ASSERT(CallNative(vm, "StringHash", {JassValue::Str("")}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "StringHash", {JassValue::Str("a")}).intVal == 97);
    TEST_ASSERT(CallNative(vm, "StringHash", {JassValue::Str("hello")}).intVal == 99162322);
    TEST_ASSERT(CallNative(vm, "StringHash", {JassValue::Str("Warcraft")}).intVal == 552832088);
    TEST_ASSERT(CallNative(vm, "StringHash", {JassValue::Str("TestString")}).intVal == -573753501);

    // Hero tokens round-trip through player state 3 (retail
    // PLAYER_STATE_RESOURCE_HERO_TOKENS); default zero.
    const int p0 = CallNative(vm, "Player", {JassValue::Int(0)}).intVal;
    TEST_ASSERT(CallNative(vm, "GetPlayerState", {JassValue::Int(p0), JassValue::Int(3)}).intVal == 0);
    CallNative(vm, "SetPlayerState", {JassValue::Int(p0), JassValue::Int(3), JassValue::Int(2)});
    TEST_ASSERT(CallNative(vm, "GetPlayerState", {JassValue::Int(p0), JassValue::Int(3)}).intVal == 2);
    return true;
}

bool TestJassAbilityObjects() {
    // CreateJassAbility/Buff return real handle objects (not 0): queryable
    // by id, attachable through the factory path, removable again. Custom
    // codes and buff handles fail closed without crashing.
    JassVM vm;
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    JassNatives natives(vm, &sim);
    natives.RegisterAll();
    auto data = natives.GetData();
    auto* u = sim.AdoptUnit(std::make_unique<CUnit>(
        9601, MakeP0Type("hfoo", War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0));
    TEST_ASSERT(u);
    sim.Update();
    const int hu = data->AllocHandle(JassHandleInfo::UNIT, 9601);

    const int atkId = (int)War3ID("Aatk").value;
    const int customId = (int)War3ID("X001").value;
    const int ha = CallNative(vm, "CreateJassAbility", {JassValue::Int(atkId)}).intVal;
    TEST_ASSERT(ha != 0);
    TEST_ASSERT(CallNative(vm, "BlzGetAbilityId", {JassValue::Int(ha)}).intVal == atkId);
    // Factory-known code attaches through the normal path.
    TEST_ASSERT(CallNative(vm, "UnitAddAbility", {JassValue::Int(hu), JassValue::Int(ha)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetUnitAbility",
        {JassValue::Int(hu), JassValue::Int(atkId)}).intVal != 0);
    TEST_ASSERT(CallNative(vm, "UnitRemoveAbility", {JassValue::Int(hu), JassValue::Int(atkId)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetUnitAbility",
        {JassValue::Int(hu), JassValue::Int(atkId)}).intVal == 0);

    // Custom code: object stays queryable; attach lands a do-nothing shim
    // (Warsmash-tolerant; retail returns false) instead of crashing.
    const int hx = CallNative(vm, "CreateJassAbility", {JassValue::Int(customId)}).intVal;
    TEST_ASSERT(hx != 0 && hx != ha);
    TEST_ASSERT(CallNative(vm, "BlzGetAbilityId", {JassValue::Int(hx)}).intVal == customId);
    TEST_ASSERT(CallNative(vm, "UnitAddAbility", {JassValue::Int(hu), JassValue::Int(hx)}).boolVal);
    TEST_ASSERT(CallNative(vm, "GetUnitAbility",
        {JassValue::Int(hu), JassValue::Int(customId)}).intVal != 0);

    // Buff handle carries code + alias; attach waits on the buff factory.
    const int buffId = (int)War3ID("B001").value;
    const int aliasId = (int)War3ID("B002").value;
    const int hb = CallNative(vm, "CreateJassBuff",
        {JassValue::Int(buffId), JassValue::Int(aliasId)}).intVal;
    TEST_ASSERT(hb != 0);
    TEST_ASSERT(CallNative(vm, "BlzGetAbilityId", {JassValue::Int(hb)}).intVal == buffId);
    TEST_ASSERT(!CallNative(vm, "UnitAddAbility", {JassValue::Int(hu), JassValue::Int(hb)}).boolVal);

    // Bad args stay null-safe.
    TEST_ASSERT(CallNative(vm, "CreateJassAbility", {}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "CreateJassBuff", {JassValue::Int(buffId)}).intVal == 0);
    TEST_ASSERT(CallNative(vm, "BlzGetAbilityId", {JassValue::Int(999999)}).intVal == 0);
    return true;
}

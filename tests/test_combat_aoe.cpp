// Splash / bounce / evasion / critical-strike combat tests.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/unitdata.h"
#include "game/war3/abilities/war3_spells.h"
#include "ecs/combat/combat_system.h"
#include "ecs/combat/projectile.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_attack.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " << (a) << " vs " << (b) << "\n"; return false; } } while (0)

namespace {

CUnitType* MakeAoeTestType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 0;
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
    return sim.AdoptUnit(
        std::make_unique<CUnit>(handle, MakeAoeTestType(), player, War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestSplashDamagesEnemiesNotAllies() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3001, 0, -400, 0);
    auto* primary = AdoptAt(sim, 3002, 1, 0, 0);
    auto* halfRing = AdoptAt(sim, 3003, 1, 150, 0);   // within half(200), outside full(100)
    auto* quarterRing = AdoptAt(sim, 3004, 1, 250, 0); // within quarter(300)
    auto* ally = AdoptAt(sim, 3005, 0, 150, 0);        // same player as attacker
    auto* far = AdoptAt(sim, 3006, 1, 900, 0);
    TEST_ASSERT(attacker && primary && halfRing && quarterRing && ally && far);
    sim.Update();

    CUnitAttack splash;
    splash.attackType = CAttackType::NORMAL;
    splash.areaFull = 100.0f;
    splash.areaHalf = 200.0f;
    splash.areaQuarter = 300.0f;
    splash.damageFactorHalf = 0.5f;
    splash.damageFactorQuarter = 0.25f;

    auto* combat = sim.GetCombatSystem();
    TEST_ASSERT(combat != nullptr);
    const float dealt = combat->ApplySplashDamage(sim, attacker, primary, true, false,
                                                  CAttackType::NORMAL, CDamageType::NORMAL,
                                                  "", 100.0f, 0, 0, 0, splash);
    TEST_ASSERT(dealt > 0);
    TEST_NEAR(10000.0f - primary->GetLife(), 100.0f, 0.01f);
    TEST_NEAR(10000.0f - halfRing->GetLife(), 50.0f, 0.01f);
    TEST_NEAR(10000.0f - quarterRing->GetLife(), 25.0f, 0.01f);
    TEST_NEAR(ally->GetLife(), 10000.0f, 0.01f);
    TEST_NEAR(far->GetLife(), 10000.0f, 0.01f);
    return true;
}

bool TestProjectileBounceChainsWithFalloff() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3101, 0, -400, 0);
    auto* first = AdoptAt(sim, 3102, 1, 0, 0);
    auto* second = AdoptAt(sim, 3103, 1, 200, 0);
    TEST_ASSERT(attacker && first && second);
    sim.Update();

    CProjectile shot;
    shot.attackerHandleId = 3101;
    shot.targetHandleId = 3102;
    shot.x = -400.0f;
    shot.y = 0.0f;
    shot.speed = 2000.0f;
    shot.damage = 100.0f;
    shot.attackType = CAttackType::NORMAL;
    shot.maxTravel = 2000.0f;
    shot.bouncesLeft = 1;
    shot.bounceRange = 600.0f;
    shot.bounceLossFactor = 0.5f;
    sim.SpawnProjectile(std::move(shot));

    for (int i = 0; i < 500; i++) {
        sim.UpdateProjectiles();
        if (sim.GetProjectiles().empty()) break;
    }
    TEST_ASSERT(sim.GetProjectiles().empty());
    TEST_NEAR(10000.0f - first->GetLife(), 100.0f, 0.5f);
    TEST_NEAR(10000.0f - second->GetLife(), 50.0f, 0.5f);
    return true;
}

bool TestEvasionNegatesAttacks() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3201, 0, -100, 0);
    auto* dodgy = AdoptAt(sim, 3202, 1, 0, 0);
    auto* plain = AdoptAt(sim, 3203, 1, 0, 100);
    TEST_ASSERT(attacker && dodgy && plain);
    sim.Update();

    dodgy->SetEvasionChance(1.0f);
    auto* combat = sim.GetCombatSystem();
    DamageResult r = combat->CalculateDamage(attacker, dodgy, true, false,
                                             CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(r.isEvaded);
    TEST_ASSERT(r.damageDealt == 0.0f);
    DamageResult r2 = combat->CalculateDamage(attacker, plain, true, false,
                                              CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(!r2.isEvaded);
    TEST_ASSERT(r2.damageDealt > 0.0f);
    return true;
}

bool TestCriticalStrikeMultipliesDamage() {
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3301, 0, -100, 0);
    auto* target = AdoptAt(sim, 3302, 1, 0, 0);
    TEST_ASSERT(attacker && target);
    sim.Update();

    attacker->SetCriticalStrike(1.0f, 2.0f);
    auto* combat = sim.GetCombatSystem();
    DamageResult r = combat->CalculateDamage(attacker, target, true, false,
                                             CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(r.isCritical);
    TEST_NEAR(r.damageDealt, 200.0f, 0.01f);
    return true;
}

bool TestEvasionSourcesCombine() {
    // Ref evasionListeners: independent rolls per source; removing one
    // source leaves the others (the brawler-wipes-DH bug is gone).
    CSimulation sim;
    auto* unit = AdoptAt(sim, 3401, 0, 0, 0);
    TEST_ASSERT(unit);
    sim.Update();

    unit->AddEvasionSource(War3ID("AEev"), 0.5f);
    unit->AddEvasionSource(War3ID("ANdb"), 0.5f);
    TEST_NEAR(unit->GetEvasionChance(), 0.75f, 1e-6f);
    unit->RemoveEvasionSource(War3ID("ANdb"));
    TEST_NEAR(unit->GetEvasionChance(), 0.5f, 1e-6f);
    // Same-id reinstall replaces instead of duplicating.
    unit->AddEvasionSource(War3ID("AEev"), 0.25f);
    TEST_NEAR(unit->GetEvasionChance(), 0.25f, 1e-6f);
    TEST_ASSERT(unit->GetEvasionSources().size() == 1);
    // Stale removals are ignored.
    unit->RemoveEvasionSource(War3ID("ANdb"));
    TEST_NEAR(unit->GetEvasionChance(), 0.25f, 1e-6f);
    return true;
}

bool TestCritSourcesHighestWins() {
    // Independent rolls, strongest multiplier wins (never multiplied).
    CSimulation sim;
    auto* attacker = AdoptAt(sim, 3411, 0, -100, 0);
    auto* target = AdoptAt(sim, 3412, 1, 0, 0);
    TEST_ASSERT(attacker && target);
    sim.Update();

    attacker->AddCritSource(War3ID("AOcr"), 1.0f, 2.0f);
    attacker->AddCritSource(War3ID("ANdb"), 1.0f, 3.0f);
    auto* combat = sim.GetCombatSystem();
    DamageResult r = combat->CalculateDamage(attacker, target, true, false,
                                             CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(r.isCritical);
    TEST_NEAR(r.damageDealt, 300.0f, 0.01f);
    // Display pair tracks the likeliest source.
    TEST_NEAR(attacker->GetCritChance(), 1.0f, 1e-6f);
    TEST_ASSERT(attacker->GetCritSources().size() == 2);
    attacker->RemoveCritSource(War3ID("ANdb"));
    DamageResult r2 = combat->CalculateDamage(attacker, target, true, false,
                                              CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    TEST_ASSERT(r2.isCritical);
    TEST_NEAR(r2.damageDealt, 200.0f, 0.01f);
    return true;
}

bool TestCombatTextEventsFire() {
    struct Event {
        int attacker = -2;
        int victim = -2;
        CSimulation::CombatTextKind kind = CSimulation::CombatTextKind::MISS;
        float amount = -1.0f;
    };
    CSimulation sim;
    std::vector<Event> events;
    sim.SetCombatTextCallback(
        [&](int a, int v, CSimulation::CombatTextKind k, float amount) {
            events.push_back({a, v, k, amount});
        });
    auto* attacker = AdoptAt(sim, 3421, 0, -100, 0);
    auto* dodgy = AdoptAt(sim, 3422, 1, 0, 0);
    auto* plain = AdoptAt(sim, 3423, 1, 0, 100);
    TEST_ASSERT(attacker && dodgy && plain);
    sim.Update();

    // Evasion reports MISS with both handles.
    dodgy->AddEvasionSource(War3ID("AEev"), 1.0f);
    const float lifeBefore = dodgy->GetLife();
    dodgy->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(dodgy->GetLife(), lifeBefore, 0.001f);
    TEST_ASSERT(events.size() == 1);
    TEST_ASSERT(events[0].kind == CSimulation::CombatTextKind::MISS);
    TEST_ASSERT(events[0].attacker == attacker->GetHandleId());
    TEST_ASSERT(events[0].victim == dodgy->GetHandleId());
    TEST_NEAR(events[0].amount, 0.0f, 0.001f);

    // Crit reports CRIT with the final number.
    events.clear();
    attacker->AddCritSource(War3ID("AOcr"), 1.0f, 2.0f);
    const float plainBefore = plain->GetLife();
    plain->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(plainBefore - plain->GetLife(), 200.0f, 0.01f);
    TEST_ASSERT(events.size() == 1);
    TEST_ASSERT(events[0].kind == CSimulation::CombatTextKind::CRIT);
    TEST_NEAR(events[0].amount, 200.0f, 0.01f);
    return true;
}

bool TestBashFiresCombatText() {
    struct Event {
        CSimulation::CombatTextKind kind = CSimulation::CombatTextKind::MISS;
    };
    CSimulation sim;
    std::vector<Event> events;
    sim.SetCombatTextCallback(
        [&](int a, int v, CSimulation::CombatTextKind k, float amount) {
            (void)a; (void)v; (void)amount;
            events.push_back({k});
        });
    auto* attacker = AdoptAt(sim, 3431, 0, -100, 0);
    auto* target = AdoptAt(sim, 3432, 1, 0, 0);
    TEST_ASSERT(attacker && target);
    sim.Update();

    auto bash = std::make_unique<CAbilityBash>(34301);
    bash->SetChance(1.0f);
    attacker->AddAbility(sim, std::move(bash));
    target->Damage(sim, attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(!events.empty());
    TEST_ASSERT(events.back().kind == CSimulation::CombatTextKind::BASH);
    return true;
}

namespace {

CUnitAttack MakeShellAttack() {
    // Mortar-like ARTILLERY shell: lobbed projectile, three-ring splash.
    CUnitAttack atk;
    atk.attackType = CAttackType::SIEGE;
    atk.weaponType = CWeaponType::ARTILLERY;
    atk.damageBase = 100;
    atk.cooldown = 1.0f;
    atk.range = 1000.0f;
    atk.projectileSpeed = 900.0f;
    atk.areaFull = 100.0f;
    atk.areaHalf = 200.0f;
    atk.areaQuarter = 300.0f;
    atk.damageFactorHalf = 0.5f;
    atk.damageFactorQuarter = 0.25f;
    return atk;
}

CUnitAttack MakeLineAttack() {
    // Ballista-like MLINE bolt: homing missile with a spill corridor.
    CUnitAttack atk;
    atk.attackType = CAttackType::PIERCE;
    atk.weaponType = CWeaponType::MISSILE_LINE;
    atk.damageBase = 100;
    atk.cooldown = 1.0f;
    atk.range = 1000.0f;
    atk.projectileSpeed = 1200.0f;
    atk.damageSpillDistance = 300.0f;
    atk.damageSpillRadius = 50.0f;
    return atk;
}

std::unique_ptr<CUnitType> MakeWeaponType(const char* id, CUnitAttack atk) {
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

} // namespace

bool TestArtilleryProjectileSplashes() {
    // Ref CBehaviorAttack: ARTILLERY stamps the launch-time position and
    // detonates splash there (previously fell through to single-target).
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    CUnitData data;
    data.RegisterUnitType(War3ID("hmtm"), MakeWeaponType("hmtm", MakeShellAttack()));
    data.RegisterUnitType(War3ID("hfoo"), MakeWeaponType("hfoo", CUnitAttack()));
    sim.SetUnitData(&data);
    CUnit* mortar = sim.CreateUnit(War3ID("hmtm"), 0, -400.0f, 0.0f, 0.0f);
    CUnit* primary = sim.CreateUnit(War3ID("hfoo"), 1, 0.0f, 0.0f, 0.0f);
    CUnit* halfRing = sim.CreateUnit(War3ID("hfoo"), 1, 150.0f, 0.0f, 0.0f);
    CUnit* ally = sim.CreateUnit(War3ID("hfoo"), 0, 150.0f, 60.0f, 0.0f);
    TEST_ASSERT(mortar && primary && halfRing && ally);
    sim.Update();
    sim.Update();

    mortar->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::ATTACK, primary));
    bool impacted = false;
    for (int i = 0; i < 600 && !impacted; i++) {
        sim.Update();
        if (10000.0f - primary->GetLife() > 0.5f) impacted = true;
    }
    TEST_ASSERT(impacted);
    // One stamped shell: full at the crater, half ring scaled, allies safe.
    TEST_NEAR(10000.0f - primary->GetLife(), 100.0f, 0.5f);
    TEST_NEAR(10000.0f - halfRing->GetLife(), 50.0f, 0.5f);
    TEST_NEAR(ally->GetLife(), 10000.0f, 0.5f);
    return true;
}

bool TestAttackGroundShellsPoint() {
    // Attack-ground orders reach shelling weapons (previously nullptr) and
    // damage the rings around the ordered point; line rifles refuse them.
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    CUnitData data;
    data.RegisterUnitType(War3ID("hmtm"), MakeWeaponType("hmtm", MakeShellAttack()));
    data.RegisterUnitType(War3ID("hfoo"), MakeWeaponType("hfoo", CUnitAttack()));
    sim.SetUnitData(&data);
    CUnit* mortar = sim.CreateUnit(War3ID("hmtm"), 0, -400.0f, 0.0f, 0.0f);
    CUnit* near = sim.CreateUnit(War3ID("hfoo"), 1, 150.0f, 0.0f, 0.0f);
    CUnit* far = sim.CreateUnit(War3ID("hfoo"), 1, 900.0f, 0.0f, 0.0f);
    CUnit* footman = sim.CreateUnit(War3ID("hfoo"), 0, -400.0f, 200.0f, 0.0f);
    TEST_ASSERT(mortar && near && far && footman);
    sim.Update();
    sim.Update();

    // Footmen cannot attack ground: the order falls back to stop.
    footman->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::ATTACK_GROUND, 0.0f, 0.0f));
    sim.Update();
    TEST_ASSERT(footman->GetCurrentBehavior() == nullptr ||
                footman->GetCurrentBehavior()->GetCategory() == CBehaviorCategory::STOP);
    TEST_ASSERT(sim.GetProjectiles().empty());

    // The mortar holds and shells the ordered point (no widget there).
    mortar->IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::ATTACK_GROUND, 0.0f, 0.0f));
    sim.Update();
    TEST_ASSERT(mortar->GetCurrentBehavior() == mortar->GetAttackBehavior());
    bool impacted = false;
    for (int i = 0; i < 600 && !impacted; i++) {
        sim.Update();
        if (10000.0f - near->GetLife() > 0.5f) impacted = true;
    }
    TEST_ASSERT(impacted);
    TEST_NEAR(10000.0f - near->GetLife(), 50.0f, 0.5f);
    TEST_NEAR(far->GetLife(), 10000.0f, 0.5f);
    return true;
}

bool TestMissileLineSpillsCorridor() {
    // MLINE bolts pierce: full damage down the spill corridor past impact,
    // the primary is never double-hit, outsiders are safe.
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
    auto* attacker = AdoptAt(sim, 3401, 0, 0, 0);
    auto* primary = AdoptAt(sim, 3402, 1, 200, 0);
    auto* inLine = AdoptAt(sim, 3403, 1, 350, 0);
    auto* offLine = AdoptAt(sim, 3404, 1, 350, 150);
    auto* ally = AdoptAt(sim, 3405, 0, 350, 0);
    TEST_ASSERT(attacker && primary && inLine && offLine && ally);
    sim.Update();

    CProjectile bolt;
    bolt.attackerHandleId = 3401;
    bolt.targetHandleId = 3402;
    bolt.x = 0.0f;
    bolt.y = 0.0f;
    bolt.fx = 0.0f;
    bolt.fy = 0.0f;
    bolt.speed = 2000.0f;
    bolt.damage = 100.0f;
    bolt.attackType = CAttackType::PIERCE;
    bolt.maxTravel = 2000.0f;
    bolt.spillDistance = 300.0f;
    bolt.spillRadius = 50.0f;
    sim.SpawnProjectile(std::move(bolt));
    for (int i = 0; i < 500; i++) {
        sim.UpdateProjectiles();
        if (sim.GetProjectiles().empty()) break;
    }
    TEST_ASSERT(sim.GetProjectiles().empty());
    TEST_NEAR(10000.0f - primary->GetLife(), 100.0f, 0.5f);
    TEST_NEAR(10000.0f - inLine->GetLife(), 100.0f, 0.5f);
    TEST_NEAR(offLine->GetLife(), 10000.0f, 0.5f);
    TEST_NEAR(ally->GetLife(), 10000.0f, 0.5f);

    // The behavior fire fork carries the data spill onto the missile.
    CUnitData data;
    data.RegisterUnitType(War3ID("hbal"), MakeWeaponType("hbal", MakeLineAttack()));
    data.RegisterUnitType(War3ID("hfoo"), MakeWeaponType("hfoo", CUnitAttack()));
    sim.SetUnitData(&data);
    CUnit* ballista = sim.CreateUnit(War3ID("hbal"), 0, -400.0f, 0.0f, 0.0f);
    CUnit* victim = sim.CreateUnit(War3ID("hfoo"), 1, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT(ballista && victim);
    sim.Update();
    sim.Update();
    ballista->IssueOrder(std::make_unique<COrderTargetWidget>(OrderIds::ATTACK, victim));
    bool launched = false;
    for (int i = 0; i < 200 && !launched; i++) {
        sim.Update();
        if (!sim.GetProjectiles().empty()) launched = true;
    }
    TEST_ASSERT(launched);
    TEST_ASSERT(!sim.GetProjectiles().empty());
    TEST_NEAR(sim.GetProjectiles().front().spillDistance, 300.0f, 0.001f);
    TEST_NEAR(sim.GetProjectiles().front().spillRadius, 50.0f, 0.001f);
    return true;
}

// Step-2 combat tests: attack/defense table, unified damage formula.
#include <cmath>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/behaviors/behavior_attack.h"
#include "ecs/behaviors/behavior_hold_position.h"
#include "ecs/behaviors/behavior_move.h"
#include "ecs/behaviors/behavior_stun.h"
#include "ecs/combat/combat_system.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "map/w3e.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeCombatType(CDefenseType defType, int defense) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, defense, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, defType,
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

bool TestAttackDefenseTableMatchesOfficialChart() {
    // Official TFT chart (classic.battle.net), columns L/M/H/F/Hero/U.
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::NORMAL, CDefenseType::MEDIUM), 1.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::PIERCE, CDefenseType::LIGHT), 2.0f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::PIERCE, CDefenseType::MEDIUM), 0.75f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::PIERCE, CDefenseType::UNARMORED), 1.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SIEGE, CDefenseType::FORTIFIED), 1.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SIEGE, CDefenseType::HERO), 0.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::MAGIC, CDefenseType::HEAVY), 2.0f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::MAGIC, CDefenseType::LIGHT), 1.25f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SPELLS, CDefenseType::HERO), 0.7f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::SPELLS, CDefenseType::MEDIUM), 1.0f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::HERO, CDefenseType::FORTIFIED), 0.5f, 1e-5f);
    TEST_NEAR(CCombatSystem::GetAttackDefenseMultiplier(CAttackType::CHAOS, CDefenseType::FORTIFIED), 1.0f, 1e-5f);
    return true;
}

bool TestUnitDamageAppliesArmorTypeMultiplier() {
    CSimulation sim;
    // Normal attack vs Medium armor, 0 armor value: 100 * 1.5 = 150.
    CUnit target(1, MakeCombatType(CDefenseType::MEDIUM, 0), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 150.0f, 0.01f);
    return true;
}

bool TestNonNormalDamageTypeIgnoresArmorValue() {
    CSimulation sim;
    // 100 armor would block ~86% of physical; spell damage ignores it.
    CUnit target(2, MakeCombatType(CDefenseType::NONE, 100), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, false, false, CAttackType::SPELLS, CDamageType::COLD, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 100.0f, 0.01f);
    // ...while physical is heavily reduced.
    CUnit target2(3, MakeCombatType(CDefenseType::NONE, 100), 0, War3ID("hfoo"), 0, 0, 0);
    const float life2Before = target2.GetLife();
    target2.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(life2Before - target2.GetLife() < 20.0f);
    return true;
}

bool TestNegativeArmorAmplifiesDamage() {
    CSimulation sim;
    // Ref formula: 2 - 0.94^10 = ~1.4609x at -10 armor.
    CUnit target(4, MakeCombatType(CDefenseType::NONE, -10), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 100.0f * (2.0f - std::pow(0.94f, 10.0f)), 0.05f);
    return true;
}

bool TestBonusDamageIsMultipliedNotBypassing() {
    CSimulation sim;
    // (100 base + 10 bonus) * 1.5 (Normal vs Medium), 0 armor.
    CUnit target(5, MakeCombatType(CDefenseType::MEDIUM, 0), 0, War3ID("hfoo"), 0, 0, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, nullptr, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 10.0f);
    TEST_NEAR(lifeBefore - target.GetLife(), 165.0f, 0.01f);
    return true;
}

bool TestCombatSystemBlockedMathConsistent() {
    CSimulation sim;
    // 100 dmg vs 6 armor (NONE type): reduction = 0.36/1.36.
    CUnit target(6, MakeCombatType(CDefenseType::NONE, 6), 0, War3ID("hfoo"), 0, 0, 0);
    DamageResult r = sim.GetCombatSystem()->CalculateDamage(
        nullptr, &target, true, false, CAttackType::NORMAL, CDamageType::NORMAL, 100.0f, 0, 0, 0);
    const float red = 0.36f / 1.36f;
    TEST_NEAR(r.damageBlocked, 100.0f * red, 0.01f);
    TEST_NEAR(r.damageDealt, 100.0f * (1.0f - red), 0.01f);
    TEST_NEAR(r.damageBlocked + r.damageDealt, 100.0f, 0.01f);
    return true;
}

namespace {

CUnitType* MakePeonType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    keep.push_back(std::make_unique<CUnitType>(
        "Peon", "Peon", "icon", War3ID("opeo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{CUnitClassification::PEON},
        std::vector<CUnitAttack>{},
        0, std::string(""), false, true, CDefenseType::NONE,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Orc", War3ID("O")},
        100, 50, 1, 0, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, true, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

void AddTwoPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

} // namespace

bool TestDamagedUnitReturnsFire() {
    CSimulation sim;
    AddTwoPlayers(sim);
    CUnit victim(11, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 0, 0, 0);
    CUnit attacker(12, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 300, 0, 0);
    victim.SetAttackBehavior(std::make_unique<CBehaviorAttack>());
    victim.SetMoveBehavior(std::make_unique<CBehaviorMove>());
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(victim.GetCurrentBehavior() == victim.GetAttackBehavior());
    TEST_ASSERT(victim.GetAttackBehavior()->GetTarget() == &attacker);
    return true;
}

bool TestDamagedPeonFlees() {
    CSimulation sim;
    AddTwoPlayers(sim);
    CUnit victim(13, MakePeonType(), 0, War3ID("opeo"), 0, 0, 0);
    CUnit attacker(14, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 300, 0, 0);
    victim.SetMoveBehavior(std::make_unique<CBehaviorMove>());
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 10.0f, 0);
    // No attacks and PEON: return fire skipped, move behavior adopted.
    TEST_ASSERT(victim.GetCurrentBehavior() == victim.GetMoveBehavior());
    TEST_ASSERT(victim.GetCurrentBehavior()->GetCategory() == CBehaviorCategory::MOVE);
    return true;
}

bool TestBusyUnitIgnoresReturnFire() {
    CSimulation sim;
    AddTwoPlayers(sim);
    CUnit victim(15, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 0, 0, 0);
    CUnit attacker(16, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 300, 0, 0);
    victim.SetAttackBehavior(std::make_unique<CBehaviorAttack>());
    victim.SetMoveBehavior(std::make_unique<CBehaviorMove>());
    // A non-default active behavior blocks return fire/flee.
    victim.SetCurrentBehavior(std::make_unique<CBehaviorHoldPosition>());
    CBehavior* busy = victim.GetCurrentBehavior();
    TEST_ASSERT(busy != nullptr);
    victim.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(victim.GetCurrentBehavior() == busy);
    TEST_ASSERT(victim.GetAttackBehavior()->GetTarget() == nullptr);
    return true;
}

bool TestNewOrderPreemptsMove() {
    CSimulation sim;
    AddTwoPlayers(sim);
    CUnit unit(21, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 0, 0, 0);
    auto move = std::make_unique<CBehaviorMove>();
    move->SetTarget(5000.0f, 0.0f);
    unit.SetCurrentBehavior(std::move(move));
    TEST_ASSERT(unit.GetCurrentBehavior() != nullptr);
    // No abilities: the preempted order falls back to stop, but the move
    // must be gone and the queue consumed immediately (no waiting).
    unit.IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, 100.0f, 100.0f));
    unit.Update(sim);
    TEST_ASSERT(!unit.HasQueuedOrders());
    TEST_ASSERT(unit.GetCurrentBehavior() != nullptr);
    TEST_ASSERT(unit.GetCurrentBehavior()->GetCategory() == CBehaviorCategory::STOP);
    return true;
}

bool TestStunDefersOrdersUntilExpiry() {
    CSimulation sim;
    AddTwoPlayers(sim);
    CUnit unit(22, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 0, 0, 0);
    auto stun = std::make_unique<CBehaviorStun>();
    stun->SetDuration(0.2f);
    unit.SetCurrentBehavior(std::move(stun));
    unit.GetCurrentBehavior()->Begin(sim);
    // Two orders during stun collapse to the latest and wait.
    unit.IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, 100.0f, 0.0f));
    unit.IssueOrder(std::make_unique<COrderTargetPoint>(OrderIds::SMART, 200.0f, 0.0f));
    unit.Update(sim);
    TEST_ASSERT(unit.GetCurrentBehavior() != nullptr);
    TEST_ASSERT(unit.GetCurrentBehavior()->GetCategory() == CBehaviorCategory::STUN);
    TEST_ASSERT(unit.HasQueuedOrders());
    // After expiry the deferred order runs and the queue drains.
    for (int i = 0; i < 50 && unit.HasQueuedOrders(); i++) unit.Update(sim);
    TEST_ASSERT(!unit.HasQueuedOrders());
    return true;
}

namespace {

W3EMap MakeCliffMap(int leftLayer, int rightLayer) {
    // Two 128-unit tiles side by side: x in [0,128) -> left, [128,256) -> right.
    W3EMap map;
    map.version = 11;
    map.tileset = 'A';
    map.customTilesets = 0;
    map.tileWidth = 2;
    map.tileHeight = 1;
    map.offsetX = 0.0f;
    map.offsetY = 0.0f;
    map.tiles.resize(2);
    map.tiles[0].cliffAndLayer = (uint8_t)(leftLayer & 0x0F);
    map.tiles[1].cliffAndLayer = (uint8_t)(rightLayer & 0x0F);
    return map;
}

void SetAlwaysMiss(CSimulation& sim) {
    CGameplayConstants gc;
    gc.chanceToMiss = 1.0f;
    sim.SetGameplayConstants(gc);
}

} // namespace

bool TestHighlandMissUphill() {
    // Retail uphill rule (ref CUnitDefaultAccuracyCheckListener): a weapon
    // attack from a lower cliff layer at a higher unit misses.
    CSimulation sim;
    SetAlwaysMiss(sim);
    W3EMap map = MakeCliffMap(0, 2);
    sim.SetTerrainMap(&map);
    TEST_ASSERT(sim.GetTerrainHeight(64.0f, 64.0f) == 0);
    TEST_ASSERT(sim.GetTerrainHeight(192.0f, 64.0f) == 2);
    CUnit attacker(31, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 64.0f, 64.0f, 0);
    CUnit target(32, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 192.0f, 64.0f, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(target.GetLife(), lifeBefore, 0.001f);
    // The splash/JASS path shares the rule (ref: the listener runs for
    // every attack, whichever damage entry point it funnels through).
    CUnit target2(33, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 192.0f, 64.0f, 0);
    const float dealt = sim.GetCombatSystem()->ApplyDamage(
        sim, &attacker, &target2, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(dealt, 0.0f, 0.001f);
    TEST_NEAR(target2.GetLife(), lifeBefore, 0.001f);
    return true;
}

bool TestHighlandNoMissDownhillFlatAndSpells() {
    CSimulation sim;
    SetAlwaysMiss(sim);
    W3EMap map = MakeCliffMap(0, 2);
    sim.SetTerrainMap(&map);
    // Downhill always lands, even with chance forced to 1.
    CUnit attacker(34, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 192.0f, 64.0f, 0);
    CUnit target(35, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 64.0f, 64.0f, 0);
    const float lifeBefore = target.GetLife();
    target.Damage(sim, &attacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(lifeBefore - target.GetLife(), 100.0f, 0.01f);
    // Flat ground lands too.
    CUnit flatAttacker(36, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 64.0f, 64.0f, 0);
    CUnit flatTarget(37, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 70.0f, 70.0f, 0);
    const float flatBefore = flatTarget.GetLife();
    flatTarget.Damage(sim, &flatAttacker, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(flatBefore - flatTarget.GetLife(), 100.0f, 0.01f);
    // Spells are not weapon attacks: uphill still lands.
    CUnit caster(38, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 64.0f, 64.0f, 0);
    CUnit victim(39, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 192.0f, 64.0f, 0);
    const float spellBefore = victim.GetLife();
    victim.Damage(sim, &caster, false, false, CAttackType::MAGIC, CDamageType::MAGIC, "", 100.0f, 0);
    TEST_ASSERT(spellBefore - victim.GetLife() > 0.0f);
    return true;
}

bool TestHighlandNoMissVsDestructableAndOffMap() {
    CSimulation sim;
    SetAlwaysMiss(sim);
    W3EMap map = MakeCliffMap(0, 2);
    sim.SetTerrainMap(&map);
    // Retail gates on CUnit targets: uphill at a tree still fells it.
    CUnit attacker(40, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 64.0f, 64.0f, 0);
    CDestructable tree(41, War3ID("LTlt"), 192.0f, 64.0f, 1000.0f);
    const float dealt = sim.GetCombatSystem()->ApplyDamage(
        sim, &attacker, &tree, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(dealt, 100.0f, 0.01f);
    // Mapless sim reports 999 everywhere (ref null corner): no height
    // diff, so even chance 1.0 lands.
    CSimulation flat;
    SetAlwaysMiss(flat);
    TEST_ASSERT(flat.GetTerrainHeight(0.0f, 0.0f) == 999);
    CUnit a2(42, MakeCombatType(CDefenseType::NONE, 0), 0, War3ID("hfoo"), 0.0f, 0.0f, 0);
    CUnit t2(43, MakeCombatType(CDefenseType::NONE, 0), 1, War3ID("hfoo"), 5000.0f, 0.0f, 0);
    const float before = t2.GetLife();
    t2.Damage(flat, &a2, true, false, CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_NEAR(before - t2.GetLife(), 100.0f, 0.01f);
    return true;
}

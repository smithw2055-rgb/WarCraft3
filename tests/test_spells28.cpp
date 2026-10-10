// Batch-28 tests: pairing, blight, harvest, cargo, waygate, shops, shade.
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/unitdata.h"
#include "ecs/abilities/ability_build.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/item_bonuses.h"
#include "map/wpm.h"
#include "ecs/abilities/ability_harvest.h"
#include "ecs/abilities/ability_inventory.h"
#include "ecs/abilities/ability_upgrade.h"
#include "ecs/abilities/spells25.h"
#include "ecs/behaviors/behavior.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/item.h"
#include "ecs/abilities/spells28.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

std::unique_ptr<CUnitType> MakeType(const char* id) {
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
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

CUnitType* SharedType() {
    static std::unique_ptr<CUnitType> keep = MakeType("hfoo");
    return keep.get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, SharedType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim) {
    for (int i = 0; i < 2; i++) sim.Update();
}

War3ID abilityCode(CPairMode mode) {
    switch (mode) {
    case CPairMode::MilitiaCall: return War3ID("Amic");
    case CPairMode::Militia: return War3ID("Amil");
    case CPairMode::Mount: return War3ID("Acoa");
    case CPairMode::LoadPartner: return War3ID("Acoh");
    case CPairMode::Couple: return War3ID("Acoi");
    }
    return War3ID("Amil");
}

CAbilityPairing* GivePair(CSimulation& sim, CUnit* unit, int handle, CPairMode mode) {
    auto ability = std::make_unique<CAbilityPairing>(handle, abilityCode(mode), mode);
    CAbilityPairing* raw = ability.get();
    unit->AddAbility(sim, std::move(ability));
    return raw;
}

} // namespace

bool TestMilitiaAndMountPairing() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hmil"), MakeType("hmil"));
    data.RegisterUnitType(War3ID("ehpr"), MakeType("ehpr"));
    data.RegisterUnitType(War3ID("ecop"), MakeType("ecop"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* hall = AdoptAt(sim, 2801, 0, 0, 0);
    auto* peasant = AdoptAt(sim, 2802, 0, 100, 0);
    auto* far = AdoptAt(sim, 2803, 0, 5000, 0);
    GivePair(sim, hall, 28901, CPairMode::MilitiaCall);
    auto* militia = GivePair(sim, peasant, 28902, CPairMode::Militia);
    GivePair(sim, far, 28903, CPairMode::Militia);
    Settle(sim);

    auto* call = dynamic_cast<CAbilityPairing*>(hall->GetAbility(War3ID("Amic")));
    TEST_ASSERT(call != nullptr);
    call->DoEffect(sim, hall);
    TEST_ASSERT(peasant->GetTypeId() == War3ID("hmil"));
    TEST_ASSERT(militia->IsConverted());
    TEST_ASSERT(far->GetTypeId() == War3ID("hfoo"));
    militia->DoEffect(sim, peasant);
    TEST_ASSERT(!peasant->IsMorphed());
    TEST_ASSERT(!militia->IsConverted());

    auto* mount = AdoptAt(sim, 2804, 0, 0, 0);
    auto* archer = AdoptAt(sim, 2805, 0, 40, 0);
    GivePair(sim, mount, 28904, CPairMode::LoadPartner);
    auto* climb = GivePair(sim, archer, 28905, CPairMode::Mount);
    Settle(sim);
    climb->DoEffect(sim, archer);
    TEST_ASSERT(archer->IsLoadedAboard());
    TEST_ASSERT(mount->GetCargoUnits().size() == 1);
    TEST_ASSERT(mount->GetTypeId() == War3ID("ehpr"));

    auto* left = AdoptAt(sim, 2806, 0, 0, 200);
    auto* right = AdoptAt(sim, 2807, 0, 40, 200);
    auto* couple = GivePair(sim, left, 28906, CPairMode::Couple);
    GivePair(sim, right, 28907, CPairMode::Couple);
    Settle(sim);
    couple->DoEffect(sim, left);
    TEST_ASSERT(left->GetTypeId() == War3ID("ecop"));
    TEST_ASSERT(right->IsDead());
    return true;
}

bool TestMilitiaReturnsCargoAndExpires() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hmil"), MakeType("hmil"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* hall = AdoptAt(sim, 2871, 0, 0, 0);
    auto* peasant = AdoptAt(sim, 2872, 0, 40, 0);
    GivePair(sim, hall, 28981, CPairMode::MilitiaCall);
    auto* militia = GivePair(sim, peasant, 28982, CPairMode::Militia);
    militia->SetMilitiaDuration(0.2f);
    auto harvestAbility = std::make_unique<CAbilityHarvest>(28983);
    CAbilityHarvest* harvest = harvestAbility.get();
    peasant->AddAbility(sim, std::move(harvestAbility));
    harvest->SetCarried(CResourceType::GOLD, 10);
    hall->AddAbility(sim, std::make_unique<CAbilityGoldMine>(28984));
    harvest->SetLastTargetHandle(hall->GetHandleId());
    Settle(sim);

    auto* call = dynamic_cast<CAbilityPairing*>(hall->GetAbility(War3ID("Amic")));
    call->DoEffect(sim, hall);
    TEST_ASSERT(peasant->GetTypeId() == War3ID("hmil"));
    TEST_ASSERT(peasant->GetBuff(War3ID("Bmil")) != nullptr);
    TEST_ASSERT(harvest->IsDisabled());
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 10);
    TEST_ASSERT(harvest->GetCarriedAmount() == 0);

    for (int i = 0; i < 20 && militia->IsConverted(); i++) peasant->Update(sim);
    TEST_ASSERT(!militia->IsConverted());
    TEST_ASSERT(!peasant->IsMorphed());
    TEST_ASSERT(!harvest->IsDisabled());
    peasant->Update(sim);
    TEST_ASSERT(peasant->GetCurrentBehavior() != nullptr);
    TEST_ASSERT(peasant->GetCurrentBehavior()->GetCategory() == CBehaviorCategory::HARVEST);
    return true;
}

bool TestUpgradeAndPowerup() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hkee"), MakeType("hkee"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.GetPlayer(0)->AddGold(1000);
    sim.GetPlayer(0)->AddLumber(1000);
    auto* hall = AdoptAt(sim, 2881, 0, 0, 0);
    CAbilityUpgrade upgrade(28991);
    upgrade.AddUpgradeTo(War3ID("hkee"));
    TEST_ASSERT(upgrade.BeginNoTarget(sim, hall, static_cast<int>(War3ID("hkee").value)) == nullptr);
    TEST_ASSERT(upgrade.IsUpgrading());
    TEST_ASSERT(hall->IsConstructing());
    for (int i = 0; i < 800 && upgrade.IsUpgrading(); i++) upgrade.OnTick(sim, hall);
    TEST_ASSERT(!upgrade.IsUpgrading());
    TEST_ASSERT(hall->GetTypeId() == War3ID("hkee"));

    auto* bearer = AdoptAt(sim, 2882, 0, 10, 0);
    const float life = bearer->GetMaxLife();
    CAbilityInventory inventory(28992);
    CItem tome(28993, War3ID("tkno"), 0, 0);
    tome.SetPowerup(true);
    tome.AddAbility(War3ID("AImi"));
    TEST_ASSERT(inventory.PickUpItem(sim, bearer, &tome, 0));
    TEST_ASSERT(bearer->GetMaxLife() == life + 50.0f);
    return true;
}

bool TestItemPickupAppliesBonuses() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* bearer = AdoptAt(sim, 2883, 0, 0, 0);
    const int before = bearer->GetBonusDamage();
    CAbilityInventory inventory(28994);
    CItem item(28995, War3ID("ratc"), 0, 0);
    item.SetBonuses({{CItemBonusType::DAMAGE, 7}});
    TEST_ASSERT(inventory.PickUpItem(sim, bearer, &item, 0));
    TEST_ASSERT(bearer->GetBonusDamage() == before + 7);
    TEST_ASSERT(inventory.DropItem(sim, bearer, 0));
    TEST_ASSERT(bearer->GetBonusDamage() == before);
    return true;
}

bool TestBuildRejectsUnbuildableCell() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("hbar"), MakeType("hbar"));
    sim.SetUnitData(&data);
    WpmData wpm;
    wpm.width = 16;
    wpm.height = 16;
    wpm.pathing.assign(256, 0);
    wpm.pathing[2 + 2 * 16] = WpmData::UNBUILDABLE;
    sim.SetPathingMap(&wpm, 0.0f, 0.0f);
    TEST_ASSERT(CAbilityBuild::IsBuildLocationObstructed(sim, 80.0f, 80.0f, 0, War3ID("hbar")));
    TEST_ASSERT(!CAbilityBuild::IsBuildLocationObstructed(sim, 400.0f, 400.0f, 0, War3ID("hbar")));

    CUnitType* type = data.GetUnitType(War3ID("hbar"));
    TEST_ASSERT(type != nullptr);
    type->SetRequiredPathingTypes({CBuildingPathingType::BLIGHTED});
    for (int cy = 11; cy <= 13; ++cy) {
        for (int cx = 11; cx <= 13; ++cx) {
            wpm.pathing[(size_t)cy * 16u + (size_t)cx] = WpmData::BLIGHTED;
        }
    }
    TEST_ASSERT(!CAbilityBuild::IsBuildLocationObstructed(sim, 400.0f, 400.0f, 0, War3ID("hbar")));
    wpm.pathing[11 + 11 * 16] = 0;
    TEST_ASSERT(CAbilityBuild::IsBuildLocationObstructed(sim, 400.0f, 400.0f, 0, War3ID("hbar")));
    return true;
}

bool TestBlightAndHauntedMine() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* necropolis = AdoptAt(sim, 2811, 0, 0, 0);
    CAbilityBlight blight(28911);
    blight.OnAdd(sim, necropolis);
    for (int i = 0; i < 20; i++) blight.OnTick(sim, necropolis);
    TEST_ASSERT(sim.IsPointBlighted(0, 0));
    TEST_ASSERT(!sim.IsPointBlighted(5000, 0));
    TEST_ASSERT(blight.GetCurrentRadius() > 0.0f);

    auto* mineUnit = AdoptAt(sim, 2812, 0, 200, 0);
    auto* acolyte = AdoptAt(sim, 2813, 0, 220, 0);
    mineUnit->AddAbility(sim, std::make_unique<CAbilityBlightedGoldMine>(28913));
    auto* live = dynamic_cast<CAbilityBlightedGoldMine*>(mineUnit->GetAbility(War3ID("Abgm")));
    TEST_ASSERT(live != nullptr);
    live->SetGold(100);
    CAbilityAcolyteHarvest harvest(28914);
    harvest.DoEffect(sim, acolyte, mineUnit);
    TEST_ASSERT(live->GetMinerCount() == 1);
    CPlayer* player = sim.GetPlayer(0);
    for (int i = 0; i < 200 && player->GetGold() == 0; i++) live->OnTick(sim, mineUnit);
    TEST_ASSERT(player->GetGold() == 10);
    TEST_ASSERT(live->GetGold() == 90);

    auto overlay = AbilityFactory::CreateAbility(28915, War3ID("Agl2"));
    TEST_ASSERT(dynamic_cast<CAbilityOverlayedGoldMine*>(overlay.get()) != nullptr);
    return true;
}

bool TestWispAndLumberHarvest() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* wisp = AdoptAt(sim, 2821, 0, 0, 0);
    auto* chopper = AdoptAt(sim, 2822, 0, 30, 0);
    CDestructable* tree = sim.CreateDestructable(War3ID("LTlt"), 10, 0, 100.0f);
    CDestructable* tree2 = sim.CreateDestructable(War3ID("LTlt"), 40, 0, 100.0f);
    TEST_ASSERT(tree != nullptr && tree2 != nullptr);
    const float treeLife = tree->GetLife();
    CAbilityWispHarvest gather(28921);
    gather.DoEffect(sim, wisp, tree);
    gather.OnTick(sim, wisp);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 5);
    TEST_ASSERT(tree->GetLife() < treeLife);

    const float before = tree2->GetLife();
    CAbilityHarvestLumber lumber(28922);
    lumber.DoEffect(sim, chopper, tree2);
    TEST_ASSERT(tree2->GetLife() < before);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 15);
    return true;
}

bool TestAcolyteBindingPersistsAndReleasesOnDeath() {
    // ref CBehaviorAcolyteHarvest: bound indefinitely (no timeout) until
    // death/removal/rebind. The old 30s auto-unbind silently slowed the
    // mine trickle while the acolyte kept standing there.
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* mineUnit = AdoptAt(sim, 2851, 0, 200, 0);
    mineUnit->AddAbility(sim, std::make_unique<CAbilityBlightedGoldMine>(28953));
    auto* live = dynamic_cast<CAbilityBlightedGoldMine*>(mineUnit->GetAbility(War3ID("Abgm")));
    TEST_ASSERT(live != nullptr);
    live->SetGold(100000);
    auto* acolyte = AdoptAt(sim, 2852, 0, 220, 0);
    acolyte->AddAbility(sim, std::make_unique<CAbilityAcolyteHarvest>(28954));
    auto* harvest = dynamic_cast<CAbilityAcolyteHarvest*>(acolyte->GetAbility(War3ID("Aaha")));
    TEST_ASSERT(harvest != nullptr);
    harvest->DoEffect(sim, acolyte, mineUnit);
    TEST_ASSERT(harvest->IsBound());
    TEST_ASSERT(live->GetMinerCount() == 1);

    // 40s of mine ticks (past the old 30s timeout): still bound, still paid.
    const int goldBefore = sim.GetPlayer(0)->GetGold();
    for (int i = 0; i < 800; i++) live->OnTick(sim, mineUnit);
    TEST_ASSERT(live->GetMinerCount() == 1);
    TEST_ASSERT(harvest->IsBound());
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() > goldBefore);

    // Lethal damage dispatches OnDeath on the next Update -> slot released.
    acolyte->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::NORMAL,
                    "", 1e9f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(live->GetMinerCount() == 0);
    TEST_ASSERT(!harvest->IsBound());
    return true;
}

bool TestWispJumpsOntoTreeAndDetachesOnDeath() {
    // ref CBehaviorWispHarvest: instant jump onto the tree, trickle until
    // the tree dies, then detach (handle cleared, no dangling pointer).
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* wisp = AdoptAt(sim, 2861, 0, 0, 0);
    CDestructable* tree = sim.CreateDestructable(War3ID("LTlt"), 100, 0, 100.0f);
    TEST_ASSERT(tree != nullptr);
    CAbilityWispHarvest gather(28961);
    gather.DoEffect(sim, wisp, tree);
    TEST_ASSERT(wisp->GetX() == tree->GetX() && wisp->GetY() == tree->GetY());
    TEST_ASSERT(gather.GetBoundTreeHandle() == tree->GetHandleId());
    gather.OnTick(sim, wisp);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == 5);

    // Fell the tree: binding clears and trickling stops.
    tree->Damage(sim, nullptr, false, false, CAttackType::NORMAL, CDamageType::NORMAL,
                 "", 1e9f, 0);
    TEST_ASSERT(tree->IsDead());
    const int lumberBefore = sim.GetPlayer(0)->GetLumber();
    gather.OnTick(sim, wisp);
    TEST_ASSERT(gather.GetBoundTreeHandle() == -1);
    TEST_ASSERT(sim.GetPlayer(0)->GetLumber() == lumberBefore);
    return true;
}

bool TestGoldMineCapacityMatchesRetail() {
    // Retail Agld DataC1 = 5 miners; the old hardcoded 3 starved the 4th/5th
    // worker. Both AddMiner and CanAddMiner enforce the cap.
    CAbilityGoldMine mine(29011);
    TEST_ASSERT(mine.GetMiningCapacity() == 5);
    CBehaviorHarvest m1, m2, m3, m4, m5, m6;
    mine.AddMiner(&m1);
    mine.AddMiner(&m2);
    mine.AddMiner(&m3);
    mine.AddMiner(&m4);
    mine.AddMiner(&m5);
    TEST_ASSERT(mine.GetActiveMinerCount() == 5);
    TEST_ASSERT(!mine.CanAddMiner());
    mine.AddMiner(&m6); // refused
    TEST_ASSERT(mine.GetActiveMinerCount() == 5);
    mine.RemoveMiner(&m1);
    TEST_ASSERT(mine.CanAddMiner());
    mine.AddMiner(&m6);
    TEST_ASSERT(mine.GetActiveMinerCount() == 5);
    return true;
}

bool TestCargoBurrowAndDrop() {
    CSimulation sim;
    auto* holder = AdoptAt(sim, 2831, 0, 0, 0);
    auto* peon = AdoptAt(sim, 2832, 0, 10, 0);
    auto* extra = AdoptAt(sim, 2833, 0, 20, 0);
    CAbilityCargoHold cargo(28931, War3ID("Acar"), false);
    cargo.OnAdd(sim, holder);
    TEST_ASSERT(cargo.TryLoad(holder, peon));
    TEST_ASSERT(peon->IsLoadedAboard());
    TEST_ASSERT(peon->IsHidden());
    for (int i = 0; i < cargo.GetCapacity() - 1; i++) {
        auto* filler = AdoptAt(sim, 2840 + i, 0, 30.0f + i, 0);
        TEST_ASSERT(cargo.TryLoad(holder, filler));
    }
    TEST_ASSERT(!cargo.TryLoad(holder, extra));
    CAbilityDropInstant drop(28932, War3ID("Adri"));
    holder->AddAbility(sim, std::make_unique<CAbilityCargoHold>(28933, War3ID("Acar"), false));
    // The drop looks up the ability on the unit, which is empty. Unload
    // the local hold first, then exercise the unit-owned hold.
    cargo.UnloadAll(holder);
    TEST_ASSERT(!peon->IsLoadedAboard());
    TEST_ASSERT(!peon->IsHidden());

    auto* burrow = AdoptAt(sim, 2834, 0, 100, 0);
    auto* worker = AdoptAt(sim, 2835, 0, 110, 0);
    auto hold = std::make_unique<CAbilityCargoHold>(28934, War3ID("Abun"), true);
    CAbilityCargoHold* burrowHold = hold.get();
    burrow->AddAbility(sim, std::move(hold));
    TEST_ASSERT(burrow->AreAttacksDisabled());
    TEST_ASSERT(burrowHold->TryLoad(burrow, worker));
    TEST_ASSERT(!burrow->AreAttacksDisabled());
    TEST_ASSERT(std::fabs(burrow->GetAttackSpeedMultiplier() - 2.0f) < 0.01f);
    CAbilityDropInstant stand(28935, War3ID("Astd"));
    stand.DoEffect(sim, burrow);
    TEST_ASSERT(!worker->IsLoadedAboard());
    TEST_ASSERT(burrow->AreAttacksDisabled());
    TEST_ASSERT(std::fabs(burrow->GetAttackSpeedMultiplier() - 1.0f) < 0.01f);
    return true;
}

bool TestWaygateAndShops() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* gateUnit = AdoptAt(sim, 2851, 0, 0, 0);
    auto* walker = AdoptAt(sim, 2852, 0, 20, 0);
    Settle(sim);
    auto* outsider = AdoptAt(sim, 2856, 0, 400, 0);
    CAbilityWayGate gate(28951);
    gate.SetDestination(gateUnit, 1000, 1000);
    gate.Begin(sim, gateUnit, OrderIds::SMART, outsider);
    TEST_ASSERT(std::fabs(outsider->GetX() - 400.0f) < 0.01f);
    gate.Begin(sim, gateUnit, OrderIds::SMART, walker);
    TEST_ASSERT(std::fabs(walker->GetX() - 1000.0f) < 0.01f);
    TEST_ASSERT(std::fabs(walker->GetY() - 1000.0f) < 0.01f);
    TEST_ASSERT(gateUnit->IsWaygateActive());

    auto* shop = AdoptAt(sim, 2853, 0, 0, 500);
    auto* ally = AdoptAt(sim, 2854, 0, 40, 500);
    auto* foe = AdoptAt(sim, 2855, 1, 10, 500);
    auto* closer = AdoptAt(sim, 2857, 0, 5, 500);
    Settle(sim);
    CAbilityNeutralBuilding shared(28952, War3ID("Aall"), true);
    shared.OnTick(sim, shop);
    TEST_ASSERT(shared.GetSelectedUnit(0) == ally);
    TEST_ASSERT(shared.GetSelectedUnit(1) == nullptr);
    CAbilityNeutralBuilding any(28953, War3ID("Aneu"), false);
    any.OnTick(sim, shop);
    TEST_ASSERT(any.GetSelectedUnit(0) == ally);
    TEST_ASSERT(any.GetSelectedUnit(1) == foe);
    for (int i = 0; i < 80; i++) any.OnTick(sim, shop);
    TEST_ASSERT(any.GetSelectedUnit(0) == ally);
    any.Begin(sim, shop, OrderIds::NEUTRALINTERACT, closer);
    TEST_ASSERT(any.GetSelectedUnit(0) == closer);
    auto purchase = AbilityFactory::CreateAbility(28954, War3ID("Apit"));
    TEST_ASSERT(purchase != nullptr);
    TEST_ASSERT(purchase->GetCode() == War3ID("Apit"));
    return true;
}

bool TestSacrificeSummonsShade() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("ushd"), MakeType("ushd"));
    sim.SetUnitData(&data);
    auto* pit = AdoptAt(sim, 2861, 0, 0, 0);
    auto* victim = AdoptAt(sim, 2862, 0, 30, 0);
    CAbilitySacrifice sacrifice(28961);
    sacrifice.DoEffect(sim, pit, victim);
    TEST_ASSERT(victim->IsDead());
    Settle(sim);
    int shades = 0;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && unit->GetTypeId() == War3ID("ushd") && !unit->IsDead()) shades++;
    }
    TEST_ASSERT(shades == 1);
    return true;
}

bool TestSpells28FactoryCoverage() {
    const char* codes[] = {
        "Amic", "Amil", "Acoa", "Acoh", "Acoi",
        "Abli", "Abgm", "Agl2", "Aaha", "Awha", "Ahrl",
        "Acar", "Abun", "Adri", "Astd", "Awrp",
        "Apit", "Aneu", "Aall"
    };
    for (const char* code : codes) {
        auto ability = AbilityFactory::CreateAbility(28970, War3ID(code));
        if (!ability) {
            std::cerr << "missing factory ability " << code << "\n";
            return false;
        }
    }
    return true;
}

bool TestSpells28OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amic")) == 852071);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Amic"), 852651));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Amil")) == 852072);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Amil"), 852073));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acoa")) == 852143);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acoh")) == 852142);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acoi")) == 852508);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aaha")) == 852185);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Awha")) == 852214);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Ahrl")) == 852022);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Adri")) == 852049);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Astd")) == 852113);
    return true;
}

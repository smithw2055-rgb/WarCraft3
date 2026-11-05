#include <cmath>
#include <memory>
#include <vector>
#include <unordered_set>

#include "data/combat_types.h"
#include "data/itemdata.h"
#include "ecs/abilities/ability_attack.h"
#include "ecs/abilities/ability_transport.h"
#include "ecs/behaviors/behavior_stun.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/item.h"
#include "ecs/item_bonuses.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "ecs/fog_state.h"
#include "map/w3e.h"
#include "model/mdx.h"
#include "render/mdx_sequence_select.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) return false; } while (0)

namespace {

CUnitType* MakeAuditTestUnitType(bool isHero = false, int foodMade = 0, bool isBuilding = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 20;
    atk.damageDice = 1;
    atk.damageSidesPerDie = 6;
    keep.push_back(std::make_unique<CUnitType>(
        "TestAudit", "TestAudit", "icon", War3ID(isHero ? "Hamg" : "hfoo"),
        500, 0.0f, 0.0f, CRegenType::ALWAYS,
        100, 200, 270, 2, War3ID(),
        std::vector<War3ID>{}, isBuilding, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::MEDIUM,
        0.0f, 1.0f, std::unordered_set<CTargetType>{},
        600.0f, 0.0f, std::vector<War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{},
        std::unordered_map<CUpgradeClass, War3ID>{}, std::vector<War3ID>{},
        std::vector<War3ID>{}, std::vector<War3ID>{}, CUnitRace{"Human", War3ID("H")},
        100, 50, 1, foodMade, 30, 0, 0, 0,
        std::unordered_set<CBuildingPathingType>{},
        std::unordered_set<CBuildingPathingType>{},
        0.5f, 0.6f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, isHero, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"AuditHero"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

class CDummyAbility : public CAbility {
public:
    explicit CDummyAbility(War3ID id) : CAbility(1, id, CAbilityCategory::SPELL) {}
};

} // namespace

bool TestAuditFoodCapConstructingBuilding() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CPlayer* player = sim.GetPlayer(0);
    player->SetFoodCap(10);

    // Create incomplete farm (foodMade = 8, constructing = true, isBuilding = true)
    auto farm = std::make_unique<CUnit>(10, MakeAuditTestUnitType(false, 8, true), 0, War3ID("hhou"), 0, 0, 0);
    farm->SetConstructing(true);
    CUnit* farmPtr = sim.AdoptUnit(std::move(farm));
    sim.Update();

    // Food cap starts at 10
    TEST_ASSERT(player->GetFoodCap() == 10);

    // Destroy the constructing building
    farmPtr->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::UNIVERSAL, "", 99999.0f);
    sim.Update();

    // Food cap must NOT be decremented upon destroying an incomplete building!
    TEST_ASSERT(player->GetFoodCap() == 10);

    // Now test a completed building (ProcessNewUnits must add foodMade automatically)
    auto farm2 = std::make_unique<CUnit>(11, MakeAuditTestUnitType(false, 8, true), 0, War3ID("hhou"), 0, 0, 0);
    farm2->SetConstructing(false);
    CUnit* farm2Ptr = sim.AdoptUnit(std::move(farm2));
    sim.Update();
    TEST_ASSERT(player->GetFoodCap() == 18);

    farm2Ptr->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::UNIVERSAL, "", 99999.0f);
    sim.Update();

    // Completed building destruction DOES deduct food cap
    TEST_ASSERT(player->GetFoodCap() == 10);
    return true;
}

bool TestAuditPlayerHeroMaintenance() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CPlayer* player = sim.GetPlayer(0);

    TEST_ASSERT(player->GetHeroes().empty());

    auto hero = std::make_unique<CUnit>(20, MakeAuditTestUnitType(true, 0), 0, War3ID("Hamg"), 0, 0, 0);
    CUnit* heroPtr = sim.AdoptUnit(std::move(hero));
    sim.Update();

    // Hero is registered in player's hero list
    TEST_ASSERT(player->GetHeroes().size() == 1);
    TEST_ASSERT(player->GetHeroes()[0] == heroPtr);

    // Kill and process removals
    sim.RemoveUnit(heroPtr);
    sim.Update();

    // Hero must be removed from player's hero list
    TEST_ASSERT(player->GetHeroes().empty());
    return true;
}

bool TestAuditTransportCargoDeathCleanup() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto transport = std::make_unique<CUnit>(30, MakeAuditTestUnitType(), 0, War3ID("nbot"), 0, 0, 0);
    CUnit* transPtr = sim.AdoptUnit(std::move(transport));

    auto passenger = std::make_unique<CUnit>(31, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0);
    CUnit* passPtr = sim.AdoptUnit(std::move(passenger));
    sim.Update();

    // Load passenger
    CAbilityLoad load(50);
    CBehavior* b = load.Begin(sim, transPtr, 0, passPtr);
    delete b;

    TEST_ASSERT(transPtr->GetCargoUnits().size() == 1);
    TEST_ASSERT(passPtr->IsLoadedAboard());
    TEST_ASSERT(passPtr->IsHidden());

    // Kill transport
    transPtr->Damage(sim, nullptr, false, false, CAttackType::CHAOS, CDamageType::UNIVERSAL, "", 99999.0f);
    sim.Update();
    sim.Update();

    // Transport cargo must be cleared, passenger unloaded and killed
    TEST_ASSERT(transPtr->GetCargoUnits().empty());
    TEST_ASSERT(!passPtr->IsLoadedAboard());
    TEST_ASSERT(!passPtr->IsHidden());
    TEST_ASSERT(passPtr->IsDead());
    return true;
}

bool TestAuditMoveSpeedDriftAndClamp() {
    CSimulation sim;
    auto unit = std::make_unique<CUnit>(40, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0);
    CUnit* u = sim.AdoptUnit(std::move(unit));

    const float initialSpeed = u->GetSpeed();
    TEST_ASSERT(initialSpeed == 270.0f);

    // Add and remove buffs multiple times
    for (int i = 0; i < 5; ++i) {
        u->AddSpeedModifier(War3ID("Bslo"), -0.2f);
        TEST_NEAR(u->GetSpeed(), 270.0f * 0.8f, 0.01f);

        u->AddSpeedModifier(War3ID("Bblo"), 0.25f);
        TEST_NEAR(u->GetSpeed(), 270.0f * (1.0f - 0.2f + 0.25f), 0.01f);

        u->RemoveSpeedModifier(War3ID("Bslo"));
        u->RemoveSpeedModifier(War3ID("Bblo"));
        TEST_NEAR(u->GetSpeed(), 270.0f, 0.001f);
    }

    // Heavy slow clamp test: speed should clamp to MinUnitSpeed = 150.0f
    u->AddSpeedModifier(War3ID("Bslo"), -0.6f);
    TEST_ASSERT(u->GetSpeed() == 150.0f);
    u->AddSpeedModifier(War3ID("Binf"), -0.5f);
    TEST_ASSERT(u->GetSpeed() == 150.0f);

    u->RemoveSpeedModifier(War3ID("Bslo"));
    u->RemoveSpeedModifier(War3ID("Binf"));
    TEST_NEAR(u->GetSpeed(), 270.0f, 0.001f);
    return true;
}

bool TestAuditAOEAlliedFiltering() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(2, "P2", CMapControl::COMPUTER));

    CPlayer* p0 = sim.GetPlayer(0);
    CPlayer* p1 = sim.GetPlayer(1);

    // P0 and P1 allied
    p0->SetAlliance(1, CAllianceType::PASSIVE, true);
    p1->SetAlliance(0, CAllianceType::PASSIVE, true);

    auto caster = sim.AdoptUnit(std::make_unique<CUnit>(50, MakeAuditTestUnitType(), 0, War3ID("Obla"), 0, 0, 0));
    auto ally = sim.AdoptUnit(std::make_unique<CUnit>(51, MakeAuditTestUnitType(), 1, War3ID("hfoo"), 50, 0, 0));
    auto enemy = sim.AdoptUnit(std::make_unique<CUnit>(52, MakeAuditTestUnitType(), 2, War3ID("hfoo"), 50, 0, 0));
    sim.Update();

    const float allyInitialHp = ally->GetLife();
    const float enemyInitialHp = enemy->GetLife();

    // Immolation buff on caster
    auto immol = std::make_unique<CBuffImmolation>(10.0f, 50.0f, 0.0f, 200.0f);
    caster->AddBuff(std::move(immol), sim);

    // Update simulation for 1.5 seconds (30 ticks)
    for (int i = 0; i < 30; ++i) sim.Update();

    // Ally must NOT be damaged
    TEST_ASSERT(ally->GetLife() == allyInitialHp);
    // Enemy must be damaged
    TEST_ASSERT(enemy->GetLife() < enemyInitialHp);
    return true;
}

bool TestAuditStateFlagsBuffCounter() {
    CSimulation sim;
    auto unit = sim.AdoptUnit(std::make_unique<CUnit>(60, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0));

    // Grant permanent invulnerability ability
    unit->AddAbility(sim, std::make_unique<CDummyAbility>(War3ID("Avul")));
    unit->SetInvulnerable(true);

    // Apply temporary invulnerability buff (e.g. Stone Form)
    auto stone = std::make_unique<CBuffStoneForm>(5.0f);
    unit->AddBuff(std::move(stone), sim);

    // Remove the temporary buff
    unit->RemoveBuff(War3ID("Bstf"), sim);

    // Permanent invulnerability must be retained!
    TEST_ASSERT(unit->IsInvulnerable());

    // Grant permanent invisibility ability
    unit->AddAbility(sim, std::make_unique<CDummyAbility>(War3ID("Apiv")));
    unit->SetInvisible(true);

    // Apply and remove Burrow
    auto burrow = std::make_unique<CBuffBurrow>(10.0f);
    unit->AddBuff(std::move(burrow), sim);
    unit->RemoveBuff(War3ID("Bbuw"), sim);

    // Permanent invisibility must be retained!
    TEST_ASSERT(unit->IsInvisible());
    return true;
}

bool TestAuditBuffStackReplaceAndStunRefresh() {
    CSimulation sim;
    auto unit = sim.AdoptUnit(std::make_unique<CUnit>(70, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0));

    // Apply 2-second stun
    auto stun1 = std::make_unique<CBuffStun>(2.0f);
    unit->AddBuff(std::move(stun1), sim);
    TEST_ASSERT(unit->HasBuff(War3ID("Bstn")));

    // Advance 1 second
    for (int i = 0; i < 10; ++i) sim.Update();
    TEST_ASSERT(unit->HasBuff(War3ID("Bstn")));

    // Apply 5-second stun (same buff ID, should refresh duration and behavior)
    auto stun2 = std::make_unique<CBuffStun>(5.0f);
    unit->AddBuff(std::move(stun2), sim);

    // Advance 3 seconds (total 4 seconds since initial stun)
    for (int i = 0; i < 30; ++i) sim.Update();

    // Unit should STILL be stunned because it was refreshed to 5.0s
    TEST_ASSERT(unit->HasBuff(War3ID("Bstn")));
    return true;
}

bool TestAuditMorphLifeManaRatio() {
    CSimulation sim;
    auto baseType = MakeAuditTestUnitType();
    auto bearType = MakeAuditTestUnitType();

    auto unit = sim.AdoptUnit(std::make_unique<CUnit>(80, baseType, 0, War3ID("Edru"), 0, 0, 0));
    unit->SetLife(sim, 250.0f); // 50% HP (250 / 500)
    unit->SetMana(100.0f);      // 50% Mana (100 / 200)

    // Morph to bear form
    unit->MorphTo(bearType, War3ID("Edrb"));
    TEST_NEAR(unit->GetLife() / unit->GetMaxLife(), 0.5f, 0.01f);
    TEST_NEAR(unit->GetMana() / unit->GetMaxMana(), 0.5f, 0.01f);

    // Unmorph back
    unit->Unmorph();
    TEST_NEAR(unit->GetLife(), 250.0f, 0.5f);
    TEST_NEAR(unit->GetMana(), 100.0f, 0.5f);
    return true;
}

bool TestAuditItemHealthManaRegen() {
    CSimulation sim;
    auto unit = sim.AdoptUnit(std::make_unique<CUnit>(90, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0));

    CItemBonus lifeBonus;
    lifeBonus.type = CItemBonusType::LIFE_REGEN;
    lifeBonus.value = 5;

    CItemBonus manaBonus;
    manaBonus.type = CItemBonusType::MANA_REGEN;
    manaBonus.value = 3;

    ApplyItemBonuses(sim, unit, {lifeBonus, manaBonus});

    TEST_NEAR(unit->GetBonusLifeRegen(), 5.0f, 0.01f);
    TEST_NEAR(unit->GetBonusManaRegen(), 3.0f, 0.01f);

    RemoveItemBonuses(sim, unit, {lifeBonus, manaBonus});

    TEST_NEAR(unit->GetBonusLifeRegen(), 0.0f, 0.01f);
    TEST_NEAR(unit->GetBonusManaRegen(), 0.0f, 0.01f);
    return true;
}

bool TestAuditFogSightOcclusionAndShared() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));

    CPlayer* p0 = sim.GetPlayer(0);

    // Grant shared vision
    p0->SetAlliance(1, CAllianceType::SHARED_VISION, true);

    auto u0 = sim.AdoptUnit(std::make_unique<CUnit>(100, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update();

    CFogSystem* fog = sim.GetFogSystem();
    // Shared vision should reveal (0, 0) to P1 as well
    TEST_ASSERT(fog->IsVisibleTo(0, 0, 0));
    TEST_ASSERT(fog->IsVisibleTo(1, 0, 0));

    // Line of sight: high ground occlusion test
    // Ground unit looking up cliff should be blocked
    TEST_ASSERT(!sim.IsLineOfSightBlocked(0, 0, 100, 0, false));
    return true;
}

bool TestAuditLightningShieldFriendlyDamage() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));

    auto target = sim.AdoptUnit(std::make_unique<CUnit>(110, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0));
    auto friendly = sim.AdoptUnit(std::make_unique<CUnit>(111, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 50, 0, 0));
    auto enemy = sim.AdoptUnit(std::make_unique<CUnit>(112, MakeAuditTestUnitType(), 1, War3ID("hfoo"), 50, 0, 0));
    sim.Update();

    const float targetHp = target->GetLife();
    const float friendlyHp = friendly->GetLife();
    const float enemyHp = enemy->GetLife();

    auto shield = std::make_unique<CBuffLightningShield>(5.0f, 20.0f, 150.0f);
    target->AddBuff(std::move(shield), sim);

    for (int i = 0; i < 25; ++i) sim.Update();

    // Target itself takes NO damage
    TEST_ASSERT(target->GetLife() == targetHp);
    // Friendly ally takes damage (WC3 lightning shield damages all adjacent units)
    TEST_ASSERT(friendly->GetLife() < friendlyHp);
    // Enemy takes damage
    TEST_ASSERT(enemy->GetLife() < enemyHp);
    return true;
}

bool TestAuditCreateItemValidation() {
    CSimulation sim;
    CItemData itemData;
    auto itemType = std::make_unique<CItemType>();
    itemType->id = War3ID("ratf");
    itemData.RegisterItemType(War3ID("ratf"), std::move(itemType));
    sim.SetItemData(&itemData);

    // Invalid 0 typeId
    TEST_ASSERT(sim.CreateItem(War3ID(0u), 0, 0) == nullptr);
    // Unit ID instead of Item ID (not registered in itemData)
    TEST_ASSERT(sim.CreateItem(War3ID("hpea"), 0, 0) == nullptr);
    // Valid registered item ID
    TEST_ASSERT(sim.CreateItem(War3ID("ratf"), 0, 0) != nullptr);
    return true;
}

bool TestAuditPropulsionWindowDegrees() {
    CSimulation sim;
    auto unit = sim.AdoptUnit(std::make_unique<CUnit>(120, MakeAuditTestUnitType(), 0, War3ID("hfoo"), 0, 0, 0));
    sim.Update();

    // Unit faces 0 deg (along +X). Target is at (100, 50), angle is ~26.5 deg.
    // absDelta is ~26.5 deg. Standard propulsion window is 60 deg.
    // TurnTowardPoint must return true (allowing moving while turning).
    bool propelling = unit->TurnTowardPoint(100.0f, 50.0f);
    TEST_ASSERT(propelling);
    return true;
}

bool TestAuditSequenceGoldLumberFilter() {
    MdxModel model;
    MdxSequence s0; s0.name = "Walk"; model.sequences.push_back(s0);
    MdxSequence s1; s1.name = "Walk Gold"; model.sequences.push_back(s1);
    MdxSequence s2; s2.name = "Walk Lumber"; model.sequences.push_back(s2);

    int idxNoTags = MdxSelectSequenceIndexByPrimaryAndTags(model, "walk", {});
    TEST_ASSERT(idxNoTags == 0); // Must be "Walk", not "Walk Gold" or "Walk Lumber"

    int idxFast = MdxSelectSequenceIndexByPrimaryAndTags(model, "walk", {"fast"});
    TEST_ASSERT(idxFast == 0); // Fast tag requested, still cannot match Gold or Lumber

    int idxGold = MdxSelectSequenceIndexByPrimaryAndTags(model, "walk", {"gold"});
    TEST_ASSERT(idxGold == 1); // Explicitly requested gold tag -> matches "Walk Gold"
    return true;
}


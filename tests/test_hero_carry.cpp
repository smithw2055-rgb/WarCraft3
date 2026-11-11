// Hero carry-over tests (retail ChangeLevel): snapshot preserves level,
// XP, skill points, learned skills and inventory; respawn restores them
// into a fresh sim; dead/computer-owned heroes stay behind.
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/ability_inventory.h"
#include "game/war3/abilities/war3_spells.h"
#include "ecs/csimulation.h"
#include "ecs/hero_carry.h"
#include "ecs/item.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeHeroType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Archmage", "Archmage", "icon", War3ID("Hamg"),
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
        1, true, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::INTELLIGENCE, std::vector<War3ID>{},
        std::vector<std::string>{"Jaina"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

void RegisterHeroType(CUnitData& data) {
    // Fresh CUnitType per registration (RegisterUnitType takes ownership).
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    auto type = std::make_unique<CUnitType>(
        "Archmage", "Archmage", "icon", War3ID("Hamg"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{War3ID("AInv")}, false, PathingGridMovementType::FOOT,
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
        1, true, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::INTELLIGENCE, std::vector<War3ID>{},
        std::vector<std::string>{"Jaina"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false);
    data.RegisterUnitType(War3ID("Hamg"), std::move(type));
}

CUnit* BuildHero(CSimulation& sim, int handle, int player) {
    auto* unit = sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeHeroType(), player,
                                                       War3ID("Hamg"), 0, 0, 0));
    // Configure AFTER AddAbility: OnAdd resets level/XP/skill points.
    unit->AddAbility(sim, std::make_unique<CAbilityHero>(handle + 1000));
    for (CAbility* ab : unit->GetAbilities()) {
        if (auto* heroAb = dynamic_cast<CAbilityHero*>(ab)) {
            heroAb->AddLearnableSkill(War3ID("AOcr"));
            heroAb->SetSkillPoints(3);
            break;
        }
    }
    unit->AddAbility(sim, std::make_unique<CAbilityInventory>(handle + 2000));
    return unit;
}

} // namespace

bool TestHeroCarryRoundTrip() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnit* hero = BuildHero(sim, 701, 0);
    auto* heroAb = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
    if (!heroAb) {
        for (CAbility* ab : hero->GetAbilities()) {
            heroAb = dynamic_cast<CAbilityHero*>(ab);
            if (heroAb) break;
        }
    }
    TEST_ASSERT(heroAb != nullptr);
    TEST_ASSERT(heroAb->SelectHeroSkill(sim, hero, War3ID("AOcr")));
    TEST_ASSERT(heroAb->SelectHeroSkill(sim, hero, War3ID("AOcr")));
    heroAb->SetHeroLevel(sim, hero, 3);
    heroAb->SetSkillPoints(5);
    heroAb->SetProperName("Jaina Proudmoore");
    auto* inv = dynamic_cast<CAbilityInventory*>(hero->GetAbility(War3ID("AInv")));
    TEST_ASSERT(inv != nullptr);
    CItem* item = sim.CreateItem(War3ID("I000"), 0, 0);
    TEST_ASSERT(item != nullptr);
    item->SetCharges(7);
    TEST_ASSERT(inv->PickUpItem(sim, hero, item));

    CarriedHero snap = SnapshotHero(hero, sim);
    TEST_ASSERT(snap.typeId == War3ID("Hamg"));
    TEST_ASSERT(snap.ownerPlayer == 0);
    TEST_ASSERT(snap.level == 3);
    TEST_ASSERT(snap.xp == heroAb->GetXP());
    TEST_ASSERT(snap.skillPoints == 5);
    TEST_ASSERT(snap.properName == "Jaina Proudmoore");
    TEST_ASSERT(snap.learnedSkills.size() == 1);
    TEST_ASSERT(snap.learnedSkills[0].first == War3ID("AOcr"));
    TEST_ASSERT(snap.learnedSkills[0].second == 2);
    TEST_ASSERT(snap.items.size() == 1);
    TEST_ASSERT(snap.items[0].typeId == War3ID("I000"));
    TEST_ASSERT(snap.items[0].charges == 7);

    // Fresh sim (next map): respawn restores everything.
    CSimulation next;
    CUnitData unitData;
    RegisterHeroType(unitData);
    next.SetUnitData(&unitData);
    next.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnit* reborn = RespawnCarriedHero(next, snap, 500, 600);
    TEST_ASSERT(reborn != nullptr);
    TEST_ASSERT(reborn->GetTypeId() == War3ID("Hamg"));
    TEST_ASSERT(reborn->GetPlayerIndex() == 0);
    CAbilityHero* rebornAb = nullptr;
    for (CAbility* ab : reborn->GetAbilities()) {
        rebornAb = dynamic_cast<CAbilityHero*>(ab);
        if (rebornAb) break;
    }
    TEST_ASSERT(rebornAb != nullptr);
    TEST_ASSERT(rebornAb->GetHeroLevel() == 3);
    TEST_ASSERT(rebornAb->GetXP() == heroAb->GetXP());
    TEST_ASSERT(rebornAb->GetSkillPoints() == 5);
    TEST_ASSERT(rebornAb->GetProperName() == "Jaina Proudmoore");
    auto* skill = reborn->GetAbility(War3ID("AOcr"));
    TEST_ASSERT(skill != nullptr && skill->GetLevel() == 2);
    auto* rebornInv = dynamic_cast<CAbilityInventory*>(reborn->GetAbility(War3ID("AInv")));
    TEST_ASSERT(rebornInv != nullptr && rebornInv->GetItemCount() == 1);
    TEST_ASSERT(rebornInv->GetItem(0)->GetTypeId() == War3ID("I000"));
    TEST_ASSERT(rebornInv->GetItem(0)->GetCharges() == 7);
    return true;
}

bool TestHeroCarryFilters() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "AI", CMapControl::COMPUTER));

    // Dead hero can be carried over in campaign cache.
    CUnit* dead = BuildHero(sim, 711, 0);
    dead->SetLife(sim, 0.0f);
    TEST_ASSERT(dead->IsDead());
    TEST_ASSERT(IsCarriableHero(dead, sim));
    TEST_ASSERT(SnapshotHero(dead, sim).typeId.value == War3ID("Hamg").value);

    // Computer-owned hero stays behind.
    CUnit* aiHero = BuildHero(sim, 712, 1);
    TEST_ASSERT(!IsCarriableHero(aiHero, sim));

    // Hero-typed unit without a hero ability is not carriable.
    CUnit* noAb = sim.AdoptUnit(std::make_unique<CUnit>(714, MakeHeroType(), 0,
                                                        War3ID("Hamg"), 0, 0, 0));
    TEST_ASSERT(!IsCarriableHero(noAb, sim));

    // Respawn guards: empty snapshot, missing owner, missing type.
    CSimulation next;
    next.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CarriedHero empty;
    TEST_ASSERT(RespawnCarriedHero(next, empty, 0, 0) == nullptr);
    CarriedHero noOwner;
    noOwner.typeId = War3ID("Hamg");
    noOwner.ownerPlayer = 7;
    TEST_ASSERT(RespawnCarriedHero(next, noOwner, 0, 0) == nullptr);
    CUnitData unitData; // no Hamg registered
    next.SetUnitData(&unitData);
    CarriedHero noType;
    noType.typeId = War3ID("Hamg");
    noType.ownerPlayer = 0;
    TEST_ASSERT(RespawnCarriedHero(next, noType, 0, 0) == nullptr);
    return true;
}

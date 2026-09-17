// Max life/mana layering tests: type base + hero bonus + item/upgrade bonus
// must never wipe each other when one layer is recomputed.
#include <memory>
#include <vector>

#include "ecs/csimulation.h"
#include "ecs/item_bonuses.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "ecs/upgrade_effect.h"
#include "ecs/abilities/ability_hero.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

CUnitType* MakeLayerType(bool hero) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", hero ? War3ID("Hpal") : War3ID("hfoo"),
        500, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 2, War3ID(),
        std::vector<War3ID>{}, false, PathingGridMovementType::FOOT,
        0.0f, 32.0f, std::vector<std::string>{},
        std::unordered_set<CUnitClassification>{},
        std::vector<CUnitAttack>{atk},
        1, std::string(""), false, true, CDefenseType::MEDIUM,
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

} // namespace

bool TestItemLifeSurvivesHeroRecalc() {
    CSimulation sim;
    CUnit u(1, MakeLayerType(true), 0, War3ID("Hpal"), 0, 0, 0);
    u.AddAbility(sim, std::make_unique<CAbilityHero>(101));
    // Base 500 + 20 STR * 25 = 1000 after OnAdd.
    TEST_ASSERT(u.GetMaxLife() == 1000.0f);
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::MAX_LIFE, 150}});
    TEST_ASSERT(u.GetMaxLife() == 1150.0f);
    // STR item triggers full derived-stat recalc; item layer must survive.
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::STRENGTH, 2}});
    TEST_ASSERT(u.GetMaxLife() == 1200.0f);
    RemoveItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::MAX_LIFE, 150}});
    TEST_ASSERT(u.GetMaxLife() == 1050.0f);
    return true;
}

bool TestUpgradeLifeSurvivesHeroLevelUp() {
    CSimulation sim;
    CUnit u(2, MakeLayerType(true), 0, War3ID("Hpal"), 0, 0, 0);
    u.AddAbility(sim, std::make_unique<CAbilityHero>(102));
    CUpgradeEffectHitPoints hp(War3ID(), 100, 0);
    hp.Apply(sim, &u, 1);
    TEST_ASSERT(u.GetMaxLife() == 1100.0f);
    // Level 2: STR 20 -> 22, hero layer 500 -> 550; upgrade layer kept.
    auto* hero = dynamic_cast<CAbilityHero*>(u.GetAbility(War3ID("AHer")));
    TEST_ASSERT(hero != nullptr);
    hero->AddXP(sim, &u, 200);
    TEST_ASSERT(hero->GetHeroLevel() == 2);
    TEST_ASSERT(u.GetMaxLife() == 1150.0f);
    return true;
}

bool TestMaxLossClampsLifeButGainDoesNotHeal() {
    CSimulation sim;
    CUnit u(3, MakeLayerType(false), 0, War3ID("hfoo"), 0, 0, 0);
    TEST_ASSERT(u.GetLife() == 500.0f);
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::MAX_LIFE, 150}});
    TEST_ASSERT(u.GetMaxLife() == 650.0f);
    TEST_ASSERT(u.GetLife() == 500.0f); // gaining max does not heal
    u.SetLife(sim, 650.0f);
    RemoveItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::MAX_LIFE, 150}});
    TEST_ASSERT(u.GetMaxLife() == 500.0f);
    TEST_ASSERT(u.GetLife() == 500.0f); // current clamped to new max
    return true;
}

bool TestManaLayersStack() {
    CSimulation sim;
    CUnit u(4, MakeLayerType(true), 0, War3ID("Hpal"), 0, 0, 0);
    u.AddAbility(sim, std::make_unique<CAbilityHero>(103));
    // Base 200 + 10 INT * 15 = 350 after OnAdd.
    TEST_ASSERT(u.GetMaxMana() == 350);
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::MAX_MANA, 50}});
    TEST_ASSERT(u.GetMaxMana() == 400);
    // INT recalc must keep the item layer: 200 + 11*15 + 50 = 415.
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::INTELLIGENCE, 1}});
    TEST_ASSERT(u.GetMaxMana() == 415);
    return true;
}

bool TestNonHeroFallbackStacksWithItemAndUpgrade() {
    CSimulation sim;
    CUnit u(5, MakeLayerType(false), 0, War3ID("hfoo"), 0, 0, 0);
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::STRENGTH, 1}});
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::MAX_LIFE, 100}});
    CUpgradeEffectHitPoints hp(War3ID(), 50, 0);
    hp.Apply(sim, &u, 1);
    TEST_ASSERT(u.GetMaxLife() == 675.0f); // 500 + 25 + 100 + 50
    RemoveItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::STRENGTH, 1}});
    TEST_ASSERT(u.GetMaxLife() == 650.0f);
    hp.Unapply(sim, &u, 1);
    TEST_ASSERT(u.GetMaxLife() == 600.0f);
    return true;
}

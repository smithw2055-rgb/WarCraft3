// Step-1 simulation tests: item bonuses, upgrade effects, buffs.
#include <memory>
#include <vector>

#include "ecs/behaviors/behavior.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/item_bonuses.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "ecs/upgrade_effect.h"
#include "ecs/abilities/ability_hero.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

CUnitType* MakeTestType(bool hero, War3ID typeId) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", typeId,
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

bool TestItemDamageArmorApplyRemoveSymmetric() {
    CSimulation sim;
    CUnit u(1, MakeTestType(false, War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0);
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::DAMAGE, 5}});
    TEST_ASSERT(u.GetBonusDamage() == 5);
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::ARMOR, 3}});
    TEST_ASSERT(u.GetTotalDefense() == 2 + 3);
    RemoveItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::DAMAGE, 5}});
    TEST_ASSERT(u.GetBonusDamage() == 0);
    RemoveItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::ARMOR, 3}});
    TEST_ASSERT(u.GetTotalDefense() == 2);
    return true;
}

bool TestItemStrengthFlowsThroughHeroAbility() {
    CSimulation sim;
    CUnit u(2, MakeTestType(true, War3ID("Hpal")), 0, War3ID("Hpal"), 0, 0, 0);
    u.AddAbility(sim, std::make_unique<CAbilityHero>(101));
    auto* hero = dynamic_cast<CAbilityHero*>(u.GetAbility(War3ID("AHer")));
    TEST_ASSERT(hero != nullptr);
    const float lifeBefore = u.GetMaxLife();
    ApplyItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::STRENGTH, 2}});
    TEST_ASSERT(hero->GetStrength().bonus == 2);
    TEST_ASSERT(u.GetMaxLife() == lifeBefore + 50.0f); // 2 STR * 25 HP
    TEST_ASSERT(u.GetBonusDamage() == 22);             // 20 base STR + 2 bonus
    RemoveItemBonuses(sim, &u, std::vector<CItemBonus>{{CItemBonusType::STRENGTH, 2}});
    TEST_ASSERT(hero->GetStrength().bonus == 0);
    TEST_ASSERT(u.GetBonusDamage() == 20);
    return true;
}

bool TestUpgradeEffectsApplyAndUnapply() {
    CSimulation sim;
    CUnit u(4, MakeTestType(false, War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0);
    CUpgradeEffectAttackDamage dmg(War3ID(), 10, 5);
    dmg.Apply(sim, &u, 1);
    TEST_ASSERT(u.GetBonusDamage() == 10);
    dmg.Unapply(sim, &u, 1);
    TEST_ASSERT(u.GetBonusDamage() == 0);
    CUpgradeEffectDefense def(War3ID(), 2, 1);
    def.Apply(sim, &u, 2);
    TEST_ASSERT(u.GetTotalDefense() == 2 + 3);
    def.Unapply(sim, &u, 2);
    TEST_ASSERT(u.GetTotalDefense() == 2);
    CUpgradeEffectAttackRange rng(War3ID(), 50, 0);
    rng.Apply(sim, &u, 1);
    TEST_ASSERT(u.GetAttackRange(0) == 150.0f);
    rng.Unapply(sim, &u, 1);
    TEST_ASSERT(u.GetAttackRange(0) == 100.0f);
    return true;
}

bool TestSlowAndBloodlustRestoreSpeed() {
    CSimulation sim;
    CUnit slowed(5, MakeTestType(false, War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0);
    slowed.AddBuff(std::make_unique<CBuffSlow>(10.0f, 0.5f), sim);
    // In WC3, MinUnitSpeed clamps movement speed to 150.0f
    TEST_ASSERT(slowed.GetSpeed() == 150);
    slowed.RemoveBuff(War3ID("Bslo"), sim);
    TEST_ASSERT(slowed.GetSpeed() == 200);

    CUnit hasted(6, MakeTestType(false, War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0);
    hasted.AddBuff(std::make_unique<CBuffBloodlust>(10.0f, 0.4f, 0.5f), sim);
    TEST_ASSERT(hasted.GetSpeed() == 280);
    TEST_ASSERT(hasted.GetAttackSpeedMultiplier() == 1.5f);
    hasted.RemoveBuff(War3ID("Bblo"), sim);
    TEST_ASSERT(hasted.GetSpeed() == 200);
    TEST_ASSERT(hasted.GetAttackSpeedMultiplier() == 1.0f);
    return true;
}

bool TestThornsReflectsOnceWithoutPingPong() {
    CSimulation sim;
    CUnit target(7, MakeTestType(false, War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0);
    CUnit attacker(8, MakeTestType(false, War3ID("hfoo")), 1, War3ID("hfoo"), 100, 0, 0);
    target.AddBuff(std::make_unique<CBuffThorns>(10.0f, 0.5f), sim);
    const float atkLifeBefore = attacker.GetLife();
    const float tgtLifeBefore = target.GetLife();
    target.Damage(sim, &attacker, true, false,
                  CAttackType::NORMAL, CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(target.GetLife() < tgtLifeBefore);
    TEST_ASSERT(attacker.GetLife() < atkLifeBefore);
    const float reflected = atkLifeBefore - attacker.GetLife();
    const float dealt = tgtLifeBefore - target.GetLife();
    TEST_ASSERT(reflected > 0 && reflected <= dealt);
    return true;
}

bool TestStunParksAndClearsBehavior() {
    CSimulation sim;
    CUnit u(9, MakeTestType(false, War3ID("hfoo")), 0, War3ID("hfoo"), 0, 0, 0);
    u.AddBuff(std::make_unique<CBuffStun>(5.0f), sim);
    TEST_ASSERT(u.GetCurrentBehavior() != nullptr);
    TEST_ASSERT(u.GetCurrentBehavior()->GetCategory() == CBehaviorCategory::STUN);
    u.RemoveBuff(War3ID("Bstn"), sim);
    TEST_ASSERT(u.GetCurrentBehavior() == nullptr);
    return true;
}

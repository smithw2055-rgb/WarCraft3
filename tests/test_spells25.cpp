// Batch-25 tests: curse, moon well, eat tree, sacrifice, hide, ethereal
// collision, chest of gold, and item stat passives.
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells25.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/destructable.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell25Type() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell25Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestCurseMakesAttacksMiss() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2501, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2502, 1, 100, 0);
    caster->SetMana(1000);
    CAbilityCurse curse(25901);
    curse.DoEffect(sim, caster, foe);
    TEST_ASSERT(foe->GetBuff(War3ID("Bcrs")) != nullptr);

    auto* attacker = AdoptAt(sim, 2503, 1, 0, 50);
    auto* victim = AdoptAt(sim, 2504, 0, 40, 50);
    const float lifeBefore = victim->GetLife();
    attacker->AddBuff(std::make_unique<CBuffCurse>(10.0f, 1.0f), sim);
    const float dealt = victim->Damage(sim, attacker, true, false,
                                       CAttackType::NORMAL, CDamageType::NORMAL,
                                       "", 40.0f, 0);
    TEST_ASSERT(dealt == 0.0f);
    TEST_ASSERT(victim->GetLife() == lifeBefore);
    return true;
}

bool TestMoonWellSpendsMana() {
    CSimulation sim;
    auto* well = AdoptAt(sim, 2511, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2512, 0, 100, 0);
    well->SetMana(100);
    ally->SetMaxLife(200);
    ally->SetLife(sim, 50);
    CAbilityMoonWell moon(25911);
    moon.DoEffect(sim, well, ally);
    // 100 mana / 4 per hit point restores 25 life and spends the pool.
    TEST_ASSERT(std::fabs(ally->GetLife() - 75.0f) < 0.01f);
    TEST_ASSERT(well->GetMana() == 0.0f);
    return true;
}

bool TestEatTreeFellsAndHeals() {
    CSimulation sim;
    auto* ancient = AdoptAt(sim, 2521, 0, 0, 0);
    ancient->SetMaxLife(500);
    ancient->SetLife(sim, 50);
    auto* tree = sim.CreateDestructable(War3ID("LTlt"), 40, 0, 80);
    TEST_ASSERT(tree != nullptr);
    CAbilityEatTree eat(25921);
    eat.DoEffect(sim, ancient, tree);
    TEST_ASSERT(tree->IsDead());
    TEST_ASSERT(std::fabs(ancient->GetLife() - 150.0f) < 0.01f);
    return true;
}

bool TestSacrificeKillsTheOffering() {
    CSimulation sim;
    auto* pit = AdoptAt(sim, 2531, 0, 0, 0);
    auto* acolyte = AdoptAt(sim, 2532, 0, 60, 0);
    CAbilitySacrifice sacrifice(25931);
    sacrifice.DoEffect(sim, pit, acolyte);
    TEST_ASSERT(acolyte->IsDead());
    TEST_ASSERT(!pit->IsDead());

    auto* acolyte2 = AdoptAt(sim, 2533, 0, 80, 0);
    auto* pit2 = AdoptAt(sim, 2534, 0, 0, 40);
    CAbilityRequestSacrifice request(25932);
    request.DoEffect(sim, acolyte2, pit2);
    TEST_ASSERT(acolyte2->IsDead());
    TEST_ASSERT(!pit2->IsDead());
    return true;
}

bool TestHideAndEtherealAndGold() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* unit = AdoptAt(sim, 2541, 0, 0, 0);
    CAbilityHide hide(25941);
    hide.DoEffect(sim, unit);
    TEST_ASSERT(unit->IsInvisible());

    const float collision = unit->GetCollisionSize();
    CAbilityEthereal ethereal(25942);
    ethereal.OnAdd(sim, unit);
    TEST_ASSERT(unit->GetCollisionSize() == 0.0f);
    TEST_ASSERT(unit->IsEthereal());
    ethereal.OnRemove(sim, unit);
    TEST_ASSERT(unit->GetCollisionSize() == collision);
    TEST_ASSERT(!unit->IsEthereal());

    CAbilityChestOfGold chest(25943);
    chest.DoEffect(sim, unit);
    TEST_ASSERT(sim.GetPlayer(0)->GetGold() == 250);
    return true;
}

bool TestItemStatBonusesApply() {
    CSimulation sim;
    auto* hero = AdoptAt(sim, 2551, 0, 0, 0);
    hero->AddAbility(sim, std::make_unique<CAbilityHero>(25950));
    const float lifeBefore = hero->GetMaxLife();
    const int manaBefore = hero->GetMaxMana();
    const int damageBefore = hero->GetBonusDamage();

    hero->AddAbility(sim, std::make_unique<CAbilityItemDamageBonus>(25951));
    hero->AddAbility(sim, std::make_unique<CAbilityItemArmorBonus>(25952));
    hero->AddAbility(sim, std::make_unique<CAbilityItemLifeBonus>(25953));
    hero->AddAbility(sim, std::make_unique<CAbilityItemManaPoolBonus>(25954));
    TEST_ASSERT(hero->GetBonusDamage() == damageBefore + 3);
    TEST_ASSERT(hero->GetBonusArmor() == 2);
    TEST_ASSERT(std::fabs(hero->GetMaxLife() - (lifeBefore + 150.0f)) < 0.01f);
    TEST_ASSERT(hero->GetMaxMana() == manaBefore + 100);

    hero->AddAbility(sim, std::make_unique<CAbilityItemAttributeBonus>(25955));
    auto* heroAbil = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
    TEST_ASSERT(heroAbil != nullptr);
    TEST_ASSERT(heroAbil->GetStrength().bonus == 1);
    TEST_ASSERT(heroAbil->GetAgility().bonus == 1);
    TEST_ASSERT(heroAbil->GetIntelligence().bonus == 1);

    hero->RemoveAbility(sim, War3ID("AIat"));
    hero->RemoveAbility(sim, War3ID("AIde"));
    hero->RemoveAbility(sim, War3ID("AIml"));
    hero->RemoveAbility(sim, War3ID("AImm"));
    hero->RemoveAbility(sim, War3ID("AIab"));
    TEST_ASSERT(hero->GetBonusDamage() == damageBefore);
    TEST_ASSERT(hero->GetBonusArmor() == 0);
    TEST_ASSERT(std::fabs(hero->GetMaxLife() - lifeBefore) < 0.01f);
    TEST_ASSERT(hero->GetMaxMana() == manaBefore);
    TEST_ASSERT(heroAbil->GetStrength().bonus == 0);
    return true;
}

bool TestSpells25FactoryCoverage() {
    TEST_ASSERT(dynamic_cast<CAbilityCurse*>(
        AbilityFactory::CreateAbility(25961, War3ID("Acrs")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityMoonWell*>(
        AbilityFactory::CreateAbility(25962, War3ID("Ambt")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityEatTree*>(
        AbilityFactory::CreateAbility(25963, War3ID("Aeat")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilitySacrifice*>(
        AbilityFactory::CreateAbility(25964, War3ID("Asac")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityRequestSacrifice*>(
        AbilityFactory::CreateAbility(25965, War3ID("Alam")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityHide*>(
        AbilityFactory::CreateAbility(25966, War3ID("Ahid")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityEthereal*>(
        AbilityFactory::CreateAbility(25967, War3ID("Aeth")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChestOfGold*>(
        AbilityFactory::CreateAbility(25968, War3ID("AIgo")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemDamageBonus*>(
        AbilityFactory::CreateAbility(25969, War3ID("AIat")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemArmorBonus*>(
        AbilityFactory::CreateAbility(25970, War3ID("AIde")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemLifeBonus*>(
        AbilityFactory::CreateAbility(25971, War3ID("AIml")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemManaPoolBonus*>(
        AbilityFactory::CreateAbility(25972, War3ID("AImm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemAttributeBonus*>(
        AbilityFactory::CreateAbility(25973, War3ID("AIab")).get()) != nullptr);
    return true;
}

bool TestSpells25OrderIds() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2561, 0, 0, 0);
    caster->SetMana(1000);
    auto curse = AbilityFactory::CreateAbility(25981, War3ID("Acrs"));
    TEST_ASSERT(curse->CheckCanUse(sim, caster, 852190));
    auto moon = AbilityFactory::CreateAbility(25982, War3ID("Ambt"));
    TEST_ASSERT(moon->CheckCanUse(sim, caster, 852542));
    auto eat = AbilityFactory::CreateAbility(25983, War3ID("Aeat"));
    TEST_ASSERT(eat->CheckCanUse(sim, caster, 852146));
    auto hide = AbilityFactory::CreateAbility(25984, War3ID("Ahid"));
    TEST_ASSERT(hide->CheckCanUse(sim, caster, 852131));
    auto gold = AbilityFactory::CreateAbility(25985, War3ID("AIgo"));
    TEST_ASSERT(gold->CheckCanUse(sim, caster, 852610));
    TEST_ASSERT(SpellBaseOrderId(War3ID("Asac")) == 852205);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Alam")) == 852201);
    TEST_ASSERT(OrderIdFromString("curse") == 852190);
    return true;
}

// Batch-26 tests: spell shield, consumable hero items, form toggles, clock.
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells26.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell26Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell26Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim) {
    for (int i = 0; i < 2; i++) sim.Update();
}

} // namespace

bool TestSpellShieldBlocksHostileCast() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* bearer = AdoptAt(sim, 2601, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2602, 0, 40, 0);
    auto* foe = AdoptAt(sim, 2603, 1, 80, 0);
    CAbilitySpellShieldPassive shield(26901);
    shield.OnAdd(sim, bearer);
    TEST_ASSERT(bearer->HasBuff(War3ID("BNss")));
    TEST_ASSERT(bearer->CheckSpellEffectReaction(sim, ally));
    TEST_ASSERT(bearer->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!bearer->CheckSpellEffectReaction(sim, foe));
    TEST_ASSERT(!bearer->HasBuff(War3ID("BNss")));

    bool rearmed = false;
    for (int i = 0; i < 900 && !rearmed; i++) {
        shield.OnTick(sim, bearer);
        rearmed = bearer->HasBuff(War3ID("BNss"));
    }
    TEST_ASSERT(rearmed);
    return true;
}

bool TestSpellShieldAreaCoversAllies() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    auto* caster = AdoptAt(sim, 2611, 0, 0, 0);
    auto* ally = AdoptAt(sim, 2612, 0, 100, 0);
    auto* foe = AdoptAt(sim, 2613, 1, 100, 0);
    auto* far = AdoptAt(sim, 2614, 0, 2000, 0);
    Settle(sim);
    CAbilitySpellShieldArea area(26911);
    area.DoEffect(sim, caster, 0, 0);
    TEST_ASSERT(caster->HasBuff(War3ID("BNss")));
    TEST_ASSERT(ally->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!foe->HasBuff(War3ID("BNss")));
    TEST_ASSERT(!far->HasBuff(War3ID("BNss")));
    return true;
}

bool TestPermanentItemsChangeTheHero() {
    CSimulation sim;
    auto* hero = AdoptAt(sim, 2621, 0, 0, 0);
    hero->AddAbility(sim, std::make_unique<CAbilityHero>(26920));
    auto* heroAbil = dynamic_cast<CAbilityHero*>(hero->GetAbility(War3ID("AHer")));
    TEST_ASSERT(heroAbil != nullptr);
    const int strBefore = heroAbil->GetStrength().base;

    CAbilityItemPermanentStats tome(26921, War3ID("AIim"), 0, 0, 1);
    tome.DoEffect(sim, hero);
    TEST_ASSERT(heroAbil->GetStrength().base == strBefore + 1);
    const float lifeBefore = hero->GetMaxLife();

    CAbilityItemPermanentLife life(26922);
    life.DoEffect(sim, hero);
    TEST_ASSERT(std::fabs(hero->GetMaxLife() - (lifeBefore + 50.0f)) < 0.01f);

    CAbilityItemExperience xp(26923);
    xp.DoEffect(sim, hero);
    TEST_ASSERT(heroAbil->GetXP() == 100);
    TEST_ASSERT(heroAbil->GetHeroLevel() == 1);

    CAbilityItemLevelGain levels(26924);
    levels.DoEffect(sim, hero);
    TEST_ASSERT(heroAbil->GetHeroLevel() == 2);

    CAbilityPermanentInvulnerable invuln(26925);
    invuln.OnAdd(sim, hero);
    TEST_ASSERT(hero->IsInvulnerable());
    invuln.OnRemove(sim, hero);
    TEST_ASSERT(!hero->IsInvulnerable());
    // The tome stays after the ability is gone.
    TEST_ASSERT(heroAbil->GetStrength().base == strBefore + 1);
    return true;
}

bool TestMoonstoneSetsTheClock() {
    CSimulation sim;
    TEST_ASSERT(std::fabs(sim.GetGameTimeOfDay() - 12.0f) < 0.01f);
    auto* caster = AdoptAt(sim, 2631, 0, 0, 0);
    CAbilityChangeTimeOfDay moon(26931);
    moon.DoEffect(sim, caster);
    TEST_ASSERT(std::fabs(sim.GetGameTimeOfDay()) < 0.01f);
    return true;
}

bool TestFormToggleAndChaos() {
    CSimulation sim;
    auto* walker = AdoptAt(sim, 2641, 0, 0, 0);
    CAbilityToggleForm form(26941, War3ID("Acpf"));
    form.DoEffect(sim, walker);
    TEST_ASSERT(walker->GetBuff(War3ID("Acpf")) != nullptr);
    form.DoEffect(sim, walker);
    TEST_ASSERT(walker->GetBuff(War3ID("Acpf")) == nullptr);

    auto* chaos = AdoptAt(sim, 2642, 0, 40, 0);
    CAbilityChaos ability(26942);
    ability.OnAdd(sim, chaos);
    TEST_ASSERT(chaos->GetBuff(War3ID("Acha")) != nullptr);
    ability.OnRemove(sim, chaos);
    TEST_ASSERT(chaos->GetBuff(War3ID("Acha")) == nullptr);
    return true;
}

bool TestSpells26FactoryCoverage() {
    TEST_ASSERT(dynamic_cast<CAbilitySpellShieldPassive*>(
        AbilityFactory::CreateAbility(26951, War3ID("ANss")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilitySpellShieldArea*>(
        AbilityFactory::CreateAbility(26952, War3ID("ANse")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26953, War3ID("AIim")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26954, War3ID("AIam")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26955, War3ID("AIxm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentStats*>(
        AbilityFactory::CreateAbility(26956, War3ID("AIsm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemPermanentLife*>(
        AbilityFactory::CreateAbility(26957, War3ID("AImi")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemExperience*>(
        AbilityFactory::CreateAbility(26958, War3ID("AIem")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemLevelGain*>(
        AbilityFactory::CreateAbility(26959, War3ID("AIlm")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityItemFigurine*>(
        AbilityFactory::CreateAbility(26960, War3ID("AIfs")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityPermanentInvulnerable*>(
        AbilityFactory::CreateAbility(26961, War3ID("Avul")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChangeTimeOfDay*>(
        AbilityFactory::CreateAbility(26962, War3ID("AIct")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityToggleForm*>(
        AbilityFactory::CreateAbility(26963, War3ID("Acpf")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityToggleForm*>(
        AbilityFactory::CreateAbility(26964, War3ID("Aetf")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityToggleForm*>(
        AbilityFactory::CreateAbility(26965, War3ID("Aave")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityChaos*>(
        AbilityFactory::CreateAbility(26966, War3ID("Acha")).get()) != nullptr);
    return true;
}

bool TestSpells26OrderIds() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2651, 0, 0, 0);
    caster->SetMana(1000);
    auto shield = AbilityFactory::CreateAbility(26971, War3ID("ANse"));
    TEST_ASSERT(shield->CheckCanUse(sim, caster, 852572));
    auto clock = AbilityFactory::CreateAbility(26972, War3ID("AIct"));
    TEST_ASSERT(clock->CheckCanUse(sim, caster, 852621));
    auto form = AbilityFactory::CreateAbility(26973, War3ID("Acpf"));
    TEST_ASSERT(form->CheckCanUse(sim, caster, 852493));
    TEST_ASSERT(form->CheckCanUse(sim, caster, 852494));
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIim")) == 852214001);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AImi")) == 852264);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIem")) == 852260);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIlm")) == 852263);
    TEST_ASSERT(SpellBaseOrderId(War3ID("AIfs")) == 852261);
    TEST_ASSERT(OrderIdFromString("etherealform") == 852496);
    return true;
}

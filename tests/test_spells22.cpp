// Batch-22 tests: Finger of Death / Howl of Terror code variants (Afod,
// Acht) reuse the ANfd/ANht classes through aliases, and the ANin/Aams
// variants answer their own retail orders via the widened alias match.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/spells17.h"
#include "ecs/abilities/spells2.h"
#include "ecs/ability_factory.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell22Type(bool hero) {
    static std::vector<std::unique_ptr<CUnitType>> keepFoot;
    static std::vector<std::unique_ptr<CUnitType>> keepHero;
    auto& keep = hero ? keepHero : keepFoot;
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
        1, hero, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y, bool hero = false) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell22Type(hero), player,
                                                 hero ? War3ID("Hpal") : War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

} // namespace

bool TestFingerOfDeathExecutes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2201, 0, 0, 0);
    auto* victim = AdoptAt(sim, 2202, 1, 100, 0);
    Settle(sim);
    // Afod variant shares the ANfd implementation (lethal to non-heroes).
    auto finger = AbilityFactory::CreateAbility(22901, War3ID("Afod"));
    TEST_ASSERT(finger != nullptr);
    TEST_ASSERT(finger->GetAlias() == War3ID("Afod"));
    auto* fod = dynamic_cast<CAbilityFingerOfDeath*>(finger.get());
    TEST_ASSERT(fod != nullptr);
    fod->DoEffect(sim, caster, victim);
    TEST_ASSERT(victim->IsDead());
    return true;
}

bool TestFingerOfDeathSparesHeroes() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2211, 0, 0, 0);
    auto* hero = AdoptAt(sim, 2212, 1, 100, 0, true);
    TEST_ASSERT(hero->IsHero());
    Settle(sim);
    CAbilityFingerOfDeath finger(22902);
    finger.DoEffect(sim, caster, hero);
    // Heroes withstand it for DataC (500): scratched, not executed.
    TEST_ASSERT(!hero->IsDead());
    if (std::fabs(hero->GetLife() - 9500.0f) > 1.0f) return false;
    return true;
}

bool TestHowlCripplesEnemiesOnly() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2221, 0, 0, 0);
    auto* foe = AdoptAt(sim, 2222, 1, 100, 0);
    auto* friend_ = AdoptAt(sim, 2223, 0, 150, 0);
    Settle(sim);
    auto howl = AbilityFactory::CreateAbility(22903, War3ID("Acht"));
    TEST_ASSERT(howl != nullptr);
    auto* howlPtr = dynamic_cast<CAbilityHowlOfTerror*>(howl.get());
    TEST_ASSERT(howlPtr != nullptr);
    howlPtr->DoEffect(sim, caster);
    TEST_ASSERT(foe->GetBuff(War3ID("BNht")) != nullptr);
    TEST_ASSERT(friend_->GetBuff(War3ID("BNht")) == nullptr);
    TEST_ASSERT(caster->GetBuff(War3ID("BNht")) == nullptr);
    return true;
}

bool TestAliasAnswersOwnOrder() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2231, 0, 0, 0);
    caster->SetMana(1000);
    Settle(sim);
    // Generic Inferno code shares the Dreadlord class but answers 852232.
    auto inferno = AbilityFactory::CreateAbility(22911, War3ID("ANin"));
    TEST_ASSERT(inferno != nullptr);
    TEST_ASSERT(inferno->GetAlias() == War3ID("ANin"));
    TEST_ASSERT(inferno->CheckCanUse(sim, caster, 852232));
    TEST_ASSERT(inferno->CheckCanUse(sim, caster, 852224)); // base still works
    TEST_ASSERT(!inferno->CheckCanUse(sim, caster, 852486));
    // Base Anti-Magic Shell variant likewise.
    auto shell = AbilityFactory::CreateAbility(22912, War3ID("Aams"));
    TEST_ASSERT(shell != nullptr);
    TEST_ASSERT(shell->GetAlias() == War3ID("Aams"));
    TEST_ASSERT(shell->CheckCanUse(sim, caster, 852186));
    return true;
}

bool TestSpells22FactoryCoverage() {
    auto finger = AbilityFactory::CreateAbility(22921, War3ID("Afod"));
    TEST_ASSERT(finger != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityFingerOfDeath*>(finger.get()) != nullptr);
    TEST_ASSERT(finger->GetAlias() == War3ID("Afod"));
    auto howl = AbilityFactory::CreateAbility(22922, War3ID("Acht"));
    TEST_ASSERT(howl != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityHowlOfTerror*>(howl.get()) != nullptr);
    TEST_ASSERT(howl->GetAlias() == War3ID("Acht"));
    return true;
}

bool TestSpells22OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Afod")) == 852230);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Acht")) == 852588);
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANin")) == 852232);
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aams")) == 852186);
    TEST_ASSERT(OrderIdFromString("fingerofdeath") == 852230);
    TEST_ASSERT(OrderIdFromString("howlofterror") == 852588);
    TEST_ASSERT(OrderIdFromString("inferno") == 852232);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Afod"), 852230));
    TEST_ASSERT(SpellMatchesOrder(War3ID("Acht"), 852588));
    TEST_ASSERT(!SpellMatchesOrder(War3ID("Afod"), 852588));
    return true;
}

// Batch-5 hero spell tests: point-damage waves, line impale, instant
// knives, passive carapace, sacrifice-for-mana, strike with slow+poison.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells5.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "resource/mpq_storm.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR ""
#endif

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell5Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell5Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

} // namespace

bool TestFlameStrikeWaves() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 501, 0, 0, 0);
    auto* foe = AdoptAt(sim, 502, 1, 100, 100);
    auto* far = AdoptAt(sim, 503, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityFlameStrike spell(5001);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestDeathAndDecayHitsAll() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 511, 0, 0, 0);
    auto* foe = AdoptAt(sim, 512, 1, 100, 100);
    auto* ally = AdoptAt(sim, 513, 0, 150, 100);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    const float allyBefore = ally->GetLife();
    CAbilityDeathAndDecay spell(5101);
    spell.DoEffect(sim, caster, 125, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() < allyBefore); // corrupts friend and foe alike
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestImpaleLineStuns() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 521, 0, 0, 0);
    auto* target = AdoptAt(sim, 522, 1, 300, 0);
    auto* behind = AdoptAt(sim, 523, 1, 500, 0);
    auto* offline = AdoptAt(sim, 524, 1, 300, 300);
    Settle(sim);
    const float targetBefore = target->GetLife();
    const float behindBefore = behind->GetLife();
    CAbilityImpale spell(5201);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() < targetBefore);
    TEST_ASSERT(target->GetBuff(War3ID("Bstn")) != nullptr);
    TEST_ASSERT(behind->GetLife() < behindBefore);
    TEST_ASSERT(behind->GetBuff(War3ID("Bstn")) != nullptr);
    TEST_ASSERT(offline->GetLife() == offline->GetMaxLife());
    TEST_ASSERT(offline->GetBuff(War3ID("Bstn")) == nullptr);
    return true;
}

bool TestFanOfKnivesHitsEnemiesOnly() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 531, 0, 0, 0);
    auto* foe = AdoptAt(sim, 532, 1, 100, 0);
    auto* ally = AdoptAt(sim, 533, 0, -100, 0);
    auto* far = AdoptAt(sim, 534, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityFanOfKnives spell(5301);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestRainOfFireWaves() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 541, 0, 0, 0);
    auto* foe = AdoptAt(sim, 542, 1, 100, 100);
    auto* far = AdoptAt(sim, 543, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityRainOfFire spell(5401);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    return true;
}

bool TestSpikedCarapaceArmorAndReflect() {
    CSimulation sim;
    AddPlayers(sim);
    auto* bearer = AdoptAt(sim, 551, 0, 0, 0);
    auto* attacker = AdoptAt(sim, 552, 1, 100, 0);
    Settle(sim);
    TEST_ASSERT(bearer->GetBonusArmor() == 0);
    bearer->AddAbility(sim, std::make_unique<CAbilitySpikedCarapace>(5501));
    TEST_ASSERT(bearer->GetBonusArmor() == 4);
    const float attackerBefore = attacker->GetLife();
    bearer->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(attacker->GetLife() < attackerBefore); // melee reflected
    TEST_ASSERT(bearer->RemoveAbility(sim, War3ID("AUts")));
    TEST_ASSERT(bearer->GetBonusArmor() == 0);
    return true;
}

bool TestDarkRitualGrantsMana() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 561, 0, 0, 0);
    auto* sacrifice = AdoptAt(sim, 562, 0, 100, 0);
    auto* enemy = AdoptAt(sim, 563, 1, 200, 0);
    Settle(sim);
    caster->SetMana(0.0f);
    CAbilityDarkRitual spell(5601);
    // Enemies cannot be sacrificed.
    spell.DoEffect(sim, caster, enemy);
    TEST_ASSERT(!enemy->IsDead());
    TEST_ASSERT(caster->GetMana() == 0.0f);
    spell.DoEffect(sim, caster, sacrifice);
    TEST_ASSERT(sacrifice->IsDead());
    TEST_ASSERT(caster->GetMana() > 0.0f);
    TEST_ASSERT(caster->GetMana() <= static_cast<float>(caster->GetMaxMana()));
    return true;
}

bool TestShadowStrikeSlowsAndPoisons() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 571, 0, 0, 0);
    auto* target = AdoptAt(sim, 572, 1, 100, 0);
    Settle(sim);
    const float speedBefore = target->GetSpeed();
    CAbilityShadowStrike spell(5701);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    TEST_ASSERT(target->GetSpeed() < speedBefore);
    const float afterStrike = target->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // poison ticks land
    TEST_ASSERT(target->GetLife() < afterStrike);
    return true;
}

bool TestSpells5FactoryCoverage() {
    const char* codes[] = {"AHfs", "AUdd", "AUim", "AEfk",
                           "ANrf", "AUts", "AUdr", "AEsh"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(5800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

// Batch-16 (abilities) tests: devour, devour magic, cold arrows,
// monsoon, absorb mana, unholy frenzy.
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/ability_factory.h"
#include "ecs/abilities/spells16.h"
#include "ecs/buffs.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " #a " vs " #b "\n"; return false; } } while (0)

namespace {

CUnitType* MakeSpell16Type(bool fly = false) {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    static std::vector<std::unique_ptr<CUnitType>> keepFly;
    auto& store = fly ? keepFly : keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    store.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 1.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 200, 0, War3ID(),
        std::vector<War3ID>{}, false,
        fly ? PathingGridMovementType::FLY : PathingGridMovementType::FOOT,
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
    return store.back().get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y, bool fly = false) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell16Type(fly), player,
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

bool TestDevourSwallows() {
    CSimulation sim;
    AddPlayers(sim);
    auto* kodo = AdoptAt(sim, 1601, 0, 0, 0);
    auto* victim = AdoptAt(sim, 1602, 1, 50, 0);
    auto* flyer = AdoptAt(sim, 1603, 1, 60, 0, true);
    Settle(sim);
    CAbilityDevour spell(16001);
    spell.DoEffect(sim, kodo, flyer); // flyers cannot be swallowed
    TEST_ASSERT(!flyer->IsHidden());
    TEST_ASSERT(flyer->GetBuff(War3ID("Bdig")) == nullptr);
    spell.DoEffect(sim, kodo, victim);
    TEST_ASSERT(victim->IsHidden()); // riding in cargo
    TEST_ASSERT(victim->GetBuff(War3ID("Bdig")) != nullptr);
    TEST_ASSERT(kodo->GetCargoUnits().size() == 1);
    const float before = victim->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // digest ticks land
    TEST_ASSERT(victim->GetLife() < before);
    TEST_ASSERT(kodo->GetCargoUnits().size() == 1); // no release
    return true;
}

bool TestDevourMagicDispels() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1611, 0, 0, 0);
    auto* target = AdoptAt(sim, 1612, 1, 100, 0);
    Settle(sim);
    target->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    target->AddBuff(std::make_unique<CBuffSlow>(10.0f, 0.3f, -1), sim);
    TEST_ASSERT(target->GetBuffs().size() == 2);
    caster->SetMana(0.0f);
    CAbilityDevourMagic spell(16101);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuffs().empty());
    const float cap = (float)caster->GetMaxMana();
    TEST_ASSERT(caster->GetMana() == (100.0f < cap ? 100.0f : cap)); // 2 buffs
    return true;
}

bool TestColdArrowsProc() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 1621, 0, 0, 0);
    auto* holder = AdoptAt(sim, 1622, 0, 0, 100);
    auto* targetA = AdoptAt(sim, 1623, 1, 100, 0);
    auto* targetB = AdoptAt(sim, 1624, 1, 100, 100);
    Settle(sim);
    attacker->AddAbility(sim, std::make_unique<CAbilityColdArrows>(16201));
    auto frost = std::make_unique<CAbilityColdArrows>(16202);
    frost->SetAlias(War3ID("ANfa")); // sea-witch skin shares the base
    holder->AddAbility(sim, std::move(frost));
    targetA->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(targetA->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(targetA->GetBuff(War3ID("Bpoi")) != nullptr);
    targetB->Damage(sim, holder, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 10.0f, 0);
    TEST_ASSERT(targetB->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(targetB->GetBuff(War3ID("Bpoi")) != nullptr);
    return true;
}

bool TestMonsoonWaves() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1631, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1632, 1, 100, 100);
    auto* ally = AdoptAt(sim, 1633, 0, 150, 100);
    auto* far = AdoptAt(sim, 1634, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityMonsoon spell(16301);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestAbsorbManaPops() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1641, 0, 0, 0);
    auto* summoned = AdoptAt(sim, 1642, 1, 100, 0);
    auto* regular = AdoptAt(sim, 1643, 1, 200, 0);
    Settle(sim);
    summoned->AddBuff(std::make_unique<CBuffTimedLife>(War3ID("AOsf"), 120.0f), sim);
    TEST_ASSERT(summoned->IsSummoned());
    caster->SetMana(0.0f);
    CAbilityAbsorbMana spell(16401);
    spell.DoEffect(sim, caster, regular); // the real stays
    TEST_ASSERT(!regular->IsDead());
    TEST_ASSERT(caster->GetMana() == 0.0f);
    spell.DoEffect(sim, caster, summoned);
    TEST_ASSERT(summoned->IsDead());
    TEST_ASSERT(caster->GetMana() > 0.0f);
    return true;
}

bool TestUnholyFrenzyHastes() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1651, 0, 0, 0);
    auto* ally = AdoptAt(sim, 1652, 0, 100, 0);
    Settle(sim);
    CAbilityUnholyFrenzy spell(16501);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("Babr")) != nullptr);
    TEST_NEAR(ally->GetAttackSpeedMultiplier(), 1.5f, 0.001f);
    return true;
}

bool TestSpells16FactoryCoverage() {
    const char* codes[] = {"Adev", "Advm", "AHca", "ANfa",
                           "ANmo", "Aabs", "Auhf"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(16600, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    // Sea-witch skin keeps its alias for the shared proc hook.
    auto frost = AbilityFactory::CreateAbility(16601, War3ID("ANfa"));
    TEST_ASSERT(frost != nullptr && frost->GetAlias() == War3ID("ANfa"));
    return true;
}

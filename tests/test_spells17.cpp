// Batch-17 (abilities) tests: howl, breath of fire, drunken haze,
// mana shield, locust swarm, cyclone, vengeance.
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/gameplay_constants.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_hero.h"
#include "ecs/abilities/spells17.h"
#include "ecs/ability_factory.h"
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
#define TEST_NEAR(a, b, eps) do { if (std::fabs((a) - (b)) > (eps)) { std::cerr << "NEAR @" << __LINE__ << ": " #a " vs " #b "\n"; return false; } } while (0)

namespace fs = std::filesystem;

namespace {

CUnitType* MakeSpell17Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell17Type(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

void Settle(CSimulation& sim, int ticks = 2) {
    for (int i = 0; i < ticks; i++) sim.Update();
}

void AddPlayers(CSimulation& sim) {
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::COMPUTER));
}

bool ResourcesPresent() {
    return fs::exists(fs::path(PROJECT_SOURCE_DIR) / "resources" / "war3x.mpq");
}

void LoadUnitData(MpqManager& mpq, CGameplayConstants& gc, CUnitData& unitData) {
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    unitData = CUnitData::LoadFromMpq(mpq, gc);
}

} // namespace

bool TestHowlWeakens() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1701, 0, 0, 0);
    auto* howled = AdoptAt(sim, 1702, 1, 100, 0);
    auto* control = AdoptAt(sim, 1703, 1, 5000, 5000);
    auto* targetA = AdoptAt(sim, 1704, 0, 200, 0);
    auto* targetB = AdoptAt(sim, 1705, 0, 200, 100);
    Settle(sim);
    CAbilityHowlOfTerror spell(17001);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(howled->GetBuff(War3ID("BNht")) != nullptr);
    TEST_ASSERT(control->GetBuff(War3ID("BNht")) == nullptr);
    const float beforeA = targetA->GetLife();
    const float beforeB = targetB->GetLife();
    targetA->Damage(sim, howled, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 200.0f, 0);
    targetB->Damage(sim, control, true, false, CAttackType::NORMAL,
                    CDamageType::NORMAL, "", 200.0f, 0);
    TEST_NEAR((beforeA - targetA->GetLife()) / (beforeB - targetB->GetLife()),
              0.7f, 0.01f);
    return true;
}

bool TestBreathOfFireBurns() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1711, 0, 0, 0);
    auto* target = AdoptAt(sim, 1712, 1, 100, 0);
    auto* neighbor = AdoptAt(sim, 1713, 1, 150, 0);
    auto* ally = AdoptAt(sim, 1714, 0, 120, 0);
    Settle(sim);
    const float targetBefore = target->GetLife();
    const float neighborBefore = neighbor->GetLife();
    CAbilityBreathOfFire spell(17101);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetLife() < targetBefore);
    TEST_ASSERT(neighbor->GetLife() < neighborBefore);
    TEST_ASSERT(target->GetBuff(War3ID("Bpoi")) != nullptr);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    const float afterStrike = target->GetLife();
    for (int i = 0; i < 40; i++) sim.Update(); // burn ticks land
    TEST_ASSERT(target->GetLife() < afterStrike);
    return true;
}

bool TestDrunkenHazeMisses() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1721, 0, 0, 0);
    auto* target = AdoptAt(sim, 1722, 1, 100, 100);
    auto* far = AdoptAt(sim, 1723, 1, 5000, 5000);
    // Shooters stand clear of the haze so only the direct buff applies.
    auto* attacker = AdoptAt(sim, 1724, 1, 2000, 100);
    auto* victim = AdoptAt(sim, 1725, 0, 2100, 100);
    Settle(sim);
    CAbilityDrunkenHaze spell(17201);
    spell.DoEffect(sim, caster, 100, 100);
    TEST_ASSERT(target->GetBuff(War3ID("BNdh")) != nullptr);
    TEST_ASSERT(target->GetBuff(War3ID("Bslo")) != nullptr);
    TEST_ASSERT(far->GetBuff(War3ID("BNdh")) == nullptr);
    // Certain miss: the swing lands nothing.
    attacker->AddBuff(std::make_unique<CBuffHaze>(30.0f, 1.0f), sim);
    const float victimBefore = victim->GetLife();
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    TEST_ASSERT(victim->GetLife() == victimBefore);
    // Certain hit: the swing lands fully.
    attacker->RemoveBuff(War3ID("BNdh"), sim);
    attacker->AddBuff(std::make_unique<CBuffHaze>(30.0f, 0.0f), sim);
    victim->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    TEST_ASSERT(victim->GetLife() < victimBefore);
    return true;
}

bool TestManaShieldAbsorbs() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1731, 0, 0, 0);
    Settle(sim);
    CAbilityManaShield spell(17301);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("BNms")) != nullptr);
    caster->SetMana(200.0f);
    const float lifeBefore = caster->GetLife();
    caster->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                   CDamageType::FIRE, "", 150.0f, 0);
    TEST_ASSERT(caster->GetLife() == lifeBefore); // mana took it all
    TEST_ASSERT(caster->GetMana() == 50.0f);
    // Empty pool: steel bites again.
    caster->SetMana(0.0f);
    caster->Damage(sim, nullptr, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 200.0f, 0);
    TEST_ASSERT(caster->GetLife() < lifeBefore);
    return true;
}

bool TestLocustSwarmDevours() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1741, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1742, 1, 100, 0);
    auto* ally = AdoptAt(sim, 1743, 0, 100, 100);
    auto* far = AdoptAt(sim, 1744, 1, 5000, 5000);
    Settle(sim);
    const float foeBefore = foe->GetLife();
    CAbilityLocustSwarm spell(17401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(foe->GetLife() < foeBefore);
    TEST_ASSERT(ally->GetLife() == ally->GetMaxLife());
    TEST_ASSERT(far->GetLife() == far->GetMaxLife());
    TEST_ASSERT(caster->GetLife() == caster->GetMaxLife());
    return true;
}

bool TestCycloneSuspends() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1751, 0, 0, 0);
    auto* target = AdoptAt(sim, 1752, 1, 100, 0);
    Settle(sim);
    CAbilityCyclone spell(17501);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bcyc")) != nullptr);
    TEST_ASSERT(target->IsInvulnerable()); // tossed and untouchable
    const float before = target->GetLife();
    target->Damage(sim, caster, false, false, CAttackType::SPELLS,
                   CDamageType::FIRE, "", 5000.0f, 0);
    TEST_ASSERT(target->GetLife() == before);
    target->RemoveBuff(War3ID("Bcyc"), sim); // touchdown restores both
    TEST_ASSERT(!target->IsInvulnerable());
    return true;
}

bool TestVengeanceRises() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("even"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* bearer = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    auto* killer = sim.CreateUnit(War3ID("hfoo"), 1, 100, 0, 0);
    TEST_ASSERT(bearer && killer);
    Settle(sim);
    bearer->AddAbility(sim, std::make_unique<CAbilityVengeance>(17601));
    bearer->Damage(sim, killer, false, false, CAttackType::CHAOS,
                   CDamageType::NORMAL, "", 99999.0f, 0);
    for (int i = 0; i < 5; i++) sim.Update();
    TEST_ASSERT(bearer->IsDead());
    bool spiritFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("even") &&
            unit->GetPlayerIndex() == 0) {
            spiritFound = true;
        }
    }
    TEST_ASSERT(spiritFound);
    return true;
}

bool TestSpells17FactoryCoverage() {
    const char* codes[] = {"ANht", "ANbf", "ANdh", "ANms",
                           "AUls", "Acyc", "Avng"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(17700, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

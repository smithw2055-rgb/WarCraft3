// Batch-7 tests: serpent ward, polymorph, inner fire, priest heal,
// dispel magic, rejuvenation, roar, defend.
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
#include "ecs/abilities/spells7.h"
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

CUnitType* MakeSpell7Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell7Type(), player,
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

} // namespace

bool TestSerpentWardSummons() {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    const std::string res = std::string(PROJECT_SOURCE_DIR) + "/resources/";
    for (auto a : {"War3Patch.mpq", "war3x.mpq", "war3.mpq"}) mpq.Open(res + a);
    CGameplayConstants gc;
    CUnitData unitData = CUnitData::LoadFromMpq(mpq, gc);
    if (!unitData.HasUnitType(War3ID("osp1"))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    TEST_ASSERT(caster != nullptr);
    Settle(sim);
    CAbilitySerpentWard spell(7001);
    spell.DoEffect(sim, caster, 100, 0);
    for (int i = 0; i < 3; i++) sim.Update(); // join new units
    bool wardFound = false;
    for (CUnit* unit : sim.GetUnits()) {
        if (unit && !unit->IsDead() && unit->GetTypeId() == War3ID("osp1") &&
            unit->GetPlayerIndex() == 0) {
            wardFound = true;
        }
    }
    TEST_ASSERT(wardFound);
    return true;
}

bool TestPolymorphHexes() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 711, 0, 0, 0);
    auto* target = AdoptAt(sim, 712, 1, 100, 0);
    Settle(sim);
    CAbilityPolymorph spell(7101);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bply")) != nullptr);
    TEST_ASSERT(target->IsHexed()); // sheep cannot attack or cast
    return true;
}

bool TestInnerFireBuffs() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 721, 0, 0, 0);
    auto* ally = AdoptAt(sim, 722, 0, 100, 0);
    Settle(sim);
    const int dmgBefore = ally->GetBonusDamage();
    const int armorBefore = ally->GetTotalDefense();
    CAbilityInnerFire spell(7201);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("Binf")) != nullptr);
    TEST_ASSERT(ally->GetBonusDamage() == dmgBefore + 5);
    TEST_ASSERT(ally->GetTotalDefense() == armorBefore + 3);
    ally->RemoveBuff(War3ID("Binf"), sim); // expiry path restores both
    TEST_ASSERT(ally->GetBonusDamage() == dmgBefore);
    TEST_ASSERT(ally->GetTotalDefense() == armorBefore);
    return true;
}

bool TestPriestHealHeals() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 731, 0, 0, 0);
    auto* ally = AdoptAt(sim, 732, 0, 100, 0);
    Settle(sim);
    ally->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", 5000.0f, 0);
    const float before = ally->GetLife();
    CAbilityHeal spell(7301);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetLife() > before);
    TEST_ASSERT(ally->GetLife() <= ally->GetMaxLife());
    return true;
}

bool TestDispelMagicStripsArea() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 741, 0, 0, 0);
    auto* foe = AdoptAt(sim, 742, 1, 100, 100);
    auto* ally = AdoptAt(sim, 743, 0, 150, 100);
    Settle(sim);
    caster->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    foe->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    ally->AddBuff(std::make_unique<CBuffBloodlust>(30.0f, 0.4f, 0.5f), sim);
    CAbilityDispelMagic spell(7401);
    spell.DoEffect(sim, caster, 125, 100);
    TEST_ASSERT(foe->GetBuff(War3ID("Bblo")) == nullptr);
    TEST_ASSERT(ally->GetBuff(War3ID("Bblo")) == nullptr);
    TEST_ASSERT(caster->GetBuff(War3ID("Bblo")) != nullptr); // caster exempt
    return true;
}

bool TestRejuvenationHealsOverTime() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 751, 0, 0, 0);
    auto* ally = AdoptAt(sim, 752, 0, 100, 0);
    Settle(sim);
    ally->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                 CDamageType::NORMAL, "", 5000.0f, 0);
    CAbilityRejuvenation spell(7501);
    spell.DoEffect(sim, caster, ally);
    TEST_ASSERT(ally->GetBuff(War3ID("Brej")) != nullptr);
    const float afterCast = ally->GetLife();
    for (int i = 0; i < 30; i++) sim.Update(); // regen ticks land
    TEST_ASSERT(ally->GetLife() > afterCast);
    return true;
}

bool TestRoarRalliesAllies() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 761, 0, 0, 0);
    auto* near = AdoptAt(sim, 762, 0, 30, 0);
    auto* far = AdoptAt(sim, 763, 0, 100, 0);
    auto* foe = AdoptAt(sim, 764, 1, 30, 30);
    Settle(sim);
    const int nearBefore = near->GetBonusDamage();
    const int farBefore = far->GetBonusDamage();
    const int foeBefore = foe->GetBonusDamage();
    CAbilityRoar spell(7601);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(near->GetBonusDamage() == nearBefore + 5);
    TEST_ASSERT(far->GetBonusDamage() == farBefore); // outside the 50 radius
    TEST_ASSERT(foe->GetBonusDamage() == foeBefore);
    return true;
}

bool TestDefendHalvesAttacks() {
    CSimulation sim;
    AddPlayers(sim);
    auto* attacker = AdoptAt(sim, 771, 1, 0, 0);
    auto* plain = AdoptAt(sim, 772, 0, 100, 0);
    auto* defender = AdoptAt(sim, 773, 0, 200, 0);
    Settle(sim);
    CAbilityDefend spell(7701);
    spell.DoEffect(sim, defender);
    TEST_ASSERT(defender->GetBuff(War3ID("Bdef")) != nullptr);
    const float plainBefore = plain->GetLife();
    const float defenderBefore = defender->GetLife();
    plain->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                  CDamageType::NORMAL, "", 200.0f, 0);
    defender->Damage(sim, attacker, true, false, CAttackType::NORMAL,
                     CDamageType::NORMAL, "", 200.0f, 0);
    const float plainLoss = plainBefore - plain->GetLife();
    const float defenderLoss = defenderBefore - defender->GetLife();
    TEST_ASSERT(defenderLoss < plainLoss);
    TEST_NEAR(defenderLoss * 2.0f, plainLoss, 0.01f);
    return true;
}

bool TestSpells7FactoryCoverage() {
    const char* codes[] = {"AOsw", "Aply", "Ainf", "Ahea",
                           "Adis", "Arej", "Aroa", "Adef"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(7800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

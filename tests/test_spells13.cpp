// Batch-13 tests: alternate-form morphs, stone form, avatar, siphon
// mana, big bad voodoo.
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
#include "ecs/abilities/spells13.h"
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

CUnitType* MakeSpell13Type() {
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
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, MakeSpell13Type(), player,
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

bool TestMorphRoundTrip(const char* formCode, War3ID buffId, int handleBase) {
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID(formCode))) return true;
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;

    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    if (!caster) return false;
    Settle(sim);
    TEST_ASSERT(!caster->IsMorphed());
    TEST_ASSERT(caster->GetTypeId() == War3ID("hfoo"));
    caster->AddBuff(std::make_unique<CBuffTransform>(buffId, 60.0f, War3ID(formCode)), sim);
    TEST_ASSERT(caster->IsMorphed());
    TEST_ASSERT(caster->GetTypeId() == War3ID(formCode));
    caster->RemoveBuff(buffId, sim); // demorph restores the original
    TEST_ASSERT(!caster->IsMorphed());
    TEST_ASSERT(caster->GetTypeId() == War3ID("hfoo"));
    (void)handleBase;
    return true;
}

} // namespace

bool TestMetamorphosisMorphs() {
    if (!TestMorphRoundTrip("Edem", War3ID("BEme"), 13001)) return false;
    if (!ResourcesPresent()) return true;
    MpqManager mpq;
    CGameplayConstants gc;
    CUnitData unitData;
    LoadUnitData(mpq, gc, unitData);
    if (!unitData.HasUnitType(War3ID("hfoo"))) return true;
    CSimulation sim;
    AddPlayers(sim);
    sim.SetUnitData(&unitData);
    auto* caster = sim.CreateUnit(War3ID("hfoo"), 0, 0, 0, 0);
    if (!caster) return false;
    Settle(sim);
    CAbilityMetamorphosis spell(13002);
    spell.DoEffect(sim, caster);
    if (!unitData.HasUnitType(War3ID("Edem"))) return true;
    TEST_ASSERT(caster->GetBuff(War3ID("BEme")) != nullptr);
    TEST_ASSERT(caster->IsMorphed());
    return true;
}

bool TestBearFormMorphs() {
    return TestMorphRoundTrip("edcm", War3ID("Abrf"), 13101);
}

bool TestRavenFormMorphs() {
    return TestMorphRoundTrip("edot", War3ID("Arav"), 13201);
}

bool TestRoboGoblinMorphs() {
    return TestMorphRoundTrip("Ntin", War3ID("ANrg"), 13301);
}

bool TestStoneFormStatues() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1341, 0, 0, 0);
    auto* foe = AdoptAt(sim, 1342, 1, 300, 0);
    Settle(sim);
    CAbilityStoneForm spell(13401);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("Bstm")) != nullptr);
    TEST_ASSERT(caster->IsInvulnerable());
    TEST_ASSERT(caster->IsRooted());
    const float before = caster->GetLife();
    caster->Damage(sim, foe, true, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 5000.0f, 0);
    TEST_ASSERT(caster->GetLife() == before); // stone takes nothing
    caster->RemoveBuff(War3ID("Bstm"), sim);
    TEST_ASSERT(!caster->IsInvulnerable());
    TEST_ASSERT(!caster->IsRooted());
    return true;
}

bool TestAvatarRages() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1351, 0, 0, 0);
    Settle(sim);
    const int dmgBefore = caster->GetBonusDamage();
    const float maxBefore = caster->GetMaxLife();
    CAbilityAvatar spell(13501);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->GetBuff(War3ID("BAvt")) != nullptr);
    TEST_ASSERT(caster->GetBonusDamage() == dmgBefore + 20);
    TEST_ASSERT(caster->GetMaxLife() == maxBefore + 500.0f);
    const float lifeBefore = caster->GetLife();
    caster->Damage(sim, nullptr, false, false, CAttackType::SPELLS,
                   CDamageType::FIRE, "", 5000.0f, 0);
    TEST_ASSERT(caster->GetLife() == lifeBefore); // spells wash off
    caster->Damage(sim, nullptr, false, false, CAttackType::NORMAL,
                   CDamageType::NORMAL, "", 100.0f, 0);
    TEST_ASSERT(caster->GetLife() < lifeBefore); // steel still bites
    caster->RemoveBuff(War3ID("BAvt"), sim);
    TEST_ASSERT(caster->GetBonusDamage() == dmgBefore);
    TEST_ASSERT(caster->GetMaxLife() == maxBefore);
    return true;
}

bool TestSiphonManaDrains() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1361, 0, 0, 0);
    auto* target = AdoptAt(sim, 1362, 1, 100, 0);
    Settle(sim);
    caster->SetMana(0.0f);
    target->SetMana(200.0f);
    CAbilitySiphonMana spell(13601);
    spell.DoEffect(sim, caster, target);
    TEST_ASSERT(target->GetBuff(War3ID("Bsip")) != nullptr);
    for (int i = 0; i < 30; i++) sim.Update(); // siphon ticks land
    TEST_ASSERT(target->GetMana() < 200.0f);
    TEST_ASSERT(caster->GetMana() > 0.0f);
    return true;
}

bool TestBigBadVoodooShields() {
    CSimulation sim;
    AddPlayers(sim);
    auto* caster = AdoptAt(sim, 1371, 0, 0, 0);
    auto* ally = AdoptAt(sim, 1372, 0, 100, 0);
    auto* far = AdoptAt(sim, 1373, 0, 5000, 5000);
    auto* foe = AdoptAt(sim, 1374, 1, 100, 100);
    Settle(sim);
    CAbilityBigBadVoodoo spell(13701);
    spell.DoEffect(sim, caster);
    TEST_ASSERT(caster->IsInvulnerable());
    TEST_ASSERT(ally->IsInvulnerable());
    TEST_ASSERT(!far->IsInvulnerable());
    TEST_ASSERT(!foe->IsInvulnerable());
    return true;
}

bool TestSpells13FactoryCoverage() {
    const char* codes[] = {"AEme", "Abrf", "Arav", "ANrg",
                           "Astn", "AHav", "AHdr", "AOvd"};
    for (auto code : codes) {
        auto ability = AbilityFactory::CreateAbility(13800, War3ID(code));
        TEST_ASSERT(ability != nullptr);
    }
    return true;
}

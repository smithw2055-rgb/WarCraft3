// Batch-29 tests: entangle gold mine, channel test, drunken brawler dodge.
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/unitdata.h"
#include "ecs/abilities/ability_gold_mine.h"
#include "ecs/abilities/spells28.h"
#include "ecs/behaviors/behavior_harvest.h"
#include "ecs/abilities/spells29.h"
#include "ecs/ability_factory.h"
#include "ecs/csimulation.h"
#include "ecs/orders/order.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

std::unique_ptr<CUnitType> MakeType(const char* id) {
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    return std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID(id),
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
        10, 1, 6, 5, 1, 3, false);
}

CUnitType* SharedType() {
    static std::unique_ptr<CUnitType> keep = MakeType("hfoo");
    return keep.get();
}

CUnit* AdoptAt(CSimulation& sim, int handle, int player, float x, float y) {
    return sim.AdoptUnit(std::make_unique<CUnit>(handle, SharedType(), player,
                                                 War3ID("hfoo"), x, y, 0));
}

} // namespace

bool TestEntangleGoldMinePaysFromParent() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("egol"), MakeType("egol"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* mineUnit = AdoptAt(sim, 2901, 0, 0, 0);
    auto* tree = AdoptAt(sim, 2902, 0, 40, 0);
    auto* wisp = AdoptAt(sim, 2903, 0, 20, 0);
    auto goldAbility = std::make_unique<CAbilityGoldMine>(29901);
    CAbilityGoldMine* gold = goldAbility.get();
    mineUnit->AddAbility(sim, std::move(goldAbility));
    gold->SetGold(100);

    CAbilityEntangleGoldMine entangle(29902);
    entangle.DoEffect(sim, tree, mineUnit);
    CUnit* replacement = entangle.GetReplacement();
    TEST_ASSERT(replacement != nullptr);
    TEST_ASSERT(replacement->GetTypeId() == War3ID("egol"));
    TEST_ASSERT(mineUnit->IsHidden());
    TEST_ASSERT(entangle.GetOriginal() == mineUnit);

    auto* cargo = dynamic_cast<CAbilityCargoHold*>(replacement->GetAbility(War3ID("Aenc")));
    auto* haunted = dynamic_cast<CAbilityEntangledMine*>(replacement->GetAbility(War3ID("Aegm")));
    TEST_ASSERT(cargo != nullptr && haunted != nullptr);
    auto* overlay = dynamic_cast<CAbilityOverlayedGoldMine*>(replacement->GetAbility(War3ID("Agl2")));
    TEST_ASSERT(overlay != nullptr);
    TEST_ASSERT(overlay->GetGold() == 100);
    TEST_ASSERT(mineUnit->IsPaused());
    TEST_ASSERT(cargo->TryLoad(replacement, wisp));

    CPlayer* player = sim.GetPlayer(0);
    for (int i = 0; i < 400 && player->GetGold() == 0; i++) haunted->OnTick(sim, replacement);
    TEST_ASSERT(player->GetGold() == 10);
    TEST_ASSERT(gold->GetGold() == 90);

    gold->SetGold(10);
    for (int i = 0; i < 400 && !replacement->IsDead(); i++) haunted->OnTick(sim, replacement);
    TEST_ASSERT(replacement->IsDead());
    TEST_ASSERT(gold->GetGold() == 0);
    entangle.OnTick(sim, tree);
    TEST_ASSERT(!mineUnit->IsHidden());
    return true;
}

bool TestEntangleConstruction() {
    CSimulation sim;
    CUnitData data;
    data.RegisterUnitType(War3ID("egol"), MakeType("egol"));
    sim.SetUnitData(&data);
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));

    auto* mineUnit = AdoptAt(sim, 2931, 0, 0, 0);
    auto* tree = AdoptAt(sim, 2932, 0, 40, 0);
    mineUnit->AddAbility(sim, std::make_unique<CAbilityGoldMine>(29941));

    CAbilityEntangleGoldMine entangle(29942);
    CBehavior* order = entangle.Begin(sim, tree, 852147, mineUnit);
    delete order;
    entangle.DoEffect(sim, tree, mineUnit);
    CUnit* replacement = entangle.GetReplacement();
    TEST_ASSERT(replacement != nullptr);
    TEST_ASSERT(entangle.IsConstructing());
    TEST_ASSERT(replacement->IsConstructing());
    TEST_ASSERT(replacement->GetLife() < replacement->GetMaxLife() * 0.5f);
    TEST_ASSERT(replacement->GetAbility(War3ID("Aegm"))->IsDisabled());

    for (int i = 0; i < 800 && entangle.IsConstructing(); i++) entangle.OnTick(sim, tree);
    TEST_ASSERT(!entangle.IsConstructing());
    TEST_ASSERT(!replacement->IsConstructing());
    TEST_ASSERT(replacement->GetLife() == replacement->GetMaxLife());
    TEST_ASSERT(!replacement->GetAbility(War3ID("Aegm"))->IsDisabled());

    auto* mineInstant = AdoptAt(sim, 2933, 0, 200, 0);
    auto* treeInstant = AdoptAt(sim, 2934, 0, 240, 0);
    mineInstant->AddAbility(sim, std::make_unique<CAbilityGoldMine>(29943));
    CAbilityEntangleGoldMine instant(29944);
    CBehavior* instantOrder = instant.Begin(sim, treeInstant, 852148, mineInstant);
    delete instantOrder;
    instant.DoEffect(sim, treeInstant, mineInstant);
    TEST_ASSERT(instant.GetReplacement() != nullptr);
    TEST_ASSERT(!instant.IsConstructing());
    TEST_ASSERT(instant.GetReplacement()->GetLife() == instant.GetReplacement()->GetMaxLife());
    return true;
}

bool TestOverlayedMinePaysParent() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    auto* parent = AdoptAt(sim, 2921, 0, 0, 0);
    auto* face = AdoptAt(sim, 2922, 0, 0, 0);
    auto* worker = AdoptAt(sim, 2923, 0, 30, 0);
    auto goldAbility = std::make_unique<CAbilityGoldMine>(29931);
    CAbilityGoldMine* gold = goldAbility.get();
    parent->AddAbility(sim, std::move(goldAbility));
    gold->SetGold(30);
    parent->SetHidden(true);
    parent->SetPaused(true);

    auto overlayAbility = std::make_unique<CAbilityOverlayedGoldMine>(29932);
    CAbilityOverlayedGoldMine* overlay = overlayAbility.get();
    face->AddAbility(sim, std::move(overlayAbility));
    overlay->SetParentMine(parent, gold);
    TEST_ASSERT(overlay->GetGold() == 30);

    CBehaviorHarvest miner;
    miner.SetUnit(worker);
    miner.SetCapacity(10);
    worker->SetHidden(true);
    overlay->AddMiner(&miner);
    overlay->OnTick(sim, face);
    TEST_ASSERT(gold->GetGold() == 20);
    TEST_ASSERT(!worker->IsHidden());

    gold->SetGold(0);
    overlay->OnTick(sim, face);
    TEST_ASSERT(face->IsDead());
    face->Update(sim);
    TEST_ASSERT(!parent->IsHidden());
    TEST_ASSERT(!parent->IsPaused());
    return true;
}

bool TestChannelAndDrunkenBrawler() {
    CSimulation sim;
    auto* caster = AdoptAt(sim, 2911, 0, 0, 0);
    auto* hero = AdoptAt(sim, 2912, 0, 10, 0);
    CAbilityChannelTest channel(29911);
    channel.DoEffect(sim, caster);
    TEST_ASSERT(channel.IsChanneling());
    for (int i = 0; i < 80 && channel.IsChanneling(); i++) channel.OnTick(sim, caster);
    TEST_ASSERT(!channel.IsChanneling());

    CAbilityDrunkenBrawler brawler(29912);
    brawler.OnAdd(sim, hero);
    TEST_ASSERT(hero->GetCritChance() > 0.0f);
    TEST_ASSERT(hero->GetEvasionChance() > 0.0f);
    brawler.OnRemove(sim, hero);
    TEST_ASSERT(hero->GetEvasionChance() == 0.0f);
    return true;
}

bool TestSpells29FactoryCoverage() {
    const char* codes[] = {"Aent", "Aegm", "Aenc", "ANcl", "ANdb"};
    for (const char* code : codes) {
        auto ability = AbilityFactory::CreateAbility(29920, War3ID(code));
        if (!ability) {
            std::cerr << "missing factory ability " << code << "\n";
            return false;
        }
    }
    TEST_ASSERT(dynamic_cast<CAbilityCriticalStrike*>(
        AbilityFactory::CreateAbility(29921, War3ID("ANdb")).get()) != nullptr);
    TEST_ASSERT(dynamic_cast<CAbilityDrunkenBrawler*>(
        AbilityFactory::CreateAbility(29922, War3ID("ANdb")).get()) != nullptr);
    return true;
}

bool TestSpells29OrderIds() {
    TEST_ASSERT(SpellBaseOrderId(War3ID("Aent")) == 852147);
    TEST_ASSERT(SpellMatchesOrder(War3ID("Aent"), 852148));
    TEST_ASSERT(SpellBaseOrderId(War3ID("ANcl")) == 852600);
    return true;
}

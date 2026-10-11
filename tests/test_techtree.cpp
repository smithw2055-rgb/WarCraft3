// Techtree requirement tests: flat Requires with levels, hero tier
// lists, possession unlock lifecycle (adopt/kill/remove), and training
// gates in the queue and tavern paths.
#include <iostream>
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/tech_tree.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"

#define TEST_ASSERT(cond) do { if (!(cond)) { std::cerr << "ASSERT @" << __LINE__ << ": " #cond "\n"; return false; } } while (0)

namespace {

std::unique_ptr<CUnitType> MakeReqType(const char* id, bool hero,
                                       std::vector<CUnitTypeRequirement> reqs = {},
                                       std::vector<std::vector<CUnitTypeRequirement>> tiers = {}) {
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
        0.5f, 0.6f, reqs, tiers,
        1, hero, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false);
}

void Settle(CSimulation& sim) {
    for (int i = 0; i < 4; i++) sim.Update();
}

} // namespace

bool TestTechtreeUnlockLifecycle() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitData data;
    data.RegisterUnitType(War3ID("hkee"), MakeReqType("hkee", false));
    data.RegisterUnitType(War3ID("hfoo"), MakeReqType("hfoo", false));
    sim.SetUnitData(&data);
    CPlayer* player = sim.GetPlayer(0);

    // Adopting unlocks; killing a non-hero drops it.
    auto* keep = sim.AdoptUnit(std::make_unique<CUnit>(401, data.GetUnitType(War3ID("hkee")),
                                                       0, War3ID("hkee"), 0, 0, 0));
    Settle(sim);
    TEST_ASSERT(player->IsTechUnlocked(War3ID("hkee")));
    TEST_ASSERT(player->GetTechLevel(War3ID("hkee")) == 1);
    keep->SetLife(sim, 0.0f);
    Settle(sim);
    TEST_ASSERT(keep->IsDead());
    TEST_ASSERT(!player->IsTechUnlocked(War3ID("hkee")));

    // Live removal drops it too.
    auto* foot = sim.AdoptUnit(std::make_unique<CUnit>(402, data.GetUnitType(War3ID("hfoo")),
                                                       0, War3ID("hfoo"), 0, 0, 0));
    Settle(sim);
    TEST_ASSERT(player->IsTechUnlocked(War3ID("hfoo")));
    sim.RemoveUnit(foot);
    Settle(sim);
    TEST_ASSERT(!player->IsTechUnlocked(War3ID("hfoo")));
    return true;
}

bool TestTechtreeFlatRequirementsGate() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitData data;
    data.RegisterUnitType(War3ID("hkee"), MakeReqType("hkee", false));
    // Workshop needs a Keep standing.
    data.RegisterUnitType(War3ID("harm"), MakeReqType("harm", false,
        std::vector<CUnitTypeRequirement>{{War3ID("hkee"), 1}}));
    sim.SetUnitData(&data);

    TEST_ASSERT(!TechTree::MeetsRequirements(sim, 0, War3ID("harm")));
    TEST_ASSERT(TechTree::GetMissingRequirements(sim, 0, War3ID("harm")).size() == 1);
    auto* keep = sim.AdoptUnit(std::make_unique<CUnit>(411, data.GetUnitType(War3ID("hkee")),
                                                       0, War3ID("hkee"), 0, 0, 0));
    Settle(sim);
    TEST_ASSERT(TechTree::MeetsRequirements(sim, 0, War3ID("harm")));
    // Razed Keep closes the gate again.
    keep->SetLife(sim, 0.0f);
    Settle(sim);
    TEST_ASSERT(!TechTree::MeetsRequirements(sim, 0, War3ID("harm")));
    return true;
}

bool TestHeroTierGatesSecondHero() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    CUnitData data;
    data.RegisterUnitType(War3ID("hkee"), MakeReqType("hkee", false));
    data.RegisterUnitType(War3ID("halt"), MakeReqType("halt", false));
    // Archmage: tier 1 (2nd hero) needs a Keep; 1st hero free.
    data.RegisterUnitType(War3ID("Hamg"), MakeReqType("Hamg", true, {},
        std::vector<std::vector<CUnitTypeRequirement>>{{{War3ID("hkee"), 1}}}));
    sim.SetUnitData(&data);
    CPlayer* player = sim.GetPlayer(0);
    player->SetGold(5000);
    player->SetLumber(2000);
    player->SetFoodCap(100);

    auto* altar = sim.AdoptUnit(std::make_unique<CUnit>(421, data.GetUnitType(War3ID("halt")),
                                                        0, War3ID("halt"), 0, 0, 0));
    altar->AddAbility(sim, std::make_unique<CAbilityQueue>(422));
    auto* queue = dynamic_cast<CAbilityQueue*>(altar->GetAbility(War3ID("Aque")));
    TEST_ASSERT(queue != nullptr);
    queue->AddTrainedUnit(War3ID("Hamg"));
    Settle(sim);

    // First hero trains with no Keep standing.
    queue->QueueUnit(sim, altar, War3ID("Hamg"));
    TEST_ASSERT(queue->GetQueueCount() == 1);
    // Second hero refused until a Keep stands (tier 1 = [hkee]).
    auto* hero1 = sim.AdoptUnit(std::make_unique<CUnit>(423, data.GetUnitType(War3ID("Hamg")),
                                                        0, War3ID("Hamg"), 100, 0, 0));
    (void)hero1;
    Settle(sim);
    const int before = queue->GetQueueCount();
    queue->QueueUnit(sim, altar, War3ID("Hamg"));
    TEST_ASSERT(queue->GetQueueCount() == before); // still gated
    // Raise the Keep: gate opens.
    sim.AdoptUnit(std::make_unique<CUnit>(424, data.GetUnitType(War3ID("hkee")),
                                          0, War3ID("hkee"), 200, 0, 0));
    Settle(sim);
    queue->QueueUnit(sim, altar, War3ID("Hamg"));
    TEST_ASSERT(queue->GetQueueCount() == before + 1);
    return true;
}

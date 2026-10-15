// P1-12 w3i techtree availability: bans/caps gate training and research,
// and the loader applies the w3i lists at map start.
#include <memory>
#include <vector>

#include "data/combat_types.h"
#include "data/unitdata.h"
#include "ecs/abilities/ability_queue.h"
#include "ecs/csimulation.h"
#include "ecs/player.h"
#include "ecs/unit.h"
#include "ecs/unit_type.h"
#include "data/upgradedata.h"
#include "map/w3i.h"

#define TEST_ASSERT(cond) do { if (!(cond)) return false; } while (0)

namespace {

CUnitType* MakeAvailType() {
    static std::vector<std::unique_ptr<CUnitType>> keep;
    CUnitAttack atk;
    atk.range = 100.0f;
    atk.cooldown = 1.0f;
    atk.damageBase = 10;
    keep.push_back(std::make_unique<CUnitType>(
        "Test", "Test", "icon", War3ID("hfoo"),
        10000, 0.0f, 0.5f, CRegenType::ALWAYS,
        100, 200, 300, 0, War3ID(),
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
        0.5f, 10.0f, std::vector<CUnitTypeRequirement>{},
        std::vector<std::vector<CUnitTypeRequirement>>{},
        1, false, 20, 2.0f, 15, 1.5f, 10, 1.0f,
        CPrimaryAttribute::STRENGTH, std::vector<War3ID>{},
        std::vector<std::string>{"Bob"}, 1, false, 1, false, 100,
        0.3f, 0.3f, false, false, 1, 800, 600, false,
        10, 1, 6, 5, 1, 3, false));
    return keep.back().get();
}

struct AvailFixture {
    CSimulation sim;
    CUnitData unitData;
    CUpgradeData upgradeData;
    CUnit* factory = nullptr;
    CAbilityQueue* queue = nullptr;
    int trainEvents = 0;
    int researchEvents = 0;

    AvailFixture() {
        auto player = std::make_unique<CPlayer>(0, "P0", CMapControl::USER);
        player->SetGold(100000);
        player->SetLumber(100000);
        player->SetFoodCap(100);
        sim.AddPlayer(std::move(player));
        unitData.RegisterUnitType(War3ID("hfoo"),
                                  std::make_unique<CUnitType>(*MakeAvailType()));
        sim.SetUnitData(&unitData);
        auto upgrade = std::make_unique<CUpgradeType>();
        upgrade->id = War3ID("R001");
        upgrade->goldCost = 50;
        upgrade->maxLevel = 2;
        upgradeData.RegisterUpgrade(War3ID("R001"), std::move(upgrade));
        sim.SetUpgradeData(&upgradeData);
        factory = sim.AdoptUnit(std::make_unique<CUnit>(
            501, MakeAvailType(), 0, War3ID("hbar"), 0, 0, 0));
        factory->AddAbility(sim, std::make_unique<CAbilityQueue>(502));
        queue = dynamic_cast<CAbilityQueue*>(factory->GetAbility(War3ID("Aque")));
        queue->AddTrainedUnit(War3ID("hfoo"));
        queue->AddResearch(War3ID("R001"));
        sim.SetQueueEventCallback([this](int, int, int kind, bool started) {
            if (!started) return;
            if (kind == 0) trainEvents++;
            if (kind == 1) researchEvents++;
        });
        sim.Update();
        sim.Update();
    }
};

} // namespace

// Cap counting: banned (0) refuses, uncapped (-1) allows, a reached cap
// refuses while headroom allows.
bool TestTechCapCountsPossession() {
    CSimulation sim;
    CPlayer player(0, "P0", CMapControl::USER);
    TEST_ASSERT(player.IsTechtreeAllowedByMax(War3ID("hpea")));
    player.SetTechMaxAllowed(War3ID("hpea"), 0);
    TEST_ASSERT(!player.IsTechtreeAllowedByMax(War3ID("hpea")));
    player.SetTechMaxAllowed(War3ID("hpea"), 2);
    TEST_ASSERT(player.IsTechtreeAllowedByMax(War3ID("hpea")));
    player.AddTechUnlocked(sim, War3ID("hpea"));
    player.AddTechUnlocked(sim, War3ID("hpea"));
    TEST_ASSERT(!player.IsTechtreeAllowedByMax(War3ID("hpea")));
    return true;
}

// A banned unit cannot be trained (no event, no gold taken); lifting the
// ban restores training.
bool TestTechBanBlocksTraining() {
    AvailFixture f;
    TEST_ASSERT(f.queue != nullptr);
    CPlayer* player = f.sim.GetPlayer(0);
    const int goldBefore = player->GetGold();
    player->SetTechMaxAllowed(War3ID("hfoo"), 0);
    f.queue->QueueUnit(f.sim, f.factory, War3ID("hfoo"));
    TEST_ASSERT(f.trainEvents == 0);
    TEST_ASSERT(player->GetGold() == goldBefore);
    player->SetTechMaxAllowed(War3ID("hfoo"), -1);
    f.queue->QueueUnit(f.sim, f.factory, War3ID("hfoo"));
    TEST_ASSERT(f.trainEvents == 1);
    return true;
}

// A banned upgrade cannot be researched.
bool TestTechBanBlocksResearch() {
    AvailFixture f;
    TEST_ASSERT(f.queue != nullptr);
    CPlayer* player = f.sim.GetPlayer(0);
    const int goldBefore = player->GetGold();
    player->SetTechMaxAllowed(War3ID("R001"), 0);
    f.queue->QueueResearch(f.sim, f.factory, War3ID("R001"));
    TEST_ASSERT(f.researchEvents == 0);
    TEST_ASSERT(player->GetGold() == goldBefore);
    player->SetTechMaxAllowed(War3ID("R001"), -1);
    f.queue->QueueResearch(f.sim, f.factory, War3ID("R001"));
    TEST_ASSERT(f.researchEvents == 1);
    return true;
}

// Loader application: tech entries ban flagged players only; upgrade
// entries map 0->ban, 1->unlimited, 2->researched at levelAffected+1.
bool TestApplyW3iAvailability() {
    CSimulation sim;
    sim.AddPlayer(std::make_unique<CPlayer>(0, "P0", CMapControl::USER));
    sim.AddPlayer(std::make_unique<CPlayer>(1, "P1", CMapControl::USER));
    W3iInfo w3i;
    W3iTechAvailability ban;
    ban.playerFlags = 1u << 1; // player 1 only
    ban.id = War3ID("hfoo").value;
    w3i.techAvailability.push_back(ban);
    W3iUpgradeAvailability noUp;
    noUp.playerFlags = 0xFFFFFFFFu;
    noUp.id = War3ID("R001").value;
    noUp.levelAffected = 0;
    noUp.availability = 0;
    w3i.upgradeAvailability.push_back(noUp);
    W3iUpgradeAvailability yesUp;
    yesUp.playerFlags = 1u << 0;
    yesUp.id = War3ID("R002").value;
    yesUp.levelAffected = 0;
    yesUp.availability = 1;
    w3i.upgradeAvailability.push_back(yesUp);
    W3iUpgradeAvailability doneUp;
    doneUp.playerFlags = 1u << 0;
    doneUp.id = War3ID("R003").value;
    doneUp.levelAffected = 1; // 0-based -> researched level 2
    doneUp.availability = 2;
    w3i.upgradeAvailability.push_back(doneUp);
    ApplyW3iTechtreeAvailability(sim, w3i);
    CPlayer* p0 = sim.GetPlayer(0);
    CPlayer* p1 = sim.GetPlayer(1);
    TEST_ASSERT(p0->GetTechMaxAllowed(War3ID("hfoo")) == -1); // unflagged
    TEST_ASSERT(p1->GetTechMaxAllowed(War3ID("hfoo")) == 0); // banned
    TEST_ASSERT(!p1->IsTechtreeAllowedByMax(War3ID("hfoo")));
    TEST_ASSERT(p0->GetTechMaxAllowed(War3ID("R001")) == 0);
    TEST_ASSERT(p0->GetTechMaxAllowed(War3ID("R002")) == -1);
    TEST_ASSERT(p0->GetTechMaxAllowed(War3ID("R003")) == -1);
    TEST_ASSERT(p0->GetTechLevel(War3ID("R003")) == 2);
    return true;
}
